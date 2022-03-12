//////////////////////////////////////////////////////////////////////////////
//
// Copyright (c) 2004-2021 musikcube team
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

#include <stdafx.h>
#include "TransportWindow.h"

#include <cursespp/Screen.h>
#include <cursespp/Colors.h>
#include <cursespp/Text.h>

#include <musikcore/support/Duration.h>

#include <musikcore/debug.h>
#include <musikcore/library/LocalLibraryConstants.h>
#include <musikcore/support/PreferenceKeys.h>
#include <musikcore/runtime/Message.h>
#include <musikcore/support/Playback.h>

#include <app/util/Hotkeys.h>
#include <app/util/Messages.h>
#include <app/overlay/PlayQueueOverlays.h>

#include <limits.h>

#include <algorithm>
#include <memory>
#include <deque>
#include <chrono>
#include <map>

using namespace musik::core;
using namespace musik::core::audio;
using namespace musik::core::library;
using namespace musik::core::db;
using namespace musik::core::sdk;
using namespace musik::core::runtime;
using namespace musik::cube;
using namespace std::chrono;
using namespace cursespp;

#define REFRESH_INTERVAL_MS 1000
#define DEFAULT_TIME -1.0f
#define TIME_SLOP 3.0f

#define MIN_WIDTH 20
#define MIN_HEIGHT 2

#define DEBOUNCE_REFRESH(mode, delay) \
    this->Debounce(message::RefreshTransport, (int64_t) mode, 0, delay);

#define ON(w, a) if (a != Color::Default) { wattron(w, a); }
#define OFF(w, a) if (a != Color::Default) { wattroff(w, a); }

static const std::string kStateToken = "$state";
static const std::string kTitleToken = "$title";
static const std::string kArtistToken = "$artist";
static const std::string kAlbumToken = "$album";

struct Token {
    enum Type { Normal, Placeholder };

    static std::unique_ptr<Token> New(const std::string& value, Type type) {
        return std::make_unique<Token>(value, type);
    }

    Token(const std::string& value, Type type) {
        this->value = value;
        this->type = type;
    }

    std::string value;
    Type type;
};

typedef std::unique_ptr<Token> TokenPtr;
typedef std::vector<TokenPtr> TokenList;

/* tokenizes an input string that has $placeholder values */
void tokenize(const std::string& format, TokenList& tokens) {
    tokens.clear();
    Token::Type type = Token::Normal;
    size_t i = 0;
    size_t start = 0;
    while (i < format.size()) {
        const char c = format[i];
        if ((type == Token::Placeholder && c == ' ') ||
            (type == Token::Normal && c == '$')) {
            /* escape $ with $$ */
            if (c == '$' && i < format.size() - 1 && format.at(i + 1) == '$') {
                i++;
            }
            else {
                if (i > start) {
                    tokens.push_back(Token::New(format.substr(start, i - start), type));
                }
                start = i;
                type = (c == ' ')  ? Token::Normal : Token::Placeholder;
            }
        }
        ++i;
    }

    if (i > 0) {
        tokens.push_back(Token::New(format.substr(start, i - start), type));
    }
}

/* a cache of localized, pre-formatted strings we use every second. */
static struct StringCache {
    std::string PLAYING_FORMAT;
    std::string PLAYING;
    std::string BUFFERING;
    std::string STOPPED;
    std::string EMPTY_SONG;
    std::string EMPTY_ALBUM;
    std::string EMPTY_ARTIST;
    std::string SHUFFLE;
    std::string MUTED;
    std::string VOLUME;
    std::string REPEAT_LIST;
    std::string REPEAT_TRACK;
    std::string REPEAT_OFF;

    void Initialize() {
        PLAYING_FORMAT = _TSTR("transport_playing_format");
        PLAYING = _TSTR("transport_playing_format_playing");
        BUFFERING = _TSTR("transport_playing_format_buffering");
        STOPPED = _TSTR("transport_stopped");
        EMPTY_SONG = _TSTR("transport_empty_song");
        EMPTY_ALBUM = _TSTR("transport_empty_album");
        EMPTY_ARTIST = _TSTR("transport_empty_artist");
        SHUFFLE = "  " + _TSTR("transport_shuffle");
        MUTED = _TSTR("transport_muted") + "  ";
        VOLUME = _TSTR("transport_volume") + " ";
        REPEAT_LIST = "  " + _TSTR("transport_repeat_list");
        REPEAT_TRACK = "  " + _TSTR("transport_repeat_track");
        REPEAT_OFF = "  " + _TSTR("transport_repeat_off");
    }
} Strings;

