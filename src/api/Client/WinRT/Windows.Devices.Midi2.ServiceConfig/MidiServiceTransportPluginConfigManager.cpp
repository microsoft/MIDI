// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App SDK and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================


#include "pch.h"

#include "MidiServiceTransportPluginConfigManager.g.cpp"

namespace winrt::Windows::Devices::Midi2::ServiceConfig::implementation
{
    _Use_decl_annotations_
    json::JsonObject MidiServiceTransportPluginConfigManager::InternalSendConfigJsonAndGetResponse(
        winrt::guid const& transportId, 
        json::JsonObject const& configObject
    ) noexcept
    {
        auto iid = __uuidof(IMidiTransportConfigurationManager);
        winrt::com_ptr<IMidiTransportConfigurationManager> configManager;

        auto serviceTransport = winrt::create_instance<IMidiTransport>(__uuidof(Midi2MidiSrvTransport), CLSCTX_ALL);

        // default to failed
        auto response = internal::BuildConfigurationResponseObject(false);

        if (serviceTransport)
        {
            auto activateConfigManagerResult = serviceTransport->Activate(iid, (void**)&configManager);

            if (FAILED(activateConfigManagerResult) || configManager == nullptr)
            {
                LOG_IF_FAILED(activateConfigManagerResult);   // this also generates a fallback error with file and line number info

                TraceLoggingWrite(
                    Midi2SdkTelemetryProvider::Provider(),
                    MIDI_SDK_TRACE_EVENT_ERROR,
                    TraceLoggingString(__FUNCTION__, MIDI_SDK_TRACE_LOCATION_FIELD),
                    TraceLoggingLevel(WINEVENT_LEVEL_ERROR),
                    TraceLoggingWideString(MIDI_SDK_STATIC_THIS_PLACEHOLDER_FIELD_VALUE, MIDI_SDK_TRACE_THIS_FIELD),
                    TraceLoggingWideString(L"Config manager activation failed with hresult or null config manager", MIDI_SDK_TRACE_MESSAGE_FIELD),
                    TraceLoggingHResult(activateConfigManagerResult, MIDI_SDK_TRACE_HRESULT_FIELD)
                    );

                return response;
            }

            auto initializeResult = configManager->Initialize(transportId, nullptr, nullptr);

            if (FAILED(initializeResult))
            {
                LOG_IF_FAILED(initializeResult);   // this also generates a fallback error with file and line number info

                TraceLoggingWrite(
                    Midi2SdkTelemetryProvider::Provider(),
                    MIDI_SDK_TRACE_EVENT_ERROR,
                    TraceLoggingString(__FUNCTION__, MIDI_SDK_TRACE_LOCATION_FIELD),
                    TraceLoggingLevel(WINEVENT_LEVEL_ERROR),
                    TraceLoggingWideString(MIDI_SDK_STATIC_THIS_PLACEHOLDER_FIELD_VALUE, MIDI_SDK_TRACE_THIS_FIELD),
                    TraceLoggingWideString(L"Failed to initialize config manager", MIDI_SDK_TRACE_MESSAGE_FIELD),
                    TraceLoggingHResult(initializeResult, MIDI_SDK_TRACE_HRESULT_FIELD)
                );

                // return a fail result
                return response;
            }

            auto jsonPayload = configObject.Stringify();

            LPWSTR rpcResponseString{ nullptr };


            TraceLoggingWrite(
                Midi2SdkTelemetryProvider::Provider(),
                MIDI_SDK_TRACE_EVENT_INFO,
                TraceLoggingString(__FUNCTION__, MIDI_SDK_TRACE_LOCATION_FIELD),
                TraceLoggingLevel(WINEVENT_LEVEL_INFO),
                TraceLoggingWideString(MIDI_SDK_STATIC_THIS_PLACEHOLDER_FIELD_VALUE, MIDI_SDK_TRACE_THIS_FIELD),
                TraceLoggingWideString(L"Failed to initialize config manager", MIDI_SDK_TRACE_MESSAGE_FIELD),
                TraceLoggingPointer(&rpcResponseString, "rpc response string local address")
            );

            // send up the payload
            auto callStatus = configManager->UpdateConfiguration(
                jsonPayload.c_str(), 
                &rpcResponseString
            );




            if (SUCCEEDED(callStatus) && rpcResponseString != nullptr && wcslen(rpcResponseString) > 0)
            {
                winrt::hstring hstr{ rpcResponseString };

                SAFE_COTASKMEMFREE(rpcResponseString);

                json::JsonObject responseObject = json::JsonObject::Parse(hstr);

                if (responseObject != nullptr)
                {
                    // returns the JSON we parsed from the service response
                    return responseObject;
                }
                else
                {
                    // failed

                    LOG_IF_FAILED(E_FAIL);   // this also generates a fallback error with file and line number info

                    TraceLoggingWrite(
                        Midi2SdkTelemetryProvider::Provider(),
                        MIDI_SDK_TRACE_EVENT_ERROR,
                        TraceLoggingString(__FUNCTION__, MIDI_SDK_TRACE_LOCATION_FIELD),
                        TraceLoggingLevel(WINEVENT_LEVEL_ERROR),
                        TraceLoggingWideString(MIDI_SDK_STATIC_THIS_PLACEHOLDER_FIELD_VALUE, MIDI_SDK_TRACE_THIS_FIELD),
                        TraceLoggingWideString(L"Unable to read response object from string", MIDI_SDK_TRACE_MESSAGE_FIELD)
                    );

                    return response;
                }
            }
            else if (rpcResponseString != nullptr && wcslen(rpcResponseString) > 0)
            {
                // the service calls return E_FAIL or other failure codes, and then still send 
                // back the reason in the response. This is technically incorrect, but already 
                // in production,so need to handle it here

                winrt::hstring hstr{ rpcResponseString };

                SAFE_COTASKMEMFREE(rpcResponseString);

                json::JsonObject responseObject = json::JsonObject::Parse(hstr);

                if (responseObject != nullptr)
                {
                    // returns the JSON we parsed from the service response
                    return responseObject;
                }
                else
                {
                    // failed

                    LOG_IF_FAILED(E_FAIL);   // this also generates a fallback error with file and line number info

                    TraceLoggingWrite(
                        Midi2SdkTelemetryProvider::Provider(),
                        MIDI_SDK_TRACE_EVENT_ERROR,
                        TraceLoggingString(__FUNCTION__, MIDI_SDK_TRACE_LOCATION_FIELD),
                        TraceLoggingLevel(WINEVENT_LEVEL_ERROR),
                        TraceLoggingWideString(MIDI_SDK_STATIC_THIS_PLACEHOLDER_FIELD_VALUE, MIDI_SDK_TRACE_THIS_FIELD),
                        TraceLoggingWideString(L"Unable to read response object from string", MIDI_SDK_TRACE_MESSAGE_FIELD)
                    );

                    return response;
                }

            }
            else
            {
                // failed

                LOG_IF_FAILED(E_FAIL);   // this also generates a fallback error with file and line number info

                TraceLoggingWrite(
                    Midi2SdkTelemetryProvider::Provider(),
                    MIDI_SDK_TRACE_EVENT_ERROR,
                    TraceLoggingString(__FUNCTION__, MIDI_SDK_TRACE_LOCATION_FIELD),
                    TraceLoggingLevel(WINEVENT_LEVEL_ERROR),
                    TraceLoggingWideString(MIDI_SDK_STATIC_THIS_PLACEHOLDER_FIELD_VALUE, MIDI_SDK_TRACE_THIS_FIELD),
                    TraceLoggingWideString(L"Service config call failed", MIDI_SDK_TRACE_MESSAGE_FIELD),
                    TraceLoggingHResult(callStatus, "hresult"),
                    TraceLoggingWideString(rpcResponseString, "service response string")
                );

                return response;
            }
        }
        else
        {
            // failed
            LOG_IF_FAILED(E_FAIL);   // this also generates a fallback error with file and line number info

            TraceLoggingWrite(
                Midi2SdkTelemetryProvider::Provider(),
                MIDI_SDK_TRACE_EVENT_ERROR,
                TraceLoggingString(__FUNCTION__, MIDI_SDK_TRACE_LOCATION_FIELD),
                TraceLoggingLevel(WINEVENT_LEVEL_ERROR),
                TraceLoggingWideString(MIDI_SDK_STATIC_THIS_PLACEHOLDER_FIELD_VALUE, MIDI_SDK_TRACE_THIS_FIELD),
                TraceLoggingWideString(L"Failed to create service transport", MIDI_SDK_TRACE_MESSAGE_FIELD)
            );

            return response;
        }

    }



