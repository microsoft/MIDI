// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services WinRT API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================


#include "pch.h"

#include "MidiServiceTransportPluginConfigManager.h"
#include "ServiceConfig.MidiServiceTransportPluginConfigManager.g.cpp"

#include "MidiServiceConfigResponse.h"

#include "MidiServiceConfigSaveResponse.h"
#include "MidiConfigFile.h"


namespace winrt::Windows::Devices::Midi2::ServiceConfig::implementation
{
    _Use_decl_annotations_
    json::JsonObject MidiServiceTransportPluginConfigManager::InternalEnsureTransportWrapper(
        winrt::guid const& transportId,
        json::JsonObject const& configObject) noexcept
    {
        try
        {
            if (configObject == nullptr)
            {
                return nullptr;
            }

            if (configObject.HasKey(MIDI_CONFIG_JSON_TRANSPORT_PLUGIN_SETTINGS_OBJECT))
            {
                return configObject;
            }

            auto const transportKey = winrt::hstring{ internal::GuidToString(transportId) };

            // Re-parsing rather than reusing the object keeps the caller's copy untouched. Adding
            // the original to a new parent would otherwise share the same underlying node.
            json::JsonObject section{ nullptr };

            if (!json::JsonObject::TryParse(configObject.Stringify(), section) || section == nullptr)
            {
                return nullptr;
            }

            json::JsonObject pluginSettings{};

            // a caller may already have keyed by transport id but left off the outer wrapper
            if (section.HasKey(transportKey))
            {
                pluginSettings = section;
            }
            else
            {
                pluginSettings.SetNamedValue(transportKey, section);
            }

            json::JsonObject wrapper{};
            wrapper.SetNamedValue(MIDI_CONFIG_JSON_TRANSPORT_PLUGIN_SETTINGS_OBJECT, pluginSettings);

            return wrapper;
        }
        catch (...)
        {
            return nullptr;
        }
    }

    _Use_decl_annotations_
    json::JsonObject MidiServiceTransportPluginConfigManager::InternalGetTransportSection(
        winrt::guid const& transportId,
        json::JsonObject const& configObject) noexcept
    {
        try
        {
            auto const wrapped = InternalEnsureTransportWrapper(transportId, configObject);

            if (wrapped == nullptr || !wrapped.HasKey(MIDI_CONFIG_JSON_TRANSPORT_PLUGIN_SETTINGS_OBJECT))
            {
                return nullptr;
            }

            auto const pluginSettingsValue = wrapped.GetNamedValue(MIDI_CONFIG_JSON_TRANSPORT_PLUGIN_SETTINGS_OBJECT);

            if (pluginSettingsValue == nullptr || pluginSettingsValue.ValueType() != json::JsonValueType::Object)
            {
                return nullptr;
            }

            auto const pluginSettings = pluginSettingsValue.GetObject();
            auto const wanted = internal::ToUpperTrimmedWStringCopy(internal::GuidToString(transportId));

            for (auto const& pair : pluginSettings)
            {
                if (internal::ToUpperTrimmedWStringCopy(std::wstring{ pair.Key() }) == wanted)
                {
                    auto const value = pair.Value();

                    if (value != nullptr && value.ValueType() == json::JsonValueType::Object)
                    {
                        return value.GetObject();
                    }
                }
            }

            return nullptr;
        }
        catch (...)
        {
            return nullptr;
        }
    }

