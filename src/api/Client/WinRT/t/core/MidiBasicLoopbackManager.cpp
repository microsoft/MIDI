// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services WinRT API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================


#include "pch.h"

#include "MidiBasicLoopbackManager.h"
#include "Transports.BasicLoopback.MidiBasicLoopbackManager.g.cpp"

#include "MidiBasicLoopbackCreationResponse.h"
#include "MidiBasicLoopbackRemovalResponse.h"
#include "MidiBasicLoopbackUpdateResponse.h"
#include "MidiBasicLoopbackEntry.h"


#include "MidiReporting.h"
#include "MidiServiceConfigResponse.h"
#include "MidiServiceTransportPluginConfigManager.h"


//#include "..\..\api\Transport\BasicLoopbackMidiTransport\basic_loopback_transport_error_codes.h"

#define MIDI_BLOOP_INSTANCE_ID_PREFIX L"MIDIU_BLOOP_"

namespace winrt::Windows::Devices::Midi2::Transports::BasicLoopback::implementation
{
    bool MidiBasicLoopbackManager::IsTransportAvailable() noexcept
    {
        try
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
        catch (winrt::hresult_error const& ex)
        {
            MIDI_SDK_LOG_HRESULT_EXCEPTION(nullptr, ex, L"hresult error checking basic loopback transport availability.");
            return false;
        }
        catch (...)
        {
            MIDI_SDK_LOG_GENERAL_EXCEPTION(nullptr, L"General exception checking basic loopback transport availability.");
            return false;
        }
    }


    _Use_decl_annotations_
    bloop::MidiBasicLoopbackCreationResponse MidiBasicLoopbackManager::CreateTransientLoopback(
        bloop::MidiBasicLoopbackCreationConfig const& creationConfig) noexcept
    {
        // the success code in this defaults to False
        auto result = winrt::make_self<implementation::MidiBasicLoopbackCreationResponse>();
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

        if (creationConfig.EndpointDefinition().UniqueId().empty())
        {
            // the RemoveInvalidSWDUniqueIdCharacters is currently redundant with the TruncateHStringCopy, but we want to keep it in case we change the logic in the future
            std::wstring id{ internal::RemoveInvalidSWDUniqueIdCharacters(internal::GuidToHexDigitsOnlyString(creationConfig.AssociationId())) };
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
                        creationConfig.EndpointDefinition().ImageFileName(),
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
                        internal::ResourceGetHString(IDS_LOOPBACK_ERROR_EMPTY_RETURNED_DEVICE_ID));

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
    bloop::MidiBasicLoopbackRemovalResponse MidiBasicLoopbackManager::RemoveTransientLoopback(
        bloop::MidiBasicLoopbackRemovalConfig const& removalConfig) noexcept
    {
        auto result = winrt::make_self<MidiBasicLoopbackRemovalResponse>();

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
                internal::ResourceGetHString(IDS_ERROR_GENERAL_EXCEPTION));

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
        try
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
        catch (winrt::hresult_error const& ex)
        {
            MIDI_SDK_LOG_HRESULT_EXCEPTION(nullptr, ex, L"hresult error getting basic loopback association id.");
            return foundation::GuidHelper::Empty();
        }
        catch (...)
        {
            MIDI_SDK_LOG_GENERAL_EXCEPTION(nullptr, L"General exception getting basic loopback association id.");
            return foundation::GuidHelper::Empty();
        }
    }


    winrt::hstring BuildDeviceId(_In_ std::wstring prefix, _In_ std::wstring const& uniqueId)
    {
        return internal::NormalizeEndpointInterfaceIdHStringCopy(winrt::hstring{ std::format(L"\\\\?\\swd#midisrv#{}{}#{{e7cce071-3c03-423f-88d3-f1045d02552b}}", prefix, uniqueId) });
    }

