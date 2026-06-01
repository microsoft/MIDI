// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services WinRT API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================


#pragma once
#include "MidiEndpointDeviceInformationRemovedEventArgs.g.h"


namespace winrt::Windows::Devices::Midi2::Enumeration::implementation
{
    struct MidiEndpointDeviceInformationRemovedEventArgs : MidiEndpointDeviceInformationRemovedEventArgsT<MidiEndpointDeviceInformationRemovedEventArgs>
    {
        MidiEndpointDeviceInformationRemovedEventArgs() = default;

        winrt::hstring EndpointDeviceId() const noexcept { return m_removedDeviceId; }
        enumeration::DeviceInformationUpdate DeviceInformationUpdate() const noexcept { return m_deviceInformationUpdate; }

        void InternalInitialize(
            _In_ winrt::hstring const removedDeviceId,
            _In_ enumeration::DeviceInformationUpdate const& deviceInformationUpdate
        ) noexcept;

    private:
        winrt::hstring m_removedDeviceId{};
       enumeration::DeviceInformationUpdate m_deviceInformationUpdate{ nullptr };


    };
}
