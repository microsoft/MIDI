// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================


#pragma once
#include "MidiMessageWriter.g.h"

#include "midi_service_interface.h"


namespace winrt::Windows::Devices::Midi2::implementation
{
    struct MidiMessageWriter : MidiMessageWriterT<MidiMessageWriter>
    {
        MidiMessageWriter() = default;

        void WriteUmpWords32(uint64_t midiTimestamp, uint32_t umpWord1);
        void WriteUmpWords64(uint64_t midiTimestamp, uint32_t umpWord1, uint32_t umpWord2);
        void WriteUmpWords96(uint64_t midiTimestamp, uint32_t umpWord1, uint32_t umpWord2, uint32_t umpWord3);
        void WriteUmpWords128(uint64_t midiTimestamp, uint32_t umpWord1, uint32_t umpWord2, uint32_t umpWord3, uint32_t umpWord4);
        void WriteUmpWords(uint64_t midiTimeStamp, array_view<uint32_t const> words, uint8_t wordCount);

        uint32_t WriteBuffer(winrt::Windows::Foundation::IMemoryBufferReference const& buffer, uint32_t byteOffsetInBuffer, uint32_t maxBytesToWrite);

        void WriteUmp32Struct(winrt::Windows::Devices::Midi2::MidiUmp32 const& ump);
        void WriteUmp64Struct(winrt::Windows::Devices::Midi2::MidiUmp64 const& ump);
        void WriteUmp96Struct(winrt::Windows::Devices::Midi2::MidiUmp96 const& ump);
        void WriteUmp128Struct(winrt::Windows::Devices::Midi2::MidiUmp128 const& ump);

        void WriteUmp32(winrt::Windows::Devices::Midi2::IMidiUmp32 const& ump);
        void WriteUmp64(winrt::Windows::Devices::Midi2::IMidiUmp64 const& ump);
        void WriteUmp96(winrt::Windows::Devices::Midi2::IMidiUmp96 const& ump);
        void WriteUmp128(winrt::Windows::Devices::Midi2::IMidiUmp128 const& ump);

    private:
        // only one of these gets used for the writer
        com_ptr<IMidiBiDi> _bidiDevice = nullptr;
        com_ptr<IMidiOut> _outputOnlyDevice = nullptr;

    };
}
