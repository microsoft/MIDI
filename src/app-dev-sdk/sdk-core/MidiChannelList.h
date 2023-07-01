// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App SDK and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================

#pragma once
#include "MidiChannelList.g.h"
#include "MidiListBase.h"

namespace winrt::Microsoft::Devices::Midi2::implementation
{
    struct MidiChannelList : MidiChannelListT<MidiChannelList, Microsoft::Devices::Midi2::implementation::MidiListBase>
    {
        MidiChannelList() = default;

    };
}
namespace winrt::Microsoft::Devices::Midi2::factory_implementation
{
    struct MidiChannelList : MidiChannelListT<MidiChannelList, implementation::MidiChannelList>
    {
    };
}
