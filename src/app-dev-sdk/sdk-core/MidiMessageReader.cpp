// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App SDK and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================

#include "pch.h"
#include "MidiMessageReader.h"
#include "MidiMessageReader.g.cpp"

namespace winrt::Microsoft::Devices::Midi2::implementation
{
    winrt::Microsoft::Devices::Midi2::MidiMessageReaderNoMoreDataBehavior MidiMessageReader::NoMoreDataBehavior()
    {
        throw hresult_not_implemented();
    }
    void MidiMessageReader::NoMoreDataBehavior(winrt::Microsoft::Devices::Midi2::MidiMessageReaderNoMoreDataBehavior const& value)
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
    winrt::Microsoft::Devices::Midi2::MidiMessageType MidiMessageReader::PeekNextMessageType()
    {
        throw hresult_not_implemented();
    }
    winrt::Microsoft::Devices::Midi2::UmpWithTimestamp MidiMessageReader::PeekNextMessage()
    {
        throw hresult_not_implemented();
    }
    winrt::Windows::Foundation::Collections::IVector<winrt::Microsoft::Devices::Midi2::UmpWithTimestamp> MidiMessageReader::ReadToEnd()
    {
        throw hresult_not_implemented();
    }
    winrt::Microsoft::Devices::Midi2::UmpWithTimestamp MidiMessageReader::ReadNextMessage()
    {
        throw hresult_not_implemented();
    }
}
