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

#include <string>
#include <vector>
#include <functional>
#include <limits>

#include <sigslot/sigslot.h>

#include <musikcore/library/IIndexer.h>
#include <musikcore/library/IQuery.h>
#include <musikcore/sdk/ITrack.h>
#include <musikcore/runtime/IMessageQueue.h>

namespace musik { namespace core {

    static size_t kWaitIndefinite = std::numeric_limits<size_t>::max();

    class ILibrary {
        public:
            using QueryPtr = std::shared_ptr<musik::core::db::IQuery>;
            using Callback = std::function<void(QueryPtr)>;

            enum class Type: int {
                Local = 1,
                Remote = 2
            };

            enum class ConnectionState: int {
                Disconnected = 0,
                Connected = 1,
                Connecting = 2,
                AuthenticationFailure = 3
            };

            class IResourceLocator {
                public:
                    EXPORT virtual std::string GetTrackUri(
                        musik::core::sdk::ITrack* track,
                        const std::string& defaultUri = "") = 0;
            };

            sigslot::signal1<musik::core::db::IQuery*> QueryCompleted;
            sigslot::signal1<ConnectionState> ConnectionStateChanged;

            EXPORT virtual ~ILibrary() { }

            EXPORT virtual int Enqueue(QueryPtr query, Callback cb = Callback()) = 0;
            EXPORT virtual int EnqueueAndWait(QueryPtr query, size_t timeoutMs = kWaitIndefinite, Callback cb = Callback()) = 0;
            EXPORT virtual IIndexer *Indexer() = 0;
            EXPORT virtual int Id() = 0;
            EXPORT virtual const std::string& Name() = 0;
            EXPORT virtual void SetMessageQueue(musik::core::runtime::IMessageQueue& queue) = 0;
            EXPORT virtual musik::core::runtime::IMessageQueue& GetMessageQueue() = 0;
            EXPORT virtual IResourceLocator& GetResourceLocator() = 0;
            EXPORT virtual bool IsConfigured() = 0;
            EXPORT virtual ConnectionState GetConnectionState() const = 0;
            EXPORT virtual Type GetType() const = 0;
            EXPORT virtual void Close() = 0;
    };

    typedef std::shared_ptr<ILibrary> ILibraryPtr;

} }
