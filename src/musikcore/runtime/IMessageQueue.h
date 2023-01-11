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

#include "IMessage.h"
#include "IMessageTarget.h"

namespace musik { namespace core { namespace runtime {

    class IMessageQueue {
        public:
            EXPORT virtual ~IMessageQueue() { }
            EXPORT virtual void Post(IMessagePtr message, int64_t delayMs = 0) = 0;
            EXPORT virtual int Remove(IMessageTarget *target, int type = -1) = 0;
            EXPORT virtual void Broadcast(IMessagePtr message, int64_t delayMs = 0) = 0;
            EXPORT virtual bool Contains(IMessageTarget *target, int type = -1) = 0;
            EXPORT virtual void Debounce(IMessagePtr message, int64_t delayMs = 0) = 0;
            EXPORT virtual void Register(IMessageTarget* target) = 0;
            EXPORT virtual void Unregister(IMessageTarget* target) = 0;
            EXPORT virtual void RegisterForBroadcasts(IMessageTargetPtr target) = 0;
            EXPORT virtual void UnregisterForBroadcasts(IMessageTarget *target) = 0;
            EXPORT virtual void WaitAndDispatch(int64_t timeoutMillis = -1) = 0;
            EXPORT virtual void Dispatch() = 0;
    };

} } }