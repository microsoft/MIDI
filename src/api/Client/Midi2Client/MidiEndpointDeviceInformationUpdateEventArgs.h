// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================

#pragma once
#include "MidiEndpointDeviceInformationUpdateEventArgs.g.h"

namespace winrt::Windows::Devices::Midi2::implementation
{
    struct MidiEndpointDeviceInformationUpdateEventArgs : MidiEndpointDeviceInformationUpdateEventArgsT<MidiEndpointDeviceInformationUpdateEventArgs>
    {
        MidiEndpointDeviceInformationUpdateEventArgs() = default;

        winrt::hstring Id() { return m_endpointDeviceId; }

        bool UpdatedName() { return m_updatedName; }
        bool UpdatedInProtocolEndpointInformation() { return m_updatedInProtocolEndpointInformation; }
        bool UpdatedFunctionBlocks() { return m_updatedFunctionBlocks; }
        bool UpdatedProtocol() { return m_updatedProtocol; }
        bool UpdatedUserMetadata() { return m_updatedUserMetadata; }
        bool UpdatedAdditionalCapabilities() { return m_updatedAdditionalCapabilities; }
        bool UpdatedJRTimestampHandling() { return m_updatedJRTimestampHandling; }

        winrt::Windows::Devices::Enumeration::DeviceInformationUpdate DeviceInformationUpdate() { return m_deviceInformationUpdate; }

        void InternalInitialize(
            _In_ winrt::hstring endpointDeviceId, 
            _In_ winrt::Windows::Devices::Enumeration::DeviceInformationUpdate deviceInformationUpdate,
            _In_ bool updatedName,
            _In_ bool updatedInProtocolEndpointInformation,
            _In_ bool updatedFunctionBlocks,
            _In_ bool updatedProtocol,
            _In_ bool updatedJRTimestampHandling,
            _In_ bool updatedUserMetadata,
            _In_ bool updatedAdditionalCapabilities
        );

    private:
        bool m_updatedName{ false };
        bool m_updatedInProtocolEndpointInformation{ false };
        bool m_updatedFunctionBlocks{ false };
        bool m_updatedProtocol{ false };
        bool m_updatedUserMetadata{ false };
        bool m_updatedAdditionalCapabilities{ false };
        bool m_updatedJRTimestampHandling{ false };

        winrt::hstring m_endpointDeviceId{};
        winrt::Windows::Devices::Enumeration::DeviceInformationUpdate m_deviceInformationUpdate{ nullptr };
    };
}
