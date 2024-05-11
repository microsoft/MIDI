// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================

#include "pch.h"
#include "MidiLoopbackEndpointManager.h"
#include "MidiLoopbackEndpointManager.g.cpp"

namespace winrt::Microsoft::Devices::Midi2::Endpoints::Loopback::implementation
{
    bool MidiLoopbackEndpointManager::IsTransportAvailable()
    {
        throw hresult_not_implemented();


        // TODO: Check to see if service abstraction is installed and running. May require a new service call
    }

    _Use_decl_annotations_
    loop::MidiLoopbackEndpointCreationResult MidiLoopbackEndpointManager::CreateTransientLoopbackEndpoints(
        loop::MidiLoopbackEndpointCreationConfiguration creationConfiguration)
    {
        // the success code in this defaults to False
        loop::MidiLoopbackEndpointCreationResult result{};
        result.Success = false;

        json::JsonObject configurationObject;

        if (json::JsonObject::TryParse(creationConfiguration.GetConfigurationJson(), configurationObject))
        {
            // send it up

            auto serviceResponse = midi2::MidiService::UpdateTransportPluginConfiguration(creationConfiguration);


            // parse the results
            auto successResult = serviceResponse.Status() == midi2::MidiServiceConfigurationResponseStatus::Success;

            if (successResult)
            {
                json::JsonObject serviceResponseJson;

                if (json::JsonObject::TryParse(serviceResponse.ResponseJson(), serviceResponseJson))
                {
                    auto deviceIdA = serviceResponseJson.GetNamedString(MIDI_CONFIG_JSON_ENDPOINT_LOOPBACK_DEVICE_RESPONSE_CREATED_ENDPOINT_A_ID_KEY, L"");
                    auto deviceIdB = serviceResponseJson.GetNamedString(MIDI_CONFIG_JSON_ENDPOINT_LOOPBACK_DEVICE_RESPONSE_CREATED_ENDPOINT_B_ID_KEY, L"");

                    if (!deviceIdA.empty() && !deviceIdB.empty())
                    {
                        // update the response object with the new ids
                        result.AssociationId = creationConfiguration.AssociationId();
                        result.EndpointDeviceIdA = deviceIdA;
                        result.EndpointDeviceIdB = deviceIdB;
                        result.Success = true;
                    }

                }
            }
            else
            {
                //internal::LogGeneralError(__FUNCTION__, L"Device creation failed (payload has false success value)");
            }
        }
        else
        {
            // unable to read the json from the config object
        }

        return result;
    }

    _Use_decl_annotations_
    bool MidiLoopbackEndpointManager::RemoveTransientLoopbackEndpoints(
        loop::MidiLoopbackEndpointDeletionConfiguration deletionConfiguration)
    {
        // the success code in this defaults to False
        bool result = false;

        json::JsonObject configurationObject;

        if (json::JsonObject::TryParse(deletionConfiguration.GetConfigurationJson(), configurationObject))
        {
            // send it up

            auto serviceResponse = midi2::MidiService::UpdateTransportPluginConfiguration(deletionConfiguration);

            result = (serviceResponse.Status() == midi2::MidiServiceConfigurationResponseStatus::Success);
        }
        else
        {
            // unable to read the json from the config object
            result = false;
        }

        return result;
    }

}
