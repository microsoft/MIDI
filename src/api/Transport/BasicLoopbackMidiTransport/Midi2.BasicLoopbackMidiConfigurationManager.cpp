// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

#include "pch.h"

#include "MidiEndpointCustomProperties.h"
#include "json_transport_command_helper.h"
#include <mmdeviceapi.h>

_Use_decl_annotations_
HRESULT
CMidi2BasicLoopbackMidiConfigurationManager::Initialize(
    GUID transportId,
    IMidiDeviceManager* midiDeviceManager,
    IMidiServiceConfigurationManager* midiServiceConfigurationManagerInterface
)
{
    UNREFERENCED_PARAMETER(midiServiceConfigurationManagerInterface);


    TraceLoggingWrite(
        MidiBasicLoopbackMidiTransportTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this")
    );

    RETURN_HR_IF_NULL(E_INVALIDARG, midiDeviceManager);
    RETURN_IF_FAILED(midiDeviceManager->QueryInterface(__uuidof(IMidiDeviceManager), (void**)&m_MidiDeviceManager));

    m_TransportId = transportId;

    return S_OK;
}


_Use_decl_annotations_
HRESULT
CMidi2BasicLoopbackMidiConfigurationManager::ExecuteCommandChangeMutedState(
    std::wstring const& associationId,
    bool const isMuted)
{
    RETURN_HR_IF_NULL(E_UNEXPECTED, TransportState::Current().GetEndpointTable());

    auto device = TransportState::Current().GetEndpointTable()->GetDevice(associationId);
    RETURN_HR_IF_NULL(E_NOTFOUND, device);

    RETURN_HR_IF_NULL(E_UNEXPECTED, TransportState::Current().GetEndpointManager());

    device->Definition.IsMuted = isMuted;
    RETURN_IF_FAILED(TransportState::Current().GetEndpointManager()->UpdateEndpointMutedStateProperty(device->Definition));

    return S_OK;
}


_Use_decl_annotations_
HRESULT
CMidi2BasicLoopbackMidiConfigurationManager::ProcessCommand(
    json::JsonObject const& transportObject,
    json::JsonObject& responseObject)
{
    auto commandHelper = internal::MidiTransportCommandHelper::ParseCommand(transportObject);

    if (commandHelper.Command().empty())
    {
        internal::SetConfigurationResponseObjectFail(responseObject, L"Missing command.");

        // we S_OK this because the response object is valid and should be read
    }
    else if (commandHelper.Command() == MIDI_CONFIG_JSON_TRANSPORT_COMMAND_QUERY_CAPABILITIES)
    {
        std::map<std::wstring, bool> capabilities{};

        capabilities.emplace(MIDI_CONFIG_JSON_TRANSPORT_COMMAND_CAPABILITY_CUSTOMIZE_ENDPOINT, false);
        capabilities.emplace(MIDI_CONFIG_JSON_TRANSPORT_COMMAND_CAPABILITY_CUSTOMIZE_PORTS, false);

        // revisit these once the functions are added in
        capabilities.emplace(MIDI_CONFIG_JSON_TRANSPORT_COMMAND_CAPABILITY_RESTART_ENDPOINT, false);
        capabilities.emplace(MIDI_CONFIG_JSON_TRANSPORT_COMMAND_CAPABILITY_DISCONNECT_ENDPOINT, false);
        capabilities.emplace(MIDI_CONFIG_JSON_TRANSPORT_COMMAND_CAPABILITY_RECONNECT_ENDPOINT, false);
        capabilities.emplace(MIDI_CONFIG_JSON_TRANSPORT_COMMAND_CAPABILITY_MUTE_ENDPOINT, true);

        internal::SetConfigurationResponseObjectSuccess(responseObject);
        internal::SetConfigurationCommandResponseQueryCapabilities(responseObject, capabilities);
    }
    else if (commandHelper.Command() == MIDI_CONFIG_JSON_TRANSPORT_COMMAND_MUTE_ENDPOINT ||
        commandHelper.Command() == MIDI_CONFIG_JSON_TRANSPORT_COMMAND_UNMUTE_ENDPOINT)
    {
        bool mute = (commandHelper.Command() == MIDI_CONFIG_JSON_TRANSPORT_COMMAND_MUTE_ENDPOINT);

        // Check to see if we have an endpointdeviceid
        if (auto arg = commandHelper.Arguments()->find(MIDI_CONFIG_JSON_TRANSPORT_COMMAND_COMMON_PARAMETER_ENDPOINT_ASSOCIATION_ID); 
            arg != commandHelper.Arguments()->end())
        {
            auto hr = ExecuteCommandChangeMutedState(arg->second, mute);

            if (hr == E_NOTFOUND)
            {
                internal::SetConfigurationResponseObjectFail(responseObject, L"Endpoint not found");
            }
            else if (SUCCEEDED(hr))
            {
                internal::SetConfigurationResponseObjectSuccess(responseObject);
            }
            else
            {
                RETURN_IF_FAILED(hr);
            }
        }
        else
        {
            // no endpoint id
            internal::SetConfigurationResponseObjectFail(responseObject, L"Missing association id");
        }

    }
    else
    {
        internal::SetConfigurationResponseObjectFail(responseObject, L"Unrecognized command.");
    }

    // we return S_OK no matter what, so the response object will be parsed
    return S_OK;
}


