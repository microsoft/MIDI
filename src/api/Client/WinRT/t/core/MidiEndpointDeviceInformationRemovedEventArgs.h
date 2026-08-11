// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services WinRT API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================


#pragma once
#include "Enumeration.MidiEndpointDeviceInformationRemovedEventArgs.g.h"


namespace winrt::Windows::Devices::Midi2::Enumeration::implementation
{
    struct MidiEndpointDeviceInformationRemovedEventArgs : MidiEndpointDeviceInformationRemovedEventArgsT<MidiEndpointDeviceInformationRemovedEventArgs>
    {
        MidiEndpointDeviceInformationRemovedEventArgs() = default;

        midi2enum::MidiEndpointDeviceInformation RemovedDevice() const noexcept { return m_removedDevice; }
        enumeration::DeviceInformationUpdate DeviceInformationUpdate() const noexcept { return m_deviceInformationUpdate; }

        void InternalInitialize(
            _In_ midi2enum::MidiEndpointDeviceInformation const& removedDevice,
            _In_ enumeration::DeviceInformationUpdate const& deviceInformationUpdate
        ) noexcept;

    private:
        midi2enum::MidiEndpointDeviceInformation m_removedDevice{ nullptr };
        enumeration::DeviceInformationUpdate m_deviceInformationUpdate{ nullptr };


    };
}
