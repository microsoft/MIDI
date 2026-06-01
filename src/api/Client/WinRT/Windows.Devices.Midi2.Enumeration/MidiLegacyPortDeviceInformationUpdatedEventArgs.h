// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services WinRT API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once
#include "Legacy.MidiLegacyPortDeviceInformationUpdatedEventArgs.g.h"

namespace winrt::Windows::Devices::Midi2::Enumeration::Legacy::implementation
{
    struct MidiLegacyPortDeviceInformationUpdatedEventArgs : MidiLegacyPortDeviceInformationUpdatedEventArgsT<MidiLegacyPortDeviceInformationUpdatedEventArgs>
    {
        MidiLegacyPortDeviceInformationUpdatedEventArgs() = default;

        winrt::hstring PortDeviceId() const noexcept { return m_portDeviceId; }
        enumeration::DeviceInformationUpdate DeviceInformationUpdate() const noexcept { return m_deviceInformationUpdate; }

        bool IsNameUpdated() const noexcept { return m_isNameUpdated; }
        bool IsPortNumberUpdated() const noexcept { return m_isPortNumberUpdated; }

        void InternalInitialize(
            winrt::hstring const& portDeviceId,
            enumeration::DeviceInformationUpdate const& deviceInformationUpdate,
            bool isNameUpdated,
            bool isPortNumberUpdated) noexcept
        {
            m_portDeviceId = portDeviceId;
            m_deviceInformationUpdate = deviceInformationUpdate;
            m_isNameUpdated = isNameUpdated;
            m_isPortNumberUpdated = isPortNumberUpdated;
        }

    private:
        winrt::hstring m_portDeviceId{ };
        enumeration::DeviceInformationUpdate m_deviceInformationUpdate{ nullptr };
        bool m_isNameUpdated{ false };
        bool m_isPortNumberUpdated{ false };
    };
}