    _Use_decl_annotations_
    svc::MidiServiceConfigSaveResponse MidiServiceTransportPluginConfigManager::SaveUpdate(
        winrt::guid const& transportId,
        json::JsonObject const& fullConfigObject) noexcept
    {
        auto response = winrt::make_self<MidiServiceConfigSaveResponse>();

        if (response == nullptr)
        {
            return nullptr;
        }

        try
        {
            if (fullConfigObject == nullptr)
            {
                response->InternalSetResult(svc::MidiServiceConfigSaveResult::ErrorConfigJsonNullOrEmpty);
                return *response;
            }

            auto const section = InternalGetTransportSection(transportId, fullConfigObject);

            if (section == nullptr || section.Size() == 0)
            {
                response->InternalSetResult(svc::MidiServiceConfigSaveResult::ErrorProcessingConfigJson);
                return *response;
            }

            // A command tells the service to do something now. There is nothing in it to store,
            // and storing it would run it again on every service start.
            if (section.HasKey(MIDI_CONFIG_JSON_TRANSPORT_COMMON_COMMAND_KEY))
            {
                response->InternalSetResult(svc::MidiServiceConfigSaveResult::ErrorNotPersistable);
                return *response;
            }

            if (!section.HasKey(MIDI_CONFIG_JSON_ENDPOINT_COMMON_CREATE_KEY) &&
                !section.HasKey(MIDI_CONFIG_JSON_ENDPOINT_COMMON_UPDATE_KEY) &&
                !section.HasKey(MIDI_CONFIG_JSON_ENDPOINT_COMMON_REMOVE_KEY))
            {
                // Transport-level settings, such as the Bluetooth device list, are stored as-is.
                // Anything else with no recognized shape is refused rather than guessed at.
                bool const looksLikeTransportSettings = section.Size() > 0;

                if (!looksLikeTransportSettings)
                {
                    response->InternalSetResult(svc::MidiServiceConfigSaveResult::ErrorNotPersistable);
                    return *response;
                }
            }

            auto const outcome = MidiConfigFile::SaveTransportSection(transportId, section);

            response->InternalSetResult(outcome.Result);
            response->InternalSetConfigFilePath(outcome.ConfigFilePath);
            response->InternalSetBackupFilePath(outcome.BackupFilePath);

            if (outcome.Result != svc::MidiServiceConfigSaveResult::Success)
            {
                TraceLoggingWrite(
                    Midi2SdkTelemetryProvider::Provider(),
                    MIDI_SDK_TRACE_EVENT_ERROR,
                    TraceLoggingString(__FUNCTION__, MIDI_SDK_TRACE_LOCATION_FIELD),
                    TraceLoggingLevel(WINEVENT_LEVEL_ERROR),
                    TraceLoggingWideString(MIDI_SDK_STATIC_THIS_PLACEHOLDER_FIELD_VALUE, MIDI_SDK_TRACE_THIS_FIELD),
                    TraceLoggingWideString(L"Failed to save transport configuration", MIDI_SDK_TRACE_MESSAGE_FIELD),
                    TraceLoggingUInt32(static_cast<uint32_t>(outcome.Result), "save result"),
                    TraceLoggingWideString(outcome.ConfigFilePath.c_str(), "config file"),
                    TraceLoggingGuid(transportId, "transport id")
                );
            }

            return *response;
        }
        catch (winrt::hresult_error const& ex)
        {
            MIDI_SDK_LOG_HRESULT_EXCEPTION(nullptr, ex, L"hresult error saving transport configuration.");

            response->InternalSetResult(svc::MidiServiceConfigSaveResult::ErrorUnexpected);
            return *response;
        }
        catch (...)
        {
            MIDI_SDK_LOG_GENERAL_EXCEPTION(nullptr, L"General exception saving transport configuration.");

            response->InternalSetResult(svc::MidiServiceConfigSaveResult::ErrorUnexpected);
            return *response;
        }
    }

    _Use_decl_annotations_
    svc::MidiServiceConfigSaveResponse MidiServiceTransportPluginConfigManager::SaveUpdate(
        svc::IMidiServiceTransportPluginConfig const& configUpdate) noexcept
    {
        if (configUpdate == nullptr)
        {
            auto response = winrt::make_self<MidiServiceConfigSaveResponse>();

            if (response == nullptr)
            {
                return nullptr;
            }

            response->InternalSetResult(svc::MidiServiceConfigSaveResult::ErrorConfigJsonNullOrEmpty);
            return *response;
        }

        return SaveUpdate(configUpdate.TransportId(), configUpdate.ConfigJson());
    }

#ifdef _DEBUG
    winrt::hstring MidiServiceTransportPluginConfigManager::ConfigFilePathOverride() noexcept
    {
        return winrt::hstring{ MidiConfigFile::GetPathOverride() };
    }

    _Use_decl_annotations_
    void MidiServiceTransportPluginConfigManager::ConfigFilePathOverride(winrt::hstring const& value) noexcept
    {
        MidiConfigFile::SetPathOverride(std::wstring{ value });
    }
#endif

    winrt::hstring MidiServiceTransportPluginConfigManager::ConfigFilePath() noexcept
    {
        return winrt::hstring{ MidiConfigFile::ResolvePath() };
    }
}


