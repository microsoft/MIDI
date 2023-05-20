// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================

#include "pch.h"
#include "MidiChannel.h"
#include "MidiChannel.g.cpp"


namespace winrt::Microsoft::Devices::Midi2::implementation
{
    uint8_t MidiChannel::ChannelIndex()
    {
        throw hresult_not_implemented();
    }
    uint8_t MidiChannel::ChannelNumberForDisplay()
    {
        throw hresult_not_implemented();
    }
    bool MidiChannel::IsActive()
    {
        throw hresult_not_implemented();
    }
    hstring MidiChannel::Name()
    {
        throw hresult_not_implemented();
    }
}
