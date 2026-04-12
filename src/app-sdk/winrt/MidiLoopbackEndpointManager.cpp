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

#include <algorithm>

// copied from service loopback_transport_defs.h

#define MIDI_LOOP_INSTANCE_ID_A_PREFIX L"MIDIU_LOOP_A_"
#define MIDI_LOOP_INSTANCE_ID_B_PREFIX L"MIDIU_LOOP_B_"


namespace winrt::Microsoft::Windows::Devices::Midi2::Endpoints::Loopback::implementation
{
    bool MidiLoopbackEndpointManager::IsTransportAvailable() noexcept
    {
        auto transports = rept::MidiReporting::GetInstalledTransportPlugins();

        for (auto const& transport : transports)
        {
            if (transport.Id == TransportId())
            {
                return true;
            }
        }

        return false;
    }




    _Use_decl_annotations_
    loop::MidiLoopbackEndpointCreationResult MidiLoopbackEndpointManager::CreateTransientLoopbackEndpoints(
        loop::MidiLoopbackEndpointCreationConfig creationConfig)
    {
        // the success code in this defaults to False
        loop::implementation::MidiLoopbackEndpointCreationResult result{};
        result.Success(false);
        result.AssociationId(creationConfig.AssociationId());

        // TODO: Result error code


        loop::MidiLoopbackEndpointDefinition definitionA;
        loop::MidiLoopbackEndpointDefinition definitionB;

        // we have to do all this so we don't change the method signature, but
        // can provide a default value when the uniqueId is missing. The 
        // endpoint definition is just a structure, with no constructor
        definitionA.Description = internal::TrimmedHStringCopy(creationConfig.EndpointDefinitionA().Description);
        //definitionA.IsMuted = creationConfig.EndpointDefinitionA().IsMuted;
        definitionA.Name = internal::TrimmedHStringCopy(creationConfig.EndpointDefinitionA().Name);
        definitionA.UniqueId = internal::RemoveInvalidSWDUniqueIdCharacters(creationConfig.EndpointDefinitionA().UniqueId.c_str());

        definitionB.Description = internal::TrimmedHStringCopy(creationConfig.EndpointDefinitionB().Description);
        //definitionB.IsMuted = creationConfig.EndpointDefinitionB().IsMuted;
        definitionB.Name = internal::TrimmedHStringCopy(creationConfig.EndpointDefinitionB().Name);
        definitionB.UniqueId = internal::RemoveInvalidSWDUniqueIdCharacters(creationConfig.EndpointDefinitionB().UniqueId.c_str());


        if (definitionA.UniqueId.empty())
        {
            // generate a unique id if one has not been provided
            std::wstring id{ internal::GuidToHexDigitsOnlyString(foundation::GuidHelper::CreateNewGuid()) };
            internal::InPlaceToLower(id);

            definitionA.UniqueId = id;

        }

        if (definitionB.UniqueId.empty())
        {
            definitionB.UniqueId = definitionA.UniqueId;
        }

        loop::MidiLoopbackEndpointCreationConfig updatedCreationConfig(creationConfig.AssociationId(), definitionA, definitionB);
        


        if (internal::TrimmedHStringCopy(updatedCreationConfig.EndpointDefinitionA().Name).empty())
        {
            result.ErrorInformation(internal::ResourceGetHString(IDS_VALIDATION_ERROR_LOOPBACK_MISSING_ENDPOINT_NAME_A));
            result.ErrorCode(loop::MidiLoopbackEndpointCreationResultErrorCode::InvalidOrMissingNameA);
            return result;
        }

        if (internal::TrimmedHStringCopy(updatedCreationConfig.EndpointDefinitionB().Name).empty())
        {
            result.ErrorInformation(internal::ResourceGetHString(IDS_VALIDATION_ERROR_LOOPBACK_MISSING_ENDPOINT_NAME_B));
            result.ErrorCode(loop::MidiLoopbackEndpointCreationResultErrorCode::InvalidOrMissingNameB);
            return result;
        }




        try
        {
            auto serviceResponse = svc::MidiServiceConfig::UpdateTransportPluginConfig(updatedCreationConfig);

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
                        result.EndpointDeviceIdA(deviceIdA);
                        result.EndpointDeviceIdB(deviceIdB);
                        result.Success(true);
                    }
                    else
                    {
                        TraceLoggingWrite(
                            Midi2SdkTelemetryProvider::Provider(),
                            MIDI_SDK_TRACE_EVENT_ERROR,
                            TraceLoggingString(__FUNCTION__, MIDI_SDK_TRACE_LOCATION_FIELD),
                            TraceLoggingLevel(WINEVENT_LEVEL_INFO),
                            TraceLoggingPointer(MIDI_SDK_STATIC_THIS_PLACEHOLDER_FIELD_VALUE, MIDI_SDK_TRACE_THIS_FIELD),
                            TraceLoggingWideString(L"Device creation succeeded but returned device ids are empty", MIDI_SDK_TRACE_MESSAGE_FIELD),
                            TraceLoggingGuid(creationConfig.AssociationId(), "association id")
                        );
                    }

                }
            }
            else
            {
                result.ErrorInformation(serviceResponse.ServiceMessage);

                // TODO: Need to get error code from the service response



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
        catch (winrt::hresult_error ex)
        {
            result.ErrorInformation(ex.message());

            TraceLoggingWrite(
                Midi2SdkTelemetryProvider::Provider(),
                MIDI_SDK_TRACE_EVENT_ERROR,
                TraceLoggingString(__FUNCTION__, MIDI_SDK_TRACE_LOCATION_FIELD),
                TraceLoggingLevel(WINEVENT_LEVEL_INFO),
                TraceLoggingPointer(MIDI_SDK_STATIC_THIS_PLACEHOLDER_FIELD_VALUE, MIDI_SDK_TRACE_THIS_FIELD),
                TraceLoggingWideString(L"Device creation failed with hresult exception", MIDI_SDK_TRACE_MESSAGE_FIELD),
                TraceLoggingHResult(ex.code(), MIDI_SDK_TRACE_HRESULT_FIELD),
                TraceLoggingWideString(ex.message().c_str(), "exception message"),
                TraceLoggingGuid(creationConfig.AssociationId(), "association id")
            );
        }
        catch (...)
        {
            result.ErrorInformation(L"General exception/error.");

            TraceLoggingWrite(
                Midi2SdkTelemetryProvider::Provider(),
                MIDI_SDK_TRACE_EVENT_ERROR,
                TraceLoggingString(__FUNCTION__, MIDI_SDK_TRACE_LOCATION_FIELD),
                TraceLoggingLevel(WINEVENT_LEVEL_INFO),
                TraceLoggingPointer(MIDI_SDK_STATIC_THIS_PLACEHOLDER_FIELD_VALUE, MIDI_SDK_TRACE_THIS_FIELD),
                TraceLoggingWideString(L"Device creation failed with general exception", MIDI_SDK_TRACE_MESSAGE_FIELD),
                TraceLoggingGuid(creationConfig.AssociationId(), "association id")
            );
        }

        return result;
    }

    _Use_decl_annotations_
    bool MidiLoopbackEndpointManager::RemoveTransientLoopbackEndpoints(
        loop::MidiLoopbackEndpointRemovalConfig removalConfig)
    {
        // the success code in this defaults to False
        bool result = false;

        try
        {
            auto serviceResponse = svc::MidiServiceConfig::UpdateTransportPluginConfig(removalConfig);

            result = (serviceResponse.Status == svc::MidiServiceConfigResponseStatus::Success);
        }
        catch (winrt::hresult_error ex)
        {
            TraceLoggingWrite(
                Midi2SdkTelemetryProvider::Provider(),
                MIDI_SDK_TRACE_EVENT_ERROR,
                TraceLoggingString(__FUNCTION__, MIDI_SDK_TRACE_LOCATION_FIELD),
                TraceLoggingLevel(WINEVENT_LEVEL_INFO),
                TraceLoggingPointer(MIDI_SDK_STATIC_THIS_PLACEHOLDER_FIELD_VALUE, MIDI_SDK_TRACE_THIS_FIELD),
                TraceLoggingWideString(L"Device removal failed with hresult exception", MIDI_SDK_TRACE_MESSAGE_FIELD),
                TraceLoggingHResult(ex.code(), MIDI_SDK_TRACE_HRESULT_FIELD),
                TraceLoggingWideString(ex.message().c_str(), "exception message"),
                TraceLoggingGuid(removalConfig.AssociationId(), "association id")
            );
        }
        catch (...)
        {
            TraceLoggingWrite(
                Midi2SdkTelemetryProvider::Provider(),
                MIDI_SDK_TRACE_EVENT_ERROR,
                TraceLoggingString(__FUNCTION__, MIDI_SDK_TRACE_LOCATION_FIELD),
                TraceLoggingLevel(WINEVENT_LEVEL_INFO),
                TraceLoggingPointer(MIDI_SDK_STATIC_THIS_PLACEHOLDER_FIELD_VALUE, MIDI_SDK_TRACE_THIS_FIELD),
                TraceLoggingWideString(L"Device creation failed with general exception", MIDI_SDK_TRACE_MESSAGE_FIELD),
                TraceLoggingGuid(removalConfig.AssociationId(), "association id")
            );
        }

        return result;
    }

    _Use_decl_annotations_
    winrt::guid MidiLoopbackEndpointManager::GetAssociationId(midi2::MidiEndpointDeviceInformation const& loopbackEndpoint)
    {
        if (loopbackEndpoint.Properties().HasKey(STRING_PKEY_MIDI_VirtualMidiEndpointAssociator) &&
            loopbackEndpoint.Properties().Lookup(STRING_PKEY_MIDI_VirtualMidiEndpointAssociator) != nullptr)
        {
            // we treat it as a guid, but the property itself is a string
            auto associator = winrt::unbox_value<winrt::hstring>(loopbackEndpoint.Properties().Lookup(STRING_PKEY_MIDI_VirtualMidiEndpointAssociator));

            return internal::StringToGuid(associator.c_str());
        }

        return foundation::GuidHelper::Empty();
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
        try
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
                        // we treat it as a guid, but the property itself is a string
                        auto thisAssociator = winrt::unbox_value<winrt::hstring>(ep.Properties().Lookup(STRING_PKEY_MIDI_VirtualMidiEndpointAssociator));

                        // return the endpoint if it has the matching association id
                        if (thisAssociator == associator)
                        {
                            return ep;
                        }
                    }
                }
            }
        }
        catch (winrt::hresult_error ex)
        {
            TraceLoggingWrite(
                Midi2SdkTelemetryProvider::Provider(),
                MIDI_SDK_TRACE_EVENT_ERROR,
                TraceLoggingString(__FUNCTION__, MIDI_SDK_TRACE_LOCATION_FIELD),
                TraceLoggingLevel(WINEVENT_LEVEL_INFO),
                TraceLoggingPointer(MIDI_SDK_STATIC_THIS_PLACEHOLDER_FIELD_VALUE, MIDI_SDK_TRACE_THIS_FIELD),
                TraceLoggingWideString(L"Getting associated endpoint failed with hresult exception", MIDI_SDK_TRACE_MESSAGE_FIELD),
                TraceLoggingHResult(ex.code(), MIDI_SDK_TRACE_HRESULT_FIELD),
                TraceLoggingWideString(ex.message().c_str(), "exception message"),
                TraceLoggingWideString(loopbackEndpoint.EndpointDeviceId().c_str(), MIDI_SDK_TRACE_ENDPOINT_DEVICE_ID_FIELD)
            );
        }
        catch (...)
        {
            TraceLoggingWrite(
                Midi2SdkTelemetryProvider::Provider(),
                MIDI_SDK_TRACE_EVENT_ERROR,
                TraceLoggingString(__FUNCTION__, MIDI_SDK_TRACE_LOCATION_FIELD),
                TraceLoggingLevel(WINEVENT_LEVEL_INFO),
                TraceLoggingPointer(MIDI_SDK_STATIC_THIS_PLACEHOLDER_FIELD_VALUE, MIDI_SDK_TRACE_THIS_FIELD),
                TraceLoggingWideString(L"Getting associated endpoint failed with general exception", MIDI_SDK_TRACE_MESSAGE_FIELD),
                TraceLoggingWideString(loopbackEndpoint.EndpointDeviceId().c_str(), MIDI_SDK_TRACE_ENDPOINT_DEVICE_ID_FIELD)
            );
        }

        return nullptr;

    }


    winrt::hstring BuildDeviceId(_In_ std::wstring prefix, _In_ std::wstring const& uniqueId)
    {
        return internal::NormalizeEndpointInterfaceIdHStringCopy(winrt::hstring{ std::format(L"\\\\?\\swd#midisrv#{}{}#{{e7cce071-3c03-423f-88d3-f1045d02552b}}", prefix, uniqueId) });
    }

    bool MidiLoopbackEndpointManager::DoesLoopbackAExist(_In_ winrt::hstring const& uniqueIdentifier)
    {
        winrt::hstring id = BuildDeviceId(MIDI_LOOP_INSTANCE_ID_A_PREFIX, uniqueIdentifier.c_str());

        return (internal::IsValidWindowsMidiServicesEndpointId(id) && internal::IsWindowsMidiServicesEndpointPresent(id));
    }

    bool MidiLoopbackEndpointManager::DoesLoopbackBExist(_In_ winrt::hstring const& uniqueIdentifier)
    {
        winrt::hstring id = BuildDeviceId(MIDI_LOOP_INSTANCE_ID_B_PREFIX, uniqueIdentifier.c_str());

        return (internal::IsValidWindowsMidiServicesEndpointId(id) && internal::IsWindowsMidiServicesEndpointPresent(id));
    }

}
