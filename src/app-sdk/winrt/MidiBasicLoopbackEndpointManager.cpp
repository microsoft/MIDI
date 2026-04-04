// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App SDK and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================


#include "pch.h"
#include "MidiBasicLoopbackEndpointManager.h"
#include "Endpoints.BasicLoopback.MidiBasicLoopbackEndpointManager.g.cpp"

#define MIDI_BLOOP_INSTANCE_ID_PREFIX L"MIDIU_BLOOP_"

namespace winrt::Microsoft::Windows::Devices::Midi2::Endpoints::BasicLoopback::implementation
{
    bool MidiBasicLoopbackEndpointManager::IsTransportAvailable() noexcept
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
    bloop::MidiBasicLoopbackEndpointCreationResult MidiBasicLoopbackEndpointManager::CreateTransientLoopbackEndpoint(
        bloop::MidiBasicLoopbackEndpointCreationConfig const& creationConfig) noexcept
    {
        // the success code in this defaults to False
        bloop::MidiBasicLoopbackEndpointCreationResult result{};
        result.Success = false;


        try
        {
            auto serviceResponse = svc::MidiServiceConfig::UpdateTransportPluginConfig(creationConfig);

            // parse the results
            auto successResult = serviceResponse.Status == svc::MidiServiceConfigResponseStatus::Success;

            if (successResult)
            {
                json::JsonObject serviceResponseJson;

                if (json::JsonObject::TryParse(serviceResponse.ResponseJson, serviceResponseJson))
                {
                    auto deviceId = serviceResponseJson.GetNamedString(MIDI_CONFIG_JSON_ENDPOINT_BASIC_LOOPBACK_DEVICE_RESPONSE_CREATED_ENDPOINT_ID_KEY, L"");

                    if (!deviceId.empty())
                    {
                        // update the response object with the new ids
                        result.AssociationId = creationConfig.AssociationId();
                        result.EndpointDeviceId = deviceId;
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
        catch (winrt::hresult_error ex)
        {
            result.ErrorInformation = ex.message();

            TraceLoggingWrite(
                Midi2SdkTelemetryProvider::Provider(),
                MIDI_SDK_TRACE_EVENT_ERROR,
                TraceLoggingString(__FUNCTION__, MIDI_SDK_TRACE_LOCATION_FIELD),
                TraceLoggingLevel(WINEVENT_LEVEL_INFO),
                TraceLoggingPointer(MIDI_SDK_STATIC_THIS_PLACEHOLDER_FIELD_VALUE, MIDI_SDK_TRACE_THIS_FIELD),
                TraceLoggingWideString(L"Device creation failed with hresult exception", MIDI_SDK_TRACE_MESSAGE_FIELD),
                TraceLoggingGuid(creationConfig.AssociationId(), "association id"),
                TraceLoggingHResult(ex.code(), MIDI_SDK_TRACE_HRESULT_FIELD)
                );
        }
        catch (...)
        {
            result.ErrorInformation = L"General exception / error.";

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
    bool MidiBasicLoopbackEndpointManager::RemoveTransientLoopbackEndpoint(
        bloop::MidiBasicLoopbackEndpointRemovalConfig const& removalConfig) noexcept
    {
        bool result = false;

        try
        {
            // the success code in this defaults to False

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
                TraceLoggingGuid(removalConfig.AssociationId(), "association id"),
                TraceLoggingHResult(ex.code(), MIDI_SDK_TRACE_HRESULT_FIELD)
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
                TraceLoggingWideString(L"Device removal failed with general exception", MIDI_SDK_TRACE_MESSAGE_FIELD),
                TraceLoggingGuid(removalConfig.AssociationId(), "association id")
            );
        }

        return result;
    }

    _Use_decl_annotations_
    winrt::guid MidiBasicLoopbackEndpointManager::GetAssociationId(
        midi2::MidiEndpointDeviceInformation const& basicLoopbackEndpoint) noexcept
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
    bool MidiBasicLoopbackEndpointManager::DoesLoopbackExist(winrt::hstring const& uniqueIdentifier)
    {
        winrt::hstring id = BuildDeviceId(MIDI_BLOOP_INSTANCE_ID_PREFIX, uniqueIdentifier.c_str());

        return (internal::IsValidWindowsMidiServicesEndpointId(id) && internal::IsWindowsMidiServicesEndpointPresent(id));
    }

    _Use_decl_annotations_
    bool MidiBasicLoopbackEndpointManager::MuteLoopback(_In_ winrt::guid const& associationId)
    {
        try
        {
            svc::MidiServiceTransportCommand cmd(TransportId());

            cmd.Arguments().Insert(MIDI_CONFIG_JSON_TRANSPORT_COMMAND_COMMON_PARAMETER_ENDPOINT_ASSOCIATION_ID, internal::GuidToString(associationId));
            cmd.Verb(MIDI_CONFIG_JSON_TRANSPORT_COMMAND_MUTE_ENDPOINT);

            auto result = svc::MidiServiceConfig::SendTransportCommand(cmd);

            if (result.Status == svc::MidiServiceConfigResponseStatus::Success)
            {
                return true;
            }
            else
            {
                TraceLoggingWrite(
                    Midi2SdkTelemetryProvider::Provider(),
                    MIDI_SDK_TRACE_EVENT_ERROR,
                    TraceLoggingString(__FUNCTION__, MIDI_SDK_TRACE_LOCATION_FIELD),
                    TraceLoggingLevel(WINEVENT_LEVEL_INFO),
                    TraceLoggingPointer(MIDI_SDK_STATIC_THIS_PLACEHOLDER_FIELD_VALUE, MIDI_SDK_TRACE_THIS_FIELD),
                    TraceLoggingWideString(L"Failed to mute loopback", MIDI_SDK_TRACE_MESSAGE_FIELD),
                    TraceLoggingWideString(result.ServiceMessage.c_str()),
                    TraceLoggingGuid(associationId, "association id")
                );
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
                TraceLoggingWideString(L"Failed to mute loopback. hresult exception", MIDI_SDK_TRACE_MESSAGE_FIELD),
                TraceLoggingHResult(ex.code(), MIDI_SDK_TRACE_HRESULT_FIELD),
                TraceLoggingWideString(ex.message().c_str(), "error message"),
                TraceLoggingGuid(associationId, "association id")
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
                TraceLoggingWideString(L"Failed to mute loopback. General exception", MIDI_SDK_TRACE_MESSAGE_FIELD),
                TraceLoggingGuid(associationId, "association id")
            );
        }

        return false;
    }

    _Use_decl_annotations_
    bool MidiBasicLoopbackEndpointManager::UnmuteLoopback(_In_ winrt::guid const& associationId)
    {
        try
        {
            svc::MidiServiceTransportCommand cmd(TransportId());

            cmd.Arguments().Insert(MIDI_CONFIG_JSON_TRANSPORT_COMMAND_COMMON_PARAMETER_ENDPOINT_ASSOCIATION_ID, internal::GuidToString(associationId));
            cmd.Verb(MIDI_CONFIG_JSON_TRANSPORT_COMMAND_UNMUTE_ENDPOINT);

            auto result = svc::MidiServiceConfig::SendTransportCommand(cmd);

            if (result.Status == svc::MidiServiceConfigResponseStatus::Success)
            {
                return true;
            }
            else
            {
                TraceLoggingWrite(
                    Midi2SdkTelemetryProvider::Provider(),
                    MIDI_SDK_TRACE_EVENT_ERROR,
                    TraceLoggingString(__FUNCTION__, MIDI_SDK_TRACE_LOCATION_FIELD),
                    TraceLoggingLevel(WINEVENT_LEVEL_INFO),
                    TraceLoggingPointer(MIDI_SDK_STATIC_THIS_PLACEHOLDER_FIELD_VALUE, MIDI_SDK_TRACE_THIS_FIELD),
                    TraceLoggingWideString(L"Failed to unmute loopback", MIDI_SDK_TRACE_MESSAGE_FIELD),
                    TraceLoggingWideString(result.ServiceMessage.c_str()),
                    TraceLoggingGuid(associationId, "association id")
                );
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
                TraceLoggingWideString(L"Failed to unmute loopback. hresult exception", MIDI_SDK_TRACE_MESSAGE_FIELD),
                TraceLoggingHResult(ex.code(), MIDI_SDK_TRACE_HRESULT_FIELD),
                TraceLoggingWideString(ex.message().c_str(), "error message"),
                TraceLoggingGuid(associationId, "association id")
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
                TraceLoggingWideString(L"Failed to unmute loopback. General exception", MIDI_SDK_TRACE_MESSAGE_FIELD),
                TraceLoggingGuid(associationId, "association id")
            );
        }

        return false;
    }

}
