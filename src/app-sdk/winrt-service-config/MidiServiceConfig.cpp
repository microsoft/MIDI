// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App SDK and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================


#include "pch.h"

#include "MidiServiceConfig.g.cpp"

namespace winrt::Microsoft::Windows::Devices::Midi2::ServiceConfig::implementation
{
    // returns True if the MIDI Service is available on this PC


    _Use_decl_annotations_
    json::JsonObject MidiServiceConfig::InternalSendConfigJsonAndGetResponse(
        winrt::guid const& abstractionId, 
        json::JsonObject const& configObject,
        bool const isFromCurrentConfigFile
    ) noexcept
    {
        auto iid = __uuidof(IMidiAbstractionConfigurationManager);
        winrt::com_ptr<IMidiAbstractionConfigurationManager> configManager;

        auto serviceAbstraction = winrt::create_instance<IMidiAbstraction>(__uuidof(Midi2MidiSrvAbstraction), CLSCTX_ALL);

        // default to failed
        auto response = internal::BuildConfigurationResponseObject(false);


        if (serviceAbstraction)
        {
            auto activateConfigManagerResult = serviceAbstraction->Activate(iid, (void**)&configManager);

            if (configManager == nullptr)
            {
                LOG_IF_FAILED(E_POINTER);   // this also generates a fallback error with file and line number info

                TraceLoggingWrite(
                    Midi2SdkTelemetryProvider::Provider(),
                    MIDI_SDK_TRACE_EVENT_ERROR,
                    TraceLoggingString(__FUNCTION__, MIDI_SDK_TRACE_LOCATION_FIELD),
                    TraceLoggingLevel(WINEVENT_LEVEL_ERROR),
                    TraceLoggingWideString(MIDI_SDK_STATIC_THIS_PLACEHOLDER_FIELD_VALUE, MIDI_SDK_TRACE_THIS_FIELD),
                    TraceLoggingWideString(L"Config manager is null", MIDI_SDK_TRACE_MESSAGE_FIELD)
                );
                    
                return response;
            }

            if (FAILED(activateConfigManagerResult))
            {
                LOG_IF_FAILED(activateConfigManagerResult);   // this also generates a fallback error with file and line number info

                TraceLoggingWrite(
                    Midi2SdkTelemetryProvider::Provider(),
                    MIDI_SDK_TRACE_EVENT_ERROR,
                    TraceLoggingString(__FUNCTION__, MIDI_SDK_TRACE_LOCATION_FIELD),
                    TraceLoggingLevel(WINEVENT_LEVEL_ERROR),
                    TraceLoggingWideString(MIDI_SDK_STATIC_THIS_PLACEHOLDER_FIELD_VALUE, MIDI_SDK_TRACE_THIS_FIELD),
                    TraceLoggingWideString(L"Config manager call failed", MIDI_SDK_TRACE_MESSAGE_FIELD),
                    TraceLoggingHResult(activateConfigManagerResult, MIDI_SDK_TRACE_HRESULT_FIELD)
                    );

                return response;
            }

            auto initializeResult = configManager->Initialize(abstractionId, nullptr, nullptr);

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

            CComBSTR responseString{};
            responseString.Empty();

            auto jsonPayload = configObject.Stringify();

            // send up the payload

            auto configUpdateResult = configManager->UpdateConfiguration(jsonPayload.c_str(), isFromCurrentConfigFile, &responseString);

            if (FAILED(configUpdateResult))
            {
                LOG_IF_FAILED(configUpdateResult);   // this also generates a fallback error with file and line number info

                TraceLoggingWrite(
                    Midi2SdkTelemetryProvider::Provider(),
                    MIDI_SDK_TRACE_EVENT_ERROR,
                    TraceLoggingString(__FUNCTION__, MIDI_SDK_TRACE_LOCATION_FIELD),
                    TraceLoggingLevel(WINEVENT_LEVEL_ERROR),
                    TraceLoggingWideString(MIDI_SDK_STATIC_THIS_PLACEHOLDER_FIELD_VALUE, MIDI_SDK_TRACE_THIS_FIELD),
                    TraceLoggingWideString(L"Failed to configure endpoint", MIDI_SDK_TRACE_MESSAGE_FIELD),
                    TraceLoggingHResult(configUpdateResult, MIDI_SDK_TRACE_HRESULT_FIELD)
                );

                // return a failed result
                return response;
            }
            else
            {
                json::JsonObject responseObject;

                if (internal::JsonObjectFromBSTR(&responseString, responseObject))
                {
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
                        TraceLoggingWideString(L"Unable to read response object from BSTR", MIDI_SDK_TRACE_MESSAGE_FIELD)
                    );

                    return response;
                }
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
                TraceLoggingWideString(L"Failed to create service abstraction", MIDI_SDK_TRACE_MESSAGE_FIELD)
            );


            return response;
        }

    }



    _Use_decl_annotations_
    svc::MidiServiceConfigResponse MidiServiceConfig::UpdateTransportPluginConfig(
        svc::IMidiServiceTransportPluginConfig const& configUpdate) noexcept
    {
        // this initializes to a failure state, so we can just return it when we have a fail
        //auto response = winrt::make_self<implementation::MidiServiceConfigurationResponse>();

        svc::MidiServiceConfigResponse response;

        //response.Status = midi2::MidiServiceConfigResponseStatus::ErrorOther;

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
            jsonUpdate,
            configUpdate.IsFromCurrentConfigFile()
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

    _Use_decl_annotations_
    svc::MidiServiceConfigResponse MidiServiceConfig::UpdateProcessingPluginConfig(
        svc::IMidiServiceMessageProcessingPluginConfig const& configurationUpdate) noexcept
    {
        UNREFERENCED_PARAMETER(configurationUpdate);
        // initializes to a failed value
        //auto response = winrt::make_self<implementation::MidiServiceConfigurationResponse>();

        svc::MidiServiceConfigResponse response;
        response.Status = svc::MidiServiceConfigResponseStatus::ErrorNotImplemented;


        // TODO: Implement this in service and API





        return response;
    }


}
