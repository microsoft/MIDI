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
    BOOL IsFromConfigurationFile,
    BSTR* Response
)
{
    TraceLoggingWrite(
        MidiVirtualMidiAbstractionTelemetryProvider::Provider(),
        __FUNCTION__,
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this"),
        TraceLoggingWideString(ConfigurationJsonSection, "json")
    );


    // This abstraction doesn't support creating endpoints from the configuration file.
    // They are for runtime creation only.
    if (IsFromConfigurationFile)
    {
        TraceLoggingWrite(
            MidiVirtualMidiAbstractionTelemetryProvider::Provider(),
            __FUNCTION__,
            TraceLoggingLevel(WINEVENT_LEVEL_ERROR),
            TraceLoggingPointer(this, "this"),
            TraceLoggingWideString(L"Virtual endpoints can be created only at runtime through the API, not from the configuration file.", "message")
        );

        return E_FAIL;
    }


    if (ConfigurationJsonSection == nullptr) return S_OK;
    //if (ConfigurationJsonSection == L"") return S_OK;

    json::JsonObject jsonObject;
    json::JsonObject responseObject;
    json::JsonArray createdDevicesResponseArray;


    if (!json::JsonObject::TryParse(winrt::to_hstring(ConfigurationJsonSection), jsonObject))
    {
        TraceLoggingWrite(
            MidiVirtualMidiAbstractionTelemetryProvider::Provider(),
            __FUNCTION__,
            TraceLoggingLevel(WINEVENT_LEVEL_ERROR),
            TraceLoggingPointer(this, "this"),
            TraceLoggingWideString(L"Failed to parse Configuration JSON", "message"),
            TraceLoggingWideString(ConfigurationJsonSection, "json")
        );

        return E_FAIL;
    }

    auto createArray = internal::JsonGetArrayProperty(jsonObject, MIDI_CONFIG_JSON_ENDPOINT_VIRTUAL_DEVICES_CREATE_ARRAY_KEY);

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
            MidiVirtualDeviceEndpointEntry deviceEntry;

            deviceEntry.VirtualEndpointAssociationId = internal::JsonGetWStringProperty(
                jsonEntry, 
                MIDI_CONFIG_JSON_ENDPOINT_VIRTUAL_DEVICE_ASSOCIATION_ID_PROPERTY_KEY, 
                L"");

            deviceEntry.ShortUniqueId = internal::JsonGetWStringProperty(
                jsonEntry, 
                MIDI_CONFIG_JSON_ENDPOINT_COMMON_UNIQUE_ID_PROPERTY, 
                L"");

            deviceEntry.BaseEndpointName = internal::JsonGetWStringProperty(
                jsonEntry, 
                MIDI_CONFIG_JSON_ENDPOINT_COMMON_NAME_PROPERTY, 
                L"");

            deviceEntry.Description = internal::JsonGetWStringProperty(
                jsonEntry, 
                MIDI_CONFIG_JSON_ENDPOINT_COMMON_DESCRIPTION_PROPERTY, 
                L"");

            // TODO: if no association id, or it already exists in the table, bail

            // TODO: if no unique Id, bail or maybe generate one

            // TODO: if a unique id and it's larger than the max length, truncate it

            // create the device-side endpoint
            LOG_IF_FAILED(AbstractionState::Current().GetEndpointManager()->CreateDeviceSideEndpoint(deviceEntry));


            // TODO: This should have the association Id or something in it for the client to make sense of it
            auto singleResponse = internal::JsonCreateSingleWStringPropertyObject(
                MIDI_CONFIG_JSON_ENDPOINT_VIRTUAL_DEVICE_RESPONSE_CREATED_ID_PROPERTY_KEY, 
                deviceEntry.CreatedDeviceEndpointId);

            createdDevicesResponseArray.Append(singleResponse);
        }
    }

    responseObject.SetNamedValue(
        MIDI_CONFIG_JSON_ENDPOINT_VIRTUAL_DEVICE_RESPONSE_CREATED_DEVICES_ARRAY_KEY, 
        createdDevicesResponseArray);

    // TODO: Process all "update" and "remove" instructions
   

    // TODO: Actual Success or fail response
    internal::JsonSetBoolProperty(
        responseObject,
        MIDI_CONFIG_JSON_ENDPOINT_VIRTUAL_DEVICE_RESPONSE_SUCCESS_PROPERTY_KEY,
        true);

    TraceLoggingWrite(
        MidiVirtualMidiAbstractionTelemetryProvider::Provider(),
        __FUNCTION__,
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this"),
        TraceLoggingWideString(responseObject.Stringify().c_str())
    );

    // return the json with the information the client will need
    internal::JsonStringifyObjectToOutParam(responseObject, &Response);

    return S_OK;
}


HRESULT
CMidi2VirtualMidiConfigurationManager::Cleanup()
{
    TraceLoggingWrite(
        MidiVirtualMidiAbstractionTelemetryProvider::Provider(),
        __FUNCTION__,
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this")
    );




    return S_OK;
}

