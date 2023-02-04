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

#include "pch.hpp"

#include <musikcore/debug.h>
#include <musikcore/audio/GaplessTransport.h>
#include <musikcore/plugin/PluginFactory.h>
#include <musikcore/audio/Outputs.h>
#include <algorithm>

using namespace musik::core::audio;
using namespace musik::core::sdk;

static std::string TAG = "GaplessTransport";

GaplessTransport::GaplessTransport()
: volume(1.0)
, playbackState(PlaybackState::Stopped)
, activePlayer(nullptr)
, nextPlayer(nullptr)
, nextCanStart(false)
, muted(false) {
    this->output = outputs::SelectedOutput();
}

GaplessTransport::~GaplessTransport() {
    LockT lock(this->stateMutex);
    this->ResetNextPlayer();
    this->ResetActivePlayer();
}

PlaybackState GaplessTransport::GetPlaybackState() {
    LockT lock(this->stateMutex);
    return this->playbackState;
}

StreamState GaplessTransport::GetStreamState() {
    LockT lock(this->stateMutex);
    return this->activePlayerState;
}

void GaplessTransport::PrepareNextTrack(const std::string& uri, Gain gain) {
    bool startNext = false;
    {
        LockT lock(this->stateMutex);

        this->ResetNextPlayer();

        if (uri.size()) {
            this->nextPlayer = Player::Create(
                uri,
                this->output, Player::DestroyMode::NoDrain,
                this,
                gain);
            startNext = this->nextCanStart;
        }
    }

    if (startNext) {
        this->StartWithPlayer(this->nextPlayer);
    }
}

void GaplessTransport::Start(const std::string& uri, Gain gain, StartMode mode) {
    musik::debug::info(TAG, "starting track at " + uri);
    Player* newPlayer = Player::Create(
        uri,
        this->output,
        Player::DestroyMode::NoDrain,
        this,
        gain);
    this->StartWithPlayer(newPlayer, mode);
}

void GaplessTransport::StartWithPlayer(Player* newPlayer, StartMode mode) {
    if (newPlayer) {
        bool playingNext = false;

        {
            LockT lock(this->stateMutex);

            playingNext = (newPlayer == nextPlayer);
            if (newPlayer != nextPlayer) {
                this->ResetNextPlayer();
            }

            this->ResetActivePlayer();

            this->nextPlayer = nullptr;
            this->activePlayer = newPlayer;

            if (newPlayer) {
                this->RaiseStreamEvent(newPlayer->GetStreamState(), newPlayer);
            }

            /* first argument suppresses the "Stop" event from getting triggered,
            the second param is used for gapless playback -- we won't stop the output
            and will allow pending buffers to finish if we're not automatically
            playing the next track. */
            this->StopInternal(true, !playingNext, newPlayer);
            this->SetNextCanStart(false);
            this->output->Resume();

            if (mode == StartMode::Immediate) {
                newPlayer->Play();
            }
        }
    }
}

void GaplessTransport::ReloadOutput() {
    this->Stop();
    this->output = outputs::SelectedOutput();
    this->output->SetVolume(volume);
}

void GaplessTransport::Stop() {
    this->StopInternal(false, true);
}

std::string GaplessTransport::Uri() {
    const auto player = this->activePlayer;
    return player ? player->GetUrl() : "";
}

void GaplessTransport::StopInternal(
    bool suppressStopEvent,
    bool stopOutput,
    Player const* exclude)
{
    musik::debug::info(TAG, "stop");

    /* if we stop the output, we kill all of the Players immediately.
    otherwise, we let them finish naturally; RemoveActive() will take
    care of disposing of them */
    if (stopOutput) {
        {
            LockT lock(this->stateMutex);

            this->ResetNextPlayer();
            if (this->activePlayer != exclude) {
                this->ResetActivePlayer();
            }
        }

        /* stopping the transport will stop any buffers that are currently in
        flight. this makes the sound end immediately. */
        this->output->Stop();
    }

    if (!suppressStopEvent) {
        /* if we know we're starting another track immediately, suppress
        the stop event. this functionality is not available to the public
        interface, it's an internal optimization */
        this->SetPlaybackState(PlaybackState::Stopped);
    }
}

bool GaplessTransport::Pause() {
    musik::debug::info(TAG, "pause");

    this->output->Pause();

    if (this->activePlayer) {
        this->SetPlaybackState(PlaybackState::Paused);
        return true;
    }

    return false;
}

bool GaplessTransport::Resume() {
    musik::debug::info(TAG, "resume");

    this->output->Resume();

    {
        LockT lock(this->stateMutex);

        if (this->activePlayer) {
            this->activePlayer->Play();
        }
    }

    if (this->activePlayer) {
        this->SetPlaybackState(PlaybackState::Playing);
        return true;
    }

    return false;
}

double GaplessTransport::Position() {
    LockT lock(this->stateMutex);

    if (this->activePlayer) {
        return this->activePlayer->GetPosition();
    }

    return 0;
}