    _Use_decl_annotations_
    svc::MidiServiceConfigResponse MidiServiceTransportPluginConfigManager::SendUpdate(
        svc::IMidiServiceTransportPluginConfig const& configUpdate) noexcept
    {
        auto response = winrt::make_self<MidiServiceConfigResponse>();

        if (configUpdate == nullptr)
        {
            LOG_IF_FAILED(E_INVALIDARG);   // this also generates a fallback error with file and line number info

            TraceLoggingWrite(
                Midi2SdkTelemetryProvider::Provider(),
                MIDI_SDK_TRACE_EVENT_ERROR,
                TraceLoggingString(__FUNCTION__, MIDI_SDK_TRACE_LOCATION_FIELD),
                TraceLoggingLevel(WINEVENT_LEVEL_ERROR),
                TraceLoggingWideString(MIDI_SDK_STATIC_THIS_PLACEHOLDER_FIELD_VALUE, MIDI_SDK_TRACE_THIS_FIELD),
                TraceLoggingWideString(L"Configuration object is null", MIDI_SDK_TRACE_MESSAGE_FIELD)
            );

            response->InternalSetStatus(svc::MidiServiceConfigResponseStatus::ErrorConfigJsonNullOrEmpty);
            return *response;
        }

        return SendUpdate(configUpdate.TransportId(), configUpdate.ConfigJson());

    }

