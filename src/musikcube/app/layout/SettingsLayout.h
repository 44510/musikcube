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

#pragma once

#include <cursespp/App.h>
#include <cursespp/Checkbox.h>
#include <cursespp/Colors.h>
#include <cursespp/LayoutBase.h>
#include <cursespp/ListWindow.h>
#include <cursespp/ShortcutsWindow.h>
#include <cursespp/SimpleScrollAdapter.h>
#include <cursespp/TextLabel.h>
#include <cursespp/TextInput.h>
#include <cursespp/DialogOverlay.h>
#include <cursespp/ITopLevelLayout.h>

#include <musikcore/audio/PlaybackService.h>
#include <musikcore/audio/MasterTransport.h>
#include <musikcore/library/MasterLibrary.h>
#include <musikcore/support/Preferences.h>

#include <app/window/TrackListView.h>
#include <app/model/DirectoryAdapter.h>
#include <app/util/UpdateCheck.h>

#include <sigslot/sigslot.h>

#include "LocalLibrarySettingsLayout.h"
#include "RemoteLibrarySettingsLayout.h"

namespace musik { namespace cube {
    class SettingsLayout :
        public cursespp::ITopLevelLayout,
        public cursespp::LayoutBase,
        public sigslot::has_slots<>
    {
        public:
            using MasterLibraryPtr = std::shared_ptr<musik::core::library::MasterLibrary>;

            DELETE_COPY_AND_ASSIGNMENT_DEFAULTS(SettingsLayout)

            SettingsLayout(
                cursespp::App& app,
                MasterLibraryPtr library,
                musik::core::audio::PlaybackService& playback);

            virtual ~SettingsLayout();

            /* IWindow */
            void OnVisibilityChanged(bool visible) override;
            void OnAddedToParent(IWindow* parent) override;
            void OnRemovedFromParent(IWindow* parent) override;
            void OnLayout() override;

            /* ITopLevelLayout */
            void SetShortcutsWindow(cursespp::ShortcutsWindow* w) override;

        private:
            void InitializeWindows();
            void LoadPreferences();
            void UpdateServerAvailability();
            void CreateCheckbox(std::shared_ptr<cursespp::Checkbox>& cb, const std::string& caption);

            void OnCheckboxChanged(cursespp::Checkbox* checkbox, bool checked);

            void OnLibraryTypeDropdownActivated(cursespp::TextLabel* label);
            void OnOutputDriverDropdownActivated(cursespp::TextLabel* label);
            void OnOutputDeviceDropdownActivated(cursespp::TextLabel* label);
            void OnReplayGainDropdownActivated(cursespp::TextLabel* label);
            void OnTransportDropdownActivate(cursespp::TextLabel* label);
            void OnPluginsDropdownActivate(cursespp::TextLabel* label);
            void OnHotkeyDropdownActivate(cursespp::TextLabel* label);
            void OnThemeDropdownActivate(cursespp::TextLabel* label);
            void OnLocaleDropdownActivate(cursespp::TextLabel* label);
            void OnServerDropdownActivate(cursespp::TextLabel* label);
            void OnUpdateDropdownActivate(cursespp::TextLabel* label);
            void OnLastFmDropdownActivate(cursespp::TextLabel* label);
            void OnAdvancedSettingsActivate(cursespp::TextLabel* label);

            cursespp::App& app;
            MasterLibraryPtr library;
            musik::core::IIndexer* indexer;
            musik::core::audio::PlaybackService& playback;

            std::shared_ptr<musik::core::Preferences> prefs;

            using Text = std::shared_ptr<cursespp::TextLabel>;
            Text libraryTypeDropdown;
            Text localeDropdown;
            Text outputDriverDropdown;
            Text outputDeviceDropdown;
            Text replayGainDropdown;
            Text transportDropdown;
            Text lastFmDropdown;
            Text pluginsDropdown;
            Text hotkeyDropdown;
            Text serverDropdown;
            Text updateDropdown;
            Text themeDropdown;
            Text advancedDropdown;
            Text appVersion;

            using Check = std::shared_ptr<cursespp::Checkbox>;
            Check paletteCheckbox;
            Check enableTransparencyCheckbox;
            Check dotfileCheckbox;
            Check syncOnStartupCheckbox;
            Check removeCheckbox;
            Check saveSessionCheckbox;

            std::shared_ptr<LocalLibrarySettingsLayout> localLibraryLayout;
            std::shared_ptr<RemoteLibrarySettingsLayout> remoteLibraryLayout;

            UpdateCheck updateCheck;
            bool serverAvailable = false;
    };
} }