_Use_decl_annotations_
HRESULT
CMidi2BasicLoopbackMidiConfigurationManager::UpdateConfiguration(
    LPCWSTR configurationJsonSection,
    LPWSTR* response
)
{
    TraceLoggingWrite(
        MidiBasicLoopbackMidiTransportTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this"),
        TraceLoggingWideString(configurationJsonSection, "json")
    );

    RETURN_HR_IF_NULL(E_POINTER, m_MidiDeviceManager);

    // empty config is ok in this case. We just ignore
    if (configurationJsonSection == nullptr) return S_OK;

    // use this to track any ids in use from this one config file update
    std::map<std::wstring, bool> allocatedUniqueIds{};

    // default to failure
    auto responseObject = internal::BuildConfigurationResponseObject(false);

    try
    {
        json::JsonObject jsonObject{};

        if (!json::JsonObject::TryParse(winrt::to_hstring(configurationJsonSection), jsonObject))
        {
            TraceLoggingWrite(
                MidiBasicLoopbackMidiTransportTelemetryProvider::Provider(),
                MIDI_TRACE_EVENT_ERROR,
                TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
                TraceLoggingLevel(WINEVENT_LEVEL_ERROR),
                TraceLoggingPointer(this, "this"),
                TraceLoggingWideString(L"Failed to parse Configuration JSON", MIDI_TRACE_EVENT_MESSAGE_FIELD)
            );

            //internal::JsonStringifyObjectToOutParam(responseObject, &response);

            RETURN_IF_FAILED(E_INVALIDARG);
        }


        // command. If there's a command in the payload, we ignore anything else
        if (internal::MidiTransportCommandHelper::TransportObjectContainsCommand(jsonObject))
        {
            auto hr = ProcessCommand(jsonObject, responseObject);

            internal::JsonStringifyObjectToOutParam(responseObject, response);

            return hr;
        }

        std::wstring instanceIdPrefix = MIDI_BASIC_LOOP_INSTANCE_ID_PREFIX;

        // we should probably set a property based on this as well.

        auto createObject = jsonObject.GetNamedObject(MIDI_CONFIG_JSON_ENDPOINT_COMMON_CREATE_KEY, nullptr);

        // Create ----------------------------------

        if (createObject != nullptr && createObject.Size() > 0)
        {
            auto o = createObject.First();

            while (o.HasCurrent())
            {
                std::shared_ptr<MidiBasicLoopbackDeviceDefinition> definition = std::make_shared<MidiBasicLoopbackDeviceDefinition>();

                // get the association string (GUID) name
                auto associationKey = o.Current().Key();

                auto associationObj = o.Current().Value().GetObject();
               
                if (associationObj)
                {
                    definition->AssociationId = associationKey;

                    auto endpointObject = associationObj.GetNamedObject(MIDI_CONFIG_JSON_ENDPOINT_BASIC_LOOPBACK_DEVICE_ENDPOINT_KEY, nullptr);

                    if (endpointObject != nullptr)
                    {
                        // we don't look for user-specified names and descriptions here. There's no need to.

                        definition->EndpointName = endpointObject.GetNamedString(MIDI_CONFIG_JSON_ENDPOINT_COMMON_NAME_PROPERTY, L"");
                        definition->EndpointDescription = endpointObject.GetNamedString(MIDI_CONFIG_JSON_ENDPOINT_COMMON_DESCRIPTION_PROPERTY, L"");
                        definition->EndpointUniqueIdentifier = endpointObject.GetNamedString(MIDI_CONFIG_JSON_ENDPOINT_COMMON_UNIQUE_ID_PROPERTY, L"");
                        definition->InstanceIdPrefix = instanceIdPrefix;
                        definition->IsMuted = endpointObject.GetNamedBoolean(MIDI_CONFIG_JSON_ENDPOINT_COMMON_MUTED_PROPERTY, false);

                        if (definition->EndpointName.empty())
                        {
                            TraceLoggingWrite(
                                MidiBasicLoopbackMidiTransportTelemetryProvider::Provider(),
                                MIDI_TRACE_EVENT_ERROR,
                                TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
                                TraceLoggingLevel(WINEVENT_LEVEL_ERROR),
                                TraceLoggingPointer(this, "this"),
                                TraceLoggingWideString(L"Endpoint name missing or empty", MIDI_TRACE_EVENT_MESSAGE_FIELD)
                            );

                            responseObject.SetNamedValue(MIDI_CONFIG_JSON_CONFIGURATION_RESPONSE_MESSAGE_PROPERTY_KEY,
                                json::JsonValue::CreateStringValue(internal::ResourceGetHString(IDS_ERROR_MISSING_NAME)));

                            internal::JsonStringifyObjectToOutParam(responseObject, response);

                            return E_FAIL;
                        }


                        if (definition->EndpointUniqueIdentifier.empty())
                        {
                            TraceLoggingWrite(
                                MidiBasicLoopbackMidiTransportTelemetryProvider::Provider(),
                                MIDI_TRACE_EVENT_ERROR,
                                TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
                                TraceLoggingLevel(WINEVENT_LEVEL_ERROR),
                                TraceLoggingPointer(this, "this"),
                                TraceLoggingWideString(L"Unique identifier missing or empty", MIDI_TRACE_EVENT_MESSAGE_FIELD)
                            );

                            responseObject.SetNamedValue(MIDI_CONFIG_JSON_CONFIGURATION_RESPONSE_MESSAGE_PROPERTY_KEY,
                                json::JsonValue::CreateStringValue(internal::ResourceGetHString(IDS_ERROR_MISSING_UNIQUE_ID)));

                            internal::JsonStringifyObjectToOutParam(responseObject, response);

                            return E_FAIL;
                        }


                        // check for an identifier already in use. Failure to do this results in crashing the 
                        // service due to issues creating the device
                        if (allocatedUniqueIds.find(definition->EndpointUniqueIdentifier) != allocatedUniqueIds.end() ||
                            TransportState::Current().GetEndpointTable()->IsUniqueIdentifierInUseForLoopback(definition->EndpointUniqueIdentifier))
                        {
                            TraceLoggingWrite(
                                MidiBasicLoopbackMidiTransportTelemetryProvider::Provider(),
                                MIDI_TRACE_EVENT_ERROR,
                                TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
                                TraceLoggingLevel(WINEVENT_LEVEL_ERROR),
                                TraceLoggingPointer(this, "this"),
                                TraceLoggingWideString(L"Unique identifier for Loopback Definition A already in use", MIDI_TRACE_EVENT_MESSAGE_FIELD),
                                TraceLoggingWideString(definition->EndpointUniqueIdentifier.c_str(), "identifier")
                            );

                            responseObject.SetNamedValue(MIDI_CONFIG_JSON_CONFIGURATION_RESPONSE_MESSAGE_PROPERTY_KEY,
                                json::JsonValue::CreateStringValue(internal::ResourceGetHString(IDS_ERROR_DUPLICATE_UNIQUE_ID)));

                            internal::JsonStringifyObjectToOutParam(responseObject, response);

                            return E_FAIL;
                        }
                        allocatedUniqueIds.emplace(definition->EndpointUniqueIdentifier, true);

                        if (TransportState::Current().GetEndpointManager() != nullptr && 
                            TransportState::Current().GetEndpointManager()->IsInitialized())
                        {
                            // endpoint manager is initialized, so do the full creation so we can return the result
                            // This happens when the SDK is used to create the endpoints

                            if (SUCCEEDED(TransportState::Current().GetEndpointManager()->CreateEndpoint(definition)))
                            {
                                TraceLoggingWrite(
                                    MidiBasicLoopbackMidiTransportTelemetryProvider::Provider(),
                                    MIDI_TRACE_EVENT_INFO,
                                    TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
                                    TraceLoggingLevel(WINEVENT_LEVEL_INFO),
                                    TraceLoggingPointer(this, "this"),
                                    TraceLoggingWideString(L"Loopback endpoint created", MIDI_TRACE_EVENT_MESSAGE_FIELD)
                                );

                                // all good

                                auto successVal = json::JsonValue::CreateBooleanValue(true);
                                responseObject.SetNamedValue(
                                    MIDI_CONFIG_JSON_CONFIGURATION_RESPONSE_SUCCESS_PROPERTY_KEY,
                                    successVal);

                                // update the return json with the new Ids
                                auto endpointIdAVal = json::JsonValue::CreateStringValue(definition->CreatedEndpointInterfaceId);
                                responseObject.SetNamedValue(
                                    MIDI_CONFIG_JSON_ENDPOINT_BASIC_LOOPBACK_DEVICE_RESPONSE_CREATED_ENDPOINT_ID_KEY,
                                    endpointIdAVal);

                            }
                            else
                            {
                                // we failed to create the endpoints. Exit and return a fail.
                                TraceLoggingWrite(
                                    MidiBasicLoopbackMidiTransportTelemetryProvider::Provider(),
                                    MIDI_TRACE_EVENT_ERROR,
                                    TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
                                    TraceLoggingLevel(WINEVENT_LEVEL_ERROR),
                                    TraceLoggingPointer(this, "this"),
                                    TraceLoggingWideString(L"Failed to create endpoints", MIDI_TRACE_EVENT_MESSAGE_FIELD)
                                );

                                responseObject.SetNamedValue(MIDI_CONFIG_JSON_CONFIGURATION_RESPONSE_MESSAGE_PROPERTY_KEY,
                                    json::JsonValue::CreateStringValue(internal::ResourceGetHString(IDS_ERROR_CREATION_FAILED)));

                                internal::JsonStringifyObjectToOutParam(responseObject, response);

                                return E_FAIL;
                            }

                        }
                        else
                        {
                            // endpoint manager has not yet been initialized, so queue the work for later processing.
                            // This happens when the service starts up and the endpoints are in the config file
                            // rather than coming from an SDK call.

                            RETURN_IF_FAILED(TransportState::Current().GetEndpointWorkQueue()->CreateEndpoint(definition));
                        }

                    }
                    else
                    {
                        // couldn't get the endpoint object. Exit and return a fail

                        TraceLoggingWrite(
                            MidiBasicLoopbackMidiTransportTelemetryProvider::Provider(),
                            MIDI_TRACE_EVENT_ERROR,
                            TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
                            TraceLoggingLevel(WINEVENT_LEVEL_ERROR),
                            TraceLoggingPointer(this, "this"),
                            TraceLoggingWideString(associationKey.c_str(), "association key"),
                            TraceLoggingWideString(L"Failed to get endpoint from the JSON", MIDI_TRACE_EVENT_MESSAGE_FIELD)
                        );

                        responseObject.SetNamedValue(MIDI_CONFIG_JSON_CONFIGURATION_RESPONSE_MESSAGE_PROPERTY_KEY,
                            json::JsonValue::CreateStringValue(internal::ResourceGetHString(IDS_ERROR_PARSING_JSON)));

                        internal::JsonStringifyObjectToOutParam(responseObject, response);

                        return E_FAIL;
                    }
                }
                else
                {
                    TraceLoggingWrite(
                        MidiBasicLoopbackMidiTransportTelemetryProvider::Provider(),
                        MIDI_TRACE_EVENT_ERROR,
                        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
                        TraceLoggingLevel(WINEVENT_LEVEL_ERROR),
                        TraceLoggingPointer(this, "this"),
                        TraceLoggingWideString(associationKey.c_str(), "association key"),
                        TraceLoggingWideString(L"Unable to convert association id property to a JsonObject", MIDI_TRACE_EVENT_MESSAGE_FIELD)
                    );

                    responseObject.SetNamedValue(MIDI_CONFIG_JSON_CONFIGURATION_RESPONSE_MESSAGE_PROPERTY_KEY,
                        json::JsonValue::CreateStringValue(internal::ResourceGetHString(IDS_ERROR_PARSING_JSON)));

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

        // Delete ----------------------------------

        if (deleteArray != nullptr && deleteArray.Size() > 0)
        {
            auto o = deleteArray.First();

            while (o.HasCurrent())
            {
                // each entry is an association id

                auto associationId = o.Current().GetString();
                auto device = TransportState::Current().GetEndpointTable()->GetDevice(associationId.c_str());

                //auto uniqueId = device->Definition.EndpointUniqueIdentifier;

                auto removalHr = TransportState::Current().GetEndpointManager()->DeleteEndpoint(device->Definition);

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
                        MidiBasicLoopbackMidiTransportTelemetryProvider::Provider(),
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

        
        if (response != nullptr)
        {
            internal::JsonStringifyObjectToOutParam(responseObject, response);
        }

        return S_OK;

    }
    catch (const std::exception& e)
    {
        TraceLoggingWrite(
            MidiBasicLoopbackMidiTransportTelemetryProvider::Provider(),
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
            MidiBasicLoopbackMidiTransportTelemetryProvider::Provider(),
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
CMidi2BasicLoopbackMidiConfigurationManager::Shutdown()
{
    TraceLoggingWrite(
        MidiBasicLoopbackMidiTransportTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this")
    );

    TransportState::Current().Shutdown();

    m_MidiDeviceManager.reset();

    return S_OK;
}

