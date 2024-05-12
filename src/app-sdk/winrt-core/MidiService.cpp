// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

#include "pch.h"

#include "MidiService.g.cpp"

namespace winrt::Microsoft::Devices::Midi2::implementation
{
    // returns True if the MIDI Service is available on this PC
    bool MidiService::EnsureAvailable()
    {
        try
        {
            auto serviceAbstraction = winrt::create_instance<IMidiAbstraction>(__uuidof(Midi2MidiSrvAbstraction), CLSCTX_ALL);

            // winrt::try_create_instance indicates failure by returning an empty com ptr
            // winrt::create_instance indicates failure with an exception
            if (serviceAbstraction == nullptr)
            {
                LOG_IF_FAILED(E_POINTER);   // this also generates a fallback error with file and line number info

                TraceLoggingWrite(
                    Midi2SdkTelemetryProvider::Provider(),
                    MIDI_SDK_TRACE_EVENT_ERROR,
                    TraceLoggingString(__FUNCTION__, MIDI_SDK_TRACE_LOCATION_FIELD),
                    TraceLoggingLevel(WINEVENT_LEVEL_ERROR),
                    TraceLoggingWideString(MIDI_SDK_STATIC_THIS_PLACEHOLDER_FIELD_VALUE, MIDI_SDK_TRACE_THIS_FIELD),
                    TraceLoggingWideString(L"Error contacting service. Service abstraction is nullptr", MIDI_SDK_TRACE_MESSAGE_FIELD)
                );                
                
                return false;
            }

            winrt::com_ptr<IMidiSessionTracker> tracker;

            auto sessionTrackerResult = serviceAbstraction->Activate(__uuidof(IMidiSessionTracker), (void**)&tracker);
            if (FAILED(sessionTrackerResult))
            {
                LOG_IF_FAILED(sessionTrackerResult);   // this also generates a fallback error with file and line number info

                TraceLoggingWrite(
                    Midi2SdkTelemetryProvider::Provider(),
                    MIDI_SDK_TRACE_EVENT_ERROR,
                    TraceLoggingString(__FUNCTION__, MIDI_SDK_TRACE_LOCATION_FIELD),
                    TraceLoggingLevel(WINEVENT_LEVEL_ERROR),
                    TraceLoggingWideString(MIDI_SDK_STATIC_THIS_PLACEHOLDER_FIELD_VALUE, MIDI_SDK_TRACE_THIS_FIELD),
                    TraceLoggingWideString(L"Failure hresult received activating interface", MIDI_SDK_TRACE_MESSAGE_FIELD),
                    TraceLoggingHResult(sessionTrackerResult, MIDI_SDK_TRACE_HRESULT_FIELD)
                );

                return false;
            }

            auto verifyConnectivityResult = tracker->VerifyConnectivity();
            if (FAILED(verifyConnectivityResult))
            {
                LOG_IF_FAILED(verifyConnectivityResult);   // this also generates a fallback error with file and line number info

                TraceLoggingWrite(
                    Midi2SdkTelemetryProvider::Provider(),
                    MIDI_SDK_TRACE_EVENT_ERROR,
                    TraceLoggingString(__FUNCTION__, MIDI_SDK_TRACE_LOCATION_FIELD),
                    TraceLoggingLevel(WINEVENT_LEVEL_ERROR),
                    TraceLoggingWideString(MIDI_SDK_STATIC_THIS_PLACEHOLDER_FIELD_VALUE, MIDI_SDK_TRACE_THIS_FIELD),
                    TraceLoggingWideString(L"Failure hresult received verifying connectivity", MIDI_SDK_TRACE_MESSAGE_FIELD),
                    TraceLoggingHResult(verifyConnectivityResult, MIDI_SDK_TRACE_HRESULT_FIELD)
                );

                return false;
            }

            return true;
        }
        catch (winrt::hresult_error ex)
        {
            LOG_IF_FAILED(static_cast<HRESULT>(ex.code()));   // this also generates a fallback error with file and line number info

            TraceLoggingWrite(
                Midi2SdkTelemetryProvider::Provider(),
                MIDI_SDK_TRACE_EVENT_ERROR,
                TraceLoggingString(__FUNCTION__, MIDI_SDK_TRACE_LOCATION_FIELD),
                TraceLoggingLevel(WINEVENT_LEVEL_ERROR),
                TraceLoggingWideString(MIDI_SDK_STATIC_THIS_PLACEHOLDER_FIELD_VALUE, MIDI_SDK_TRACE_THIS_FIELD),
                TraceLoggingWideString(L"Error contacting service. It may be unavailable", MIDI_SDK_TRACE_MESSAGE_FIELD),
                TraceLoggingHResult(static_cast<HRESULT>(ex.code()), MIDI_SDK_TRACE_HRESULT_FIELD),
                TraceLoggingWideString(ex.message().c_str(), MIDI_SDK_TRACE_ERROR_FIELD)
            );


            // winrt::create_instance fails by throwing an exception
            return false;
        }
        catch (...)
        {
            LOG_IF_FAILED(E_FAIL);   // this also generates a fallback error with file and line number info

            TraceLoggingWrite(
                Midi2SdkTelemetryProvider::Provider(),
                MIDI_SDK_TRACE_EVENT_ERROR,
                TraceLoggingString(__FUNCTION__, MIDI_SDK_TRACE_LOCATION_FIELD),
                TraceLoggingLevel(WINEVENT_LEVEL_ERROR),
                TraceLoggingWideString(MIDI_SDK_STATIC_THIS_PLACEHOLDER_FIELD_VALUE, MIDI_SDK_TRACE_THIS_FIELD),
                TraceLoggingWideString(L"Error contacting service. It may be unavailable", MIDI_SDK_TRACE_MESSAGE_FIELD)
            );

            // winrt::create_instance fails by throwing an exception
            return false;
        }
    }

