//////////////////////////////////////////////////////////////////////////////
//
// Copyright (c) 2007-2016 musikcube team
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

#include <core/debug.h>
#include <core/audio/DynamicStream.h>
#include <core/sdk/IDecoderFactory.h>
#include <core/plugin/PluginFactory.h>
#include <core/audio/Streams.h>

using namespace musik::core::audio;
using namespace musik::core::sdk;
using musik::core::PluginFactory;

static std::string TAG = "DynamicStream";

DynamicStream::DynamicStream(unsigned int options)
: preferedBufferSampleSize(4096)
, options(options)
, decoderSampleRate(0)
, decoderChannels(0)
, decoderSamplePosition(0) {
    if ((this->options & NoDSP) == 0) {
        typedef PluginFactory::DestroyDeleter<IDSP> Deleter;
        this->dsps = streams::GetDspPlugins();
    }
}

DynamicStream::~DynamicStream() {
}

StreamPtr DynamicStream::Create(unsigned int options) {
    return StreamPtr(new DynamicStream(options));
}

double DynamicStream::SetPosition(double requestedSeconds) {
    double actualSeconds = this->decoder->SetPosition(requestedSeconds);

    if (actualSeconds != -1) {
        double rate = (double) this->decoderSampleRate;

        this->decoderSamplePosition =
            (uint64)(actualSeconds * rate) * this->decoderChannels;
    }

    return actualSeconds;
}

bool DynamicStream::OpenStream(std::string uri) {
    musik::debug::info(TAG, "opening " + uri);

    /* use our file stream abstraction to open the data at the
    specified URI */
    this->dataStream = musik::core::io::DataStreamFactory::OpenUri(uri.c_str());

    if (!this->dataStream) {
        musik::debug::err(TAG, "failed to open " + uri);
        return false;
    }

    this->decoder = streams::GetDecoderForDataStream(this->dataStream);
    return !!this->decoder;
}

void DynamicStream::OnBufferProcessedByPlayer(BufferPtr buffer) {
    this->RecycleBuffer(buffer);
}

BufferPtr DynamicStream::GetNextBufferFromDecoder() {
    /* get a spare buffer, then ask the decoder for some data */
    BufferPtr buffer = this->GetEmptyBuffer();
    if (!this->decoder->GetBuffer(buffer.get())) {
        return BufferPtr(); /* return NULL */
    }

    this->decoderSampleRate = buffer->SampleRate();
    this->decoderChannels = buffer->Channels();
    this->decoderSamplePosition += buffer->Samples();

    /* calculate the position (seconds) in the buffer */
    buffer->SetPosition(
        ((double) this->decoderSamplePosition) /
        ((double)buffer->Channels()) /
        ((double) this->decoderSampleRate));

    return buffer;
}

BufferPtr DynamicStream::GetNextProcessedOutputBuffer() {
    /* ask the decoder for the next buffer */
    BufferPtr currentBuffer = this->GetNextBufferFromDecoder();

    if (currentBuffer) {
        /* try to fill the buffer to its optimal size; if the decoder didn't return
        a full buffer, ask it for some more data. */
        bool moreBuffers = true;
        while (currentBuffer->Samples() < this->preferedBufferSampleSize && moreBuffers) {
            BufferPtr bufferToAppend = this->GetNextBufferFromDecoder();
            if (bufferToAppend) {
                currentBuffer->Append(
                    bufferToAppend->BufferPointer(),
                    bufferToAppend->Samples());

                this->RecycleBuffer(bufferToAppend);
            }
            else {
                moreBuffers = false;
            }
        }

        /* let DSP plugins process the buffer */
        if (this->dsps.size() > 0) {
            BufferPtr oldBuffer = this->GetEmptyBuffer();

            for (Dsps::iterator dsp = this->dsps.begin(); dsp != this->dsps.end(); ++dsp) {
                oldBuffer->CopyFormat(currentBuffer);
                oldBuffer->SetPosition(currentBuffer->Position());

                if ((*dsp)->Process(currentBuffer.get(), oldBuffer.get())) {
                    currentBuffer.swap(oldBuffer);
                }
            }

            this->RecycleBuffer(oldBuffer);
        }
    }

    return currentBuffer;
}

/* returns a previously used buffer, if one is available. otherwise, a
new one will be allocated. */
BufferPtr DynamicStream::GetEmptyBuffer() {
    BufferPtr buffer;
    if (!this->recycledBuffers.empty()) {
        buffer = this->recycledBuffers.front();
        this->recycledBuffers.pop_front();
    }
    else {
        buffer = Buffer::Create();
    }

    return buffer;
}

/* marks a used buffer as recycled so it can be re-used later. */
void DynamicStream::RecycleBuffer(BufferPtr oldBuffer) {
    this->recycledBuffers.push_back(oldBuffer);
}