// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services WinRT API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once
#include "Enumeration.Legacy.MidiLegacyPortDeviceInformationUpdatedEventArgs.g.h"

namespace winrt::Windows::Devices::Midi2::Enumeration::Legacy::implementation
{
    struct MidiLegacyPortDeviceInformationUpdatedEventArgs : MidiLegacyPortDeviceInformationUpdatedEventArgsT<MidiLegacyPortDeviceInformationUpdatedEventArgs>
    {
        MidiLegacyPortDeviceInformationUpdatedEventArgs() = default;

        legacy::MidiLegacyPortDeviceInformation UpdatedDevice() const noexcept { return m_device; }

        winrt::Windows::Devices::Enumeration::DeviceInformationUpdate DeviceInformationUpdate() const noexcept { return m_deviceInformationUpdate; }

        bool IsNameUpdated() const noexcept { return m_isNameUpdated; }
        bool IsNumberUpdated() const noexcept { return m_isPortNumberUpdated; }

        void InternalInitialize(
            _In_ legacy::MidiLegacyPortDeviceInformation const& device,
            _In_ enumeration::DeviceInformationUpdate const& deviceInformationUpdate,
            _In_ bool isNameUpdated,
            _In_ bool isPortNumberUpdated) noexcept
        {
            m_device = device;
            m_isNameUpdated = isNameUpdated;
            m_isPortNumberUpdated = isPortNumberUpdated;
            m_deviceInformationUpdate = deviceInformationUpdate;
        }

    private:
        legacy::MidiLegacyPortDeviceInformation m_device{ nullptr };
        enumeration::DeviceInformationUpdate m_deviceInformationUpdate{ nullptr };
        bool m_isNameUpdated{ false };
        bool m_isPortNumberUpdated{ false };
    };
}
