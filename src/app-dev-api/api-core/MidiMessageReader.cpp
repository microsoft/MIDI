// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================


#include "pch.h"
#include "MidiMessageReader.h"
#include "MidiMessageReader.g.cpp"


namespace winrt::Windows::Devices::Midi2::implementation
{
    winrt::Windows::Devices::Midi2::MidiMessageReaderEndOfMessagesBehavior MidiMessageReader::EndOfMessagesBehavior()
    {
        throw hresult_not_implemented();
    }
    void MidiMessageReader::EndOfMessagesBehavior(winrt::Windows::Devices::Midi2::MidiMessageReaderEndOfMessagesBehavior const& value)
    {
        throw hresult_not_implemented();
    }
    bool MidiMessageReader::EndOfMessages()
    {
        throw hresult_not_implemented();
    }
    uint64_t MidiMessageReader::PeekNextTimestamp()
    {
        throw hresult_not_implemented();
    }
    winrt::Windows::Devices::Midi2::MidiUmpSize MidiMessageReader::PeekNextUmpSize()
    {
        throw hresult_not_implemented();
    }
    winrt::Windows::Devices::Midi2::MidiUmp32 MidiMessageReader::ReadUmp32()
    {
        throw hresult_not_implemented();
    }
    winrt::Windows::Devices::Midi2::MidiUmp64 MidiMessageReader::ReadUmp64()
    {
        throw hresult_not_implemented();
    }
    winrt::Windows::Devices::Midi2::MidiUmp96 MidiMessageReader::ReadUmp96()
    {
        throw hresult_not_implemented();
    }
    winrt::Windows::Devices::Midi2::MidiUmp128 MidiMessageReader::ReadUmp128()
    {
        throw hresult_not_implemented();
    }
    uint32_t MidiMessageReader::ReadBuffer(winrt::Windows::Foundation::IMemoryBufferReference const& buffer, uint32_t byteOffsetinBuffer, uint32_t maxBytesToRead)
    {
        throw hresult_not_implemented();
    }
}
