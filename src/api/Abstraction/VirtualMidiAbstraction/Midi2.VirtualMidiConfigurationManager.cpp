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
CMidi2VirtualMidiConfigurationManager::Initialize(
    GUID AbstractionId, 
    IUnknown* MidiDeviceManager
)
{
    OutputDebugString(L"" __FUNCTION__ " Enter");

    TraceLoggingWrite(
        MidiVirtualMidiAbstractionTelemetryProvider::Provider(),
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
CMidi2VirtualMidiConfigurationManager::UpdateConfiguration(
    LPCWSTR ConfigurationJsonSection, 
    BSTR* Response
)
{
    OutputDebugString(L"" __FUNCTION__ " Enter");

    TraceLoggingWrite(
        MidiVirtualMidiAbstractionTelemetryProvider::Provider(),
        __FUNCTION__,
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this")
    );


    if (ConfigurationJsonSection == nullptr) return S_OK;
    //if (ConfigurationJsonSection == L"") return S_OK;

    json::JsonObject jsonObject;
    json::JsonObject responseObject;
    json::JsonArray createdDevicesResponseArray;


    if (json::JsonObject::TryParse(winrt::to_hstring(ConfigurationJsonSection), jsonObject))
    {
        auto createArray = internal::JsonGetArrayProperty(jsonObject, MIDI_CONFIG_JSON_ENDPOINT_VIRTUAL_DEVICES_CREATE_ARRAY_KEY);

        // under this should be a set of objects, each representing a single device to modify

        if (createArray && createArray.Size() > 0)
        {
            for (uint32_t i = 0; i < createArray.Size(); i++)
            {
                auto jsonEntry = (createArray.GetObjectAt(i));

                if (jsonEntry)
                {
                    // add to our endpoint table
                    MidiVirtualDeviceEndpointEntry deviceEntry;

                    deviceEntry.VirtualEndpointAssociationId = internal::JsonGetWStringProperty(jsonEntry, MIDI_CONFIG_JSON_ENDPOINT_VIRTUAL_DEVICE_ASSOCIATION_ID_PROPERTY_KEY, L"");
                    deviceEntry.ShortUniqueId = internal::JsonGetWStringProperty(jsonEntry, MIDI_CONFIG_JSON_ENDPOINT_VIRTUAL_DEVICE_UNIQUE_ID_PROPERTY_KEY, L"");
                    deviceEntry.BaseEndpointName = internal::JsonGetWStringProperty(jsonEntry, MIDI_CONFIG_JSON_ENDPOINT_COMMON_NAME_PROPERTY, L"");
                    deviceEntry.Description = internal::JsonGetWStringProperty(jsonEntry, MIDI_CONFIG_JSON_ENDPOINT_COMMON_DESCRIPTION_PROPERTY, L"");

                    // if no association id, or it already exists in the table, bail

                    // if no unique Id, bail or maybe generate one

                    // if a unique id and it's larger than the max length, truncate it

                    // create the device-side endpoint
                    LOG_IF_FAILED(AbstractionState::Current().GetEndpointManager()->CreateDeviceSideEndpoint(deviceEntry));


                    // TODO: This should have the association Id or something in it for hte client to make sense of it
                    auto singleResponse = internal::JsonCreateSingleWStringPropertyObject(
                        MIDI_CONFIG_JSON_ENDPOINT_VIRTUAL_DEVICE_RESPONSE_CREATED_ID_PROPERTY_KEY, 
                        deviceEntry.CreatedDeviceEndpointId);

                    createdDevicesResponseArray.Append(singleResponse);
                }
            }

            responseObject.SetNamedValue(MIDI_CONFIG_JSON_ENDPOINT_VIRTUAL_DEVICE_RESPONSE_CREATED_DEVICES_ARRAY_KEY, createdDevicesResponseArray);

        }
        else
        {
            // nothing in the array. Maybe we're looking at update or delete 

            OutputDebugString(__FUNCTION__ L" Empty create array");
        }

        // TODO: Update and Remove nodes




        // TODO: Actual Success or fail response
        internal::JsonSetBoolProperty(
            responseObject,
            MIDI_CONFIG_JSON_ENDPOINT_VIRTUAL_DEVICE_RESPONSE_SUCCESS_PROPERTY_KEY,
            true);

        // return the json with the information the client will need
        internal::JsonStringifyObjectToOutParam(responseObject, &Response);
    }

    return S_OK;

}


HRESULT
CMidi2VirtualMidiConfigurationManager::Cleanup()
{
    OutputDebugString(L"" __FUNCTION__ " Enter");

    TraceLoggingWrite(
        MidiVirtualMidiAbstractionTelemetryProvider::Provider(),
        __FUNCTION__,
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this")
    );




    return S_OK;
}