    _Use_decl_annotations_
    bool MidiBasicLoopbackManager::DoesLoopbackExist(winrt::hstring const& uniqueIdentifier)
    {
        try
        {
            winrt::hstring cleanId { internal::RemoveInvalidSWDUniqueIdCharacters(uniqueIdentifier.c_str()) };
            cleanId = internal::TruncateHStringCopy(cleanId.c_str(), MIDI_MAX_UMP_ENDPOINT_UNIQUE_ID_CHARACTER_COUNT);
            winrt::hstring id = BuildDeviceId(MIDI_BLOOP_INSTANCE_ID_PREFIX, cleanId.c_str());

            return (internal::IsValidWindowsMidiServicesEndpointId(id) && internal::IsWindowsMidiServicesEndpointPresent(id));
        }
        catch (winrt::hresult_error const& ex)
        {
            MIDI_SDK_LOG_HRESULT_EXCEPTION(nullptr, ex, L"hresult error checking basic loopback existence.");
            return false;
        }
        catch (...)
        {
            MIDI_SDK_LOG_GENERAL_EXCEPTION(nullptr, L"General exception checking basic loopback existence.");
            return false;
        }
    }

    _Use_decl_annotations_
    bloop::MidiBasicLoopbackUpdateResponse MidiBasicLoopbackManager::MuteLoopback(_In_ winrt::guid const& associationId)
    {
        auto result = winrt::make_self<MidiBasicLoopbackUpdateResponse>();

        if (result == nullptr)
        {
            return nullptr;
        }

        try
        {
            auto supportsMuteAndUnmute = svc::MidiServiceTransportPluginConfigManager::QueryCapability(
                TransportId(),
                MIDI_CONFIG_JSON_TRANSPORT_COMMAND_CAPABILITY_MUTE_ENDPOINT);

            if (!supportsMuteAndUnmute)
            {
                result->InternalSetFailure(
                    bloop::MidiBasicLoopbackErrorCode::ClientApiException,
                    internal::ResourceGetHString(IDS_LOOPBACK_ERROR_MUTE_NOT_SUPPORTED));

                TraceLoggingWrite(
                    Midi2SdkTelemetryProvider::Provider(),
                    MIDI_SDK_TRACE_EVENT_ERROR,
                    TraceLoggingString(__FUNCTION__, MIDI_SDK_TRACE_LOCATION_FIELD),
                    TraceLoggingLevel(WINEVENT_LEVEL_INFO),
                    TraceLoggingPointer(MIDI_SDK_STATIC_THIS_PLACEHOLDER_FIELD_VALUE, MIDI_SDK_TRACE_THIS_FIELD),
                    TraceLoggingWideString(L"Failed to mute loopback. Transport does not support mute operation.", MIDI_SDK_TRACE_MESSAGE_FIELD)
                );

                return *result;
            }

            svc::MidiServiceTransportCommand cmd(TransportId());

            cmd.Arguments().Insert(MIDI_CONFIG_JSON_TRANSPORT_COMMAND_COMMON_PARAMETER_ENDPOINT_ASSOCIATION_ID, internal::GuidToString(associationId));
            cmd.Verb(MIDI_CONFIG_JSON_TRANSPORT_COMMAND_MUTE_ENDPOINT);

            auto serviceResponse = svc::MidiServiceTransportPluginConfigManager::SendCommand(cmd);

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
                internal::ResourceGetHString(IDS_ERROR_GENERAL_EXCEPTION));

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
    bloop::MidiBasicLoopbackUpdateResponse MidiBasicLoopbackManager::UnmuteLoopback(_In_ winrt::guid const& associationId)
    {
        auto result = winrt::make_self<MidiBasicLoopbackUpdateResponse>();

        if (result == nullptr)
        {
            return nullptr;
        }

        try
        {
            auto supportsMuteAndUnmute = svc::MidiServiceTransportPluginConfigManager::QueryCapability(
                TransportId(),
                MIDI_CONFIG_JSON_TRANSPORT_COMMAND_CAPABILITY_MUTE_ENDPOINT);

            if (!supportsMuteAndUnmute)
            {
                result->InternalSetFailure(
                    bloop::MidiBasicLoopbackErrorCode::ClientApiException,
                    internal::ResourceGetHString(IDS_LOOPBACK_ERROR_UNMUTE_NOT_SUPPORTED));

                TraceLoggingWrite(
                    Midi2SdkTelemetryProvider::Provider(),
                    MIDI_SDK_TRACE_EVENT_ERROR,
                    TraceLoggingString(__FUNCTION__, MIDI_SDK_TRACE_LOCATION_FIELD),
                    TraceLoggingLevel(WINEVENT_LEVEL_INFO),
                    TraceLoggingPointer(MIDI_SDK_STATIC_THIS_PLACEHOLDER_FIELD_VALUE, MIDI_SDK_TRACE_THIS_FIELD),
                    TraceLoggingWideString(L"Failed to unmute loopback. Transport does not support unmute operation.", MIDI_SDK_TRACE_MESSAGE_FIELD)
                );

                return *result;
            }

            svc::MidiServiceTransportCommand cmd(TransportId());

            cmd.Arguments().Insert(MIDI_CONFIG_JSON_TRANSPORT_COMMAND_COMMON_PARAMETER_ENDPOINT_ASSOCIATION_ID, internal::GuidToString(associationId));
            cmd.Verb(MIDI_CONFIG_JSON_TRANSPORT_COMMAND_UNMUTE_ENDPOINT);

            auto serviceResponse = svc::MidiServiceTransportPluginConfigManager::SendCommand(cmd);

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
                internal::ResourceGetHString(IDS_ERROR_GENERAL_EXCEPTION));

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
        auto results = winrt::single_threaded_vector<bloop::MidiBasicLoopbackEntry>();

        try
        {
            auto supportsListEntries = svc::MidiServiceTransportPluginConfigManager::QueryCapability(
                TransportId(),
                MIDI_CONFIG_JSON_TRANSPORT_COMMAND_CAPABILITY_LIST_ENTRIES);

            if (supportsListEntries)
            {
                svc::MidiServiceTransportCommand cmd(TransportId());
                cmd.Verb(svc::MidiServiceTransportCommonCommands::ListEntries());

                auto serviceResponse = svc::MidiServiceTransportPluginConfigManager::SendCommand(cmd);

                if (serviceResponse.Status() == svc::MidiServiceConfigResponseStatus::Success)
                {
                    if (serviceResponse.ResponseJson().HasKey(MIDI_CONFIG_JSON_ENDPOINT_BASIC_LOOPBACK_LIST_ENTRY_LIST_ARRAY_KEY))
                    {
                        auto entriesJson = serviceResponse.ResponseJson().GetNamedArray(MIDI_CONFIG_JSON_ENDPOINT_BASIC_LOOPBACK_LIST_ENTRY_LIST_ARRAY_KEY);

                        for (const auto& jsonEntry : entriesJson)
                        {
                            auto entryObject = jsonEntry.GetObject();

                            auto entry = winrt::make_self<MidiBasicLoopbackEntry>();

                            auto associationIdString = entryObject.GetNamedString(MIDI_CONFIG_JSON_ENDPOINT_BASIC_LOOPBACK_LIST_ENTRY_ASSOCIATION_ID_KEY, L"");

                            if (associationIdString.empty())
                            {
                                // invalid entry
                                continue;
                            }

                            entry->InternalInitialize(
                                winrt::guid(associationIdString),
                                entryObject.GetNamedString(MIDI_CONFIG_JSON_ENDPOINT_BASIC_LOOPBACK_LIST_ENTRY_ENDPOINT_DEVICE_ID_KEY, L""),
                                entryObject.GetNamedString(MIDI_CONFIG_JSON_ENDPOINT_BASIC_LOOPBACK_LIST_ENTRY_NAME_KEY, L""),
                                entryObject.GetNamedString(MIDI_CONFIG_JSON_ENDPOINT_BASIC_LOOPBACK_LIST_ENTRY_DESCRIPTION_KEY, L""),
                                entryObject.GetNamedString(MIDI_CONFIG_JSON_ENDPOINT_BASIC_LOOPBACK_LIST_ENTRY_IMAGE_KEY, L""),
                                // the default was L"", which is a pointer and so converted to
                                // true: an entry with no muted key reported itself muted
                                entryObject.GetNamedBoolean(MIDI_CONFIG_JSON_ENDPOINT_BASIC_LOOPBACK_LIST_ENTRY_MUTED_KEY, false)
                            );

                            results.Append(*entry);
                        }
                    }
                }
            }
        }
        catch (winrt::hresult_error const& ex)
        {
            MIDI_SDK_LOG_HRESULT_EXCEPTION(nullptr, ex, L"hresult error getting active loopback entries.");
        }
        catch (...)
        {
            MIDI_SDK_LOG_GENERAL_EXCEPTION(nullptr, L"General exception getting active loopback entries.");
        }

        return results;

    }


}
