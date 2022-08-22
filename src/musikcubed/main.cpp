#ifdef EV_ERROR
    #undef EV_ERROR
#endif

#include <iostream>
#include <fstream>
#include <csignal>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <signal.h>
#include <ev++.h>

#include <musikcore/audio/PlaybackService.h>
#include <musikcore/audio/MasterTransport.h>
#include <musikcore/debug.h>
#include <musikcore/library/LibraryFactory.h>
#include <musikcore/plugin/Plugins.h>
#include <musikcore/runtime/MessageQueue.h>
#include <musikcore/runtime/Message.h>
#include <musikcore/support/PreferenceKeys.h>
#include <musikcore/support/Common.h>

#include <boost/locale.hpp>
#include <boost/filesystem/detail/utf8_codecvt_facet.hpp>

#include "../musikcore/version.h"

using namespace musik;
using namespace musik::core;
using namespace musik::core::audio;
using namespace musik::core::runtime;

static const char* DEFAULT_LOCKFILE = "/tmp/musikcubed.lock";
static const char* LOCKFILE_OVERRIDE = "MUSIKCUBED_LOCKFILE_OVERRIDE";
static const short EVENT_DISPATCH = 1;
static const short EVENT_QUIT = 2;
static const pid_t NOT_RUNNING = (pid_t) -1;
static int pipeFd[2] = { 0 };
static bool foreground = false;

static void printHelp();
static void handleCommandLine(int argc, char** argv);
static void exitIfRunning();
static pid_t getDaemonPid();
static void initForeground();
static void initDaemon();
static void stopDaemon();
static void initUtf8();
static void run();

class EvMessageQueue: public MessageQueue {
    public:
        void Post(IMessagePtr message, int64_t delayMs) {
            MessageQueue::Post(message, delayMs);

            if (delayMs <= 0) {
                write(pipeFd[1], &EVENT_DISPATCH, sizeof(EVENT_DISPATCH));
            }
            else {
                double delayTs = (double) delayMs / 1000.0;
                loop.once<
                    EvMessageQueue,
                    &EvMessageQueue::DelayedDispatch
                >(-1, ev::TIMER, (ev::tstamp) delayTs, this);
            }
        }

        void DelayedDispatch(int revents) {
            this->Dispatch();
        }

        static void SignalQuit(ev::sig& signal, int revents) {
            write(pipeFd[1], &EVENT_QUIT, sizeof(EVENT_QUIT));
        }

        void ReadCallback(ev::io& watcher, int revents) {
            short type;
            if (read(pipeFd[0], &type, sizeof(type)) == 0) {
                std::cerr << "read() failed.\n";
                exit(EXIT_FAILURE);
            }
            switch (type) {
                case EVENT_DISPATCH: this->Dispatch(); break;
                case EVENT_QUIT: loop.break_loop(ev::ALL); break;
            }
        }

        void Run() {
            io.set(loop);
            io.set(pipeFd[0], ev::READ);
            io.set<EvMessageQueue, &EvMessageQueue::ReadCallback>(this);
            io.start();

            sio.set(loop);
            sio.set<&EvMessageQueue::SignalQuit>();
            sio.start(SIGTERM);

            write(pipeFd[1], &EVENT_DISPATCH, sizeof(EVENT_DISPATCH));

            loop.run(0);
        }

    private:
        ev::dynamic_loop loop;
        ev::io io;
        ev::sig sio;
};

static void printHelp() {
    std::cout << "\n  musikcubed:\n";
    std::cout << "    --start: start the daemon\n";
    std::cout << "    --foreground: start the in the foreground\n";
    std::cout << "    --stop: shut down the daemon\n";
    std::cout << "    --running: check if the daemon is running\n";
    std::cout << "    --version: print the version\n";
    std::cout << "    --help: show this message\n\n";
}

static void handleCommandLine(int argc, char** argv) {
    if (argc >= 2) {
        const std::string command = std::string(argv[1]);
        if (command == "--start") {
            return;
        }
        else if (command == "--foreground") {
            std::cout << "\n  musikcubed starting in the foreground...\n\n";
            ::foreground = true;
            return;
        }
        else if (command == "--stop") {
            stopDaemon();
        }
        else if (command == "--version") {
            std::cout << "\n  musikcubed version: " << VERSION << " " << VERSION_COMMIT_HASH << "\n\n";
        }
        else if (command == "--running") {
            pid_t pid = getDaemonPid();
            if (pid == NOT_RUNNING) {
                std::cout << "\n  musikcubed is NOT running\n\n";
            }
            else {
                std::cout << "\n  musikcubed is running with pid " << pid << "\n\n";
            }
        }
        else {
            printHelp();
        }
        exit(EXIT_SUCCESS);
    }
}

