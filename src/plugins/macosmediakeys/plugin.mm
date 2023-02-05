//////////////////////////////////////////////////////////////////////////////
//
// Copyright (c) 2004-2023 musikcube team
//
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//
//    * Redistributions of source code must retain the above copyright notice,
//      this list of conditions and the following disclaimer.
//
//    * Redistributions in binary form must reproduce the above copyright
//      notice, this list of conditions and the following disclaimer in the
//      documentation and/or other materials provided with the distribution.
//
//    * Neither the name of the author nor the names of other contributors may
//      be used to endorse or promote products derived from this software
//      without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
// ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
// LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
// CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
// SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
// INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
// CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
// ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
// POSSIBILITY OF SUCH DAMAGE.
//
//////////////////////////////////////////////////////////////////////////////

#include <musikcore/sdk/constants.h>
#include <musikcore/sdk/IPlugin.h>
#include <musikcore/sdk/IPlaybackRemote.h>
#include <musikcore/sdk/IDebug.h>
#include <mutex>
#include <iostream>
#include "SPMediaKeyTap.h"

using namespace musik::core::sdk;

static const char* TAG = "macosmediakeys";
static IDebug* debug = nullptr;

struct IKeyProcessor {
    virtual void ProcessKeyCode(int keyCode) = 0;
};

@interface MediaKeys: NSObject {
    SPMediaKeyTap* keyTap;
    IKeyProcessor* processor;
}
- (BOOL) register;
- (void) unregister;
- (void) setKeyProcessor: (IKeyProcessor*) processor;
@end

static class Plugin: public IPlugin {
    public:
        void Release() override { }
        const char* Name() override { return "MediaKeys IPlaybackRemote"; }
        const char* Version() override { return MUSIKCUBE_VERSION_WITH_COMMIT_HASH; }
        const char* Author() override { return "clangen"; }
        const char* Guid() override { return "e850a2eb-5aaa-4322-b63e-bf1c1593805b"; }
        bool Configurable() override { return false; }
        void Configure() override { }
        void Reload() override { }
        int SdkVersion() override { return musik::core::sdk::SdkVersion; }
} plugin;

class PlaybackRemote: public IPlaybackRemote, IKeyProcessor {
    public:
        PlaybackRemote() {
            this->mediaKeys = [[MediaKeys alloc] init];
            if ([this->mediaKeys register]) {
                [this->mediaKeys setKeyProcessor: this];
                debug->Info(TAG, "listening to media keys");
            }
            else {
                debug->Error(TAG, "failed to register media keys listener");
                this->Unregister();
            }
        }

        void Release() override {
            this->Unregister();
            delete this;
        }

        void Unregister() {
            if (this->mediaKeys) {
                [this->mediaKeys unregister];
                [this->mediaKeys release];
                this->mediaKeys = nullptr;
                debug->Info(TAG, "unregistered media keys");
            }
        }

        void SetPlaybackService(IPlaybackService* playback) override {
            std::unique_lock<decltype(mutex)> lock(mutex);
            this->playback = playback;
        }

        void ProcessKeyCode(int keyCode) override {
            std::unique_lock<decltype(mutex)> lock(mutex);
            if (this->playback) {
                switch (keyCode) {
                    case NX_KEYTYPE_PLAY: {
                        auto state = this->playback->GetPlaybackState();
                        if (state == PlaybackState::Stopped) {
                            if (this->playback->Count()) {
                                this->playback->Play(0);
                            }
                        }
                        else {
                            this->playback->PauseOrResume();
                        }
                    } return;
                    case NX_KEYTYPE_FAST:
                    case NX_KEYTYPE_NEXT:
                        this->playback->Next();
                        return;
                    case NX_KEYTYPE_REWIND:
                    case NX_KEYTYPE_PREVIOUS:
                        this->playback->Previous();
                        return;
                }
            }
        }

        void OnTrackChanged(ITrack* track) override { }
        void OnPlaybackStateChanged(PlaybackState state) override { }
        void OnPlaybackTimeChanged(double time) override { }
        void OnVolumeChanged(double volume) override { }
        void OnModeChanged(RepeatMode repeatMode, bool shuffled) override { }
        void OnPlayQueueChanged() override { }

    private:
        std::mutex mutex;
        IPlaybackService* playback { nullptr };
        MediaKeys* mediaKeys { nullptr };
};

@implementation MediaKeys
- (id) init {
    keyTap = [[SPMediaKeyTap alloc] initWithDelegate:self];
    processor = nil;

    // Register defaults for the whitelist of apps that want to use media keys
    [[NSUserDefaults standardUserDefaults] registerDefaults:[NSDictionary dictionaryWithObjectsAndKeys:
        [SPMediaKeyTap defaultMediaKeyUserBundleIdentifiers], kMediaKeyUsingBundleIdentifiersDefaultsKey,
    nil]];

    return self;
}

- (void) setKeyProcessor: (IKeyProcessor*) keyProcessor {
    processor = keyProcessor;
}

- (void) mediaKeyTap:(SPMediaKeyTap*) keyTap receivedMediaKeyEvent: (NSEvent*) event {
    assert([event type] == NSSystemDefined && [event subtype] == SPSystemDefinedEventMediaKeys);

    int keyCode = (([event data1] & 0xFFFF0000) >> 16);
    int keyFlags = ([event data1] & 0x0000FFFF);
    int keyState = (((keyFlags & 0xFF00) >> 8)) == 0xA;

    if (keyState == 1) {
        processor->ProcessKeyCode(keyCode);
    }
}

- (BOOL) register {
    if ([SPMediaKeyTap usesGlobalMediaKeyTap]) {
        return [keyTap startWatchingMediaKeys];
    }
    return NO;
}

- (void) unregister {
    if (keyTap) {
        [keyTap stopWatchingMediaKeys];
        [keyTap release];
    }
}
@end

extern "C" IPlugin* GetPlugin() {
    return &plugin;
}

extern "C" IPlaybackRemote* GetPlaybackRemote() {
    return new PlaybackRemote();
}

extern "C" void SetDebug(IDebug* debug) {
    ::debug = debug;
}