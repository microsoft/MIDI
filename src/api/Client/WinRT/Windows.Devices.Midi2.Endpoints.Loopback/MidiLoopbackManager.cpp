// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services WinRT API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================


#include "pch.h"
#include "MidiLoopbackManager.h"
#include "MidiLoopbackManager.g.cpp"

#include <algorithm>

// copied from service loopback_transport_defs.h

#define MIDI_LOOP_INSTANCE_ID_A_PREFIX L"MIDIU_LOOP_A_"
#define MIDI_LOOP_INSTANCE_ID_B_PREFIX L"MIDIU_LOOP_B_"


namespace winrt::Windows::Devices::Midi2::Endpoints::Loopback::implementation
{
    bool MidiLoopbackManager::IsTransportAvailable() noexcept
    {
        auto transports = rept::MidiReporting::GetInstalledTransportPlugins();

        for (auto const& transport : transports)
        {
            if (transport.TransportId() == TransportId())
            {
                return true;
            }
        }

        return false;
    }


    _Use_decl_annotations_
    loop::MidiLoopbackUpdateResult MidiLoopbackManager::MuteLoopback(_In_ winrt::guid const& associationId) noexcept
    {
        // TODO ===============================
        UNREFERENCED_PARAMETER(associationId);

        return nullptr;
    }

    _Use_decl_annotations_
    loop::MidiLoopbackUpdateResult MidiLoopbackManager::UnmuteLoopback(_In_ winrt::guid const& associationId) noexcept
    {
        // TODO ===============================

        UNREFERENCED_PARAMETER(associationId);

        return nullptr;
    }



    collections::IVectorView<loop::MidiLoopbackEntry> MidiLoopbackManager::GetActiveLoopbackEntries() noexcept
    {
        // TODO ===============================

        return nullptr;
    }







