// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once
#include "MidiEndpointDeviceInformationRemovedEventArgs.g.h"


namespace winrt::Microsoft::Windows::Devices::Midi2::implementation
{
    struct MidiEndpointDeviceInformationRemovedEventArgs : MidiEndpointDeviceInformationRemovedEventArgsT<MidiEndpointDeviceInformationRemovedEventArgs>
    {
        MidiEndpointDeviceInformationRemovedEventArgs() = default;

        winrt::hstring EndpointDeviceId() const noexcept { return m_removedDeviceId; }
        winrt::Windows::Devices::Enumeration::DeviceInformationUpdate DeviceInformationUpdate() const noexcept { return m_deviceInformationUpdate; }

        void InternalInitialize(
            _In_ winrt::hstring const removedDeviceId,
            _In_ winrt::Windows::Devices::Enumeration::DeviceInformationUpdate const& deviceInformationUpdate
        ) noexcept;

    private:
        winrt::hstring m_removedDeviceId{};
        winrt::Windows::Devices::Enumeration::DeviceInformationUpdate m_deviceInformationUpdate{ nullptr };


    };
}