/* ~~~~~~~~~~ TransportWindow::DisplayCache ~~~~~~~~~~ */

void TransportWindow::DisplayCache::Reset() {
    track.reset();
    title = album = artist = "";
    titleCols = albumCols = artistCols;
    secondsTotal = 0;
    totalTime = "0:00";
    totalTimeCols = 4;
}

size_t TransportWindow::DisplayCache::Columns(const std::string& str) {
    auto it = stringToColumns.find(str);
    if (it == stringToColumns.end()) {
        stringToColumns[str] = u8cols(str);
    }
    return stringToColumns[str];
}

std::string TransportWindow::DisplayCache::CurrentTime(int secondsCurrent) {
    if (secondsTotal != INT_MIN) {
        secondsCurrent = std::max(0, std::min(secondsCurrent, secondsTotal));
    }
    return musik::core::duration::Duration(secondsCurrent);
}

void TransportWindow::DisplayCache::Update(ITransport& transport, TrackPtr track) {
    /* some params don't update regularly at all, so we can safely
    cache them as long as the track hasn't actually changed. */
    if (this->track != track) {
        this->Reset();

        this->track = track;

        if (this->track) {
            title = this->track->GetString(constants::Track::TITLE);
            title = title.size() ? title : Strings.EMPTY_SONG;
            titleCols = narrow_cast<int>(u8cols(title));

            album = this->track->GetString(constants::Track::ALBUM);
            album = album.size() ? album : Strings.EMPTY_ALBUM;
            albumCols = narrow_cast<int>(u8cols(album));

            artist = this->track->GetString(constants::Track::ARTIST);
            artist = artist.size() ? artist : Strings.EMPTY_ARTIST;
            artistCols = narrow_cast<int>(u8cols(artist));
        }
    }

    /* we check duration even if the track is the same because
    looping params may have changed. */
    auto updatedTotal = static_cast<int>(transport.GetDuration());
    if (updatedTotal != secondsTotal) {
        secondsTotal = updatedTotal;
        if (secondsTotal <= 0 && secondsTotal != INT_MIN) {
            if (this->track) {
                std::string duration =
                    this->track->GetString(constants::Track::DURATION);
                if (duration.size()) {
                    secondsTotal = std::stoi(duration);
                }
            }
        }

        if (secondsTotal >= 0) {
            totalTime = musik::core::duration::Duration(secondsTotal);
        }
        else {
            totalTime = "∞";
        }

        totalTimeCols = (int) u8cols(totalTime);
    }
}

/* ~~~~~~~~~~ TransportWindow::Position ~~~~~~~~~~ */

TransportWindow::Position::Position() noexcept {
    this->x = this->y = this->width = 0;
}

TransportWindow::Position::Position(int x, int y, int width) noexcept {
    this->x = x;
    this->y = y;
    this->width = width;
}

void TransportWindow::Position::Set(int x, int width) noexcept {
    this->x = x;
    this->width = width;
}

void TransportWindow::Position::Set(int x, int y, int width) noexcept {
    this->x = x;
    this->y = y;
    this->width = width;
}

double TransportWindow::Position::Percent(int x) noexcept {
    return std::max(0.0, std::min(1.0,
        double(x - this->x) / double(this->width - 1)));
}

bool TransportWindow::Position::Contains(const IMouseHandler::Event& event) noexcept {
    return event.y == this->y &&
        event.x >= this->x &&
        event.x < this->x + this->width;
}