    _Use_decl_annotations_
    loop::MidiLoopbackCreationResult MidiLoopbackManager::CreateTransientLoopback(
        loop::MidiLoopbackCreationConfig const& creationConfig) noexcept
    {
        // the success code in this defaults to False
        auto result = winrt::make_self<MidiLoopbackCreationResult>();
        if (result == nullptr)
        {
            return nullptr;
        }
        
        auto createdLoopbackEntry = winrt::make_self<MidiLoopbackEntry>();
        if (createdLoopbackEntry == nullptr)
        {
            return nullptr;
        }

        createdLoopbackEntry->InternalSetAssociationId(creationConfig.AssociationId());

        // TODO: Result error code

        creationConfig.EndpointDefinitionA().UniqueId(internal::TruncateHStringCopy(internal::RemoveInvalidSWDUniqueIdCharacters(creationConfig.EndpointDefinitionA().UniqueId().c_str()).c_str(), MAXPNAMELEN));
        creationConfig.EndpointDefinitionB().UniqueId(internal::TruncateHStringCopy(internal::RemoveInvalidSWDUniqueIdCharacters(creationConfig.EndpointDefinitionB().UniqueId().c_str()).c_str(), MAXPNAMELEN));


        if (creationConfig.EndpointDefinitionA().UniqueId().empty())
        {
            // generate a unique id if one has not been provided
            std::wstring id{ internal::GuidToHexDigitsOnlyString(foundation::GuidHelper::CreateNewGuid()) };
            internal::InPlaceToLower(id);

            creationConfig.EndpointDefinitionA().UniqueId(id);
        }

        if (creationConfig.EndpointDefinitionB().UniqueId().empty())
        {
            creationConfig.EndpointDefinitionB().UniqueId(creationConfig.EndpointDefinitionA().UniqueId());
        }


        if (creationConfig.EndpointDefinitionA().Name().empty())
        {
            result->InternalSetFailure(
                loop::MidiLoopbackErrorCode::InvalidOrMissingNameA,
                internal::ResourceGetHString(IDS_VALIDATION_ERROR_LOOPBACK_MISSING_ENDPOINT_NAME_A)
            );

            return *result;
        }

        if (creationConfig.EndpointDefinitionB().Name().empty())
        {
            result->InternalSetFailure(
                loop::MidiLoopbackErrorCode::InvalidOrMissingNameB,
                internal::ResourceGetHString(IDS_VALIDATION_ERROR_LOOPBACK_MISSING_ENDPOINT_NAME_B)
            );

            return *result;
        }




        try
        {
            auto serviceResponse = svc::MidiServiceTransportPluginConfigManager::SendUpdate(creationConfig);

            // parse the results
            auto successResult = serviceResponse.Status() == svc::MidiServiceConfigResponseStatus::Success;

            if (successResult)
            {
                json::JsonObject serviceResponseJson = serviceResponse.ResponseJson();

                auto deviceIdA = serviceResponseJson.GetNamedString(MIDI_CONFIG_JSON_ENDPOINT_LOOPBACK_DEVICE_RESPONSE_CREATED_ENDPOINT_A_ID_KEY, L"");
                auto deviceIdB = serviceResponseJson.GetNamedString(MIDI_CONFIG_JSON_ENDPOINT_LOOPBACK_DEVICE_RESPONSE_CREATED_ENDPOINT_B_ID_KEY, L"");

                if (!deviceIdA.empty() && !deviceIdB.empty())
                {
                    auto entryA = winrt::make_self<MidiLoopbackEndpointEntry>();
                    if (entryA != nullptr)
                    {
                        entryA->InternalInitialize(
                            deviceIdA, 
                            creationConfig.EndpointDefinitionA().Name(),
                            creationConfig.EndpointDefinitionA().Description()
                        );
                    }

                    auto entryB = winrt::make_self<MidiLoopbackEndpointEntry>();
                    if (entryB != nullptr)
                    {
                        entryB->InternalInitialize(
                            deviceIdB,
                            creationConfig.EndpointDefinitionB().Name(),
                            creationConfig.EndpointDefinitionB().Description()
                        );
                    }

                    if (entryA != nullptr && entryB != nullptr)
                    {
                        createdLoopbackEntry->InternalSetEndpointEntries(*entryA, *entryB);
                    }


                    // TODO: get the associated MIDI 1.0 port ids and add them to the list in the entry info ?



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
            else
            {
                // TODO: Need to get error code from the service response
                result->InternalSetFailure(MidiLoopbackErrorCode::NoErrorInformationAvailable, serviceResponse.ServiceErrorMessage());

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
            result->InternalSetFailure(MidiLoopbackErrorCode::ExceptionThrown, ex.message());


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
            result->InternalSetFailure(MidiLoopbackErrorCode::ExceptionThrown, L"General exception/error.");


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

        return *result;
    }

    _Use_decl_annotations_
    loop::MidiLoopbackRemovalResult MidiLoopbackManager::RemoveTransientLoopback(
        loop::MidiLoopbackRemovalConfig const& removalConfig) noexcept
    {
        // the success code in this defaults to False
        auto result = winrt::make_self<MidiLoopbackRemovalResult>();
        if (result == nullptr)
        {
            return nullptr;
        }

        try
        {
            auto serviceResponse = svc::MidiServiceTransportPluginConfigManager::SendUpdate(removalConfig);

            if (serviceResponse.Status() != svc::MidiServiceConfigResponseStatus::Success)
            {
                //result->InternalSetFailure();
            }
            else
            {
                //result->InternalSetSuccess();
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

        return *result;
    }


    _Use_decl_annotations_
    winrt::guid MidiLoopbackManager::GetAssociationId(
        midi2enum::MidiEndpointDeviceInformation const& loopbackEndpoint) noexcept
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
    midi2enum::MidiEndpointDeviceInformation MidiLoopbackManager::GetAssociatedLoopbackEndpoint(
        midi2enum::MidiEndpointDeviceInformation const& loopbackEndpoint
    ) noexcept
    {
        auto domain = midi2enum::MidiEndpointDeviceInformation::FindAll();

        return GetAssociatedLoopbackEndpoint(loopbackEndpoint, domain);
    }


    _Use_decl_annotations_
    midi2enum::MidiEndpointDeviceInformation MidiLoopbackManager::GetAssociatedLoopbackEndpointForId(
        winrt::hstring const& loopbackEndpointId
    ) noexcept
    {
        auto cleanId = internal::NormalizeEndpointInterfaceIdHStringCopy(loopbackEndpointId);

        auto info = midi2enum::MidiEndpointDeviceInformation::CreateFromEndpointDeviceId(cleanId);

        return GetAssociatedLoopbackEndpoint(info);
    }


    _Use_decl_annotations_
    midi2enum::MidiEndpointDeviceInformation MidiLoopbackManager::GetAssociatedLoopbackEndpoint(
        midi2enum::MidiEndpointDeviceInformation const& loopbackEndpoint,
        collections::IIterable<midi2enum::MidiEndpointDeviceInformation> const& endpointsToSearch) noexcept
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

            auto transportId = loopbackEndpoint.GetTransportSuppliedInfo().TransportId();

            if (transportId != TransportId())
            {
                // not a loopback endpoint
                return nullptr;
            }

            // get the endpoint's association id

            if (loopbackEndpoint.Properties().HasKey(STRING_PKEY_MIDI_VirtualMidiEndpointAssociator) && 
                loopbackEndpoint.Properties().Lookup(STRING_PKEY_MIDI_VirtualMidiEndpointAssociator) != nullptr)
            {
                auto associator = internal::GetDeviceInfoProperty<winrt::hstring>(loopbackEndpoint.Properties(), STRING_PKEY_MIDI_VirtualMidiEndpointAssociator, L"");

                // find the other endpoint that has this associator
                // this is wasteful to get everything and then iterate, but there's 
                // no AQS way to search using our custom DEVPKEY properties

                for (auto const& ep : endpointsToSearch)
                {
                    if (ep.GetTransportSuppliedInfo().TransportId() != TransportId()) continue;
                    if (ep.EndpointDeviceId() == loopbackEndpoint.EndpointDeviceId()) continue;


                    if (ep.Properties().HasKey(STRING_PKEY_MIDI_VirtualMidiEndpointAssociator) &&
                        ep.Properties().Lookup(STRING_PKEY_MIDI_VirtualMidiEndpointAssociator) != nullptr)
                    {
                        // we treat it as a guid, but the property itself is a string
                        auto thisAssociator = internal::GetDeviceInfoProperty<winrt::hstring>(ep.Properties(), STRING_PKEY_MIDI_VirtualMidiEndpointAssociator, L"");

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

    bool MidiLoopbackManager::DoesLoopbackAExist(_In_ winrt::hstring const& uniqueIdentifier) noexcept
    {
        winrt::hstring id = BuildDeviceId(MIDI_LOOP_INSTANCE_ID_A_PREFIX, uniqueIdentifier.c_str());

        return (internal::IsValidWindowsMidiServicesEndpointId(id) && internal::IsWindowsMidiServicesEndpointPresent(id));
    }

    bool MidiLoopbackManager::DoesLoopbackBExist(_In_ winrt::hstring const& uniqueIdentifier) noexcept
    {
        winrt::hstring id = BuildDeviceId(MIDI_LOOP_INSTANCE_ID_B_PREFIX, uniqueIdentifier.c_str());

        return (internal::IsValidWindowsMidiServicesEndpointId(id) && internal::IsWindowsMidiServicesEndpointPresent(id));
    }

}
