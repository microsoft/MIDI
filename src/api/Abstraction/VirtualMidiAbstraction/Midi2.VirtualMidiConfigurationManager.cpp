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
CMidi2VirtualMidiConfigurationManager::Initialize(
    GUID AbstractionId, 
    IUnknown* MidiDeviceManager,
    IUnknown* MidiServiceConfigurationManagerInterface
)
{
    UNREFERENCED_PARAMETER(MidiServiceConfigurationManagerInterface);


    TraceLoggingWrite(
        MidiVirtualMidiAbstractionTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
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
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this"),
        TraceLoggingWideString(ConfigurationJsonSection, "json")
    );

    json::JsonObject jsonObject;
    json::JsonObject responseObject;
    json::JsonArray createdDevicesResponseArray;

    auto jsonFalse = json::JsonValue::CreateBooleanValue(false);
    auto jsonTrue = json::JsonValue::CreateBooleanValue(true);

    // Assume failure
    responseObject.SetNamedValue(
        MIDI_CONFIG_JSON_CONFIGURATION_RESPONSE_SUCCESS_PROPERTY_KEY,
        jsonFalse);



    // This abstraction doesn't support creating endpoints from the configuration file.
    // They are for runtime creation only.
    if (IsFromConfigurationFile)
    {
        TraceLoggingWrite(
            MidiVirtualMidiAbstractionTelemetryProvider::Provider(),
            MIDI_TRACE_EVENT_ERROR,
            TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
            TraceLoggingLevel(WINEVENT_LEVEL_WARNING),
            TraceLoggingPointer(this, "this"),
            TraceLoggingWideString(L"Virtual endpoints can be created only at runtime through the API, not from the configuration file.", MIDI_TRACE_EVENT_MESSAGE_FIELD)
        );

        // we return S_OK here because this can happen at service startup, and no reason to log an error here
        return S_OK;
    }
    else if (ConfigurationJsonSection == nullptr)
    {
        internal::JsonStringifyObjectToOutParam(responseObject, &Response);

        RETURN_IF_FAILED(E_INVALIDARG);
    }
    else
    {
        if (!json::JsonObject::TryParse(winrt::to_hstring(ConfigurationJsonSection), jsonObject))
        {
            TraceLoggingWrite(
                MidiVirtualMidiAbstractionTelemetryProvider::Provider(),
                MIDI_TRACE_EVENT_ERROR,
                TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
                TraceLoggingLevel(WINEVENT_LEVEL_ERROR),
                TraceLoggingPointer(this, "this"),
                TraceLoggingWideString(L"Failed to parse Configuration JSON", MIDI_TRACE_EVENT_MESSAGE_FIELD),
                TraceLoggingWideString(ConfigurationJsonSection, "json")
            );

            internal::JsonStringifyObjectToOutParam(responseObject, &Response);

            RETURN_IF_FAILED(E_INVALIDARG);
        }

        auto createArray = jsonObject.GetNamedArray(MIDI_CONFIG_JSON_ENDPOINT_COMMON_CREATE_KEY, nullptr);

        if (createArray == nullptr || createArray.Size() == 0)
        {
            // nothing in the array. Maybe we're looking at update or delete 

            // TODO: Set the response to something meaningful here

            internal::JsonStringifyObjectToOutParam(responseObject, &Response);

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

                deviceEntry.VirtualEndpointAssociationId = jsonEntry.GetNamedString(MIDI_CONFIG_JSON_ENDPOINT_VIRTUAL_DEVICE_ASSOCIATION_ID_PROPERTY_KEY, L"");
                deviceEntry.ShortUniqueId = jsonEntry.GetNamedString(MIDI_CONFIG_JSON_ENDPOINT_COMMON_UNIQUE_ID_PROPERTY, L"");
                deviceEntry.BaseEndpointName = jsonEntry.GetNamedString(MIDI_CONFIG_JSON_ENDPOINT_COMMON_NAME_PROPERTY, L"");
                deviceEntry.Description = jsonEntry.GetNamedString(MIDI_CONFIG_JSON_ENDPOINT_COMMON_DESCRIPTION_PROPERTY, L"");
                deviceEntry.Manufacturer = jsonEntry.GetNamedString(MIDI_CONFIG_JSON_ENDPOINT_COMMON_MANUFACTURER_PROPERTY, L"");
                deviceEntry.UMPOnly = jsonEntry.GetNamedBoolean(MIDI_CONFIG_JSON_ENDPOINT_COMMON_UMP_ONLY_PROPERTY, false);

                // If no association id generate one
                if (deviceEntry.VirtualEndpointAssociationId.empty())
                {
                    deviceEntry.VirtualEndpointAssociationId = internal::GuidToString(winrt::Windows::Foundation::GuidHelper::CreateNewGuid());
                }

                if (deviceEntry.BaseEndpointName.empty())
                {
                    deviceEntry.BaseEndpointName = L"Unnamed Virtual";
                }

                if (deviceEntry.ShortUniqueId.empty())
                {
                    // get a guid and strip all non-alphanumeric characters
                    auto guidString = internal::GuidToString(winrt::Windows::Foundation::GuidHelper::CreateNewGuid());

                    guidString.erase(
                        std::remove_if(
                            guidString.begin(),
                            guidString.end(),
                            [](wchar_t c) { return !std::isalnum(c); })
                    );

                    deviceEntry.ShortUniqueId = internal::ToLowerTrimmedWStringCopy(guidString);
                }
                else
                {
                    // If a unique id and it's larger than the max length, truncate it
                    deviceEntry.ShortUniqueId = deviceEntry.ShortUniqueId.substr(0, MIDI_CONFIG_JSON_ENDPOINT_VIRTUAL_DEVICE_UNIQUE_ID_MAX_LEN);

                    // normalize it
                    deviceEntry.ShortUniqueId = internal::ToLowerTrimmedWStringCopy(deviceEntry.ShortUniqueId);

                    // Check to see if the id is already in use

                    if (AbstractionState::Current().GetEndpointTable()->IsUniqueIdInUse(deviceEntry.ShortUniqueId))
                    {
                        responseObject.SetNamedValue(
                            MIDI_CONFIG_JSON_CONFIGURATION_RESPONSE_SUCCESS_PROPERTY_KEY,
                            jsonFalse);
                    }


                }

                internal::JsonStringifyObjectToOutParam(responseObject, &Response);

                // create the device-side endpoint. This is a critical step
                RETURN_IF_FAILED(AbstractionState::Current().GetEndpointManager()->CreateDeviceSideEndpoint(deviceEntry));

                // mark success
                responseObject.SetNamedValue(
                    MIDI_CONFIG_JSON_CONFIGURATION_RESPONSE_SUCCESS_PROPERTY_KEY,
                    jsonTrue);

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

    }


    TraceLoggingWrite(
        MidiVirtualMidiAbstractionTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
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
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this")
    );

    AbstractionState::Current().Cleanup();

    m_MidiDeviceManager.reset();

    return S_OK;
}