/* writes the colorized formatted string to the specified window. accounts for
utf8 characters and ellipsizing */
size_t TransportWindow::WritePlayingFormat(WINDOW *w, size_t width) {
    this->metadataFieldToPosition.clear();

    TokenList tokens;
    tokenize(Strings.PLAYING_FORMAT, tokens);

    int x = 0, y = 0;
    const Color dim = Color::TextDisabled;
    const Color gb = Color::TextActive;
    const Color warn = Color::TextWarning;
    size_t remaining = width;

    auto it = tokens.begin();
    while (it != tokens.end() && remaining > 0) {
        const Token *token = it->get();

        Color attr = dim;
        std::string value;
        size_t cols = 0;

        if (token->type == Token::Placeholder) {
            if (token->value == kStateToken) {
                if (buffering) {
                    attr = warn;
                    value = Strings.BUFFERING;
                }
                else {
                    value = Strings.PLAYING;
                }
                cols = u8len(value);
            }
            if (token->value == kTitleToken) {
                attr = gb;
                value = displayCache.title;
                cols = displayCache.titleCols;
            }
            else if (token->value == kAlbumToken) {
                attr = gb;
                value = displayCache.album;
                cols = displayCache.albumCols;
            }
            else if (token->value == kArtistToken) {
                attr = gb;
                value = displayCache.artist;
                cols = displayCache.artistCols;
            }
        }

        if (!value.size()) {
            value = token->value;
            cols = displayCache.Columns(value);
        }

        bool ellipsized = false;

        if (cols > remaining) {
            std::string original = value;
            value = text::Ellipsize(value, remaining);
            ellipsized = (value != original);
            cols = remaining;
        }

        /* if we're not at the last token, but there's not enough space
        to show the next token, ellipsize now and bail out of the loop */
        if (remaining - cols < 3 && it + 1 != tokens.end()) {
            if (!ellipsized) {
                value = text::Ellipsize(value, remaining - 3);
                cols = remaining;
            }
        }

        /* any % in the value string might be parsed by wprintw, so replace it */
        std::size_t percentSignIndex = value.find("%");
        while (percentSignIndex != std::string::npos) {
            value.replace(percentSignIndex, 1, "%%");
            /* we replaced one % with 2 of them, so we need to skip ahead 2 chars */
            percentSignIndex = value.find("%", percentSignIndex + 2);
        }

        getyx(w, y, x);
        metadataFieldToPosition[token->value] = Position(x, y, cols);

        ON(w, attr);
        checked_wprintw(w, "%s", value.c_str());
        OFF(w, attr);

        remaining -= cols;
        ++it;
    }

    return (width - remaining);
}

static inline bool inc(const std::string& kn) {
    return (Hotkeys::Is(Hotkeys::Right, kn));
}

static inline bool dec(const std::string& kn) {
    return (Hotkeys::Is(Hotkeys::Left, kn));
}

TransportWindow::TransportWindow(
    musik::core::ILibraryPtr library,
    musik::core::audio::PlaybackService& playback)
: Window(nullptr)
, library(library)
, replayGainMode(ReplayGainMode::Disabled)
, playback(playback)
, transport(playback.GetTransport())
, focus(FocusNone)
, hasReplayGain(false) {
    Strings.Initialize();
    this->SetFrameVisible(false);
    this->playback.TrackChanged.connect(this, &TransportWindow::OnPlaybackServiceTrackChanged);
    this->playback.ModeChanged.connect(this, &TransportWindow::OnPlaybackModeChanged);
    this->playback.Shuffled.connect(this, &TransportWindow::OnPlaybackShuffled);
    this->playback.VolumeChanged.connect(this, &TransportWindow::OnTransportVolumeChanged);
    this->playback.TimeChanged.connect(this, &TransportWindow::OnTransportTimeChanged);
    this->playback.StreamStateChanged.connect(this, &TransportWindow::OnPlaybackStreamStateChanged);
    this->paused = false;
    this->lastTime = DEFAULT_TIME;
    this->shufflePos.y = 0;
    this->repeatPos.y = 1;
    this->volumePos.y = 1;
    this->timeBarPos.y = 1;
    this->currentTrack = playback.GetPlaying();
    this->UpdateReplayGainState();
}

TransportWindow::~TransportWindow() {
    this->disconnect_all();
}

void TransportWindow::SetFocus(FocusTarget target) {
    if (target != this->focus) {
        const auto last = this->focus;
        this->focus = target;

        if (this->focus == FocusNone) {
            this->lastFocus = last;
        }
        else {
            this->Focus();
        }

        DEBOUNCE_REFRESH(TimeMode::Sync, 0);
    }
}

TransportWindow::FocusTarget TransportWindow::GetFocus() const {
    return this->focus;
}

bool TransportWindow::KeyPress(const std::string& kn) {
    if (this->focus == FocusVolume) {
        if (inc(kn)) {
            core::playback::VolumeUp(this->transport);
            return true;
        }
        else if (dec(kn)) {
            core::playback::VolumeDown(this->transport);
            return true;
        }
        else if (kn == "KEY_ENTER") {
            transport.SetMuted(!transport.IsMuted());
            return true;
        }
    }
    else if (this->focus == FocusTime) {
        if (inc(kn)) {
            core::playback::SeekForward(this->playback);
            return true;
        }
        else if (dec(kn)) {
            core::playback::SeekBack(this->playback);
            return true;
        }
    }

    return false;
}

