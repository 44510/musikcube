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

#include <musikcore/config.h>
#include <musikcore/sdk/IBuffer.h>

namespace musik { namespace core { namespace audio {

    class Buffer;
    class Stream;

    class Buffer : public musik::core::sdk::IBuffer {
        public:
            enum Flags {
                NoFlags = 0,
                ImmutableSize = 1,
                NoDelete = 2
            };

            Buffer(Flags flags = NoFlags) noexcept;
            Buffer(float* buffer, int samples) noexcept;

            virtual ~Buffer();

            long SampleRate() const noexcept override;
            void SetSampleRate(long sampleRate) noexcept override;
            int Channels() const noexcept override;
            void SetChannels(int channels) noexcept override;
            float* BufferPointer() const noexcept override;
            long Samples() const noexcept override;
            void SetSamples(long samples) override;
            long Bytes() const noexcept override;
            void Release() noexcept override { delete this; }

            double Position() const noexcept;
            void SetPosition(double position) noexcept;
            void Copy(float const* buffer, long samples, long offset = 0);
            void CopyFormat(Buffer* fromBuffer) noexcept;

        private:
            void ResizeBuffer();

            float *buffer;
            long samples;
            long internalBufferSize;
            long sampleRate;
            int channels;
            double position;
            int flags;
    };

} } }
