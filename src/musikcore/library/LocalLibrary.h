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

#include <musikcore/config.h>
#include <musikcore/db/Connection.h>

#include <musikcore/library/ILibrary.h>
#include <musikcore/library/IIndexer.h>
#include <musikcore/library/IQuery.h>
#include <musikcore/library/QueryBase.h>

#include <thread>
#include <mutex>
#include <condition_variable>
#include <string>

#include <sigslot/sigslot.h>

namespace musik { namespace core { namespace library {

    class LocalLibrary :
        public ILibrary,
        public musik::core::runtime::IMessageTarget,
        public std::enable_shared_from_this<LocalLibrary>
    {
        public:
            using LocalQuery = musik::core::library::query::QueryBase;
            using LocalQueryPtr = std::shared_ptr<LocalQuery>;
            using MessageQueue = musik::core::runtime::IMessageQueue;
            using IIndexer = musik::core::IIndexer;

            static ILibraryPtr Create(std::string name, int id, MessageQueue* messageQueue);

            EXPORT LocalLibrary(const LocalLibrary&) = delete;
            EXPORT virtual ~LocalLibrary();

            /* ILibrary */
            EXPORT int Enqueue(QueryPtr query, Callback cb = Callback()) override;
            EXPORT int EnqueueAndWait(QueryPtr query, size_t timeoutMs = kWaitIndefinite, Callback cb = Callback()) override;
            EXPORT IIndexer *Indexer() override;
            EXPORT int Id() override;
            EXPORT const std::string& Name() override;
            EXPORT void SetMessageQueue(musik::core::runtime::IMessageQueue& queue) override;
            EXPORT MessageQueue& GetMessageQueue() override { return *messageQueue; }
            EXPORT IResourceLocator& GetResourceLocator() override;
            EXPORT bool IsConfigured() override;
            EXPORT ConnectionState GetConnectionState() const override { return ConnectionState::Connected; }
            EXPORT Type GetType() const override { return Type::Local; }
            EXPORT void Close() override;

            /* IMessageTarget */
            EXPORT void ProcessMessage(musik::core::runtime::IMessage &message) override;

            /* implementation specific */
            EXPORT db::Connection& GetConnection() { return this->db; }
            EXPORT std::string GetLibraryDirectory();
            EXPORT std::string GetDatabaseFilename();
            EXPORT static void CreateDatabase(db::Connection &db);

            /* indexes */
            static void DropIndexes(db::Connection &db);
            static void CreateIndexes(db::Connection &db);
            static void InvalidateTrackMetadata(db::Connection &db);

        private:
            class QueryCompletedMessage;

            struct QueryContext {
                LocalQueryPtr query;
                Callback callback;
            };

            using QueryContextPtr = std::shared_ptr<QueryContext>;
            using QueryList = std::list<QueryContextPtr>;

            LocalLibrary(std::string name, int id, MessageQueue* messageQueue); /* ctor */

            void RunQuery(QueryContextPtr context, bool notify = true);
            void ThreadProc();
            QueryContextPtr GetNextQuery();

            QueryList queryQueue;

            musik::core::runtime::IMessageQueue* messageQueue;

            std::string identifier;
            int id;
            std::string name;

            std::thread* thread;
            std::condition_variable_any queueCondition;
            std::recursive_mutex mutex;
            std::atomic<bool> exit;

            core::IIndexer *indexer;
            core::db::Connection db;
    };

} } }
