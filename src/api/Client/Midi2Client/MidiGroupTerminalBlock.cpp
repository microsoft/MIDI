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
}
