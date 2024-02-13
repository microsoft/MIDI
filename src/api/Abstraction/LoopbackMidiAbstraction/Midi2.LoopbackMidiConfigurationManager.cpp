// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================

#include "pch.h"


_Use_decl_annotations_
HRESULT
CMidi2LoopbackMidiConfigurationManager::Initialize(
    GUID AbstractionId,
    IUnknown* MidiDeviceManager
)
{
    TraceLoggingWrite(
        MidiLoopbackMidiAbstractionTelemetryProvider::Provider(),
        __FUNCTION__,
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this")
    );

    RETURN_HR_IF_NULL(E_INVALIDARG, MidiDeviceManager);
    RETURN_IF_FAILED(MidiDeviceManager->QueryInterface(__uuidof(IMidiDeviceManagerInterface), (void**)&m_MidiDeviceManager));

    m_abstractionId = AbstractionId;

    return S_OK;
}

_Use_decl_annotations_
HRESULT
CMidi2LoopbackMidiConfigurationManager::UpdateConfiguration(
    LPCWSTR ConfigurationJsonSection,
    BOOL IsFromConfigurationFile,
    BSTR* Response
)
{
    TraceLoggingWrite(
        MidiLoopbackMidiAbstractionTelemetryProvider::Provider(),
        __FUNCTION__,
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this"),
        TraceLoggingWideString(ConfigurationJsonSection, "json")
    );


    if (ConfigurationJsonSection == nullptr) return S_OK;
    //if (ConfigurationJsonSection == L"") return S_OK;

    json::JsonObject jsonObject;
    json::JsonObject responseObject;

    // default to failure
    internal::JsonSetBoolProperty(responseObject, MIDI_CONFIG_JSON_ENDPOINT_LOOPBACK_DEVICE_RESPONSE_SUCCESS_PROPERTY_KEY, false);


    try
    {

        if (!json::JsonObject::TryParse(winrt::to_hstring(ConfigurationJsonSection), jsonObject))
        {
            TraceLoggingWrite(
                MidiLoopbackMidiAbstractionTelemetryProvider::Provider(),
                __FUNCTION__,
                TraceLoggingLevel(WINEVENT_LEVEL_ERROR),
                TraceLoggingPointer(this, "this"),
                TraceLoggingWideString(L"Failed to parse Configuration JSON", "message"),
                TraceLoggingWideString(ConfigurationJsonSection, "json")
            );

            internal::JsonStringifyObjectToOutParam(responseObject, &Response);

            return E_FAIL;
        }

        // I was tempted to call this the Prefix Code. KHAN!!
        std::wstring instanceIdPrefixA = IsFromConfigurationFile ? MIDI_PERM_LOOP_INSTANCE_ID_A_PREFIX : MIDI_TEMP_LOOP_INSTANCE_ID_A_PREFIX;
        std::wstring instanceIdPrefixB = IsFromConfigurationFile ? MIDI_PERM_LOOP_INSTANCE_ID_B_PREFIX : MIDI_TEMP_LOOP_INSTANCE_ID_B_PREFIX;
        // we should probably set a property based on this as well.

        auto createObject = jsonObject.GetNamedObject(MIDI_CONFIG_JSON_ENDPOINT_LOOPBACK_DEVICES_CREATE_KEY, nullptr);

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

                //if (!o.Current().Value().try_as<json::JsonObject>(associationObj))
                //{
                //    TraceLoggingWrite(
                //        MidiLoopbackMidiAbstractionTelemetryProvider::Provider(),
                //        __FUNCTION__,
                //        TraceLoggingLevel(WINEVENT_LEVEL_ERROR),
                //        TraceLoggingPointer(this, "this"),
                //        TraceLoggingWideString(L"Unable to read the property as a json object", "message"),
                //        TraceLoggingWideString(associationKey.c_str(), "key")
                //        TraceLoggingWideString(o.Current().Value().Stringify(), "stringify")
                //    );

                //    internal::JsonStringifyObjectToOutParam(responseObject, &Response);

                //    return E_FAIL;
                //}              

                
                if (associationObj)
                {
                    definitionA->AssociationId = associationKey;
                    definitionB->AssociationId = definitionA->AssociationId;


                    auto endpointAObject = associationObj.GetNamedObject(MIDI_CONFIG_JSON_ENDPOINT_LOOPBACK_DEVICE_ENDPOINT_A_KEY, nullptr);
                    auto endpointBObject = associationObj.GetNamedObject(MIDI_CONFIG_JSON_ENDPOINT_LOOPBACK_DEVICE_ENDPOINT_B_KEY, nullptr);

                    if (endpointAObject != nullptr && endpointBObject != nullptr)
                    {
                        definitionA->EndpointName = endpointAObject.GetNamedString(MIDI_CONFIG_JSON_ENDPOINT_COMMON_NAME_PROPERTY, L"");
                        definitionA->EndpointDescription = endpointAObject.GetNamedString(MIDI_CONFIG_JSON_ENDPOINT_COMMON_DESCRIPTION_PROPERTY, L"");
                        definitionA->EndpointUniqueIdentifier = endpointAObject.GetNamedString(MIDI_CONFIG_JSON_ENDPOINT_COMMON_UNIQUE_ID_PROPERTY, L"");
                        definitionA->InstanceIdPrefix = instanceIdPrefixA;

                        definitionB->EndpointName = endpointBObject.GetNamedString(MIDI_CONFIG_JSON_ENDPOINT_COMMON_NAME_PROPERTY, L"");
                        definitionB->EndpointDescription = endpointBObject.GetNamedString(MIDI_CONFIG_JSON_ENDPOINT_COMMON_DESCRIPTION_PROPERTY, L"");
                        definitionB->EndpointUniqueIdentifier = endpointBObject.GetNamedString(MIDI_CONFIG_JSON_ENDPOINT_COMMON_UNIQUE_ID_PROPERTY, L"");
                        definitionB->InstanceIdPrefix = instanceIdPrefixB;


                        if (definitionA->EndpointName.empty() || definitionB->EndpointName.empty())
                        {
                            TraceLoggingWrite(
                                MidiLoopbackMidiAbstractionTelemetryProvider::Provider(),
                                __FUNCTION__,
                                TraceLoggingLevel(WINEVENT_LEVEL_ERROR),
                                TraceLoggingPointer(this, "this"),
                                TraceLoggingWideString(L"Endpoint name missing or empty", "message")
                            );

                            internal::JsonStringifyObjectToOutParam(responseObject, &Response);

                            return E_FAIL;
                        }


                        if (definitionA->EndpointUniqueIdentifier.empty() || definitionB->EndpointUniqueIdentifier.empty())
                        {
                            TraceLoggingWrite(
                                MidiLoopbackMidiAbstractionTelemetryProvider::Provider(),
                                __FUNCTION__,
                                TraceLoggingLevel(WINEVENT_LEVEL_ERROR),
                                TraceLoggingPointer(this, "this"),
                                TraceLoggingWideString(L"Unique identifier missing or empty", "message")
                            );

                            internal::JsonStringifyObjectToOutParam(responseObject, &Response);

                            return E_FAIL;
                        }


                        if (SUCCEEDED(AbstractionState::Current().GetEndpointManager()->CreateEndpointPair(definitionA, definitionB)))
                        {
                            TraceLoggingWrite(
                                MidiLoopbackMidiAbstractionTelemetryProvider::Provider(),
                                __FUNCTION__,
                                TraceLoggingLevel(WINEVENT_LEVEL_INFO),
                                TraceLoggingPointer(this, "this"),
                                TraceLoggingWideString(L"Loopback endpoint pair created", "message")
                            );

                            // all good

                            internal::JsonSetBoolProperty(
                                responseObject,
                                MIDI_CONFIG_JSON_ENDPOINT_LOOPBACK_DEVICE_RESPONSE_SUCCESS_PROPERTY_KEY,
                                true);

                            // update the return json with the new Ids

                            internal::JsonSetWStringProperty(
                                responseObject,
                                MIDI_CONFIG_JSON_ENDPOINT_LOOPBACK_DEVICE_RESPONSE_CREATED_ENDPOINT_A_ID_KEY,
                                definitionA->CreatedEndpointInterfaceId);

                            internal::JsonSetWStringProperty(
                                responseObject,
                                MIDI_CONFIG_JSON_ENDPOINT_LOOPBACK_DEVICE_RESPONSE_CREATED_ENDPOINT_B_ID_KEY,
                                definitionB->CreatedEndpointInterfaceId);
                        }
                        else
                        {
                            // we failed to create the endpoints. Exit and return a fail.
                            TraceLoggingWrite(
                                MidiLoopbackMidiAbstractionTelemetryProvider::Provider(),
                                __FUNCTION__,
                                TraceLoggingLevel(WINEVENT_LEVEL_ERROR),
                                TraceLoggingPointer(this, "this"),
                                TraceLoggingWideString(L"Failed to create endpoints", "message")
                            );

                            internal::JsonStringifyObjectToOutParam(responseObject, &Response);

                            return E_FAIL;
                        }
                    }
                    else
                    {
                        // couldn't get the endpointA or endpointB objects. Exit and return a fail

                        TraceLoggingWrite(
                            MidiLoopbackMidiAbstractionTelemetryProvider::Provider(),
                            __FUNCTION__,
                            TraceLoggingLevel(WINEVENT_LEVEL_ERROR),
                            TraceLoggingPointer(this, "this"),
                            TraceLoggingWideString(associationKey.c_str(), "association key"),
                            TraceLoggingWideString(L"Failed to get one or both endpoints from the JSON", "message")
                        );

                        internal::JsonStringifyObjectToOutParam(responseObject, &Response);

                        return E_FAIL;

                    }
                }
                else
                {
                    TraceLoggingWrite(
                        MidiLoopbackMidiAbstractionTelemetryProvider::Provider(),
                        __FUNCTION__,
                        TraceLoggingLevel(WINEVENT_LEVEL_ERROR),
                        TraceLoggingPointer(this, "this"),
                        TraceLoggingWideString(associationKey.c_str(), "association key"),
                        TraceLoggingWideString(L"Unable to convert association id property to a JsonObject", "message")
                    );

                    internal::JsonStringifyObjectToOutParam(responseObject, &Response);

                    return E_FAIL;
                }

                o.MoveNext();
            }
        }
        else
        {
            // nothing to create.
        }


        //auto deleteArray = internal::JsonGetArrayProperty(jsonObject, MIDI_CONFIG_JSON_ENDPOINT_LOOPBACK_DEVICES_REMOVE_KEY);

        //// Remove ----------------------------------

        //if (deleteArray.Size() > 0)
        //{
        //    // TODO : Delete endpoints if they aren't in the config file

        //}
        //else
        //{
        //    // nothing to remove.
        //}


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

        internal::JsonStringifyObjectToOutParam(responseObject, &Response);

        return S_OK;

    }
    catch (const std::exception& e)
    {
        TraceLoggingWrite(
            MidiLoopbackMidiAbstractionTelemetryProvider::Provider(),
            __FUNCTION__,
            TraceLoggingLevel(WINEVENT_LEVEL_ERROR),
            TraceLoggingPointer(this, "this"),
            TraceLoggingWideString(L"std exception processing json", "message"),
            TraceLoggingString(e.what(), "exception"),
            TraceLoggingWideString(ConfigurationJsonSection, "json")
        );
    }
    catch (...)
    {
        TraceLoggingWrite(
            MidiLoopbackMidiAbstractionTelemetryProvider::Provider(),
            __FUNCTION__,
            TraceLoggingLevel(WINEVENT_LEVEL_ERROR),
            TraceLoggingPointer(this, "this"),
            TraceLoggingWideString(L"Other exception processing json", "message"),
            TraceLoggingWideString(ConfigurationJsonSection, "json")
        );
    }

    // return the json with the information the client will need
    internal::JsonStringifyObjectToOutParam(responseObject, &Response);

    return S_OK;
}


HRESULT
CMidi2LoopbackMidiConfigurationManager::Cleanup()
{
    TraceLoggingWrite(
        MidiLoopbackMidiAbstractionTelemetryProvider::Provider(),
        __FUNCTION__,
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this")
    );




    return S_OK;
}

