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
CMidi2VirtualPatchBayConfigurationManager::Initialize(
    GUID transportId,
    IMidiDeviceManager* midiDeviceManager,
    IMidiServiceConfigurationManager* midiServiceConfigurationManager
)
{
    TraceLoggingWrite(
        MidiVirtualPatchBayTransportTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this")
    );

    RETURN_HR_IF_NULL(E_INVALIDARG, midiDeviceManager);
    RETURN_IF_FAILED(midiDeviceManager->QueryInterface(__uuidof(IMidiDeviceManager), (void**)&m_midiDeviceManager));

    RETURN_HR_IF_NULL(E_INVALIDARG, midiServiceConfigurationManager);
    RETURN_IF_FAILED(midiServiceConfigurationManager->QueryInterface(__uuidof(IMidiServiceConfigurationManager), (void**)&m_midiServiceConfigurationManager));

    m_transportId = transportId;

    return S_OK;
}


//              "someguid":
//              {
//                  "name" : "foobarbaz",
//                  "uniqueId" : "somethingunique",
//                  "description" : "this is a virtual port which can be used in routes where we need to intercept midi out and send elsewhere"
//                  "image" : "path\to\image"
//              }
_Use_decl_annotations_
HRESULT
CMidi2VirtualPatchBayConfigurationManager::ProcessConfigEntryCreateVirtualEndpoint(
    json::JsonObject const& entry, 
    json::JsonObject& responseObject) noexcept
{



    return S_OK;
}





//"endpointTransportPluginSettings":
//{
//    "{B7C8E2D2-EE3B-4044-B9CF-6F29528AB46D}":
//    {
//        "_comment": "Virtual Patch Bay",
//        "create" :
//        {
//            "virtualEndpoints":
//            {
//              "someguid":
//              {
//                  "name" : "foobarbaz",
//                  "uniqueId" : "somethingunique",
//                  "description" : "this is a virtual port which can be used in routes where we need to intercept midi out and send elsewhere"
//              }
//            },
//            "routes":
//            {
//              "{272357f5-6ab2-4737-b920-466eb94505bb}":
//              {
//                 "name": "Test Fan Out",
//                 "description" : "A virtual patch bay entry to test fan out",
//                 "enabled" : true,
//                 "sources" :
//                 [
//                    {
//                       "match":
//                       {
//                           "endpointDeviceId" : "somenendpointdeviceidfoobarbazbal0"
//                       },
//                       "groupIndexes" : [0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15]
//                    }
//                 ] ,
//                 "destinations":
//                 [
//                    {
//                      "match":
//                      {
//                        "endpointDeviceId" : "foobarbazbal1"
//                      },
//                      "groupMap" :
//                      [
//                        [1, 3],
//                        [2, 5],
//                        [3, 3]
//                      ]
//                    },
//                    {
//                      "match":
//                      {
//                        "endpointDeviceId" : "foobarbazbal2"
//                      },
//                      "groupMap" :
//                      [
//                      ]
//                    },
//                    {
//                      "match":
//                      {
//                        "endpointDeviceId" : "foobarbazbal3"
//                      },
//                      "groupMap" :
//                      [
//                      ]
//                    }
//                 ]
//              }
//            }
//        }
//    },
//}


