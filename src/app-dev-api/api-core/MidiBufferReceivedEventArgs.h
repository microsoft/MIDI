// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================

#pragma once
#include "MidiBufferReceivedEventArgs.g.h"


namespace winrt::Windows::Devices::Midi2::implementation
{
    struct MidiBufferReceivedEventArgs : MidiBufferReceivedEventArgsT<MidiBufferReceivedEventArgs>
    {
        MidiBufferReceivedEventArgs() = default;
        ~MidiBufferReceivedEventArgs() { if (_buffer) _buffer.Close(); }

        MidiBufferReceivedEventArgs(internal::MidiTimestamp timestamp, void* data, uint32_t byteCount)
        {
            // TODO: This could use some checks, like make sure CreateReference returns a non-zero capacity

            _timestamp = timestamp;

            _buffer = Windows::Foundation::MemoryBuffer(byteCount);

            auto ref = _buffer.CreateReference();

            if (ref.Capacity() == byteCount)
            {
                memcpy(ref.data(), data, byteCount);
            }
            else
            {
                // I don't like doing this in a constructor
                throw hresult_error();
            }
        }

        winrt::Windows::Foundation::IMemoryBuffer Buffer() { return _buffer; }

        internal::MidiTimestamp Timestamp() { return _timestamp; }

    private:
        internal::MidiTimestamp _timestamp;
        Windows::Foundation::MemoryBuffer _buffer{ nullptr };
    };
}
