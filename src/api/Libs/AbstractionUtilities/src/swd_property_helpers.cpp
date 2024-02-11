// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================


#include "pch.h"


namespace Windows::Devices::Midi2::Internal
{







    _Use_decl_annotations_
    bool AddStandardEndpointProperties(
        std::vector<DEVPROPERTY>& interfaceDeviceProperties,

        GUID const& abstractionLayerGuid,
        MidiEndpointDevicePurposePropertyValue const& endpointPurpose,

        std::wstring const& friendlyName,

        std::wstring const& transportMnemonic,
        std::wstring const& transportSuppliedEndpointName,
        std::wstring const& transportSuppliedEndpointDescription,

        std::wstring const& userSuppliedEndpointName,
        std::wstring const& userSuppliedEndpointDescription,

        std::wstring const& uniqueIdentifier,

        MidiDataFormat const& supportedDataFormats,
        BYTE const& nativeDataFormat,

        bool const& supportsMultiClient,
        bool const& requiresMetadataHandler,
        bool const& generateIncomingTimestamps
    )
    {
        try
        {
            interfaceDeviceProperties.push_back(BuildGuidDevProperty(PKEY_MIDI_AbstractionLayer, abstractionLayerGuid));
            interfaceDeviceProperties.push_back(BuildUInt32DevProperty(PKEY_MIDI_EndpointDevicePurpose, endpointPurpose));

            interfaceDeviceProperties.push_back(BuildWStringDevProperty(DEVPKEY_DeviceInterface_FriendlyName, friendlyName));

            // transport information

            interfaceDeviceProperties.push_back(BuildWStringDevProperty(PKEY_MIDI_TransportMnemonic, transportMnemonic));
            interfaceDeviceProperties.push_back(BuildWStringDevProperty(PKEY_MIDI_TransportSuppliedEndpointName, transportSuppliedEndpointName));
            interfaceDeviceProperties.push_back(BuildWStringDevProperty(PKEY_MIDI_TransportSuppliedDescription, transportSuppliedEndpointDescription));

            interfaceDeviceProperties.push_back(BuildWStringDevProperty(PKEY_MIDI_SerialNumber, uniqueIdentifier));


            // user-supplied information, if available

            if (!userSuppliedEndpointName.empty())
            {
                interfaceDeviceProperties.push_back(BuildWStringDevProperty(PKEY_MIDI_UserSuppliedEndpointName, userSuppliedEndpointName));
            }

            if (!userSuppliedEndpointDescription.empty())
            {
                interfaceDeviceProperties.push_back(BuildWStringDevProperty(PKEY_MIDI_UserSuppliedDescription, userSuppliedEndpointDescription));
            }


            // data format. These each have different uses. Native format is the device's format. Supported
            // format is how we talk to it from the service (the driver may translate to UMP, for example)

            interfaceDeviceProperties.push_back(BuildByteDevProperty(PKEY_MIDI_NativeDataFormat, nativeDataFormat));
            interfaceDeviceProperties.push_back(BuildUInt32DevProperty(PKEY_MIDI_SupportedDataFormats, supportedDataFormats));

            // behavior

            interfaceDeviceProperties.push_back(BuildBooleanDevProperty(PKEY_MIDI_GenerateIncomingTimestamp, generateIncomingTimestamps));
            interfaceDeviceProperties.push_back(BuildBooleanDevProperty(PKEY_MIDI_EndpointRequiresMetadataHandler, requiresMetadataHandler));


            interfaceDeviceProperties.push_back(BuildBooleanDevProperty(PKEY_MIDI_SupportsMulticlient, supportsMultiClient));

            return true;
        }
        catch (...)
        {

        }

        return false;
    }



}

