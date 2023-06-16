// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App SDK and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================

#include "pch.h"
#include "Ump.h"
#include "Ump.g.cpp"


namespace winrt::Microsoft::Devices::Midi2::implementation
{
    com_array<uint32_t> Ump::Words()
    {
        throw hresult_not_implemented();
    }
    uint8_t Ump::WordCount()
    {
        throw hresult_not_implemented();
    }

    winrt::Microsoft::Devices::Midi2::MidiMessageType Ump::MessageType()
    {
        throw hresult_not_implemented();
    }
    void Ump::MessageType(winrt::Microsoft::Devices::Midi2::MidiMessageType const& value)
    {
        throw hresult_not_implemented();
    }
}
