// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services Client SDK and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================


#pragma once
#include "MidiChannel.g.h"


namespace winrt::Microsoft::Devices::Midi2::implementation
{
    struct MidiChannel : MidiChannelT<MidiChannel>
    {
        MidiChannel() = default;

        uint8_t ChannelIndex();
        uint8_t ChannelNumberForDisplay();
        bool IsActive();
        hstring Name();
    };
}
namespace winrt::Microsoft::Devices::Midi2::factory_implementation
{
    struct MidiChannel : MidiChannelT<MidiChannel, implementation::MidiChannel>
    {
    };
}
