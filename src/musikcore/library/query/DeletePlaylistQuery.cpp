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
#include "DeletePlaylistQuery.h"

#include <musikcore/db/ScopedTransaction.h>
#include <musikcore/db/Statement.h>
#include <musikcore/support/Messages.h>
#include <musikcore/runtime/Message.h>

#pragma warning(push, 0)
#include <nlohmann/json.hpp>
#pragma warning(pop)

using namespace musik::core;
using namespace musik::core::db;
using namespace musik::core::library::query;
using namespace musik::core::runtime;

const std::string DeletePlaylistQuery::kQueryName = "DeletePlaylistQuery";

static std::string DELETE_PLAYLIST_TRACKS_QUERY =
    "DELETE FROM playlist_tracks WHERE playlist_id=?;";

static std::string DELETE_PLAYLIST_QUERY =
    "DELETE FROM playlists WHERE id=?;";

DeletePlaylistQuery::DeletePlaylistQuery(musik::core::ILibraryPtr library, int64_t playlistId) noexcept {
    this->library = library;
    this->playlistId = playlistId;
}

bool DeletePlaylistQuery::OnRun(musik::core::db::Connection &db) {
    ScopedTransaction transaction(db);

    /* delete the tracks */
    Statement deleteTracks(DELETE_PLAYLIST_TRACKS_QUERY.c_str(), db);
    deleteTracks.BindInt64(0, this->playlistId);

    if (deleteTracks.Step() == db::Error) {
        transaction.Cancel();
        this->result = false;
    }
    else {
        /* delete the container */
        Statement deletePlaylist(DELETE_PLAYLIST_QUERY.c_str(), db);
        deletePlaylist.BindInt64(0, this->playlistId);

        if (deletePlaylist.Step() == db::Error) {
            transaction.Cancel();
            this->result = false;
        }
        else {
            this->SendPlaylistMutationBroadcast();
            this->result = true;
        }
    }
    return this->result;
}

void DeletePlaylistQuery::SendPlaylistMutationBroadcast() {
    this->library->GetMessageQueue().Broadcast(
        Message::Create(nullptr, message::PlaylistModified, playlistId));
}

/* ISerializableQuery */

std::string DeletePlaylistQuery::SerializeQuery() {
    nlohmann::json output = {
    { "name", kQueryName },
        { "options", {
            { "playlistId", this->playlistId },
        }}
    };
    return output.dump();
}

std::string DeletePlaylistQuery::SerializeResult() {
    nlohmann::json output = { { "result", this->result } };
    return output.dump();
}

void DeletePlaylistQuery::DeserializeResult(const std::string& data) {
    auto input = nlohmann::json::parse(data);
    this->result = input["result"].get<bool>();
    this->SetStatus(result ? IQuery::Finished : IQuery::Failed);
    if (this->result) {
        this->SendPlaylistMutationBroadcast();
    }
}

std::shared_ptr<DeletePlaylistQuery> DeletePlaylistQuery::DeserializeQuery(
    musik::core::ILibraryPtr library, const std::string& data)
{
    auto options = nlohmann::json::parse(data)["options"];
    return std::make_shared<DeletePlaylistQuery>(library, options["playlistId"].get<int64_t>());
}