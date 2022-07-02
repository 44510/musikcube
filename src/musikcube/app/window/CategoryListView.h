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

#pragma once

#include <sigslot/sigslot.h>

#include <cursespp/ListWindow.h>
#include <cursespp/ScrollAdapterBase.h>

#include <musikcore/library/query/CategoryListQuery.h>

#include <musikcore/audio/PlaybackService.h>
#include <musikcore/library/IQuery.h>
#include <musikcore/library/ILibrary.h>
#include <musikcore/runtime/IMessage.h>

#include <mutex>

namespace musik {
    namespace cube {
        class CategoryListView:
            public cursespp::ListWindow,
            public sigslot::has_slots<>
        {
            public:
                using MatchType = musik::core::library::query::QueryBase::MatchType;

                CategoryListView(
                    musik::core::audio::PlaybackService& playback,
                    musik::core::ILibraryPtr library,
                    const std::string& fieldName);

                virtual ~CategoryListView();

                void RequeryWithField(
                    MatchType matchType,
                    const std::string& fieldName,
                    const std::string& filter = "",
                    const int64_t selectAfterQuery = -1LL);

                void RequeryWithField(
                    const std::string& fieldName,
                    const std::string& filter = "",
                    const int64_t selectAfterQuery = -1LL);

                void Requery(
                    MatchType matchType,
                    const std::string& filter = "",
                    const int64_t selectAfterQuery = -1LL);

                void Requery(
                    const std::string& filter,
                    const int64_t selectAfterQuery = -1LL);

                void Requery(const int64_t selectAfterQuery);

                void Requery();

                void Reset();

                int64_t GetSelectedId();
                std::string GetSelectedValue();
                std::string GetFilter();
                std::string GetFieldName();
                void SetFieldName(const std::string& fieldName);

                /* IWindow */
                bool KeyPress(const std::string& key) override;
                void OnVisibilityChanged(bool visible) override;

            protected:
                /* IListWindow */
                cursespp::IScrollAdapter& GetScrollAdapter() override;
                bool OnEntryContextMenu(size_t index) override;

                void OnQueryCompleted(musik::core::db::IQuery* query);
                void ShowContextMenu();
                class Adapter : public cursespp::ScrollAdapterBase {
                public:
                    Adapter(CategoryListView &parent);

                    size_t GetEntryCount() override;

                    cursespp::IScrollAdapter::EntryPtr
                        GetEntry(cursespp::ScrollableWindow* window, size_t index) override;

                private:
                    CategoryListView &parent;
                    cursespp::IScrollAdapter::ScrollPosition spos;
                };

            private:
                void OnTrackChanged(size_t index, musik::core::TrackPtr track);
                void ScrollToPlaying();

                musik::core::audio::PlaybackService& playback;
                Adapter *adapter;

                std::shared_ptr<musik::core::library::query::CategoryListQuery> activeQuery;

                musik::core::ILibraryPtr library;
                musik::core::TrackPtr playing;

                std::string fieldName, fieldIdColumn;
                std::string filter;
                MatchType matchType{ MatchType::Substring };
                int64_t selectAfterQuery;
                musik::core::library::query::CategoryListQuery::Result metadata;
        };
    }
}