bool TransportWindow::ProcessMouseEvent(const IMouseHandler::Event& event) {
    if (event.Button1Clicked()) {
        if (this->currentTimePos.Contains(event)) {
            const auto state = this->playback.GetPlaybackState();
            if (state == PlaybackState::Playing || state == PlaybackState::Paused) {
                this->playback.PauseOrResume();
            }
            return true;
        }
        else if (this->shufflePos.Contains(event)) {
            this->playback.ToggleShuffle();
            return true;
        }
        else if (this->repeatPos.Contains(event)) {
            this->playback.ToggleRepeatMode();
            return true;
        }
        else if (this->volumePos.Contains(event)) {
            if (playback.IsMuted()) {
                playback.ToggleMute();
            }
            else {
                playback.SetVolume(this->volumePos.Percent(event.x));
            }
            return true;
        }
        else if (this->timeBarPos.Contains(event)) {
            if (playback.GetPlaybackState() != PlaybackState::Stopped) {
                const double duration = playback.GetDuration();
                const double percent = this->timeBarPos.Percent(event.x);
                playback.SetPosition(duration * percent);
            }
            return true;
        }

        for (auto entry : this->metadataFieldToPosition) {
            if (entry.second.Contains(event)) {
                const auto track = this->currentTrack;
                if (track) {
                    const auto type = entry.first;
                    if (type == kTitleToken || type == kAlbumToken || type == kArtistToken) {
                        PlayQueueOverlays::ShowAddTrackOverlay(
                            this->MessageQueue(),
                            this->library,
                            this->playback,
                            track);
                    }
                }
                break;
            }
        }
    }
    else if (event.Button3Clicked()) {
        if (this->volumePos.Contains(event)) {
            playback.ToggleMute();
        }
    }
    return Window::ProcessMouseEvent(event);
}

bool TransportWindow::FocusNext() {
    this->SetFocus((FocusTarget)(((int) this->focus + 1) % 3));
    return (this->focus != FocusNone);
}

bool TransportWindow::FocusPrev() {
    this->SetFocus((FocusTarget)(((int) this->focus - 1) % 3));
    return (this->focus != FocusNone);
}

void TransportWindow::FocusFirst() {
    this->SetFocus(FocusVolume);
}

void TransportWindow::FocusLast() {
    this->SetFocus(FocusTime);
}

void TransportWindow::RestoreFocus() {
    this->Focus();
    this->SetFocus(this->lastFocus);
}

void TransportWindow::OnFocusChanged(bool focused) {
    if (!focused) {
        this->SetFocus(FocusNone);
    }
}

void TransportWindow::ProcessMessage(IMessage &message) {
    const int type = message.Type();

    if (type == message::RefreshTransport) {
        this->Update(static_cast<TimeMode>(message.UserData1()));

        if (transport.GetPlaybackState() != PlaybackState::Stopped) {
            DEBOUNCE_REFRESH(TimeMode::Smooth, REFRESH_INTERVAL_MS)
        }
    }
    else if (type == message::TransportBuffering) {
        this->currentTrack = this->playback.GetPlaying();
        this->buffering = true;
        this->Update();
    }
    else {
        Window::ProcessMessage(message);
    }
}

void TransportWindow::OnPlaybackServiceTrackChanged(size_t index, TrackPtr track) {
    this->currentTrack = track;
    this->lastTime = DEFAULT_TIME;
    this->buffering = playback.GetTransport().GetStreamState() == StreamState::Buffering;
    this->UpdateReplayGainState();
    DEBOUNCE_REFRESH(TimeMode::Sync, 0);
}

void TransportWindow::OnPlaybackStreamStateChanged(StreamState state) {
    if (state == StreamState::Buffering) {
        this->Debounce(message::TransportBuffering, 0, 0, 250);
    }
    else {
        this->Remove(message::TransportBuffering);
        this->buffering = false;
        this->Update();
    }
}

void TransportWindow::OnPlaybackModeChanged() {
    DEBOUNCE_REFRESH(TimeMode::Sync, 0);
}

void TransportWindow::OnTransportVolumeChanged() {
    DEBOUNCE_REFRESH(TimeMode::Sync, 0);
}

