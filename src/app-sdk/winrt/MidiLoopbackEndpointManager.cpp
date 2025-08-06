// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App SDK and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================


#include "pch.h"
#include "MidiLoopbackEndpointManager.h"
#include "Endpoints.Loopback.MidiLoopbackEndpointManager.g.cpp"

// copied from service loopback_transport_defs.h

#define MIDI_LOOP_INSTANCE_ID_A_PREFIX L"MIDIU_LOOP_A_"
#define MIDI_LOOP_INSTANCE_ID_B_PREFIX L"MIDIU_LOOP_B_"


namespace winrt::Microsoft::Windows::Devices::Midi2::Endpoints::Loopback::implementation
{
    bool MidiLoopbackEndpointManager::IsTransportAvailable() noexcept
    {
        // TODO: Check to see if service transport is installed and running. May require a new service call
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
                result.ErrorInformation = serviceResponse.ServiceMessage;

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
        loop::MidiLoopbackEndpointRemovalConfig removalConfig)
    {
        // the success code in this defaults to False
        bool result = false;

        json::JsonObject configObject;

        if (json::JsonObject::TryParse(removalConfig.GetConfigJson(), configObject))
        {
            // send it up

            auto serviceResponse = svc::MidiServiceConfig::UpdateTransportPluginConfig(removalConfig);

            result = (serviceResponse.Status == svc::MidiServiceConfigResponseStatus::Success);
        }
        else
        {
            // unable to read the json from the config object
            result = false;
        }

        return result;
    }


    _Use_decl_annotations_
    midi2::MidiEndpointDeviceInformation MidiLoopbackEndpointManager::GetAssociatedLoopbackEndpoint(
        midi2::MidiEndpointDeviceInformation const& loopbackEndpoint
    )
    {
        auto domain = midi2::MidiEndpointDeviceInformation::FindAll();

        return GetAssociatedLoopbackEndpoint(loopbackEndpoint, domain);
    }


    _Use_decl_annotations_
        midi2::MidiEndpointDeviceInformation MidiLoopbackEndpointManager::GetAssociatedLoopbackEndpointForId(
        winrt::hstring loopbackEndpointId
    )
    {
        auto cleanId = internal::NormalizeEndpointInterfaceIdHStringCopy(loopbackEndpointId);

        auto info = midi2::MidiEndpointDeviceInformation::CreateFromEndpointDeviceId(cleanId);

        return GetAssociatedLoopbackEndpoint(info);
    }


    _Use_decl_annotations_
    midi2::MidiEndpointDeviceInformation MidiLoopbackEndpointManager::GetAssociatedLoopbackEndpoint(
        midi2::MidiEndpointDeviceInformation const& loopbackEndpoint,
        collections::IIterable<midi2::MidiEndpointDeviceInformation> endpointsToSearch)
    {
        if (loopbackEndpoint == nullptr)
        {
            return nullptr;
        }

        if (endpointsToSearch == nullptr)
        {
            return nullptr;
        }

        auto transportId = loopbackEndpoint.GetTransportSuppliedInfo().TransportId;

        if (transportId != TransportId())
        {
            // not a loopback endpoint
            return nullptr;
        }

        // get the endpoint's association id

        if (loopbackEndpoint.Properties().HasKey(STRING_PKEY_MIDI_VirtualMidiEndpointAssociator) && 
            loopbackEndpoint.Properties().Lookup(STRING_PKEY_MIDI_VirtualMidiEndpointAssociator) != nullptr)
        {
            auto associator = winrt::unbox_value<winrt::hstring>(loopbackEndpoint.Properties().Lookup(STRING_PKEY_MIDI_VirtualMidiEndpointAssociator));

            // find the other endpoint that has this associator
            // this is wasteful to get everything and then iterate, but there's 
            // no AQS way to search using our custom DEVPKEY properties

            for (auto const& ep : endpointsToSearch)
            {
                if (ep.GetTransportSuppliedInfo().TransportId != TransportId()) continue;
                if (ep.EndpointDeviceId() == loopbackEndpoint.EndpointDeviceId()) continue;


                if (ep.Properties().HasKey(STRING_PKEY_MIDI_VirtualMidiEndpointAssociator) &&
                    ep.Properties().Lookup(STRING_PKEY_MIDI_VirtualMidiEndpointAssociator) != nullptr)
                {
                    auto thisAssociator = winrt::unbox_value<winrt::hstring>(ep.Properties().Lookup(STRING_PKEY_MIDI_VirtualMidiEndpointAssociator));

                    // return the endpoint if it has the matching association id
                    if (thisAssociator == associator)
                    {
                        return ep;
                    }
                }
            }
        }

        return nullptr;

    }


    winrt::hstring BuildDeviceId(_In_ std::wstring prefix, _In_ std::wstring uniqueId)
    {
        return winrt::hstring{ std::format(L"\\\\?\\swd#midisrv#{}{}#{{e7cce071-3c03-423f-88d3-f1045d02552b}}", prefix, uniqueId) };
    }

    bool MidiLoopbackEndpointManager::DoesLoopbackAExist(_In_ winrt::hstring uniqueIdentifier)
    {
        return (internal::IsValidWindowsMidiServicesEndpointId(BuildDeviceId(MIDI_LOOP_INSTANCE_ID_A_PREFIX, uniqueIdentifier.c_str())));
    }

    bool MidiLoopbackEndpointManager::DoesLoopbackBExist(_In_ winrt::hstring uniqueIdentifier)
    {
        return (internal::IsValidWindowsMidiServicesEndpointId(BuildDeviceId(MIDI_LOOP_INSTANCE_ID_B_PREFIX, uniqueIdentifier.c_str())));
    }

}
