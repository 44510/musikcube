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

#include <pch.hpp>

#include "MasterTransport.h"

#include <musikcore/audio/GaplessTransport.h>
#include <musikcore/audio/CrossfadeTransport.h>
#include <musikcore/support/Preferences.h>
#include <musikcore/support/PreferenceKeys.h>

using namespace musik::core::audio;
using namespace musik::core;
using namespace musik::core::prefs;
using namespace musik::core::sdk;

MasterTransport::MasterTransport() noexcept
: prefs(Preferences::ForComponent(components::Playback)) {
    this->type = static_cast<Type>(this->prefs->GetInt(
        keys::Transport, static_cast<int>(Type::Gapless)));

    this->SwitchTo(this->type);
}

void MasterTransport::SwitchTo(Type type) {
    if (!this->transport || this->type != type) {
        this->type = type;
        this->prefs->SetInt(keys::Transport, static_cast<int>(this->type));

        const double volume = this->transport ? this->transport->Volume() : -1;

        switch (this->type) {
            case Type::Gapless:
                if (this->transport) {
                    /* hacky -- we know it's a crossfade transport, stop it
                    immediately without fading out so we don't block the UI
                    for a second or so. */
                    dynamic_cast<CrossfadeTransport*>(this->transport.get())->StopImmediately();
                }

                this->transport = std::make_shared<GaplessTransport>();
                break;

            case Type::Crossfade:
                this->transport = std::make_shared<CrossfadeTransport>();
                break;
        }

        if (volume > 0) {
            this->transport->SetVolume(volume);
        }

        this->transport->PlaybackEvent.connect(
            this, &MasterTransport::OnPlaybackEvent);

        this->transport->StreamEvent.connect(
            this, &MasterTransport::OnStreamEvent);

        this->transport->TimeChanged.connect(
            this, &MasterTransport::OnTimeChanged);

        this->transport->VolumeChanged.connect(
            this, &MasterTransport::OnVolumeChanged);
    }
}

MasterTransport::Type MasterTransport::GetType() noexcept {
    return this->type;
}

void MasterTransport::PrepareNextTrack(const std::string& uri, Gain gain) {
    this->transport->PrepareNextTrack(uri, gain);
}

void MasterTransport::Start(const std::string& uri, Gain gain, StartMode type) {
    this->transport->Start(uri, gain, type);
}

std::string MasterTransport::Uri() {
    return this->transport->Uri();
}

void MasterTransport::Stop() {
    this->transport->Stop();
}

bool MasterTransport::Pause() {
    return this->transport->Pause();
}

bool MasterTransport::Resume() {
    return this->transport->Resume();
}

double MasterTransport::Position() {
    return this->transport->Position();
}

void MasterTransport::SetPosition(double seconds) {
    this->transport->SetPosition(seconds);
}

double MasterTransport::Volume() {
    return this->transport->Volume();
}

void MasterTransport::SetVolume(double volume) {
    this->transport->SetVolume(volume);
}

double MasterTransport::GetDuration() {
    return this->transport->GetDuration();
}

bool MasterTransport::IsMuted() {
    return this->transport->IsMuted();
}

void MasterTransport::SetMuted(bool muted) {
    this->transport->SetMuted(muted);
}

void MasterTransport::ReloadOutput() {
    this->transport->ReloadOutput();
}

PlaybackState MasterTransport::GetPlaybackState() {
    return this->transport->GetPlaybackState();
}

StreamState MasterTransport::GetStreamState() {
    return this->transport->GetStreamState();
}

void MasterTransport::OnStreamEvent(musik::core::sdk::StreamState type, std::string url) {
    this->StreamEvent(type, url);
}

void MasterTransport::OnPlaybackEvent(musik::core::sdk::PlaybackState type) {
    this->PlaybackEvent(type);
}

void MasterTransport::OnVolumeChanged() {
    this->VolumeChanged();
}

void MasterTransport::OnTimeChanged(double time) {
    this->TimeChanged(time);
}