static std::string getLockfileFn() {
    std::string result = DEFAULT_LOCKFILE;
    const char* userLock = std::getenv(LOCKFILE_OVERRIDE);
    if (userLock && strlen(userLock)) {
        result = userLock;
    }
    return result;
}

static void stopDaemon() {
    pid_t pid = getDaemonPid();
    if (pid == NOT_RUNNING) {
        std::cout << "\n  musikcubed is not running\n\n";
    }
    else {
        std::cout << "\n  stopping musikcubed...";
        kill(pid, SIGTERM);
        int count = 0;
        bool dead = false;
        while (!dead && count++ < 7) { /* try for 7 seconds */
            if (kill(pid, 0) == 0) {
                std::cout << ".";
                std::cout.flush();
                usleep(500000);
            }
            else {
                dead = true;
            }
        }
        std::cout << (dead ? " success" : " failed") << "\n\n";
        if (!dead) {
            exit(EXIT_FAILURE);
        }
    }
}

static pid_t getDaemonPid() {
    std::ifstream lock(getLockfileFn());
    if (lock.good()) {
        int pid;
        lock >> pid;
        if (kill((pid_t) pid, 0) == 0) {
            return pid;
        }
    }
    return NOT_RUNNING;
}

static void exitIfRunning() {
    if (getDaemonPid() != NOT_RUNNING) {
        std::cerr << "\n musikcubed is already running!\n\n";
        exit(EXIT_SUCCESS);
    }
    std::cerr << "\n  musikcubed is starting...\n\n";
}

static void initDaemon() {
    pid_t pid = fork();

    if (pid < 0) {
        exit(EXIT_FAILURE);
    }

    if (pid > 0) {
        exit(EXIT_SUCCESS);
    }

    umask(0);

    pid_t sid = setsid();
    if (sid < 0) {
        exit(EXIT_SUCCESS);
    }

    if (chdir("/") < 0) {
        exit(EXIT_FAILURE);
    }

    if (pipe(pipeFd) != 0) {
        std::cerr << "\n  ERROR! couldn't create pipe\n\n";
        exit(EXIT_FAILURE);
    }

    close(STDIN_FILENO);
    close(STDOUT_FILENO);
    freopen("/tmp/musikcube.log", "w", stderr);

    std::ofstream lock(getLockfileFn());
    if (lock.good()) {
        lock << std::to_string((int) getpid());
    }

    debug::Start({
        new debug::SimpleFileBackend()
    });
}

static void initForeground() {
    if (pipe(pipeFd) != 0) {
        std::cerr << "\n  ERROR! couldn't create pipe\n\n";
        exit(EXIT_FAILURE);
    }

    std::ofstream lock(getLockfileFn());
    if (lock.good()) {
        lock << std::to_string((int) getpid());
    }

    debug::Start({
        new debug::ConsoleBackend(),
        new debug::SimpleFileBackend()
    });
}

static void initUtf8() {
    std::locale locale = std::locale();
    std::locale utf8Locale(locale, new boost::filesystem::detail::utf8_codecvt_facet);
    boost::filesystem::path::imbue(utf8Locale);
}

static void rescanHandler(int signal) {
    debug::info("daemon", "received SIGUSR1, rescanning the library...");
    auto library = LibraryFactory::Instance().DefaultLocalLibrary();
    library->Indexer()->Schedule(IIndexer::SyncType::All);
}

int main(int argc, char** argv) {
    initUtf8();
    std::cout << "\n  using lockfile at: " << getLockfileFn();
    handleCommandLine(argc, argv);
    exitIfRunning();

    ::foreground ? initForeground() : initDaemon();

    srand((unsigned int) time(0));

    std::signal(SIGUSR1, rescanHandler);

    plugin::Init();

    EvMessageQueue messageQueue;
    LibraryFactory::Initialize(messageQueue);
    auto library = LibraryFactory::Instance().DefaultLocalLibrary();

    {
        PlaybackService playback(messageQueue, library);

        plugin::Start(&messageQueue, &playback, library);

        auto prefs = Preferences::ForComponent(prefs::components::Settings);
        if (prefs->GetBool(prefs::keys::SyncOnStartup, true)) {
            library->Indexer()->Schedule(IIndexer::SyncType::All);
        }

        messageQueue.Run();

        library->Indexer()->Shutdown();
    }

    plugin::Shutdown();

    remove(getLockfileFn().c_str());
}