namespace winrt::Windows::Devices::Midi2::ServiceConfig::implementation
{
    _Use_decl_annotations_
    json::JsonObject MidiServiceTransportPluginConfigManager::InternalSendConfigJsonAndGetResponse(
        winrt::guid const& transportId, 
        json::JsonObject const& configObject
    ) noexcept
    {
        // default to failed
        auto response = internal::BuildConfigurationResponseObject(false);

        try
        {

            auto iid = __uuidof(IMidiTransportConfigurationManager);
            winrt::com_ptr<IMidiTransportConfigurationManager> configManager;

            auto serviceTransport = winrt::create_instance<IMidiTransport>(__uuidof(Midi2MidiSrvTransport), CLSCTX_ALL);

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

                    TraceLoggingWrite(
                        Midi2SdkTelemetryProvider::Provider(),
                        MIDI_SDK_TRACE_EVENT_INFO,
                        TraceLoggingString(__FUNCTION__, MIDI_SDK_TRACE_LOCATION_FIELD),
                        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
                        TraceLoggingWideString(MIDI_SDK_STATIC_THIS_PLACEHOLDER_FIELD_VALUE, MIDI_SDK_TRACE_THIS_FIELD),
                        TraceLoggingWideString(L"Response received from transport", MIDI_SDK_TRACE_MESSAGE_FIELD),
                        TraceLoggingWideString(hstr.c_str(), "response string")
                    );

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
                            TraceLoggingWideString(L"Unable to read response object from string", MIDI_SDK_TRACE_MESSAGE_FIELD),
                            TraceLoggingWideString(hstr.c_str(), "response string")
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
        catch (winrt::hresult_error const& ex)
        {
            MIDI_SDK_LOG_HRESULT_EXCEPTION(nullptr, ex, L"hresult error sending config json and getting response.");
            return response;
        }
        catch (...)
        {
            MIDI_SDK_LOG_GENERAL_EXCEPTION(nullptr, L"General exception sending config json and getting response.");
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
                InternalEnsureTransportWrapper(transportId, fullConfigObject)
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
        try
        {
            return SendUpdate(command.TransportId(), command.ConfigJson());
        }
        catch (winrt::hresult_error const& ex)
        {
            MIDI_SDK_LOG_HRESULT_EXCEPTION(nullptr, ex, L"hresult error sending transport command.");
            return nullptr;
        }
        catch (...)
        {
            MIDI_SDK_LOG_GENERAL_EXCEPTION(nullptr, L"General exception sending transport command.");
            return nullptr;
        }
    }



    _Use_decl_annotations_
    bool MidiServiceTransportPluginConfigManager::QueryCapability(
        winrt::guid const& transportId,
        winrt::hstring const& capabilityQueryKey) noexcept
    {
        try
        {
            svc::MidiServiceTransportCommand cmd(transportId);
            cmd.Verb(svc::MidiServiceTransportCommonCommands::QueryCapabilities());

            auto serviceResponse = svc::MidiServiceTransportPluginConfigManager::SendCommand(cmd);

            if (serviceResponse.Status() == svc::MidiServiceConfigResponseStatus::Success)
            {
                auto capabilitiesJson = serviceResponse.ResponseJson().GetNamedObject(MIDI_CONFIG_JSON_TRANSPORT_COMMAND_QUERY_CAPABILITIES);

                if (capabilitiesJson.HasKey(capabilityQueryKey))
                {
                    auto supportsCapability = capabilitiesJson.GetNamedBoolean(capabilityQueryKey);

                    return supportsCapability;
                }
            }
            else
            {
                // transport doesn't support command query. 
            }
        }
        catch (winrt::hresult_error const& ex)
        {
            MIDI_SDK_LOG_HRESULT_EXCEPTION(nullptr, ex, L"hresult error from QueryCapability.");
        }
        catch (...)
        {
            MIDI_SDK_LOG_GENERAL_EXCEPTION(nullptr, L"General exception from QueryCapability.");
        }

        return false;
    }



    collections::IMapView<winrt::hstring, bool> MidiServiceTransportPluginConfigManager::QueryAllCapabilities(
        _In_ winrt::guid const& transportId)
    {
        auto capabilitiesMap = single_threaded_map<winrt::hstring, bool>();

        try
        {
            svc::MidiServiceTransportCommand cmd(transportId);
            cmd.Verb(svc::MidiServiceTransportCommonCommands::QueryCapabilities());

            auto serviceResponse = svc::MidiServiceTransportPluginConfigManager::SendCommand(cmd);

            if (serviceResponse.Status() == svc::MidiServiceConfigResponseStatus::Success)
            {
                auto capabilitiesJson = serviceResponse.ResponseJson().GetNamedObject(MIDI_CONFIG_JSON_TRANSPORT_COMMAND_QUERY_CAPABILITIES);

                for (auto const& capability : capabilitiesJson)
                {
                    auto key = capability.Key();
                    auto value = capability.Value().GetBoolean();

                    capabilitiesMap.Insert(key, value);
                }
            }
            else
            {
                // transport doesn't support command query. 
            }
        }
        catch (winrt::hresult_error const& ex)
        {
            MIDI_SDK_LOG_HRESULT_EXCEPTION(nullptr, ex, L"hresult error from QueryAllCapabilities.");
        }
        catch (...)
        {
            MIDI_SDK_LOG_GENERAL_EXCEPTION(nullptr, L"General exception from QueryAllCapabilities.");
        }


        return capabilitiesMap.GetView();

    }




}
