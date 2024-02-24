// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================

#include "pch.h"
#include "MidiEndpointDeviceInformationUpdatedEventArgs.h"
#include "MidiEndpointDeviceInformationUpdatedEventArgs.g.cpp"


namespace winrt::Windows::Devices::Midi2::implementation
{
    _Use_decl_annotations_
    void MidiEndpointDeviceInformationUpdatedEventArgs::InternalInitialize(
        winrt::hstring const endpointDeviceId,
        winrt::Windows::Devices::Enumeration::DeviceInformationUpdate const& deviceInformationUpdate,
        bool const updatedName,
        bool const updatedInProtocolEndpointInformation,
        bool const updatedDeviceIdentity,
        bool const updatedStreamConfiguration,
        bool const updatedFunctionBlocks,
        bool const updatedUserMetadata,
        bool const updatedAdditionalCapabilities
    ) noexcept
    {

        m_updatedName = updatedName;
        m_updatedInProtocolEndpointInformation = updatedInProtocolEndpointInformation;
        m_updatedDeviceIdentity = updatedDeviceIdentity;
        m_updatedStreamConfiguration = updatedStreamConfiguration;
        m_updatedFunctionBlocks = updatedFunctionBlocks;
        m_updatedUserMetadata = updatedUserMetadata;
        m_updatedAdditionalCapabilities = updatedAdditionalCapabilities;

        m_endpointDeviceId = endpointDeviceId;
        m_deviceInformationUpdate = deviceInformationUpdate;
    }

}
