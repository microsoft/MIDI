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
    winrt::Microsoft::Devices::Midi2::MidiGroupList MidiGroupTerminalBlock::IncludedGroups()
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
