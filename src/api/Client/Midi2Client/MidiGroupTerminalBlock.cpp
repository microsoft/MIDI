// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================

#include "pch.h"
#include "MidiGroupTerminalBlock.h"
#include "MidiGroupTerminalBlock.g.cpp"

namespace winrt::Windows::Devices::Midi2::implementation
{
    _Use_decl_annotations_
    bool MidiGroupTerminalBlock::InternalUpdateFromPropertyData(UMP_GROUP_TERMINAL_BLOCK_HEADER* const header, std::wstring& name)
    {
        if (header != nullptr)
        {
            m_number = header->Number;
            m_direction = (midi2::MidiGroupTerminalBlockDirection)header->Direction;
            m_firstGroupIndex = header->FirstGroupIndex;
            m_numberOfGroupsSpanned = header->GroupCount;
            m_protocol = (midi2::MidiGroupTerminalBlockProtocol)header->Protocol;
            m_maxDeviceInputBandwidthIn4KBSecondUnits = header->MaxInputBandwidth;
            m_maxDeviceOutputBandwidthIn4KBSecondUnits = header->MaxOutputBandwidth;

            m_name = internal::TrimmedHStringCopy(name);

            return true;
        }
        else
        {
            OutputDebugString(L"Header is null");
            return false;
        }
    }


    // Note: 0x0000 is unknown or not fixed
    // 0x0001 is 31.25kb/s
    // all others are 4kb/second * value

    _Use_decl_annotations_
    uint32_t MidiGroupTerminalBlock::CalculateBandwidth(uint16_t gtbBandwidthValue)
    {
        if (gtbBandwidthValue == 0)
        {
            return (uint32_t)0;
        }
        else if (gtbBandwidthValue == 0x0001)
        {
            return (uint32_t)31250;
        }
        else
        {
            // 4000, not 4096, for 4k bps
            return (uint32_t)(gtbBandwidthValue * 4000);
        }
    }

    uint32_t MidiGroupTerminalBlock::CalculatedMaxDeviceInputBandwidthBitsPerSecond()
    {
        return CalculateBandwidth(m_maxDeviceInputBandwidthIn4KBSecondUnits);
    }

    uint32_t MidiGroupTerminalBlock::CalculatedMaxDeviceOutputBandwidthBitsPerSecond()
    {
        return CalculateBandwidth(m_maxDeviceOutputBandwidthIn4KBSecondUnits);
    }


}