void TransportWindow::OnTransportTimeChanged(double time) {
    DEBOUNCE_REFRESH(TimeMode::Sync, 0);
}

void TransportWindow::OnPlaybackShuffled(bool shuffled) {
    DEBOUNCE_REFRESH(TimeMode::Sync, 0);
}

void TransportWindow::OnRedraw() {
    this->Update();
}

void TransportWindow::UpdateReplayGainState() {
    using Mode = ReplayGainMode;

    auto preferences = Preferences::ForComponent(core::prefs::components::Playback);

    this->replayGainMode = static_cast<Mode>(
        preferences->GetInt(
            core::prefs::keys::ReplayGainMode.c_str(),
            static_cast<int>(Mode::Disabled)));

    this->hasReplayGain = false;

    if (this->replayGainMode != Mode::Disabled) {
        if (this->currentTrack) {
            const ReplayGain gain = this->currentTrack->GetReplayGain();
            this->hasReplayGain =
                gain.albumGain != 1.0f || gain.albumPeak != 1.0f ||
                gain.trackGain != 1.0f || gain.albumPeak != 1.0f;
        }
    }
}

void TransportWindow::Update(TimeMode timeMode) {
    this->Clear();

    size_t const cx = narrow_cast<size_t>(this->GetContentWidth());

    if (cx < MIN_WIDTH || this->GetContentHeight() < MIN_HEIGHT) {
        return;
    }

    WINDOW *c = this->GetContent();

    if (!c) {
        return;
    }

    auto const state = transport.GetPlaybackState();
    bool const paused = (state == PlaybackState::Paused);
    bool const prepared = (state == PlaybackState::Prepared);
    bool const stopped = (state == PlaybackState::Stopped);
    bool const muted = transport.IsMuted();
    bool const replayGainEnabled = (this->replayGainMode != ReplayGainMode::Disabled);

    Color const gb = Color::TextActive;
    Color const disabled = Color::TextDisabled;
    Color const bright = Color::TextDefault;
    Color volumeAttrs = Color::Default;

    if (this->focus == FocusVolume) {
        volumeAttrs = Color::TextFocused;
    }
    else if (muted) {
        volumeAttrs = gb;
    }

    Color const timerAttrs = (this->focus == FocusTime)
        ? Color::TextFocused : Color::Default;

    /* prepare the "shuffle" label */

    std::string shuffleLabel = Strings.SHUFFLE;
    size_t const shuffleWidth = displayCache.Columns(shuffleLabel);

    /* playing SONG TITLE from ALBUM NAME */

    if (stopped && !this->buffering) {
        ON(c, disabled);
        checked_wprintw(c, "%s", Strings.STOPPED.c_str());
        displayCache.Reset();
        OFF(c, disabled);
    }
    else {
        displayCache.Update(transport, this->currentTrack);
        this->WritePlayingFormat(c, cx - shuffleWidth);
    }

    /* draw the "shuffle" label */
    const int shuffleOffset = narrow_cast<int>(cx - shuffleWidth);
    wmove(c, 0, shuffleOffset);
    Color const shuffleAttrs = this->playback.IsShuffled() ? gb : disabled;
    ON(c, shuffleAttrs);
    checked_wprintw(c, "%s", shuffleLabel.c_str());
    OFF(c, shuffleAttrs);
    this->shufflePos.Set(shuffleOffset, narrow_cast<int>(shuffleWidth));

    /* volume slider */

    int const volumePercent = static_cast<int>(round(this->transport.Volume() * 100.0f));
    int thumbOffset = std::min(10, (volumePercent * 10) / 100);

    std::string volume;

    if (muted) {
        volume = Strings.MUTED;
        this->volumePos.Set(0, narrow_cast<int>(u8cols(Strings.MUTED)));
    }
    else {
        volume = Strings.VOLUME;
        this->volumePos.Set(narrow_cast<int>(u8cols(Strings.VOLUME)), 11);

        for (int i = 0; i < 11; i++) {
            volume += (i == thumbOffset) ? "■" : "─";
        }

        volume += u8fmt(" %d", static_cast<int>(std::round(this->transport.Volume() * 100)));
        volume += replayGainEnabled ? "%% " : "%%  ";
    }

    /* repeat mode setup */

    RepeatMode const mode = this->playback.GetRepeatMode();
    std::string repeatModeLabel;
    Color repeatAttrs = Color::Default;
    switch (mode) {
        case RepeatMode::List:
            repeatModeLabel += Strings.REPEAT_LIST;
            repeatAttrs = gb;
            break;
        case RepeatMode::Track:
            repeatModeLabel += Strings.REPEAT_TRACK;
            repeatAttrs = gb;
            break;
        default:
            repeatModeLabel += Strings.REPEAT_OFF;
            repeatAttrs = disabled;
            break;
    }

    /* time slider */

    Color currentTimeAttrs = timerAttrs;

    if (paused) { /* blink the track if paused */
        int64_t const now = duration_cast<seconds>(
            system_clock::now().time_since_epoch()).count();

        if (now % 2 == 0) {
            currentTimeAttrs = Color::TextHidden;
        }
    }

    /* calculating playback time is inexact because it's based on buffers that
    are sent to the output. here we use a simple smoothing function to hopefully
    mitigate jumping around. basically: draw the time as one second more than the
    last time we displayed, unless they are more than few seconds apart. note this
    only works if REFRESH_INTERVAL_MS is 1000. */
    int secondsCurrent = static_cast<int>(round(this->lastTime)); /* mode == TimeLast */

    if (!this->buffering && timeMode == TimeMode::Smooth) {
        double smoothedTime = this->lastTime += 1.0f; /* 1000 millis */
        double const actualTime = playback.GetPosition();

        if (prepared || paused || stopped || fabs(smoothedTime - actualTime) > TIME_SLOP) {
            smoothedTime = actualTime;
        }

        this->lastTime = smoothedTime;
        /* end time smoothing */

        secondsCurrent = static_cast<int>(round(smoothedTime));
    }
    else {
        this->lastTime = std::max(0.0, playback.GetPosition());
        secondsCurrent = static_cast<int>(round(this->lastTime));
    }

    const std::string currentTime =
        displayCache.CurrentTime(secondsCurrent);

    const std::string replayGain = replayGainEnabled  ? "rg" : "";

    int const bottomRowControlsWidth =
        displayCache.Columns(volume) - (muted ? 0 : 1) + /* -1 for escaped percent sign when not muted */
        (replayGainEnabled ? (narrow_cast<int>(u8cols(replayGain)) + 4) : 0) +  /* [] brackets */
        narrow_cast<int>(u8cols(currentTime)) + 1 + /* +1 for space padding */
        /* timer track with thumb */
        1 + displayCache.totalTimeCols + /* +1 for space padding */
        displayCache.Columns(repeatModeLabel);

    int const timerTrackWidth =
        this->GetContentWidth() -
        bottomRowControlsWidth;

    thumbOffset = 0;

    if (displayCache.secondsTotal) {
        int const progress = (secondsCurrent * 100) / displayCache.secondsTotal;
        thumbOffset = std::min(timerTrackWidth - 1, (progress * timerTrackWidth) / 100);
    }

    std::string timerTrack = "";
    for (int i = 0; i < timerTrackWidth; i++) {
        timerTrack += (i == thumbOffset) ? "■" : "─";
    }

    /* draw second row */

    wmove(c, 1, 0); /* move cursor to the second line */

    ON(c, volumeAttrs);
    checked_wprintw(c, "%s", volume.c_str());
    OFF(c, volumeAttrs);

    if (replayGainEnabled) {
        auto const rgStyle = this->hasReplayGain ? gb : disabled;
        checked_wprintw(c, "[");
        ON(c, rgStyle); checked_wprintw(c, "%s", replayGain.c_str()); OFF(c, rgStyle);
        checked_wprintw(c, "]  ");
    }

    currentTimePos.Set(getcurx(c), 1, u8cols(currentTime));
    ON(c, currentTimeAttrs); /* blink if paused */
    checked_wprintw(c, "%s ", currentTime.c_str());
    OFF(c, currentTimeAttrs);

    ON(c, timerAttrs);
    this->timeBarPos.Set(getcurx(c), narrow_cast<int>(u8cols(timerTrack)));
    checked_waddstr(c, timerTrack.c_str()); /* may be a very long string */
    checked_wprintw(c, " %s", displayCache.totalTime.c_str());
    OFF(c, timerAttrs);

    ON(c, repeatAttrs);
    this->repeatPos.Set(getcurx(c), narrow_cast<int>(u8cols(repeatModeLabel)));
    checked_wprintw(c, "%s", repeatModeLabel.c_str());
    OFF(c, repeatAttrs);

    this->Invalidate();
}
