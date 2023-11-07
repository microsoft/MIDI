// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================

#pragma once
#include "MidiCIMessageUtility.g.h"

namespace winrt::Windows::Devices::Midi2::implementation
{
    struct MidiCIMessageUtility
    {
        MidiCIMessageUtility() = default;

        static void Foo();
    };
}
namespace winrt::Windows::Devices::Midi2::factory_implementation
{
    struct MidiCIMessageUtility : MidiCIMessageUtilityT<MidiCIMessageUtility, implementation::MidiCIMessageUtility, winrt::static_lifetime>
    {
    };
}
