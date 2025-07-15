// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App SDK and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================


#include "pch.h"
#include "MidiReporting.h"
#include "Reporting.MidiReporting.g.cpp"

namespace winrt::Microsoft::Windows::Devices::Midi2::Reporting::implementation
{
    foundation::Collections::IVector<rept::MidiServiceTransportPluginInfo> MidiReporting::GetInstalledTransportPlugins()
    {
        auto transportList = winrt::single_threaded_vector<rept::MidiServiceTransportPluginInfo>();

        try
        {
            winrt::com_ptr<IMidiTransport> serviceTransport;
            winrt::com_ptr<IMidiServicePluginMetadataReporter> metadataReporter;

            serviceTransport = winrt::create_instance<IMidiTransport>(__uuidof(Midi2MidiSrvTransport), CLSCTX_ALL);

            if (serviceTransport != nullptr)
            {


                if (SUCCEEDED(serviceTransport->Activate(__uuidof(IMidiServicePluginMetadataReporter), (void**)&metadataReporter)))
                {

                    LPWSTR rpcCallJson{ nullptr };
                    auto callStatus = metadataReporter->GetTransportList(&rpcCallJson);

                    if (SUCCEEDED(callStatus) && rpcCallJson != nullptr && wcslen(rpcCallJson) > 0)
                    {
                        winrt::hstring metadataListJsonString(rpcCallJson);

                        // parse it into json objects

                        if (metadataListJsonString.size() > 0)
                        {
                            // Parse the json, create the objects, throw them into the vector and return

                            json::JsonObject jsonObject = json::JsonObject::Parse(metadataListJsonString);

                            if (jsonObject != nullptr)
                            {
                                for (auto const& transportKV : jsonObject)
                                {
                                    rept::MidiServiceTransportPluginInfo info;

                                    auto transport = transportKV.Value().GetObject();

                                    info.Id = internal::StringToGuid(transportKV.Key().c_str());
                                    info.Name = transport.GetNamedString(MIDI_SERVICE_JSON_TRANSPORT_PLUGIN_INFO_NAME_PROPERTY_KEY, L"");
                                    info.TransportCode = transport.GetNamedString(MIDI_SERVICE_JSON_TRANSPORT_PLUGIN_INFO_TRANSPORT_CODE_PROPERTY_KEY, L"");
                                    info.Description = transport.GetNamedString(MIDI_SERVICE_JSON_TRANSPORT_PLUGIN_INFO_DESCRIPTION_PROPERTY_KEY, L"");
                                    info.SmallImagePath = transport.GetNamedString(MIDI_SERVICE_JSON_TRANSPORT_PLUGIN_INFO_SMALL_IMAGE_PATH_PROPERTY_KEY, L"");
                                    info.Author = transport.GetNamedString(MIDI_SERVICE_JSON_TRANSPORT_PLUGIN_INFO_AUTHOR_PROPERTY_KEY, L"");
                                    info.Version = transport.GetNamedString(MIDI_SERVICE_JSON_TRANSPORT_PLUGIN_INFO_VERSION_PROPERTY_KEY, L"");
                                    info.IsSystemManaged = transport.GetNamedBoolean(MIDI_SERVICE_JSON_TRANSPORT_PLUGIN_INFO_IS_SYSTEM_MANAGED_PROPERTY_KEY, false);
                                    info.IsRuntimeCreatableByApps = transport.GetNamedBoolean(MIDI_SERVICE_JSON_TRANSPORT_PLUGIN_INFO_IS_RT_CREATABLE_APPS_PROPERTY_KEY, false);
                                    info.IsRuntimeCreatableBySettings = transport.GetNamedBoolean(MIDI_SERVICE_JSON_TRANSPORT_PLUGIN_INFO_IS_RT_CREATABLE_SETTINGS_PROPERTY_KEY, false);
                                    info.CanConfigure = transport.GetNamedBoolean(MIDI_SERVICE_JSON_TRANSPORT_PLUGIN_INFO_IS_CLIENT_CONFIGURABLE_PROPERTY_KEY, false);

                                    //transport.GetNamedString(MIDI_SERVICE_JSON_TRANSPORT_PLUGIN_INFO_CLIENT_CONFIG_ASSEMBLY_PROPERTY_KEY, L"");

                                    transportList.Append(std::move(info));
                                }
                            }
                        }

                        SAFE_COTASKMEMFREE(rpcCallJson);
                    }
                    else
                    {
                        LOG_IF_FAILED(callStatus);

                        TraceLoggingWrite(
                            Midi2SdkTelemetryProvider::Provider(),
                            MIDI_SDK_TRACE_EVENT_ERROR,
                            TraceLoggingString(__FUNCTION__, MIDI_SDK_TRACE_LOCATION_FIELD),
                            TraceLoggingLevel(WINEVENT_LEVEL_ERROR),
                            TraceLoggingWideString(MIDI_SDK_STATIC_THIS_PLACEHOLDER_FIELD_VALUE, MIDI_SDK_TRACE_THIS_FIELD),
                            TraceLoggingWideString(L"Failed to call service function.", MIDI_SDK_TRACE_MESSAGE_FIELD),
                            TraceLoggingHResult(callStatus, "hresult")
                        );
                    }
                }
            }
        }
        catch (...)
        {
            LOG_IF_FAILED(E_FAIL);

            TraceLoggingWrite(
                Midi2SdkTelemetryProvider::Provider(),
                MIDI_SDK_TRACE_EVENT_ERROR,
                TraceLoggingString(__FUNCTION__, MIDI_SDK_TRACE_LOCATION_FIELD),
                TraceLoggingLevel(WINEVENT_LEVEL_ERROR),
                TraceLoggingWideString(MIDI_SDK_STATIC_THIS_PLACEHOLDER_FIELD_VALUE, MIDI_SDK_TRACE_THIS_FIELD),
                TraceLoggingWideString(L"Exception processing session tracker result json", MIDI_SDK_TRACE_MESSAGE_FIELD)
            );
        }

        return transportList;
    }