_Use_decl_annotations_
HRESULT
CMidi2VirtualPatchBayConfigurationManager::UpdateConfiguration(
    LPCWSTR configurationJsonSection,
    LPWSTR* /*responseJson*/
)
{
    TraceLoggingWrite(
        MidiVirtualPatchBayTransportTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this"),
        TraceLoggingWideString(configurationJsonSection, "json")
    );

    if (configurationJsonSection == nullptr) return S_OK;

    json::JsonObject jsonObject;
    json::JsonObject responseObject;
    json::JsonArray createdDevicesResponseArray;

    // Flow
    // Read every entry and create the appropriate entries in the patch bay table
    // Once complete, signal that we're ready for devices to be resolved


    if (!json::JsonObject::TryParse(winrt::to_hstring(configurationJsonSection), jsonObject))
    {
        LOG_IF_FAILED(E_FAIL);  // cause fallbackerror to be logged

        TraceLoggingWrite(
            MidiVirtualPatchBayTransportTelemetryProvider::Provider(),
            MIDI_TRACE_EVENT_ERROR,
            TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
            TraceLoggingLevel(WINEVENT_LEVEL_ERROR),
            TraceLoggingPointer(this, "this"),
            TraceLoggingWideString(L"Failed to parse Configuration JSON", MIDI_TRACE_EVENT_MESSAGE_FIELD),
            TraceLoggingWideString(configurationJsonSection, "json")
        );

        return E_FAIL;
    }

    auto createArray = jsonObject.GetNamedArray(MIDI_CONFIG_JSON_ENDPOINT_COMMON_CREATE_KEY, nullptr);

    if (createArray == nullptr || createArray.Size() == 0)
    {
        // nothing in the array. Maybe we're looking at update or delete 

        // TODO: Set the response to something meaningful here

        return S_OK;
    }

    // iterate through all the work we need to do. Just 
    // "create" instructions, in this case.
    for (uint32_t i = 0; i < createArray.Size(); i++)
    {
        auto jsonEntry = (createArray.GetObjectAt(i));

        if (jsonEntry)
        {






            // create the entry

        //    CMidi2VirtualPatchBayRoutingEntry entry;

      //      entry.Initialize(associationId, name, description);


            // add each source

            //entry.AddSource();
            
             

            // add each destination 
            
            //entry.AddDestination();



            // Add the entry to the patch bay table

            //MidiPatchBayTable::Current().








    //        MidiVirtualDeviceEndpointEntry deviceEntry;

    //        deviceEntry.VirtualEndpointAssociationId = jsonEntry.GetNamedString(MIDI_CONFIG_JSON_ENDPOINT_VIRTUAL_DEVICE_ASSOCIATION_ID_PROPERTY_KEY, L"");
    //        deviceEntry.ShortUniqueId = jsonEntry.GetNamedString(MIDI_CONFIG_JSON_ENDPOINT_COMMON_UNIQUE_ID_PROPERTY, L"");
    //        deviceEntry.BaseEndpointName = jsonEntry.GetNamedString(MIDI_CONFIG_JSON_ENDPOINT_COMMON_NAME_PROPERTY, L"");
    //        deviceEntry.Description = jsonEntry.GetNamedString(MIDI_CONFIG_JSON_ENDPOINT_COMMON_DESCRIPTION_PROPERTY, L"");

    //        // If no association id generate one
    //        if (deviceEntry.VirtualEndpointAssociationId.empty())
    //        {
    //            deviceEntry.VirtualEndpointAssociationId = internal::GuidToString(winrt::Windows::Foundation::GuidHelper::CreateNewGuid());
    //        }

    //        if (deviceEntry.BaseEndpointName.empty())
    //        {
    //            deviceEntry.BaseEndpointName = L"Unnamed Virtual";
    //        }

    //        if (deviceEntry.ShortUniqueId.empty())
    //        {
    //            // get a guid and strip all non-alphanumeric characters
    //            auto guidString = internal::GuidToString(winrt::Windows::Foundation::GuidHelper::CreateNewGuid());

    //            guidString.erase(
    //                std::remove_if(
    //                    guidString.begin(),
    //                    guidString.end(),
    //                    [](wchar_t c) { return !std::isalnum(c); })
    //            );

    //            deviceEntry.ShortUniqueId = guidString;
    //        }
    //        else
    //        {
    //            // If a unique id and it's larger than the max length, truncate it
    //            deviceEntry.ShortUniqueId = deviceEntry.ShortUniqueId.substr(0, MIDI_CONFIG_JSON_ENDPOINT_VIRTUAL_DEVICE_UNIQUE_ID_MAX_LEN);
    //        }

    //        // create the device-side endpoint
    //        LOG_IF_FAILED(AbstractionState::Current().GetEndpointManager()->CreateDeviceSideEndpoint(deviceEntry));


    //        // TODO: This should have the association Id or something in it for the client to make sense of it
    //        auto singleResponse = internal::JsonCreateSingleWStringPropertyObject(
    //            MIDI_CONFIG_JSON_ENDPOINT_VIRTUAL_DEVICE_RESPONSE_CREATED_ID_PROPERTY_KEY,
    //            deviceEntry.CreatedDeviceEndpointId);

    //        createdDevicesResponseArray.Append(singleResponse);
        }
    }

    //responseObject.SetNamedValue(
    //    MIDI_CONFIG_JSON_ENDPOINT_VIRTUAL_DEVICE_RESPONSE_CREATED_DEVICES_ARRAY_KEY,
    //    createdDevicesResponseArray);

    //// TODO: Process all "update" and "remove" instructions


    //// TODO: Actual Success or fail response
    //auto responseVal = json::JsonValue::CreateBooleanValue(true);
    //responseObject.SetNamedValue(
    //    MIDI_CONFIG_JSON_CONFIGURATION_RESPONSE_SUCCESS_PROPERTY_KEY,
    //    responseVal);

    //TraceLoggingWrite(
    //    MidiVirtualMidiAbstractionTelemetryProvider::Provider(),
    //    MIDI_TRACE_EVENT_INFO,
    //    TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
    //    TraceLoggingLevel(WINEVENT_LEVEL_INFO),
    //    TraceLoggingPointer(this, "this"),
    //    TraceLoggingWideString(responseObject.Stringify().c_str())
    //);

    //// return the json with the information the client will need
    //internal::JsonStringifyObjectToOutParam(responseObject, &Response);

    return S_OK;
}


HRESULT
CMidi2VirtualPatchBayConfigurationManager::Shutdown()
{
    TraceLoggingWrite(
        MidiVirtualPatchBayTransportTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this")
    );

    LOG_IF_FAILED(TransportState::Current().Shutdown());

    m_midiDeviceManager.reset();
    m_midiServiceConfigurationManager.reset();

    return S_OK;
}

