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

#include "CategorySearchLayout.h"

#include <cursespp/Colors.h>
#include <cursespp/Screen.h>
#include <cursespp/Text.h>

#include <musikcore/library/LocalLibraryConstants.h>
#include <musikcore/support/PreferenceKeys.h>

#include <app/util/Messages.h>
#include <app/util/Hotkeys.h>
#include <app/util/PreferenceKeys.h>

using namespace musik::core::library::constants;

using namespace musik::core;
using namespace musik::core::audio;
using namespace musik::core::library;
using namespace musik::core::runtime;
using namespace musik::cube;
using namespace cursespp;

namespace keys = musik::cube::prefs::keys;
namespace components = musik::core::prefs::components;

constexpr int kSearchHeight = 3;
constexpr int kRequeryIntervalMs = 300;

#define IS_CATEGORY(x) \
    x == this->albums || \
    x == this->artists || \
    x == this->genres

CategorySearchLayout::CategorySearchLayout(musik::core::audio::PlaybackService& playback, ILibraryPtr library)
: LayoutBase() {
    this->library = library;
    this->prefs = Preferences::ForComponent(components::Settings);
    this->InitializeWindows(playback);
}

CategorySearchLayout::~CategorySearchLayout() {
    this->SaveSession();
}

void CategorySearchLayout::LoadLastSession() {
    auto session = Preferences::ForComponent(components::Session);

    const std::string lastFilter = session->GetString(keys::LastCategoryFilter);
    if (lastFilter.size()) {
        this->input->SetText(lastFilter);
    }

    this->matchType = static_cast<MatchType>(session->GetInt(
        keys::LastCategoryFilterMatchType,
        static_cast<int>(MatchType::Substring)));
}

void CategorySearchLayout::SaveSession() {
    auto session = Preferences::ForComponent(components::Session);
    session->SetString(keys::LastCategoryFilter, this->input->GetText().c_str());
    session->SetInt(keys::LastCategoryFilterMatchType, static_cast<int>(this->matchType));
}

void CategorySearchLayout::OnLayout() {
    const int cx = this->GetWidth(), cy = this->GetHeight();
    constexpr int x = 0, y = 0;

    const int inputWidth = cx / 2;
    const int inputX = x + ((cx - inputWidth) / 2);
    this->input->MoveAndResize(inputX, 0, cx / 2, kSearchHeight);

    const bool inputIsRegex = this->matchType == MatchType::Regex;
    this->input->SetHint(_TSTR(inputIsRegex ? "search_regex_hint" : "search_filter_hint"));
    this->input->SetFocusedFrameColor(inputIsRegex ? Color::FrameImportant : Color::FrameFocused);

    constexpr int labelY = kSearchHeight;
    const int categoryWidth = cx / 3;
    constexpr int categoryY = labelY;
    const int categoryHeight = cy - kSearchHeight;
    const int lastCategoryWidth = cx - (categoryWidth * 2);

    this->albums->MoveAndResize(0, categoryY, categoryWidth, categoryHeight);
    this->artists->MoveAndResize(categoryWidth, categoryY, categoryWidth, categoryHeight);
    this->genres->MoveAndResize(categoryWidth * 2, categoryY, lastCategoryWidth, categoryHeight);
}

void CategorySearchLayout::CreateCategoryView(
    std::shared_ptr<CategoryListView>& view,
    musik::core::audio::PlaybackService& playback,
    const std::string& title,
    const std::string& type,
    int order)
{
    view = std::make_shared<CategoryListView>(playback, library, type);
    view->EntryActivated.connect(this, &CategorySearchLayout::OnCategoryEntryActivated);
    view->SetFrameTitle(_TSTR(title));
    view->SetAllowArrowKeyPropagation();
    view->SetFocusOrder(order);
    this->AddWindow(view);
}

void CategorySearchLayout::InitializeWindows(musik::core::audio::PlaybackService& playback) {
    this->input = std::make_shared<TextInput>();
    this->input->TextChanged.connect(this, &CategorySearchLayout::OnInputChanged);
    this->input->EnterPressed.connect(this, &CategorySearchLayout::OnEnterPressed);
    this->input->SetFocusOrder(0);
    this->AddWindow(this->input);

    this->CreateCategoryView(this->albums, playback, "browse_title_albums", constants::Track::ALBUM, 1);
    this->CreateCategoryView(this->artists, playback, "browse_title_artists", constants::Track::ARTIST, 2);
    this->CreateCategoryView(this->genres, playback, "browse_title_genres", constants::Track::GENRE, 3);
}

void CategorySearchLayout::Requery() {
    const std::string& value = this->input->GetText();
    this->albums->Requery(this->matchType, value);
    this->artists->Requery(this->matchType, value);
    this->genres->Requery(this->matchType, value);
}

void CategorySearchLayout::FocusInput() {
    this->SetFocus(this->input);
}

void CategorySearchLayout::OnInputChanged(cursespp::TextInput* sender, std::string value) {
    if (this->IsVisible()) {
        Debounce(message::RequeryCategoryList, 0, 0, kRequeryIntervalMs);
    }
}

void CategorySearchLayout::OnEnterPressed(cursespp::TextInput* sender) {
    this->SetFocus(this->albums);
}

void CategorySearchLayout::OnVisibilityChanged(bool visible) {
    LayoutBase::OnVisibilityChanged(visible);

    if (visible) {
        this->Requery();
    }
    else {
        this->SaveSession();
        this->input->SetText("");
        this->albums->Reset();
        this->artists->Reset();
        this->genres->Reset();
        this->SetFocusIndex(0, false);
    }
}

void CategorySearchLayout::OnCategoryEntryActivated(cursespp::ListWindow* listWindow, size_t index) {
    CategoryListView* category = dynamic_cast<CategoryListView*>(listWindow);
    if (category && narrow_cast<int>(index) >= 0) {
        this->SearchResultSelected(
            this,
            category->GetFieldName(),
            category->GetSelectedId());
    }
}

bool CategorySearchLayout::KeyPress(const std::string& key) {
    IWindowPtr focus = this->GetFocus();

    if (Hotkeys::Is(Hotkeys::Down, key)) {
        if (this->GetFocus() == this->input) {
            this->FocusNext();
            return true;
        }
    }
    else if (Hotkeys::Is(Hotkeys::Up, key)) {
        if (IS_CATEGORY(this->GetFocus())) {
            this->SetFocus(this->input);
            return true;
        }
    }
    else if (Hotkeys::Is(Hotkeys::SearchInputToggleMatchType, key) && this->input->IsFocused()) {
        this->ToggleMatchType();
        return true;
    }

    return LayoutBase::KeyPress(key);
}

void CategorySearchLayout::ProcessMessage(IMessage &message) {
    if (message.Type() == message::RequeryCategoryList) {
        this->Requery();
    }
    else {
        LayoutBase::ProcessMessage(message);
    }
}

void CategorySearchLayout::ToggleMatchType() {
    const bool isRegex = this->matchType == MatchType::Regex;
    this->SetMatchType(isRegex ? MatchType::Substring : MatchType::Regex);
}

void CategorySearchLayout::SetMatchType(MatchType matchType) {
    if (matchType != this->matchType) {
        this->matchType = matchType;
        this->Layout();
        this->Requery();
    }
}

