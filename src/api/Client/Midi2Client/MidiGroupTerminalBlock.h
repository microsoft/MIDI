// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================

#pragma once
#include "MidiGroupTerminalBlock.g.h"


namespace winrt::Windows::Devices::Midi2::implementation
{
    struct MidiGroupTerminalBlock : MidiGroupTerminalBlockT<MidiGroupTerminalBlock>
    {
        MidiGroupTerminalBlock() = default;

        winrt::hstring Id() { return m_id; }
        winrt::hstring Name() { return m_name; }
        midi2::MidiGroupTerminalBlockDirection Direction() { return m_direction; }
        midi2::MidiGroupTerminalBlockProtocol Protocol() { return m_protocol; }
        collections::IVectorView<midi2::MidiGroup> IncludedGroups() { return m_includedGroups.GetView(); }
        uint16_t MaxDeviceInputBandwidthIn4KBSecondUnits() { return m_maxDeviceInputBandwidthIn4KBSecondUnits; }
        uint16_t MaxDeviceOutputBandwidthIn4KBSecondUnits() { return m_maxDeviceOutputBandwidthIn4KBSecondUnits; }

    private:
        winrt::hstring m_id{};
        winrt::hstring m_name{};
        midi2::MidiGroupTerminalBlockDirection m_direction{ 0 };
        midi2::MidiGroupTerminalBlockProtocol m_protocol{ 0 };
        uint16_t m_maxDeviceInputBandwidthIn4KBSecondUnits{ 0 };
        uint16_t m_maxDeviceOutputBandwidthIn4KBSecondUnits{ 0 };

        collections::IVector<midi2::MidiGroup> m_includedGroups
            { winrt::single_threaded_vector<midi2::MidiGroup>() };

    };
}
namespace winrt::Windows::Devices::Midi2::factory_implementation
{
    struct MidiGroupTerminalBlock : MidiGroupTerminalBlockT<MidiGroupTerminalBlock, implementation::MidiGroupTerminalBlock>
    {
    };
}
