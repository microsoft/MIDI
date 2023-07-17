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

        MidiBufferReceivedEventArgs(internal::MidiTimestamp timestamp, void* data, uint32_t byteCount)
        {
            // TODO: This could use some checks, like make sure CreateReference returns a non-zero capacity

            _buffer = Windows::Foundation::MemoryBuffer(byteCount);

            memcpy(_buffer.CreateReference().data(), data, byteCount);
        }

        winrt::Windows::Foundation::IMemoryBuffer Buffer() { return _buffer; }

        internal::MidiTimestamp Timestamp() { return _timestamp; }

    private:
        internal::MidiTimestamp _timestamp;
        Windows::Foundation::MemoryBuffer _buffer{ nullptr };
    };
}
