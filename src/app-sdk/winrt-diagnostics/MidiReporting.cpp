// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

#include "pch.h"
#include "MidiReporting.h"
#include "MidiReporting.g.cpp"

namespace winrt::Microsoft::Windows::Devices::Midi2::Diagnostics::implementation
{
    foundation::Collections::IVector<diag::MidiServiceTransportPluginInfo> MidiReporting::GetInstalledTransportPlugins()
    {
        auto transportList = winrt::single_threaded_vector<diag::MidiServiceTransportPluginInfo>();

        try
        {
            winrt::com_ptr<IMidiAbstraction> serviceAbstraction;
            winrt::com_ptr<IMidiServicePluginMetadataReporterInterface> metadataReporter;

            serviceAbstraction = winrt::create_instance<IMidiAbstraction>(__uuidof(Midi2MidiSrvAbstraction), CLSCTX_ALL);

            if (serviceAbstraction != nullptr)
            {
                if (SUCCEEDED(serviceAbstraction->Activate(__uuidof(IMidiServicePluginMetadataReporterInterface), (void**)&metadataReporter)))
                {
                    CComBSTR metadataListJson;
                    metadataListJson.Empty();

                    metadataReporter->GetAbstractionList(&metadataListJson);

                    // parse it into json objects

                    if (metadataListJson.m_str != nullptr && metadataListJson.Length() > 0)
                    {
                        winrt::hstring hstr(metadataListJson, metadataListJson.Length());

                        // Parse the json, create the objects, throw them into the vector and return

                        json::JsonObject jsonObject = json::JsonObject::Parse(hstr);

                        if (jsonObject != nullptr)
                        {
                            for (auto const& transportKV : jsonObject)
                            {
                                diag::MidiServiceTransportPluginInfo info;

                                auto transport = transportKV.Value().GetObject();

                                info.Id = internal::StringToGuid(transportKV.Key().c_str());
                                info.Name = transport.GetNamedString(MIDI_SERVICE_JSON_ABSTRACTION_PLUGIN_INFO_NAME_PROPERTY_KEY, L"");
                                info.Mnemonic = transport.GetNamedString(MIDI_SERVICE_JSON_ABSTRACTION_PLUGIN_INFO_MNEMONIC_PROPERTY_KEY, L"");
                                info.Description = transport.GetNamedString(MIDI_SERVICE_JSON_ABSTRACTION_PLUGIN_INFO_DESCRIPTION_PROPERTY_KEY, L"");
                                info.SmallImagePath = transport.GetNamedString(MIDI_SERVICE_JSON_ABSTRACTION_PLUGIN_INFO_SMALL_IMAGE_PATH_PROPERTY_KEY, L"");
                                info.Author = transport.GetNamedString(MIDI_SERVICE_JSON_ABSTRACTION_PLUGIN_INFO_AUTHOR_PROPERTY_KEY, L"");
                                info.Version = transport.GetNamedString(MIDI_SERVICE_JSON_ABSTRACTION_PLUGIN_INFO_VERSION_PROPERTY_KEY, L"");
                                info.IsSystemManaged = transport.GetNamedBoolean(MIDI_SERVICE_JSON_ABSTRACTION_PLUGIN_INFO_IS_SYSTEM_MANAGED_PROPERTY_KEY, false);
                                info.IsRuntimeCreatableByApps = transport.GetNamedBoolean(MIDI_SERVICE_JSON_ABSTRACTION_PLUGIN_INFO_IS_RT_CREATABLE_APPS_PROPERTY_KEY, false);
                                info.IsRuntimeCreatableBySettings = transport.GetNamedBoolean(MIDI_SERVICE_JSON_ABSTRACTION_PLUGIN_INFO_IS_RT_CREATABLE_SETTINGS_PROPERTY_KEY, false);
                                info.CanConfigure = transport.GetNamedBoolean(MIDI_SERVICE_JSON_ABSTRACTION_PLUGIN_INFO_IS_CLIENT_CONFIGURABLE_PROPERTY_KEY, false);

                                //transport.GetNamedString(MIDI_SERVICE_JSON_ABSTRACTION_PLUGIN_INFO_CLIENT_CONFIG_ASSEMBLY_PROPERTY_KEY, L"");

                                transportList.Append(std::move(info));
                            }
                        }
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


    foundation::Collections::IVector<diag::MidiServiceMessageProcessingPluginInfo> MidiReporting::GetInstalledMessageProcessingPlugins()
    {

        // TODO: Need to implement GetInstalledMessageProcessingPlugins. For now, return an empty collection instead of throwing

        // This can be read from the registry, but the additional metadata requires calling into the objects themselves

        return winrt::single_threaded_vector<diag::MidiServiceMessageProcessingPluginInfo>();
    }


    foundation::Collections::IVector<diag::MidiServiceSessionInfo> MidiReporting::GetActiveSessions()
    {
        auto sessionList = winrt::single_threaded_vector<diag::MidiServiceSessionInfo>();

        //try
        //{
        //    winrt::com_ptr<IMidiAbstraction> serviceAbstraction;
        //    winrt::com_ptr<IMidiSessionTracker> sessionTracker;

        //    serviceAbstraction = winrt::create_instance<IMidiAbstraction>(__uuidof(Midi2MidiSrvAbstraction), CLSCTX_ALL);

        //    if (serviceAbstraction != nullptr)
        //    {
        //        if (SUCCEEDED(serviceAbstraction->Activate(__uuidof(IMidiSessionTracker), (void**)&sessionTracker)))
        //        {
        //            CComBSTR sessionListJson;
        //            sessionListJson.Empty();

        //            sessionTracker->GetSessionListJson(&sessionListJson);

        //            // parse it into json objects

        //            if (sessionListJson.m_str != nullptr && sessionListJson.Length() > 0)
        //            {
        //                winrt::hstring hstr(sessionListJson, sessionListJson.Length());

        //                // Parse the json, create the objects, throw them into the vector and return

        //                json::JsonObject jsonObject = json::JsonObject::Parse(hstr);

        //                if (jsonObject != nullptr)
        //                {
        //                    auto sessionJsonArray = jsonObject.GetNamedArray(MIDI_SESSION_TRACKER_JSON_RESULT_SESSION_ARRAY_PROPERTY_KEY);

        //                    GUID defaultGuid{};
        //                    std::chrono::time_point<std::chrono::system_clock> noTime;

        //                    for (uint32_t i = 0; i < sessionJsonArray.Size(); i++)
        //                    {
        //                        auto sessionJson = sessionJsonArray.GetObjectAt(i);
        //                        auto sessionObject = winrt::make_self<implementation::MidiServiceSessionInfo>();

        //                        //    auto startTimeString = internal::JsonGetWStringProperty(sessionJson, MIDI_SESSION_TRACKER_JSON_RESULT_SESSION_TIME_PROPERTY_KEY, L"").c_str();

        //                        auto startTime = internal::JsonGetDateTimeProperty(sessionJson, MIDI_SESSION_TRACKER_JSON_RESULT_SESSION_TIME_PROPERTY_KEY, noTime);

        //                        sessionObject->InternalInitialize(
        //                            internal::JsonGetGuidProperty(sessionJson, MIDI_SESSION_TRACKER_JSON_RESULT_SESSION_ID_PROPERTY_KEY, defaultGuid),
        //                            sessionJson.GetNamedString(MIDI_SESSION_TRACKER_JSON_RESULT_SESSION_NAME_PROPERTY_KEY, L"").c_str(),
        //                            std::stol(sessionJson.GetNamedString(MIDI_SESSION_TRACKER_JSON_RESULT_PROCESS_ID_PROPERTY_KEY, L"0").c_str()),
        //                            sessionJson.GetNamedString(MIDI_SESSION_TRACKER_JSON_RESULT_PROCESS_NAME_PROPERTY_KEY, L"").c_str(),
        //                            winrt::clock::from_sys(startTime)
        //                        );


        //                        // Add connections

        //                        auto connectionsJsonArray = sessionJson.GetNamedArray(MIDI_SESSION_TRACKER_JSON_RESULT_CONNECTION_ARRAY_PROPERTY_KEY, nullptr);

        //                        if (connectionsJsonArray != nullptr && connectionsJsonArray.Size() > 0)
        //                        {
        //                            for (uint32_t j = 0; j < connectionsJsonArray.Size(); j++)
        //                            {
        //                                auto connectionJson = connectionsJsonArray.GetObjectAt(j);
        //                                auto connectionObject = winrt::make_self<implementation::MidiServiceSessionConnectionInfo>();

        //                                auto earliestConnectionTime = internal::JsonGetDateTimeProperty(connectionJson, MIDI_SESSION_TRACKER_JSON_RESULT_CONNECTION_TIME_PROPERTY_KEY, noTime);

        //                                connectionObject->InternalInitialize(
        //                                    connectionJson.GetNamedString(MIDI_SESSION_TRACKER_JSON_RESULT_CONNECTION_ENDPOINT_ID_PROPERTY_KEY, L"").c_str(),
        //                                    (uint16_t)(connectionJson.GetNamedNumber(MIDI_SESSION_TRACKER_JSON_RESULT_CONNECTION_COUNT_PROPERTY_KEY, 0)),
        //                                    winrt::clock::from_sys(earliestConnectionTime)
        //                                );

        //                                sessionObject->InternalAddConnection(*connectionObject);
        //                            }
        //                        }

        //                        sessionList.Append(*sessionObject);
        //                    }
        //                }
        //            }
        //        }
        //    }
        //}
        //catch (...)
        //{
        //    internal::LogGeneralError(__FUNCTION__, L"Exception processing session tracker result json");
        //}

        return sessionList;

    }
}
