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
    // NOTE: The parameters in here are all passed to the API by ref, so the
    // calling code needs to keep them around until the device creation returns.
    bool AddStandardEndpointProperties(
    )
    {
        try
        {
            //interfaceDeviceProperties.push_back(BuildGuidDevProperty(PKEY_MIDI_AbstractionLayer, abstractionLayerGuid));
            //interfaceDeviceProperties.push_back(BuildUInt32DevProperty(PKEY_MIDI_EndpointDevicePurpose, endpointPurpose));

            //interfaceDeviceProperties.push_back(BuildWStringDevProperty(DEVPKEY_DeviceInterface_FriendlyName, friendlyName));

            ////// transport information

            //interfaceDeviceProperties.push_back(BuildWStringDevProperty(PKEY_MIDI_TransportMnemonic, transportMnemonic));
            //interfaceDeviceProperties.push_back(BuildWStringDevProperty(PKEY_MIDI_TransportSuppliedEndpointName, transportSuppliedEndpointName));
            //interfaceDeviceProperties.push_back(BuildWStringDevProperty(PKEY_MIDI_TransportSuppliedDescription, transportSuppliedEndpointDescription));

            //interfaceDeviceProperties.push_back(BuildWStringDevProperty(PKEY_MIDI_SerialNumber, uniqueIdentifier));


            //// user-supplied information, if available

            //interfaceDeviceProperties.push_back(BuildWStringDevProperty(PKEY_MIDI_UserSuppliedEndpointName, userSuppliedEndpointName));
            //interfaceDeviceProperties.push_back(BuildWStringDevProperty(PKEY_MIDI_UserSuppliedDescription, userSuppliedEndpointDescription));


            //// data format. These each have different uses. Native format is the device's format. Supported
            //// format is how we talk to it from the service (the driver may translate to UMP, for example)

            //interfaceDeviceProperties.push_back(BuildByteDevProperty(PKEY_MIDI_NativeDataFormat, nativeDataFormat));
            //interfaceDeviceProperties.push_back(BuildUInt32DevProperty(PKEY_MIDI_SupportedDataFormats, supportedDataFormats));

            //// behavior

            //interfaceDeviceProperties.push_back(BuildBooleanDevProperty(PKEY_MIDI_GenerateIncomingTimestamp, generateIncomingTimestamps));
            //interfaceDeviceProperties.push_back(BuildBooleanDevProperty(PKEY_MIDI_EndpointRequiresMetadataHandler, requiresMetadataHandler));

            //interfaceDeviceProperties.push_back(BuildBooleanDevProperty(PKEY_MIDI_SupportsMulticlient, supportsMultiClient));

            return true;
        }
        catch (...)
        {

        }

        return false;
    }



}

