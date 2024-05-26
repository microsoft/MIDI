// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
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

        uint8_t Number() { return m_block.Number; }
        winrt::hstring Name() { return m_block.Name.c_str(); }
        midi2::MidiGroupTerminalBlockDirection Direction() { return (midi2::MidiGroupTerminalBlockDirection)m_block.Direction; }
        midi2::MidiGroupTerminalBlockProtocol Protocol() { return (midi2::MidiGroupTerminalBlockProtocol)m_block.Protocol; }

        uint8_t FirstGroupIndex() { return m_block.FirstGroupIndex; }
        uint8_t GroupCount() { return m_block.GroupCount; }
        bool IncludesGroup(_In_ midi2::MidiGroup const& group) { return group.Index() >= FirstGroupIndex() && group.Index() < FirstGroupIndex() + GroupCount(); }


        uint16_t MaxDeviceInputBandwidthIn4KBitsPerSecondUnits() { return m_block.MaxInputBandwidth; }
        uint16_t MaxDeviceOutputBandwidthIn4KBitsPerSecondUnits() { return m_block.MaxOutputBandwidth; }

        uint32_t CalculatedMaxDeviceInputBandwidthBitsPerSecond();
        uint32_t CalculatedMaxDeviceOutputBandwidthBitsPerSecond();


        bool InternalUpdateFromPropertyData(_In_ internal::GroupTerminalBlockInternal block);


        midi2::MidiFunctionBlock AsEquivalentFunctionBlock();


    private:
        internal::GroupTerminalBlockInternal m_block;

        uint32_t CalculateBandwidth(_In_ uint16_t gtbBandwidthValue);

    };
}
//namespace MIDI_CPP_NAMESPACE::factory_implementation
//{
//    struct MidiGroupTerminalBlock : MidiGroupTerminalBlockT<MidiGroupTerminalBlock, implementation::MidiGroupTerminalBlock>
//    {
//    };
//}
