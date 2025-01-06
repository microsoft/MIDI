// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App SDK and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================


#pragma once
#include "MidiGroupTerminalBlock.g.h"

namespace winrt::Microsoft::Windows::Devices::Midi2::implementation
{
    struct MidiGroupTerminalBlock : MidiGroupTerminalBlockT<MidiGroupTerminalBlock>
    {
        MidiGroupTerminalBlock() = default;

        static winrt::hstring ShortLabel() { return internal::ResourceGetHString(IDS_MIDI_COMMON_LABEL_GROUP_TERMINAL_BLOCK_SHORT); }
        static winrt::hstring ShortLabelPlural() { return internal::ResourceGetHString(IDS_MIDI_COMMON_LABEL_GROUP_TERMINAL_BLOCK_SHORT_PLURAL); }
        static winrt::hstring LongLabel() { return internal::ResourceGetHString(IDS_MIDI_COMMON_LABEL_GROUP_TERMINAL_BLOCK_FULL); }
        static winrt::hstring LongLabelPlural() { return internal::ResourceGetHString(IDS_MIDI_COMMON_LABEL_GROUP_TERMINAL_BLOCK_FULL_PLURAL); }

        uint8_t Number() { return m_block.Number; }
        winrt::hstring Name() { return m_block.Name.c_str(); }
        midi2::MidiGroupTerminalBlockDirection Direction() { return (midi2::MidiGroupTerminalBlockDirection)m_block.Direction; }
        midi2::MidiGroupTerminalBlockProtocol Protocol() { return (midi2::MidiGroupTerminalBlockProtocol)m_block.Protocol; }

        midi2::MidiGroup FirstGroup() const noexcept { return winrt::make<midi2::implementation::MidiGroup>(m_block.FirstGroupIndex); }

        uint8_t GroupCount() { return m_block.GroupCount; }
        bool IncludesGroup(_In_ midi2::MidiGroup const& group) { return group.Index() >= m_block.FirstGroupIndex && group.Index() < m_block.FirstGroupIndex + GroupCount(); }


        uint16_t MaxDeviceInputBandwidthIn4KBitsPerSecondUnits() { return m_block.MaxInputBandwidth; }
        uint16_t MaxDeviceOutputBandwidthIn4KBitsPerSecondUnits() { return m_block.MaxOutputBandwidth; }

        uint32_t CalculatedMaxDeviceInputBandwidthBitsPerSecond();
        uint32_t CalculatedMaxDeviceOutputBandwidthBitsPerSecond();


        bool InternalUpdateFromPropertyData(_In_ internal::GroupTerminalBlockInternal block, _In_ winrt::hstring deviceName);


        midi2::MidiFunctionBlock AsEquivalentFunctionBlock();

        winrt::hstring ToString();


    private:
        internal::GroupTerminalBlockInternal m_block;

        winrt::hstring m_deviceName{};

        uint32_t CalculateBandwidth(_In_ uint16_t gtbBandwidthValue);

    };
}
namespace winrt::Microsoft::Windows::Devices::Midi2::factory_implementation
{
    struct MidiGroupTerminalBlock : MidiGroupTerminalBlockT<MidiGroupTerminalBlock, implementation::MidiGroupTerminalBlock>
    {
    };
}
