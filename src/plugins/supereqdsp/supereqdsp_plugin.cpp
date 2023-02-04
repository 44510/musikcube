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
////////////////////////////////////////////////////////////////////////////

#include "constants.h"
#include <musikcore/sdk/constants.h>
#include <musikcore/sdk/IPlugin.h>
#include <musikcore/sdk/ISchema.h>
#include <musikcore/sdk/IDSP.h>
#include "SuperEqDsp.h"

class SuperEqPlugin : public musik::core::sdk::IPlugin {
    public:
        virtual void Release() { delete this; }
        virtual const char* Name() { return "SuperEq IDSP"; }
        virtual const char* Version() { return "0.1.0"; }
        virtual const char* Author() { return "Naoki Shibata, Alexey Yakovenko, clangen"; }
        virtual const char* Guid() { return "6f0ed53b-0f13-4220-9b0a-ca496b6421cc"; }
        virtual bool Configurable() { return false; }
        virtual void Configure() { }
        virtual void Reload() { SuperEqDsp::NotifyChanged(); }
        virtual int SdkVersion() { return musik::core::sdk::SdkVersion; }
};

#ifdef WIN32
BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved) {
    return true;
}
#endif

extern "C" DLLEXPORT musik::core::sdk::IPlugin* GetPlugin() {
    return new SuperEqPlugin();
}

extern "C" DLLEXPORT musik::core::sdk::IDSP* GetDSP() {
    return new SuperEqDsp();
}

extern "C" DLLEXPORT musik::core::sdk::ISchema* GetSchema();