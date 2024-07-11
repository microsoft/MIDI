// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App SDK and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================


#include "pch.h"
#include "MidiLoopbackEndpointManager.h"
#include "MidiLoopbackEndpointManager.g.cpp"

namespace winrt::Microsoft::Windows::Devices::Midi2::Endpoints::Loopback::implementation
{
    bool MidiLoopbackEndpointManager::IsTransportAvailable() noexcept
    {
        // TODO: Check to see if service abstraction is installed and running. May require a new service call
        return true;
    }




    _Use_decl_annotations_
    loop::MidiLoopbackEndpointCreationResult MidiLoopbackEndpointManager::CreateTransientLoopbackEndpoints(
        loop::MidiLoopbackEndpointCreationConfig creationConfig)
    {
        // the success code in this defaults to False
        loop::MidiLoopbackEndpointCreationResult result{};
        result.Success = false;

        json::JsonObject configObject;

        if (json::JsonObject::TryParse(creationConfig.GetConfigJson(), configObject))
        {
            // send it up

            auto serviceResponse = svc::MidiServiceConfig::UpdateTransportPluginConfig(creationConfig);


            // parse the results
            auto successResult = serviceResponse.Status == svc::MidiServiceConfigResponseStatus::Success;

            if (successResult)
            {
                json::JsonObject serviceResponseJson;

                if (json::JsonObject::TryParse(serviceResponse.ResponseJson, serviceResponseJson))
                {
                    auto deviceIdA = serviceResponseJson.GetNamedString(MIDI_CONFIG_JSON_ENDPOINT_LOOPBACK_DEVICE_RESPONSE_CREATED_ENDPOINT_A_ID_KEY, L"");
                    auto deviceIdB = serviceResponseJson.GetNamedString(MIDI_CONFIG_JSON_ENDPOINT_LOOPBACK_DEVICE_RESPONSE_CREATED_ENDPOINT_B_ID_KEY, L"");

                    if (!deviceIdA.empty() && !deviceIdB.empty())
                    {
                        // update the response object with the new ids
                        result.AssociationId = creationConfig.AssociationId();
                        result.EndpointDeviceIdA = deviceIdA;
                        result.EndpointDeviceIdB = deviceIdB;
                        result.Success = true;
                    }

                }
            }
            else
            {
                //internal::LogGeneralError(__FUNCTION__, L"Device creation failed (payload has false success value)");

                TraceLoggingWrite(
                    Midi2SdkTelemetryProvider::Provider(),
                    MIDI_SDK_TRACE_EVENT_ERROR,
                    TraceLoggingString(__FUNCTION__, MIDI_SDK_TRACE_LOCATION_FIELD),
                    TraceLoggingLevel(WINEVENT_LEVEL_INFO),
                    TraceLoggingPointer(MIDI_SDK_STATIC_THIS_PLACEHOLDER_FIELD_VALUE, MIDI_SDK_TRACE_THIS_FIELD),
                    TraceLoggingWideString(L"Device creation failed (payload has false success value)", MIDI_SDK_TRACE_MESSAGE_FIELD),
                    TraceLoggingGuid(creationConfig.AssociationId(), "association id")
                );
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
        loop::MidiLoopbackEndpointDeletionConfig deletionConfig)
    {
        // the success code in this defaults to False
        bool result = false;

        json::JsonObject configObject;

        if (json::JsonObject::TryParse(deletionConfig.GetConfigJson(), configObject))
        {
            // send it up

            auto serviceResponse = svc::MidiServiceConfig::UpdateTransportPluginConfig(deletionConfig);

            result = (serviceResponse.Status == svc::MidiServiceConfigResponseStatus::Success);
        }
        else
        {
            // unable to read the json from the config object
            result = false;
        }

        return result;
    }

}