    _Use_decl_annotations_
    json::JsonObject MidiService::InternalSendConfigurationJsonAndGetResponse(
        winrt::guid const& abstractionId, 
        json::JsonObject const& configObject,
        bool const isFromConfigurationFile
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

            auto configUpdateResult = configManager->UpdateConfiguration(jsonPayload.c_str(), isFromConfigurationFile, &responseString);

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
    midi2::MidiServiceConfigurationResponse MidiService::UpdateTransportPluginConfiguration(
        midi2::IMidiServiceTransportPluginConfiguration const& configurationUpdate) noexcept
    {
        // this initializes to a failure state, so we can just return it when we have a fail
        auto response = winrt::make_self<implementation::MidiServiceConfigurationResponse>();
        response->InternalSetStatus(midi2::MidiServiceConfigurationResponseStatus::ErrorOther); // default

        if (configurationUpdate == nullptr)
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

            response->InternalSetStatus(midi2::MidiServiceConfigurationResponseStatus::ErrorOther);

            return *response;
        }

        json::JsonObject jsonUpdate{ nullptr }; 
            
        if (!json::JsonObject::TryParse(configurationUpdate.GetConfigurationJson(), jsonUpdate))
        {
            response->InternalSetStatus(midi2::MidiServiceConfigurationResponseStatus::ErrorProcessingJson);
            return *response;
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

            response->InternalSetStatus(midi2::MidiServiceConfigurationResponseStatus::ErrorProcessingJson);
            return *response;
        }

        auto responseJsonObject = InternalSendConfigurationJsonAndGetResponse(
            configurationUpdate.TransportId(),
            jsonUpdate,
            configurationUpdate.IsFromConfigurationFile()
        );

        if (responseJsonObject == nullptr)
        {
            response->InternalSetStatus(midi2::MidiServiceConfigurationResponseStatus::ErrorProcessingJson);
            return *response;
        }

        auto success = responseJsonObject.GetNamedBoolean(MIDI_CONFIG_JSON_CONFIGURATION_RESPONSE_SUCCESS_PROPERTY_KEY, false);

        if (success)
        {
            response->InternalSetStatus(midi2::MidiServiceConfigurationResponseStatus::Success);

            // TODO: Process the return object

            return *response;
        }
        else
        {
            response->InternalSetStatus(midi2::MidiServiceConfigurationResponseStatus::ErrorOther);

            return *response;
        }
    }

    _Use_decl_annotations_
    midi2::MidiServiceConfigurationResponse MidiService::UpdateProcessingPluginConfiguration(
        midi2::IMidiServiceMessageProcessingPluginConfiguration const& configurationUpdate) noexcept
    {
        UNREFERENCED_PARAMETER(configurationUpdate);
        // initializes to a failed value
        auto response = winrt::make_self<implementation::MidiServiceConfigurationResponse>();




        // TODO: Implement this in service and API





        return *response;
    }


}