    //foundation::Collections::IVector<rept::MidiServiceMessageProcessingPluginInfo> MidiReporting::GetInstalledMessageProcessingPlugins()
    //{

    //    // TODO: Need to implement GetInstalledMessageProcessingPlugins. For now, return an empty collection instead of throwing

    //    // This can be read from the registry, but the additional metadata requires calling into the objects themselves

    //    return winrt::single_threaded_vector<rept::MidiServiceMessageProcessingPluginInfo>();
    //}


    foundation::Collections::IVector<rept::MidiServiceSessionInfo> MidiReporting::GetActiveSessions()
    {
        auto sessionList = winrt::single_threaded_vector<rept::MidiServiceSessionInfo>();

        try
        {
            winrt::com_ptr<IMidiTransport> serviceTransport;
            winrt::com_ptr<IMidiSessionTracker> sessionTracker;

            serviceTransport = winrt::create_instance<IMidiTransport>(__uuidof(Midi2MidiSrvTransport), CLSCTX_ALL);

            if (serviceTransport == nullptr)
            {
                return sessionList;
            }

            if (SUCCEEDED(serviceTransport->Activate(__uuidof(IMidiSessionTracker), (void**)&sessionTracker)))
            {
                if (FAILED(sessionTracker->Initialize()))
                {
                    TraceLoggingWrite(
                        Midi2SdkTelemetryProvider::Provider(),
                        MIDI_SDK_TRACE_EVENT_ERROR,
                        TraceLoggingString(__FUNCTION__, MIDI_SDK_TRACE_LOCATION_FIELD),
                        TraceLoggingLevel(WINEVENT_LEVEL_ERROR),
                        TraceLoggingWideString(MIDI_SDK_STATIC_THIS_PLACEHOLDER_FIELD_VALUE, MIDI_SDK_TRACE_THIS_FIELD),
                        TraceLoggingWideString(L"Failed to initialize session tracker.", MIDI_SDK_TRACE_MESSAGE_FIELD)
                    );

                    return sessionList;
                }
            }
            else
            {
                TraceLoggingWrite(
                    Midi2SdkTelemetryProvider::Provider(),
                    MIDI_SDK_TRACE_EVENT_ERROR,
                    TraceLoggingString(__FUNCTION__, MIDI_SDK_TRACE_LOCATION_FIELD),
                    TraceLoggingLevel(WINEVENT_LEVEL_ERROR),
                    TraceLoggingWideString(MIDI_SDK_STATIC_THIS_PLACEHOLDER_FIELD_VALUE, MIDI_SDK_TRACE_THIS_FIELD),
                    TraceLoggingWideString(L"Failed to activate session tracker.", MIDI_SDK_TRACE_MESSAGE_FIELD)
                );

                return sessionList;
            }


            LPWSTR rpcSessionListJson{ nullptr };
            auto callStatus = sessionTracker->GetSessionList(&rpcSessionListJson);

            // parse it into json objects

            if (SUCCEEDED(callStatus) && rpcSessionListJson != nullptr && wcslen(rpcSessionListJson) > 0)
            {
                winrt::hstring hstr(rpcSessionListJson);

                // Parse the json, create the objects, throw them into the vector and return

                json::JsonObject jsonObject = json::JsonObject::Parse(hstr);

                if (jsonObject != nullptr)
                {
                    auto sessionJsonArray = jsonObject.GetNamedArray(MIDI_SESSION_TRACKER_JSON_RESULT_SESSION_ARRAY_PROPERTY_KEY);

                    GUID defaultGuid{};
                    std::chrono::time_point<std::chrono::system_clock> noTime;

                    for (uint32_t i = 0; i < sessionJsonArray.Size(); i++)
                    {
                        auto sessionJson = sessionJsonArray.GetObjectAt(i);
                        auto sessionObject = winrt::make_self<rept::implementation::MidiServiceSessionInfo>();

                        //    auto startTimeString = internal::JsonGetWStringProperty(sessionJson, MIDI_SESSION_TRACKER_JSON_RESULT_SESSION_TIME_PROPERTY_KEY, L"").c_str();

                        auto startTime = internal::JsonGetDateTimeProperty(sessionJson, MIDI_SESSION_TRACKER_JSON_RESULT_SESSION_TIME_PROPERTY_KEY, noTime);

                        sessionObject->InternalInitialize(
                            internal::JsonGetGuidProperty(sessionJson, MIDI_SESSION_TRACKER_JSON_RESULT_SESSION_ID_PROPERTY_KEY, defaultGuid),
                            sessionJson.GetNamedString(MIDI_SESSION_TRACKER_JSON_RESULT_SESSION_NAME_PROPERTY_KEY, L"").c_str(),
                            std::stol(sessionJson.GetNamedString(MIDI_SESSION_TRACKER_JSON_RESULT_PROCESS_ID_PROPERTY_KEY, L"0").c_str()),
                            sessionJson.GetNamedString(MIDI_SESSION_TRACKER_JSON_RESULT_PROCESS_NAME_PROPERTY_KEY, L"").c_str(),
                            winrt::clock::from_sys(startTime)
                        );


                        // Add connections

                        auto connectionsJsonArray = sessionJson.GetNamedArray(MIDI_SESSION_TRACKER_JSON_RESULT_CONNECTION_ARRAY_PROPERTY_KEY, nullptr);

                        if (connectionsJsonArray != nullptr && connectionsJsonArray.Size() > 0)
                        {
                            for (uint32_t j = 0; j < connectionsJsonArray.Size(); j++)
                            {
                                auto connectionJson = connectionsJsonArray.GetObjectAt(j);
                                rept::MidiServiceSessionConnectionInfo connectionObject;

                                auto earliestConnectionTime = internal::JsonGetDateTimeProperty(connectionJson, MIDI_SESSION_TRACKER_JSON_RESULT_CONNECTION_TIME_PROPERTY_KEY, noTime);

                                connectionObject.EndpointDeviceId = connectionJson.GetNamedString(MIDI_SESSION_TRACKER_JSON_RESULT_CONNECTION_ENDPOINT_ID_PROPERTY_KEY, L"");
                                connectionObject.InstanceCount = (uint16_t)(connectionJson.GetNamedNumber(MIDI_SESSION_TRACKER_JSON_RESULT_CONNECTION_COUNT_PROPERTY_KEY, 0));
                                connectionObject.EarliestConnectionTime = winrt::clock::from_sys(earliestConnectionTime);

                                sessionObject->InternalAddConnection(connectionObject);
                            }
                        }

                        sessionList.Append(*sessionObject);
                    }
                }

                SAFE_COTASKMEMFREE(rpcSessionListJson);
            }


        }
        catch (...)
        {
            LOG_IF_FAILED(E_FAIL);

            TraceLoggingWrite(
                Midi2SdkTelemetryProvider::Provider(),
                MIDI_SDK_TRACE_EVENT_ERROR,
                TraceLoggingString(__FUNCTION__, MIDI_SDK_TRACE_LOCATION_FIELD),
                TraceLoggingLevel(WINEVENT_LEVEL_ERROR),
                TraceLoggingWideString(MIDI_SDK_STATIC_THIS_PLACEHOLDER_FIELD_VALUE, MIDI_SDK_TRACE_THIS_FIELD),
                TraceLoggingWideString(L"Exception processing session tracker result json", MIDI_SDK_TRACE_MESSAGE_FIELD)
            );
        }

        return sessionList;

    }
}
