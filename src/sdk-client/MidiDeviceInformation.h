// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services Client SDK and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================

#pragma once
#include "MidiDeviceInformation.g.h"


namespace winrt::Microsoft::Devices::Midi2::implementation
{
    struct MidiDeviceInformation : MidiDeviceInformationT<MidiDeviceInformation>
    {
        MidiDeviceInformation() = default;

        static winrt::Microsoft::Devices::Midi2::MidiDeviceInformation FromDeviceInformation(winrt::Windows::Devices::Enumeration::DeviceInformation const& deviceInformation);
        static winrt::Windows::Devices::Enumeration::DeviceWatcher CreateWatcher();
        static winrt::Windows::Devices::Enumeration::DeviceWatcher CreateWatcher(winrt::Microsoft::Devices::Midi2::MidiDeviceSelectorMidiType const& midiDeviceType);
    };
}
namespace winrt::Microsoft::Devices::Midi2::factory_implementation
{
    struct MidiDeviceInformation : MidiDeviceInformationT<MidiDeviceInformation, implementation::MidiDeviceInformation>
    {
    };
}
