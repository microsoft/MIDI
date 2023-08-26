// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================


#pragma once
#include "MidiGroup.g.h"

namespace winrt::Windows::Devices::Midi2::implementation
{
    struct MidiGroup : MidiGroupT<MidiGroup>
    {
        MidiGroup() = default;
        MidiGroup(_In_ uint8_t index) { if (index >= 0 && index <= 15) m_index = index; }


        uint8_t Index() const { return m_index; }
        void Index(uint8_t value) { if (value >= 0 && value <= 15) m_index = value; }

        uint8_t NumberForDisplay() const { return m_index + 1; }

    private:
        uint8_t m_index{ 0 };
    };
}
namespace winrt::Windows::Devices::Midi2::factory_implementation
{
    struct MidiGroup : MidiGroupT<MidiGroup, implementation::MidiGroup>
    {
    };
}