    _Use_decl_annotations_
    svc::MidiServiceConfigResponse MidiServiceTransportPluginConfigManager::SendUpdate(
        winrt::guid const& transportId,
        json::JsonObject const& fullConfigObject) noexcept
    {
        TraceLoggingWrite(
            Midi2SdkTelemetryProvider::Provider(),
            MIDI_SDK_TRACE_EVENT_INFO,
            TraceLoggingString(__FUNCTION__, MIDI_SDK_TRACE_LOCATION_FIELD),
            TraceLoggingLevel(WINEVENT_LEVEL_ERROR),
            TraceLoggingWideString(MIDI_SDK_STATIC_THIS_PLACEHOLDER_FIELD_VALUE, MIDI_SDK_TRACE_THIS_FIELD),
            TraceLoggingWideString(L"Enter", MIDI_SDK_TRACE_MESSAGE_FIELD),
            TraceLoggingGuid(transportId, "transport id")
        );

        auto response = winrt::make_self<MidiServiceConfigResponse>();

        if (response == nullptr)
        {
            TraceLoggingWrite(
                Midi2SdkTelemetryProvider::Provider(),
                MIDI_SDK_TRACE_EVENT_ERROR,
                TraceLoggingString(__FUNCTION__, MIDI_SDK_TRACE_LOCATION_FIELD),
                TraceLoggingLevel(WINEVENT_LEVEL_ERROR),
                TraceLoggingWideString(MIDI_SDK_STATIC_THIS_PLACEHOLDER_FIELD_VALUE, MIDI_SDK_TRACE_THIS_FIELD),
                TraceLoggingWideString(L"Unable to create response object", MIDI_SDK_TRACE_MESSAGE_FIELD),
                TraceLoggingGuid(transportId, "transport id")
            );
            return nullptr;
        }

        try
        {
            if (fullConfigObject == nullptr)
            {
                LOG_IF_FAILED(E_INVALIDARG);   // this also generates a fallback error with file and line number info

                TraceLoggingWrite(
                    Midi2SdkTelemetryProvider::Provider(),
                    MIDI_SDK_TRACE_EVENT_ERROR,
                    TraceLoggingString(__FUNCTION__, MIDI_SDK_TRACE_LOCATION_FIELD),
                    TraceLoggingLevel(WINEVENT_LEVEL_ERROR),
                    TraceLoggingWideString(MIDI_SDK_STATIC_THIS_PLACEHOLDER_FIELD_VALUE, MIDI_SDK_TRACE_THIS_FIELD),
                    TraceLoggingWideString(L"Configuration object SettingsJson is null", MIDI_SDK_TRACE_MESSAGE_FIELD),
                    TraceLoggingGuid(transportId, "transport id")
                );

                response->InternalSetStatus(svc::MidiServiceConfigResponseStatus::ErrorProcessingConfigJson);
                return *response;
            }

            auto responseJsonObject = InternalSendConfigJsonAndGetResponse(
                transportId,
                fullConfigObject
            );

            if (responseJsonObject == nullptr)
            {
                response->InternalSetStatus(svc::MidiServiceConfigResponseStatus::ErrorProcessingConfigJson);

                TraceLoggingWrite(
                    Midi2SdkTelemetryProvider::Provider(),
                    MIDI_SDK_TRACE_EVENT_ERROR,
                    TraceLoggingString(__FUNCTION__, MIDI_SDK_TRACE_LOCATION_FIELD),
                    TraceLoggingLevel(WINEVENT_LEVEL_INFO),
                    TraceLoggingPointer(MIDI_SDK_STATIC_THIS_PLACEHOLDER_FIELD_VALUE, MIDI_SDK_TRACE_THIS_FIELD),
                    TraceLoggingWideString(L"Failed to update transport. Error processing config json", MIDI_SDK_TRACE_MESSAGE_FIELD),
                    TraceLoggingGuid(transportId, "transport id")
                );

                return *response;
            }

            auto success = responseJsonObject.GetNamedBoolean(MIDI_CONFIG_JSON_CONFIGURATION_RESPONSE_SUCCESS_PROPERTY_KEY, false);


            if (success)
            {
                response->InternalSetServiceSuccess(responseJsonObject);

                TraceLoggingWrite(
                    Midi2SdkTelemetryProvider::Provider(),
                    MIDI_SDK_TRACE_EVENT_INFO,
                    TraceLoggingString(__FUNCTION__, MIDI_SDK_TRACE_LOCATION_FIELD),
                    TraceLoggingLevel(WINEVENT_LEVEL_ERROR),
                    TraceLoggingWideString(MIDI_SDK_STATIC_THIS_PLACEHOLDER_FIELD_VALUE, MIDI_SDK_TRACE_THIS_FIELD),
                    TraceLoggingWideString(L"Succeeded", MIDI_SDK_TRACE_MESSAGE_FIELD),
                    TraceLoggingGuid(transportId, "transport id")
                );

                return *response;
            }
            else
            {
                response->InternalSetServiceError(
                    static_cast<uint32_t>(responseJsonObject.GetNamedNumber(MIDI_CONFIG_JSON_CONFIGURATION_RESPONSE_ERROR_CODE_PROPERTY_KEY, 0)),
                    responseJsonObject.GetNamedString(MIDI_CONFIG_JSON_CONFIGURATION_RESPONSE_MESSAGE_PROPERTY_KEY, L""),
                    responseJsonObject
                );

                TraceLoggingWrite(
                    Midi2SdkTelemetryProvider::Provider(),
                    MIDI_SDK_TRACE_EVENT_ERROR,
                    TraceLoggingString(__FUNCTION__, MIDI_SDK_TRACE_LOCATION_FIELD),
                    TraceLoggingLevel(WINEVENT_LEVEL_INFO),
                    TraceLoggingPointer(MIDI_SDK_STATIC_THIS_PLACEHOLDER_FIELD_VALUE, MIDI_SDK_TRACE_THIS_FIELD),
                    TraceLoggingWideString(L"Failed to update transport. Error from service", MIDI_SDK_TRACE_MESSAGE_FIELD),
                    TraceLoggingUInt32(response->ServiceErrorCode(), "service error code"),
                    TraceLoggingWideString(response->ServiceErrorMessage().c_str(), "service message"),
                    TraceLoggingGuid(transportId, "transport id")
                );

                return *response;
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
                TraceLoggingWideString(L"Failed to update transport. hresult exception", MIDI_SDK_TRACE_MESSAGE_FIELD),
                TraceLoggingHResult(ex.code(), "hresult"),
                TraceLoggingWideString(ex.message().c_str(), "error message"),
                TraceLoggingGuid(transportId, "transport id")
            );

            return *response;
        }
    }

    _Use_decl_annotations_
    svc::MidiServiceConfigResponse MidiServiceTransportPluginConfigManager::SendCommand(
        svc::MidiServiceTransportCommand const& command) noexcept
    {
        return SendUpdate(command.TransportId(), command.ConfigJson());
    }

}