void GaplessTransport::SetPosition(double seconds) {
    {
        LockT lock(this->stateMutex);

        if (this->activePlayer) {
            if (this->playbackState != PlaybackState::Playing) {
                this->SetPlaybackState(PlaybackState::Playing);
            }
            this->activePlayer->SetPosition(seconds);
        }
    }

    if (this->activePlayer) {
        this->TimeChanged(seconds);
    }
}

double GaplessTransport::GetDuration() {
    LockT lock(this->stateMutex);
    return this->activePlayer ? this->activePlayer->GetDuration() : -1.0f;
}

bool GaplessTransport::IsMuted() noexcept {
    return this->muted;
}

void GaplessTransport::SetMuted(bool muted) {
    if (this->muted != muted) {
        this->muted = muted;
        this->output->SetVolume(muted ? 0.0f : this->volume);
        this->VolumeChanged();
    }
}

double GaplessTransport::Volume() noexcept {
    return this->volume;
}

void GaplessTransport::SetVolume(double volume) {
    const double oldVolume = this->volume;

    volume = std::max(0.0, std::min(1.0, volume));

    this->volume = volume;
    this->output->SetVolume(this->volume);

    if (oldVolume != this->volume) {
        this->SetMuted(false);
        this->VolumeChanged();
    }
}

void GaplessTransport::SetNextCanStart(bool nextCanStart) {
    LockT lock(this->stateMutex);
    this->nextCanStart = nextCanStart;
}

void GaplessTransport::OnPlayerBuffered(Player* player) {
    if (player == this->activePlayer) {
        this->RaiseStreamEvent(StreamState::Buffered, player);
        this->SetPlaybackState(PlaybackState::Prepared);
    }
}

void GaplessTransport::OnPlayerStarted(Player* player) {
    this->RaiseStreamEvent(StreamState::Playing, player);
    this->SetPlaybackState(PlaybackState::Playing);
}

void GaplessTransport::OnPlayerAlmostEnded(Player* player) {
    this->SetNextCanStart(true);

    {
        LockT lock(this->stateMutex);

        /* if another component configured a next player while we were playing,
        go ahead and get it started now. */
        if (this->nextPlayer) {
            this->StartWithPlayer(this->nextPlayer);
        }
    }

    this->RaiseStreamEvent(StreamState::AlmostDone, player);
}

void GaplessTransport::OnPlayerFinished(Player* player) {
    this->RaiseStreamEvent(StreamState::Finished, player);

    bool stopped = false;

    {
        LockT lock(this->stateMutex);

        bool startedNext = false;
        const bool playerIsActive = (player == this->activePlayer);

        /* only start the next player if the currently active player is the
        one that just finished. */
        if (playerIsActive && this->nextPlayer) {
            this->StartWithPlayer(this->nextPlayer);
            startedNext = true;
        }

        if (!startedNext) {
            stopped = playerIsActive;
        }
    }

    if (stopped) {
        /* note we call through to StopInternal() because we don't
        want to stop the output immediately, it may still have some
        trailing samples queued up */
        this->StopInternal(false, false);
    }
}

void GaplessTransport::OnPlayerOpenFailed(Player* player) {
    bool raiseEvents = false;
    {
        LockT lock(this->stateMutex);
        if (player == this->activePlayer) {
            this->ResetActivePlayer();
            this->ResetNextPlayer();
            raiseEvents = true;
        }
        else if (player == this->nextPlayer) {
            this->ResetNextPlayer();
        }
    }
    if (raiseEvents) {
        this->RaiseStreamEvent(StreamState::OpenFailed, player);
        this->SetPlaybackState(PlaybackState::Stopped);
    }
}

void GaplessTransport::OnPlayerDestroying(Player *player) {
    LockT lock(this->stateMutex);

    if (player == this->activePlayer) {
        this->ResetActivePlayer();
        return;
    }
}

void GaplessTransport::SetPlaybackState(PlaybackState state) {
    bool changed = false;

    {
        LockT lock(this->stateMutex);
        changed = (this->playbackState != state);
        this->playbackState = static_cast<PlaybackState>(state);
    }

    if (changed) {
        this->PlaybackEvent(state);
    }
}

void GaplessTransport::RaiseStreamEvent(StreamState type, Player const* player) {
    bool eventIsFromActivePlayer = false;
    {
        LockT lock(this->stateMutex);
        eventIsFromActivePlayer = (player == activePlayer);
        if (eventIsFromActivePlayer) {
            this->activePlayerState = static_cast<StreamState>(type);
        }
    }

    if (eventIsFromActivePlayer) {
        this->StreamEvent(type, player->GetUrl());
    }
}

void GaplessTransport::ResetNextPlayer() {
    if (this->nextPlayer) {
        this->nextPlayer->Detach(this);
        this->nextPlayer->Destroy();
        this->RaiseStreamEvent(StreamState::Destroyed, this->nextPlayer);
        this->nextPlayer = nullptr;
    }
}

void GaplessTransport::ResetActivePlayer() {
    if (this->activePlayer) {
        this->activePlayer->Detach(this);
        this->activePlayer->Destroy();
        this->RaiseStreamEvent(StreamState::Destroyed, this->activePlayer);
        this->activePlayer = nullptr;
    }
}
