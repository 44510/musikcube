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

#include <musikcore/library/QueryBase.h>
#include <musikcore/library/track/TrackList.h>
#include <stdint.h>
#include <vector>

namespace musik { namespace core { namespace library { namespace query {

    class AppendPlaylistQuery : public musik::core::library::query::QueryBase {
        public:
            EXPORT static const std::string kQueryName;

            DELETE_CLASS_DEFAULTS(AppendPlaylistQuery)

            EXPORT AppendPlaylistQuery(
                musik::core::ILibraryPtr library,
                const int64_t playlistId,
                std::shared_ptr<musik::core::TrackList> tracks,
                const int offset = -1) noexcept;

            EXPORT AppendPlaylistQuery(
                musik::core::ILibraryPtr library,
                const int64_t playlistId,
                musik::core::sdk::ITrackList *tracks,
                const int offset = -1) noexcept;

            /* IQuery */
            EXPORT std::string Name() override { return kQueryName; }

            /* ISerializableQuery */
            EXPORT std::string SerializeQuery() override;
            EXPORT std::string SerializeResult() override;
            EXPORT void DeserializeResult(const std::string& data) override;
            EXPORT static std::shared_ptr<AppendPlaylistQuery> DeserializeQuery(
                musik::core::ILibraryPtr library, const std::string& data);

        protected:
            /* QueryBase */
            EXPORT bool OnRun(musik::core::db::Connection &db) override;

        private:
            void SendPlaylistMutationBroadcast();

            musik::core::ILibraryPtr library;
            std::shared_ptr<musik::core::TrackList> sharedTracks;
            musik::core::sdk::ITrackList* rawTracks;
            int64_t playlistId;
            int offset;
            bool result{ false };
    };

} } } }
