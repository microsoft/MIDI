// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services WinRT API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================


#include "pch.h"
#include "MidiBasicLoopbackManager.h"
#include "MidiBasicLoopbackManager.g.cpp"

//#include "..\..\api\Transport\BasicLoopbackMidiTransport\basic_loopback_transport_error_codes.h"

#define MIDI_BLOOP_INSTANCE_ID_PREFIX L"MIDIU_BLOOP_"

namespace winrt::Windows::Devices::Midi2::Endpoints::BasicLoopback::implementation
{
    bool MidiBasicLoopbackManager::IsTransportAvailable() noexcept
    {
        auto transports = rpt::MidiReporting::GetInstalledTransportPlugins();

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
    bloop::MidiBasicLoopbackCreationResult MidiBasicLoopbackManager::CreateTransientLoopback(
        bloop::MidiBasicLoopbackCreationConfig const& creationConfig) noexcept
    {
        // the success code in this defaults to False
        auto result = winrt::make_self<implementation::MidiBasicLoopbackCreationResult>();
        if (result == nullptr)
        {
            return nullptr;
        }

        // default to error
        result->InternalSetFailure(
            creationConfig.AssociationId(), 
            bloop::MidiBasicLoopbackErrorCode::NoErrorInformationAvailable, 
            L"");


        // validate the name
        if (internal::TrimmedHStringCopy(creationConfig.EndpointDefinition().Name()).empty())
        {
            TraceLoggingWrite(
                Midi2SdkTelemetryProvider::Provider(),
                MIDI_SDK_TRACE_EVENT_ERROR,
                TraceLoggingString(__FUNCTION__, MIDI_SDK_TRACE_LOCATION_FIELD),
                TraceLoggingLevel(WINEVENT_LEVEL_ERROR),
                TraceLoggingPointer(MIDI_SDK_STATIC_THIS_PLACEHOLDER_FIELD_VALUE, MIDI_SDK_TRACE_THIS_FIELD),
                TraceLoggingWideString(L"Missing endpoint name", MIDI_SDK_TRACE_MESSAGE_FIELD),
                TraceLoggingGuid(creationConfig.AssociationId(), "association id")
            );

            result->InternalSetFailure(
                creationConfig.AssociationId(), 
                bloop::MidiBasicLoopbackErrorCode::InvalidOrMissingEndpointName,
                internal::ResourceGetHString(IDS_VALIDATION_ERROR_LOOPBACK_MISSING_ENDPOINT_NAME));

            return *result;
        }

        // provide a unique id if none was specified
        winrt::hstring endpointUniqueId{ internal::TrimmedHStringCopy(creationConfig.EndpointDefinition().UniqueId()) };
        if (endpointUniqueId.empty())
        {
            std::wstring id{ internal::GuidToHexDigitsOnlyString(foundation::GuidHelper::CreateNewGuid()) };
            internal::InPlaceToLower(id);

            creationConfig.EndpointDefinition().UniqueId(id);
        }

        try
        {
            auto serviceResponse = svc::MidiServiceTransportPluginConfigManager::SendUpdate(creationConfig);

            // grab the results
            auto successResult = serviceResponse.Status() == svc::MidiServiceConfigResponseStatus::Success;

            if (!successResult)
            {
                result->InternalSetFailure(
                    creationConfig.AssociationId(),
                    static_cast<bloop::MidiBasicLoopbackErrorCode>(serviceResponse.ServiceErrorCode()),
                    serviceResponse.ServiceErrorMessage());

                TraceLoggingWrite(
                    Midi2SdkTelemetryProvider::Provider(),
                    MIDI_SDK_TRACE_EVENT_ERROR,
                    TraceLoggingString(__FUNCTION__, MIDI_SDK_TRACE_LOCATION_FIELD),
                    TraceLoggingLevel(WINEVENT_LEVEL_ERROR),
                    TraceLoggingPointer(MIDI_SDK_STATIC_THIS_PLACEHOLDER_FIELD_VALUE, MIDI_SDK_TRACE_THIS_FIELD),
                    TraceLoggingWideString(L"Device creation failed (payload has false success value)", MIDI_SDK_TRACE_MESSAGE_FIELD),
                    TraceLoggingGuid(creationConfig.AssociationId(), "association id")
                );

                return *result;
            }

            if (successResult)
            {
                if (serviceResponse.ResponseJson() == nullptr)
                {
                    TraceLoggingWrite(
                        Midi2SdkTelemetryProvider::Provider(),
                        MIDI_SDK_TRACE_EVENT_ERROR,
                        TraceLoggingString(__FUNCTION__, MIDI_SDK_TRACE_LOCATION_FIELD),
                        TraceLoggingLevel(WINEVENT_LEVEL_ERROR),
                        TraceLoggingPointer(MIDI_SDK_STATIC_THIS_PLACEHOLDER_FIELD_VALUE, MIDI_SDK_TRACE_THIS_FIELD),
                        TraceLoggingWideString(L"Device creation succeeded but returned response json is empty", MIDI_SDK_TRACE_MESSAGE_FIELD),
                        TraceLoggingGuid(creationConfig.AssociationId(), "association id")
                    );

                    return *result;
                }

                auto deviceId = serviceResponse.ResponseJson().GetNamedString(MIDI_CONFIG_JSON_ENDPOINT_BASIC_LOOPBACK_DEVICE_RESPONSE_CREATED_ENDPOINT_ID_KEY, L"");

                if (!deviceId.empty())
                {                   
                    auto entry = winrt::make_self<MidiBasicLoopbackEntry>();

                    entry->InternalInitialize(creationConfig.AssociationId(),
                        deviceId,
                        creationConfig.EndpointDefinition().Name(),
                        creationConfig.EndpointDefinition().Description(),
                        creationConfig.IsMuted());

                    result->InternalSetSuccess(creationConfig.AssociationId(), *entry);


                    // TODO: get created midi1 ports


                    return *result;
                }
                else
                {
                    TraceLoggingWrite(
                        Midi2SdkTelemetryProvider::Provider(),
                        MIDI_SDK_TRACE_EVENT_ERROR,
                        TraceLoggingString(__FUNCTION__, MIDI_SDK_TRACE_LOCATION_FIELD),
                        TraceLoggingLevel(WINEVENT_LEVEL_ERROR),
                        TraceLoggingPointer(MIDI_SDK_STATIC_THIS_PLACEHOLDER_FIELD_VALUE, MIDI_SDK_TRACE_THIS_FIELD),
                        TraceLoggingWideString(L"Device creation succeeded but returned device id is empty", MIDI_SDK_TRACE_MESSAGE_FIELD),
                        TraceLoggingGuid(creationConfig.AssociationId(), "association id")
                    );

                    result->InternalSetFailure(
                        creationConfig.AssociationId(),
                        bloop::MidiBasicLoopbackErrorCode::NoErrorInformationAvailable,
                        L"Device creation succeeded but returned device id is empty");

                }
            }
            else
            {
            }
        }
        catch (winrt::hresult_error ex)
        {
            result->InternalSetFailure(
                creationConfig.AssociationId(),
                bloop::MidiBasicLoopbackErrorCode::ClientApiException,
                ex.message()
            );


            TraceLoggingWrite(
                Midi2SdkTelemetryProvider::Provider(),
                MIDI_SDK_TRACE_EVENT_ERROR,
                TraceLoggingString(__FUNCTION__, MIDI_SDK_TRACE_LOCATION_FIELD),
                TraceLoggingLevel(WINEVENT_LEVEL_ERROR),
                TraceLoggingPointer(MIDI_SDK_STATIC_THIS_PLACEHOLDER_FIELD_VALUE, MIDI_SDK_TRACE_THIS_FIELD),
                TraceLoggingWideString(L"Device creation failed with hresult exception", MIDI_SDK_TRACE_MESSAGE_FIELD),
                TraceLoggingGuid(creationConfig.AssociationId(), "association id"),
                TraceLoggingHResult(ex.code(), MIDI_SDK_TRACE_HRESULT_FIELD)
                );
        }
        catch (...)
        {
            result->InternalSetFailure(
                creationConfig.AssociationId(),
                bloop::MidiBasicLoopbackErrorCode::ClientApiException,
                internal::ResourceGetHString(IDS_ERROR_UNKNOWN)
            );

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
    bloop::MidiBasicLoopbackRemovalResult MidiBasicLoopbackManager::RemoveTransientLoopback(
        bloop::MidiBasicLoopbackRemovalConfig const& removalConfig) noexcept
    {
        auto result = winrt::make_self<MidiBasicLoopbackRemovalResult>();

        if (result == nullptr)
        {
            return nullptr;
        }

        try
        {
            // the success code in this defaults to False

            auto serviceResponse = svc::MidiServiceTransportPluginConfigManager::SendUpdate(removalConfig);

            if (serviceResponse.Status() == svc::MidiServiceConfigResponseStatus::Success)
            {
                result->InternalSetSuccess();
            }
            else
            {
                result->InternalSetFailure(
                    static_cast<bloop::MidiBasicLoopbackErrorCode>(serviceResponse.ServiceErrorCode()), 
                    serviceResponse.ServiceErrorMessage());

                TraceLoggingWrite(
                    Midi2SdkTelemetryProvider::Provider(),
                    MIDI_SDK_TRACE_EVENT_ERROR,
                    TraceLoggingString(__FUNCTION__, MIDI_SDK_TRACE_LOCATION_FIELD),
                    TraceLoggingLevel(WINEVENT_LEVEL_INFO),
                    TraceLoggingPointer(MIDI_SDK_STATIC_THIS_PLACEHOLDER_FIELD_VALUE, MIDI_SDK_TRACE_THIS_FIELD),
                    TraceLoggingWideString(L"Failed to mute loopback. Service returned a failure result.", MIDI_SDK_TRACE_MESSAGE_FIELD),
                    TraceLoggingUInt32(serviceResponse.ServiceErrorCode(), "service error code"),
                    TraceLoggingWideString(serviceResponse.ServiceErrorMessage().c_str(), "service error message")
                );
            }
        }
        catch (winrt::hresult_error ex)
        {
            LOG_IF_FAILED(ex.code());

            result->InternalSetFailure(
                bloop::MidiBasicLoopbackErrorCode::ClientApiException,
                ex.message());

            TraceLoggingWrite(
                Midi2SdkTelemetryProvider::Provider(),
                MIDI_SDK_TRACE_EVENT_ERROR,
                TraceLoggingString(__FUNCTION__, MIDI_SDK_TRACE_LOCATION_FIELD),
                TraceLoggingLevel(WINEVENT_LEVEL_INFO),
                TraceLoggingPointer(MIDI_SDK_STATIC_THIS_PLACEHOLDER_FIELD_VALUE, MIDI_SDK_TRACE_THIS_FIELD),
                TraceLoggingWideString(L"Device removal failed with hresult exception", MIDI_SDK_TRACE_MESSAGE_FIELD),
                TraceLoggingGuid(removalConfig.AssociationId(), "association id"),
                TraceLoggingHResult(ex.code(), MIDI_SDK_TRACE_HRESULT_FIELD)
            );
        }
        catch (...)
        {
            result->InternalSetFailure(
                bloop::MidiBasicLoopbackErrorCode::ClientApiException,
                L"General exception.");

            TraceLoggingWrite(
                Midi2SdkTelemetryProvider::Provider(),
                MIDI_SDK_TRACE_EVENT_ERROR,
                TraceLoggingString(__FUNCTION__, MIDI_SDK_TRACE_LOCATION_FIELD),
                TraceLoggingLevel(WINEVENT_LEVEL_INFO),
                TraceLoggingPointer(MIDI_SDK_STATIC_THIS_PLACEHOLDER_FIELD_VALUE, MIDI_SDK_TRACE_THIS_FIELD),
                TraceLoggingWideString(L"Device removal failed with general exception", MIDI_SDK_TRACE_MESSAGE_FIELD),
                TraceLoggingGuid(removalConfig.AssociationId(), "association id")
            );
        }

        return *result;
    }

    _Use_decl_annotations_
    winrt::guid MidiBasicLoopbackManager::GetAssociationId(
        midi2enum::MidiEndpointDeviceInformation const& basicLoopbackEndpoint) noexcept
    {
        if (basicLoopbackEndpoint.Properties().HasKey(STRING_PKEY_MIDI_VirtualMidiEndpointAssociator) &&
            basicLoopbackEndpoint.Properties().Lookup(STRING_PKEY_MIDI_VirtualMidiEndpointAssociator) != nullptr)
        {
            // we treat it as a guid, but the property itself is a string
            auto associator = winrt::unbox_value<winrt::hstring>(basicLoopbackEndpoint.Properties().Lookup(STRING_PKEY_MIDI_VirtualMidiEndpointAssociator));

            return internal::StringToGuid(associator.c_str());
        }

        return foundation::GuidHelper::Empty();
    }


    winrt::hstring BuildDeviceId(_In_ std::wstring prefix, _In_ std::wstring const& uniqueId)
    {
        return internal::NormalizeEndpointInterfaceIdHStringCopy(winrt::hstring{ std::format(L"\\\\?\\swd#midisrv#{}{}#{{e7cce071-3c03-423f-88d3-f1045d02552b}}", prefix, uniqueId) });
    }

    _Use_decl_annotations_
    bool MidiBasicLoopbackManager::DoesLoopbackExist(winrt::hstring const& uniqueIdentifier)
    {
        winrt::hstring cleanId { internal::RemoveInvalidSWDUniqueIdCharacters(uniqueIdentifier.c_str()) };
        cleanId = internal::TruncateHStringCopy(cleanId.c_str(), MAXPNAMELEN);
        winrt::hstring id = BuildDeviceId(MIDI_BLOOP_INSTANCE_ID_PREFIX, cleanId.c_str());

        return (internal::IsValidWindowsMidiServicesEndpointId(id) && internal::IsWindowsMidiServicesEndpointPresent(id));
    }

    _Use_decl_annotations_
    bloop::MidiBasicLoopbackUpdateResult MidiBasicLoopbackManager::MuteLoopback(_In_ winrt::guid const& associationId)
    {
        auto result = winrt::make_self<MidiBasicLoopbackUpdateResult>();

        if (result == nullptr)
        {
            return nullptr;
        }

        try
        {
            svc::MidiServiceTransportCommand cmd(TransportId());

            cmd.Arguments().Insert(MIDI_CONFIG_JSON_TRANSPORT_COMMAND_COMMON_PARAMETER_ENDPOINT_ASSOCIATION_ID, internal::GuidToString(associationId));
            cmd.Verb(MIDI_CONFIG_JSON_TRANSPORT_COMMAND_MUTE_ENDPOINT);

            auto serviceResponse = svc::MidiServiceTransportPluginConfigManager::SendUpdate(cmd);

            if (serviceResponse.Status() == svc::MidiServiceConfigResponseStatus::Success)
            {
                result->InternalSetSuccess();
            }
            else
            {
                result->InternalSetFailure(
                    static_cast<bloop::MidiBasicLoopbackErrorCode>(serviceResponse.ServiceErrorCode()),
                    serviceResponse.ServiceErrorMessage());

                TraceLoggingWrite(
                    Midi2SdkTelemetryProvider::Provider(),
                    MIDI_SDK_TRACE_EVENT_ERROR,
                    TraceLoggingString(__FUNCTION__, MIDI_SDK_TRACE_LOCATION_FIELD),
                    TraceLoggingLevel(WINEVENT_LEVEL_INFO),
                    TraceLoggingPointer(MIDI_SDK_STATIC_THIS_PLACEHOLDER_FIELD_VALUE, MIDI_SDK_TRACE_THIS_FIELD),
                    TraceLoggingWideString(L"Failed to mute loopback. Service returned a failure result.", MIDI_SDK_TRACE_MESSAGE_FIELD),
                    TraceLoggingUInt32(serviceResponse.ServiceErrorCode(), "service error code"),
                    TraceLoggingWideString(serviceResponse.ServiceErrorMessage().c_str(), "service error message"),
                    TraceLoggingGuid(associationId, "association id")
                );
            }
        }
        catch (winrt::hresult_error ex)
        {
            LOG_IF_FAILED(ex.code());

            result->InternalSetFailure(
                bloop::MidiBasicLoopbackErrorCode::ClientApiException,
                ex.message());

            TraceLoggingWrite(
                Midi2SdkTelemetryProvider::Provider(),
                MIDI_SDK_TRACE_EVENT_ERROR,
                TraceLoggingString(__FUNCTION__, MIDI_SDK_TRACE_LOCATION_FIELD),
                TraceLoggingLevel(WINEVENT_LEVEL_INFO),
                TraceLoggingPointer(MIDI_SDK_STATIC_THIS_PLACEHOLDER_FIELD_VALUE, MIDI_SDK_TRACE_THIS_FIELD),
                TraceLoggingWideString(L"Failed to mute loopback. hresult exception", MIDI_SDK_TRACE_MESSAGE_FIELD),
                TraceLoggingHResult(ex.code(), MIDI_SDK_TRACE_HRESULT_FIELD),
                TraceLoggingWideString(ex.message().c_str(), "error message"),
                TraceLoggingGuid(associationId, "association id")
            );

        }
        catch (...)
        {
            LOG_IF_FAILED(E_FAIL);

            result->InternalSetFailure(
                bloop::MidiBasicLoopbackErrorCode::ClientApiException,
                L"General exception.");

            TraceLoggingWrite(
                Midi2SdkTelemetryProvider::Provider(),
                MIDI_SDK_TRACE_EVENT_ERROR,
                TraceLoggingString(__FUNCTION__, MIDI_SDK_TRACE_LOCATION_FIELD),
                TraceLoggingLevel(WINEVENT_LEVEL_INFO),
                TraceLoggingPointer(MIDI_SDK_STATIC_THIS_PLACEHOLDER_FIELD_VALUE, MIDI_SDK_TRACE_THIS_FIELD),
                TraceLoggingWideString(L"Failed to mute loopback. General exception", MIDI_SDK_TRACE_MESSAGE_FIELD),
                TraceLoggingGuid(associationId, "association id")
            );
        }

        return *result;
    }

    _Use_decl_annotations_
    bloop::MidiBasicLoopbackUpdateResult MidiBasicLoopbackManager::UnmuteLoopback(_In_ winrt::guid const& associationId)
    {
        auto result = winrt::make_self<MidiBasicLoopbackUpdateResult>();

        if (result == nullptr)
        {
            return nullptr;
        }

        try
        {
            svc::MidiServiceTransportCommand cmd(TransportId());

            cmd.Arguments().Insert(MIDI_CONFIG_JSON_TRANSPORT_COMMAND_COMMON_PARAMETER_ENDPOINT_ASSOCIATION_ID, internal::GuidToString(associationId));
            cmd.Verb(MIDI_CONFIG_JSON_TRANSPORT_COMMAND_UNMUTE_ENDPOINT);

            auto serviceResponse = svc::MidiServiceTransportPluginConfigManager::SendUpdate(cmd);

            if (serviceResponse.Status() == svc::MidiServiceConfigResponseStatus::Success)
            {
                result->InternalSetSuccess();
            }
            else
            {
                result->InternalSetFailure(
                    static_cast<bloop::MidiBasicLoopbackErrorCode>(serviceResponse.ServiceErrorCode()),
                    serviceResponse.ServiceErrorMessage());

                TraceLoggingWrite(
                    Midi2SdkTelemetryProvider::Provider(),
                    MIDI_SDK_TRACE_EVENT_ERROR,
                    TraceLoggingString(__FUNCTION__, MIDI_SDK_TRACE_LOCATION_FIELD),
                    TraceLoggingLevel(WINEVENT_LEVEL_INFO),
                    TraceLoggingPointer(MIDI_SDK_STATIC_THIS_PLACEHOLDER_FIELD_VALUE, MIDI_SDK_TRACE_THIS_FIELD),
                    TraceLoggingWideString(L"Failed to unmute loopback. Service returned a failure result.", MIDI_SDK_TRACE_MESSAGE_FIELD),
                    TraceLoggingUInt32(serviceResponse.ServiceErrorCode(), "service error code"),
                    TraceLoggingWideString(serviceResponse.ServiceErrorMessage().c_str(), "service error message"),
                    TraceLoggingGuid(associationId, "association id")
                );
            }
        }
        catch (winrt::hresult_error ex)
        {
            LOG_IF_FAILED(ex.code());

            result->InternalSetFailure(
                bloop::MidiBasicLoopbackErrorCode::ClientApiException,
                ex.message());

            TraceLoggingWrite(
                Midi2SdkTelemetryProvider::Provider(),
                MIDI_SDK_TRACE_EVENT_ERROR,
                TraceLoggingString(__FUNCTION__, MIDI_SDK_TRACE_LOCATION_FIELD),
                TraceLoggingLevel(WINEVENT_LEVEL_INFO),
                TraceLoggingPointer(MIDI_SDK_STATIC_THIS_PLACEHOLDER_FIELD_VALUE, MIDI_SDK_TRACE_THIS_FIELD),
                TraceLoggingWideString(L"Failed to unmute loopback. hresult exception", MIDI_SDK_TRACE_MESSAGE_FIELD),
                TraceLoggingHResult(ex.code(), MIDI_SDK_TRACE_HRESULT_FIELD),
                TraceLoggingWideString(ex.message().c_str(), "error message"),
                TraceLoggingGuid(associationId, "association id")
            );

        }
        catch (...)
        {
            LOG_IF_FAILED(E_FAIL);

            result->InternalSetFailure(
                bloop::MidiBasicLoopbackErrorCode::ClientApiException,
                L"General exception.");

            TraceLoggingWrite(
                Midi2SdkTelemetryProvider::Provider(),
                MIDI_SDK_TRACE_EVENT_ERROR,
                TraceLoggingString(__FUNCTION__, MIDI_SDK_TRACE_LOCATION_FIELD),
                TraceLoggingLevel(WINEVENT_LEVEL_INFO),
                TraceLoggingPointer(MIDI_SDK_STATIC_THIS_PLACEHOLDER_FIELD_VALUE, MIDI_SDK_TRACE_THIS_FIELD),
                TraceLoggingWideString(L"Failed to unmute loopback. General exception", MIDI_SDK_TRACE_MESSAGE_FIELD),
                TraceLoggingGuid(associationId, "association id")
            );
        }

        return *result;
    }


    collections::IVector<bloop::MidiBasicLoopbackEntry> MidiBasicLoopbackManager::GetActiveLoopbackEntries()
    {

        // TODO ==============================================



        return nullptr;
    }


}
