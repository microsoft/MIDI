// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App SDK and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================


#include "pch.h"

#include "ServiceConfig.MidiServiceConfig.g.cpp"

namespace winrt::Microsoft::Windows::Devices::Midi2::ServiceConfig::implementation
{
    _Use_decl_annotations_
    json::JsonObject MidiServiceConfig::InternalSendConfigJsonAndGetResponse(
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
    svc::MidiServiceConfigResponse MidiServiceConfig::UpdateTransportPluginConfig(
        svc::IMidiServiceTransportPluginConfig const& configUpdate) noexcept
    {
        // this initializes to a failure state, so we can just return it when we have a fail
        svc::MidiServiceConfigResponse response;

        if (configUpdate == nullptr)
        {
            LOG_IF_FAILED(E_POINTER);   // this also generates a fallback error with file and line number info

            TraceLoggingWrite(
                Midi2SdkTelemetryProvider::Provider(),
                MIDI_SDK_TRACE_EVENT_ERROR,
                TraceLoggingString(__FUNCTION__, MIDI_SDK_TRACE_LOCATION_FIELD),
                TraceLoggingLevel(WINEVENT_LEVEL_ERROR),
                TraceLoggingWideString(MIDI_SDK_STATIC_THIS_PLACEHOLDER_FIELD_VALUE, MIDI_SDK_TRACE_THIS_FIELD),
                TraceLoggingWideString(L"Configuration object is null", MIDI_SDK_TRACE_MESSAGE_FIELD)
            );

            response.Status = svc::MidiServiceConfigResponseStatus::ErrorConfigJsonNullOrEmpty;

            return response;
        }

        json::JsonObject jsonUpdate{ nullptr }; 
            
        if (!json::JsonObject::TryParse(configUpdate.GetConfigJson(), jsonUpdate))
        {
            response.Status = svc::MidiServiceConfigResponseStatus::ErrorProcessingConfigJson;
            return response;
        }

        if (jsonUpdate == nullptr)
        {
            LOG_IF_FAILED(E_POINTER);   // this also generates a fallback error with file and line number info

            TraceLoggingWrite(
                Midi2SdkTelemetryProvider::Provider(),
                MIDI_SDK_TRACE_EVENT_ERROR,
                TraceLoggingString(__FUNCTION__, MIDI_SDK_TRACE_LOCATION_FIELD),
                TraceLoggingLevel(WINEVENT_LEVEL_ERROR),
                TraceLoggingWideString(MIDI_SDK_STATIC_THIS_PLACEHOLDER_FIELD_VALUE, MIDI_SDK_TRACE_THIS_FIELD),
                TraceLoggingWideString(L"Configuration object SettingsJson is null", MIDI_SDK_TRACE_MESSAGE_FIELD)
            );

            response.Status = svc::MidiServiceConfigResponseStatus::ErrorProcessingConfigJson;
            return response;
        }

        auto responseJsonObject = InternalSendConfigJsonAndGetResponse(
            configUpdate.TransportId(),
            jsonUpdate
        );

        if (responseJsonObject == nullptr)
        {
            response.Status = svc::MidiServiceConfigResponseStatus::ErrorProcessingConfigJson;
            return response;
        }

        auto success = responseJsonObject.GetNamedBoolean(MIDI_CONFIG_JSON_CONFIGURATION_RESPONSE_SUCCESS_PROPERTY_KEY, false);

        if (success)
        {
            response.Status = svc::MidiServiceConfigResponseStatus::Success;
            response.ResponseJson = responseJsonObject.Stringify();

            return response;
        }
        else
        {
            response.Status = svc::MidiServiceConfigResponseStatus::ErrorProcessingResponseJson;

            return response;
        }
    }

    //_Use_decl_annotations_
    //svc::MidiServiceConfigResponse MidiServiceConfig::UpdateProcessingPluginConfig(
    //    svc::IMidiServiceMessageProcessingPluginConfig const& configurationUpdate) noexcept
    //{
    //    UNREFERENCED_PARAMETER(configurationUpdate);
    //    // initializes to a failed value
    //    //auto response = winrt::make_self<implementation::MidiServiceConfigurationResponse>();

    //    svc::MidiServiceConfigResponse response;
    //    response.Status = svc::MidiServiceConfigResponseStatus::ErrorNotImplemented;


    //    // TODO: Implement this in service and API





    //    return response;
    //}


}
