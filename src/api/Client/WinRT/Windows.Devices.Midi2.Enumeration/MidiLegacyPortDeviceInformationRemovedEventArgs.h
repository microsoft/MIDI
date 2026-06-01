// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services WinRT API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once
#include "Legacy.MidiLegacyPortDeviceInformationRemovedEventArgs.g.h"

namespace winrt::Windows::Devices::Midi2::Enumeration::Legacy::implementation
{
    struct MidiLegacyPortDeviceInformationRemovedEventArgs : MidiLegacyPortDeviceInformationRemovedEventArgsT<MidiLegacyPortDeviceInformationRemovedEventArgs>
    {
        MidiLegacyPortDeviceInformationRemovedEventArgs() = default;

        winrt::hstring PortDeviceId() const noexcept { return m_portDeviceId; }
        enumeration::DeviceInformationUpdate DeviceInformationUpdate() const noexcept { return m_deviceInformationUpdate; }

        void InternalInitialize(_In_ enumeration::DeviceInformationUpdate const& device, _In_ winrt::hstring const& portDeviceId) noexcept
        {
            m_deviceInformationUpdate = device;
            m_portDeviceId = portDeviceId;
        }

    private:
        winrt::hstring m_portDeviceId{ };
        enumeration::DeviceInformationUpdate m_deviceInformationUpdate{ nullptr };

    };
}
