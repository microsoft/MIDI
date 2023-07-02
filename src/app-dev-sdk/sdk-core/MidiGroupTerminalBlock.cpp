// Copyright(c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App SDK and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================

#include "pch.h"
#include "MidiGroupTerminalBlock.h"
#include "MidiGroupTerminalBlock.g.cpp"


namespace winrt::Microsoft::Devices::Midi2::implementation
{
    hstring MidiGroupTerminalBlock::Id()
    {
        throw hresult_not_implemented();
    }
    hstring MidiGroupTerminalBlock::Name()
    {
        throw hresult_not_implemented();
    }
    winrt::Microsoft::Devices::Midi2::MidiGroupTerminalBlockDirection MidiGroupTerminalBlock::Direction()
    {
        throw hresult_not_implemented();
    }
    winrt::Microsoft::Devices::Midi2::MidiGroupTerminalBlockProtocol MidiGroupTerminalBlock::Protocol()
    {
        throw hresult_not_implemented();
    }
    winrt::Windows::Foundation::Collections::IVectorView<winrt::Microsoft::Devices::Midi2::MidiGroup> MidiGroupTerminalBlock::IncludedGroups()
    {
        throw hresult_not_implemented();
    }
    uint16_t MidiGroupTerminalBlock::MaxDeviceInputBandwidthIn4KBSecondUnits()
    {
        throw hresult_not_implemented();
    }
    uint16_t MidiGroupTerminalBlock::MaxDeviceOutputBandwidthIn4KBSecondUnits()
    {
        throw hresult_not_implemented();
    }
}
