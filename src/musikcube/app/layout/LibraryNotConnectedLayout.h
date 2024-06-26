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

#include <cursespp/LayoutBase.h>
#include <musikcore/library/MasterLibrary.h>
#include <cursespp/ITopLevelLayout.h>
#include <cursespp/TextLabel.h>
#include <cursespp/ShortcutsWindow.h>

namespace musik { namespace cube {

    class LibraryNotConnectedLayout:
        public cursespp::LayoutBase,
        public cursespp::ITopLevelLayout,
        public sigslot::has_slots<>
    {
        public:
            using MasterLibraryPtr = std::shared_ptr<musik::core::library::MasterLibrary>;

            DELETE_CLASS_DEFAULTS(LibraryNotConnectedLayout)

            LibraryNotConnectedLayout(MasterLibraryPtr library);

            /* IWindow */
            void OnLayout() override;
            bool KeyPress(const std::string& kn) override;
            void SetShortcutsWindow(cursespp::ShortcutsWindow* w) override;
            void OnVisibilityChanged(bool visible) override;

        protected:
            void OnLibraryStateChanged(musik::core::ILibrary::ConnectionState state);

        private:
            void UpdateErrorText();

            MasterLibraryPtr library;
            std::shared_ptr<cursespp::TextLabel> messageText;
            std::shared_ptr<cursespp::TextLabel> errorText;
            std::shared_ptr<cursespp::TextLabel> helpText;
            cursespp::ShortcutsWindow* shortcuts{ nullptr };
    };

} }