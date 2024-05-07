// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================

#pragma once
#include "MidiEndpointDeviceInformationUpdatedEventArgs.g.h"

namespace MIDI_CPP_NAMESPACE::implementation
{
    struct MidiEndpointDeviceInformationUpdatedEventArgs : MidiEndpointDeviceInformationUpdatedEventArgsT<MidiEndpointDeviceInformationUpdatedEventArgs>
    {
        MidiEndpointDeviceInformationUpdatedEventArgs() = default;

        winrt::hstring Id() const noexcept { return m_endpointDeviceId; }

        bool UpdatedName() const noexcept { return m_updatedName; }
        bool UpdatedEndpointInformation() const noexcept { return m_updatedInProtocolEndpointInformation; }
        bool UpdatedDeviceIdentity() const noexcept { return m_updatedDeviceIdentity; }
        bool UpdatedStreamConfiguration() const noexcept { return m_updatedStreamConfiguration; }
        bool UpdatedFunctionBlocks() const noexcept { return m_updatedFunctionBlocks; }
        bool UpdatedUserMetadata() const noexcept { return m_updatedUserMetadata; }
        bool UpdatedAdditionalCapabilities() const noexcept { return m_updatedAdditionalCapabilities; }

        winrt::Windows::Devices::Enumeration::DeviceInformationUpdate DeviceInformationUpdate() const noexcept { return m_deviceInformationUpdate; }

        void InternalInitialize(
            _In_ winrt::hstring const endpointDeviceId,
            _In_ winrt::Windows::Devices::Enumeration::DeviceInformationUpdate const& deviceInformationUpdate,
            _In_ bool const updatedName,
            _In_ bool const updatedInProtocolEndpointInformation,
            _In_ bool const updatedDeviceIdentity,
            _In_ bool const updatedStreamConfiguration,
            _In_ bool const updatedFunctionBlocks,
            _In_ bool const updatedUserMetadata,
            _In_ bool const updatedAdditionalCapabilities
        ) noexcept;

    private:
        bool m_updatedName{ false };
        bool m_updatedInProtocolEndpointInformation{ false };
        bool m_updatedDeviceIdentity{ false };
        bool m_updatedStreamConfiguration{ false };
        bool m_updatedFunctionBlocks{ false };
        bool m_updatedUserMetadata{ false };
        bool m_updatedAdditionalCapabilities{ false };

        winrt::hstring m_endpointDeviceId{};
        winrt::Windows::Devices::Enumeration::DeviceInformationUpdate m_deviceInformationUpdate{ nullptr };
    };
}
