// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

#include "pch.h"


_Use_decl_annotations_
HRESULT
CMidi2LoopbackMidiConfigurationManager::Initialize(
    GUID transportId,
    IMidiDeviceManagerInterface* midiDeviceManager,
    IMidiServiceConfigurationManagerInterface* midiServiceConfigurationManagerInterface
)
{
    UNREFERENCED_PARAMETER(midiServiceConfigurationManagerInterface);


    TraceLoggingWrite(
        MidiLoopbackMidiTransportTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this")
    );

    RETURN_HR_IF_NULL(E_INVALIDARG, midiDeviceManager);
    RETURN_IF_FAILED(midiDeviceManager->QueryInterface(__uuidof(IMidiDeviceManagerInterface), (void**)&m_MidiDeviceManager));

    m_TransportId = transportId;

    return S_OK;
}

_Use_decl_annotations_
HRESULT
CMidi2LoopbackMidiConfigurationManager::UpdateConfiguration(
    LPCWSTR configurationJsonSection,
    LPWSTR* response
)
{
    TraceLoggingWrite(
        MidiLoopbackMidiTransportTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this"),
        TraceLoggingWideString(configurationJsonSection, "json")
    );

    // empty config is ok in this case. We just ignore
    if (configurationJsonSection == nullptr) return S_OK;


    json::JsonObject jsonObject{};

    // default to failure
    auto responseObject = internal::BuildConfigurationResponseObject(false);

    try
    {

        if (!json::JsonObject::TryParse(winrt::to_hstring(configurationJsonSection), jsonObject))
        {
            TraceLoggingWrite(
                MidiLoopbackMidiTransportTelemetryProvider::Provider(),
                MIDI_TRACE_EVENT_ERROR,
                TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
                TraceLoggingLevel(WINEVENT_LEVEL_ERROR),
                TraceLoggingPointer(this, "this"),
                TraceLoggingWideString(L"Failed to parse Configuration JSON", MIDI_TRACE_EVENT_MESSAGE_FIELD),
                TraceLoggingWideString(configurationJsonSection, "json")
            );

            internal::JsonStringifyObjectToOutParam(responseObject, response);

            return E_FAIL;
        }

        // I was tempted to call this the Prefix Code. KHAN!!
        //std::wstring instanceIdPrefixA = isFromConfigurationFile ? MIDI_PERM_LOOP_INSTANCE_ID_A_PREFIX : MIDI_TEMP_LOOP_INSTANCE_ID_A_PREFIX;
        //std::wstring instanceIdPrefixB = isFromConfigurationFile ? MIDI_PERM_LOOP_INSTANCE_ID_B_PREFIX : MIDI_TEMP_LOOP_INSTANCE_ID_B_PREFIX;

        std::wstring instanceIdPrefixA = MIDI_PERM_LOOP_INSTANCE_ID_A_PREFIX;
        std::wstring instanceIdPrefixB = MIDI_PERM_LOOP_INSTANCE_ID_B_PREFIX;

        // we should probably set a property based on this as well.

        auto createObject = jsonObject.GetNamedObject(MIDI_CONFIG_JSON_ENDPOINT_COMMON_CREATE_KEY, nullptr);

        // Create ----------------------------------

        if (createObject != nullptr && createObject.Size() > 0)
        {
            auto o = createObject.First();

            while (o.HasCurrent())
            {
                std::shared_ptr<MidiLoopbackDeviceDefinition> definitionA = std::make_shared<MidiLoopbackDeviceDefinition>();
                std::shared_ptr<MidiLoopbackDeviceDefinition> definitionB = std::make_shared<MidiLoopbackDeviceDefinition>();

                // get the association string (GUID) name
                auto associationKey = o.Current().Key();
                //json::JsonObject associationObj;

                auto associationObj = o.Current().Value().GetObject();
               
                if (associationObj)
                {
                    definitionA->AssociationId = associationKey;
                    definitionB->AssociationId = definitionA->AssociationId;


                    auto endpointAObject = associationObj.GetNamedObject(MIDI_CONFIG_JSON_ENDPOINT_LOOPBACK_DEVICE_ENDPOINT_A_KEY, nullptr);
                    auto endpointBObject = associationObj.GetNamedObject(MIDI_CONFIG_JSON_ENDPOINT_LOOPBACK_DEVICE_ENDPOINT_B_KEY, nullptr);

                    if (endpointAObject != nullptr && endpointBObject != nullptr)
                    {
                        // we don't look for user-specified names and descriptions here. There's no need to.

                        definitionA->EndpointName = endpointAObject.GetNamedString(MIDI_CONFIG_JSON_ENDPOINT_COMMON_NAME_PROPERTY, L"");
                        definitionA->EndpointDescription = endpointAObject.GetNamedString(MIDI_CONFIG_JSON_ENDPOINT_COMMON_DESCRIPTION_PROPERTY, L"");
                        definitionA->EndpointUniqueIdentifier = endpointAObject.GetNamedString(MIDI_CONFIG_JSON_ENDPOINT_COMMON_UNIQUE_ID_PROPERTY, L"");
                        definitionA->InstanceIdPrefix = instanceIdPrefixA;
                        definitionA->UMPOnly = endpointAObject.GetNamedBoolean(MIDI_CONFIG_JSON_ENDPOINT_COMMON_UMP_ONLY_PROPERTY, false);


                        definitionB->EndpointName = endpointBObject.GetNamedString(MIDI_CONFIG_JSON_ENDPOINT_COMMON_NAME_PROPERTY, L"");
                        definitionB->EndpointDescription = endpointBObject.GetNamedString(MIDI_CONFIG_JSON_ENDPOINT_COMMON_DESCRIPTION_PROPERTY, L"");
                        definitionB->EndpointUniqueIdentifier = endpointBObject.GetNamedString(MIDI_CONFIG_JSON_ENDPOINT_COMMON_UNIQUE_ID_PROPERTY, L"");
                        definitionB->InstanceIdPrefix = instanceIdPrefixB;
                        definitionB->UMPOnly = endpointBObject.GetNamedBoolean(MIDI_CONFIG_JSON_ENDPOINT_COMMON_UMP_ONLY_PROPERTY, false);


                        if (definitionA->EndpointName.empty() || definitionB->EndpointName.empty())
                        {
                            TraceLoggingWrite(
                                MidiLoopbackMidiTransportTelemetryProvider::Provider(),
                                MIDI_TRACE_EVENT_ERROR,
                                TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
                                TraceLoggingLevel(WINEVENT_LEVEL_ERROR),
                                TraceLoggingPointer(this, "this"),
                                TraceLoggingWideString(L"Endpoint name missing or empty", MIDI_TRACE_EVENT_MESSAGE_FIELD)
                            );

                            internal::JsonStringifyObjectToOutParam(responseObject, response);

                            return E_FAIL;
                        }


                        if (definitionA->EndpointUniqueIdentifier.empty() || definitionB->EndpointUniqueIdentifier.empty())
                        {
                            TraceLoggingWrite(
                                MidiLoopbackMidiTransportTelemetryProvider::Provider(),
                                MIDI_TRACE_EVENT_ERROR,
                                TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
                                TraceLoggingLevel(WINEVENT_LEVEL_ERROR),
                                TraceLoggingPointer(this, "this"),
                                TraceLoggingWideString(L"Unique identifier missing or empty", MIDI_TRACE_EVENT_MESSAGE_FIELD)
                            );

                            internal::JsonStringifyObjectToOutParam(responseObject, response);

                            return E_FAIL;
                        }


                        if (SUCCEEDED(TransportState::Current().GetEndpointManager()->CreateEndpointPair(definitionA, definitionB)))
                        {
                            TraceLoggingWrite(
                                MidiLoopbackMidiTransportTelemetryProvider::Provider(),
                                MIDI_TRACE_EVENT_INFO,
                                TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
                                TraceLoggingLevel(WINEVENT_LEVEL_INFO),
                                TraceLoggingPointer(this, "this"),
                                TraceLoggingWideString(L"Loopback endpoint pair created", MIDI_TRACE_EVENT_MESSAGE_FIELD)
                            );

                            // all good

                            auto successVal = json::JsonValue::CreateBooleanValue(true);
                            responseObject.SetNamedValue(
                                MIDI_CONFIG_JSON_CONFIGURATION_RESPONSE_SUCCESS_PROPERTY_KEY,
                                successVal);

                            // update the return json with the new Ids
                            auto endpointIdAVal = json::JsonValue::CreateStringValue(definitionA->CreatedEndpointInterfaceId);
                            responseObject.SetNamedValue(
                                MIDI_CONFIG_JSON_ENDPOINT_LOOPBACK_DEVICE_RESPONSE_CREATED_ENDPOINT_A_ID_KEY,
                                endpointIdAVal);

                            auto endpointIdBVal = json::JsonValue::CreateStringValue(definitionB->CreatedEndpointInterfaceId);
                            responseObject.SetNamedValue(
                                MIDI_CONFIG_JSON_ENDPOINT_LOOPBACK_DEVICE_RESPONSE_CREATED_ENDPOINT_B_ID_KEY,
                                endpointIdBVal);

                        }
                        else
                        {
                            // we failed to create the endpoints. Exit and return a fail.
                            TraceLoggingWrite(
                                MidiLoopbackMidiTransportTelemetryProvider::Provider(),
                                MIDI_TRACE_EVENT_ERROR,
                                TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
                                TraceLoggingLevel(WINEVENT_LEVEL_ERROR),
                                TraceLoggingPointer(this, "this"),
                                TraceLoggingWideString(L"Failed to create endpoints", MIDI_TRACE_EVENT_MESSAGE_FIELD)
                            );

                            internal::JsonStringifyObjectToOutParam(responseObject, response);

                            return E_FAIL;
                        }
                    }
                    else
                    {
                        // couldn't get the endpointA or endpointB objects. Exit and return a fail

                        TraceLoggingWrite(
                            MidiLoopbackMidiTransportTelemetryProvider::Provider(),
                            MIDI_TRACE_EVENT_ERROR,
                            TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
                            TraceLoggingLevel(WINEVENT_LEVEL_ERROR),
                            TraceLoggingPointer(this, "this"),
                            TraceLoggingWideString(associationKey.c_str(), "association key"),
                            TraceLoggingWideString(L"Failed to get one or both endpoints from the JSON", MIDI_TRACE_EVENT_MESSAGE_FIELD)
                        );

                        internal::JsonStringifyObjectToOutParam(responseObject, response);

                        return E_FAIL;

                    }
                }
                else
                {
                    TraceLoggingWrite(
                        MidiLoopbackMidiTransportTelemetryProvider::Provider(),
                        MIDI_TRACE_EVENT_ERROR,
                        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
                        TraceLoggingLevel(WINEVENT_LEVEL_ERROR),
                        TraceLoggingPointer(this, "this"),
                        TraceLoggingWideString(associationKey.c_str(), "association key"),
                        TraceLoggingWideString(L"Unable to convert association id property to a JsonObject", MIDI_TRACE_EVENT_MESSAGE_FIELD)
                    );

                    internal::JsonStringifyObjectToOutParam(responseObject, response);

                    return E_FAIL;
                }

                o.MoveNext();
            }
        }
        else
        {
            // nothing to create.
        }

        // update and delete are runtime operations, not config file operations

        auto deleteArray = jsonObject.GetNamedArray(MIDI_CONFIG_JSON_ENDPOINT_COMMON_REMOVE_KEY, nullptr);

        // Create ----------------------------------

        if (deleteArray != nullptr && deleteArray.Size() > 0)
        {
            auto o = deleteArray.First();

            while (o.HasCurrent())
            {
                // each entry is an association id

                auto associationId = o.Current().GetString();

                // if the entry was runtime-created and not from the config file, we can delete both endpoints 

                auto device = TransportState::Current().GetEndpointTable()->GetDevice(associationId.c_str());


                auto removalHr = TransportState::Current().GetEndpointManager()->DeleteEndpointPair(device->DefinitionA, device->DefinitionB);

                LOG_IF_FAILED(removalHr);

                if (SUCCEEDED(removalHr))
                {
                    TransportState::Current().GetEndpointTable()->RemoveDevice(associationId.c_str());

                    auto removeSuccessVal = json::JsonValue::CreateBooleanValue(true);
                    responseObject.SetNamedValue(
                        MIDI_CONFIG_JSON_CONFIGURATION_RESPONSE_SUCCESS_PROPERTY_KEY,
                        removeSuccessVal);
                }
                else
                {
                    // failed to remove device

                    TraceLoggingWrite(
                        MidiLoopbackMidiTransportTelemetryProvider::Provider(),
                        MIDI_TRACE_EVENT_ERROR,
                        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
                        TraceLoggingLevel(WINEVENT_LEVEL_ERROR),
                        TraceLoggingPointer(this, "this"),
                        TraceLoggingWideString(L"Failed to remove device", MIDI_TRACE_EVENT_MESSAGE_FIELD),
                        TraceLoggingWideString(configurationJsonSection, "json")
                    );
                }
                    


                o.MoveNext();
            }
        }

        //auto updateArray = internal::JsonGetArrayProperty(jsonObject, MIDI_CONFIG_JSON_ENDPOINT_LOOPBACK_DEVICES_UPDATE_KEY);

        //// Update ----------------------------------

        //if (updateArray.Size() > 0)
        //{
        //    // TODO
        //}
        //else
        //{
        //    // TODO : Update endpoints
        //}

        


        internal::JsonStringifyObjectToOutParam(responseObject, response);

        return S_OK;

    }
    catch (const std::exception& e)
    {
        TraceLoggingWrite(
            MidiLoopbackMidiTransportTelemetryProvider::Provider(),
            MIDI_TRACE_EVENT_ERROR,
            TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
            TraceLoggingLevel(WINEVENT_LEVEL_ERROR),
            TraceLoggingPointer(this, "this"),
            TraceLoggingWideString(L"std exception processing json", MIDI_TRACE_EVENT_MESSAGE_FIELD),
            TraceLoggingString(e.what(), "exception"),
            TraceLoggingWideString(configurationJsonSection, "json")
        );
    }
    catch (...)
    {
        TraceLoggingWrite(
            MidiLoopbackMidiTransportTelemetryProvider::Provider(),
            MIDI_TRACE_EVENT_ERROR,
            TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
            TraceLoggingLevel(WINEVENT_LEVEL_ERROR),
            TraceLoggingPointer(this, "this"),
            TraceLoggingWideString(L"Other exception processing json", MIDI_TRACE_EVENT_MESSAGE_FIELD),
            TraceLoggingWideString(configurationJsonSection, "json")
        );
    }

    // return the json with the information the client will need
    internal::JsonStringifyObjectToOutParam(responseObject, response);

    return S_OK;
}


HRESULT
CMidi2LoopbackMidiConfigurationManager::Shutdown()
{
    TraceLoggingWrite(
        MidiLoopbackMidiTransportTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this")
    );

    TransportState::Current().Shutdown();

    m_MidiDeviceManager.reset();

    return S_OK;
}

