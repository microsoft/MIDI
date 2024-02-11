// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================

#pragma once

#ifndef JSON_HELPERS_H
#define JSON_HELPERS_H

// this def messes with json so we need to undef it here
#pragma push_macro("GetObject")
#undef GetObject
#include <winrt/Windows.Data.Json.h>
#pragma pop_macro("GetObject")

#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Foundation.Collections.h>

#include "atlbase.h"  // for CComBSTR


namespace json = ::winrt::Windows::Data::Json;

// structural

#define MIDI_CONFIG_JSON_HEADER_OBJECT L"header"
#define MIDI_CONFIG_JSON_HEADER_PRODUCT_KEY L"product"
#define MIDI_CONFIG_JSON_HEADER_FILE_VERSION_KEY L"fileVersion"



#define MIDI_CONFIG_JSON_TRANSPORT_PLUGIN_SETTINGS_OBJECT L"endpointTransportPluginSettings"
#define MIDI_CONFIG_JSON_ENDPOINT_PROCESSING_PLUGIN_SETTINGS_OBJECT L"endpointProcessingPluginSettings"

// common properties

#define MIDI_CONFIG_JSON_ENDPOINT_COMMON_NAME_PROPERTY                          L"name"
#define MIDI_CONFIG_JSON_ENDPOINT_COMMON_DESCRIPTION_PROPERTY                   L"description"
#define MIDI_CONFIG_JSON_ENDPOINT_COMMON_UNIQUE_ID_PROPERTY                     L"uniqueIdentifier"


#define MIDI_CONFIG_JSON_ENDPOINT_COMMON_USER_SUPPLIED_NAME_PROPERTY            L"userSuppliedName"
#define MIDI_CONFIG_JSON_ENDPOINT_COMMON_USER_SUPPLIED_DESCRIPTION_PROPERTY     L"userSuppliedDescription"
#define MIDI_CONFIG_JSON_ENDPOINT_COMMON_USER_SUPPLIED_SMALL_IMAGE_PROPERTY     L"userSuppliedSmallImage"
#define MIDI_CONFIG_JSON_ENDPOINT_COMMON_USER_SUPPLIED_LARGE_IMAGE_PROPERTY     L"userSuppliedLargeImage"


// Virtual MIDI (here because also needed by the client API)

#define MIDI_CONFIG_JSON_ENDPOINT_VIRTUAL_DEVICES_CREATE_ARRAY_KEY              L"createVirtualDevices"
#define MIDI_CONFIG_JSON_ENDPOINT_VIRTUAL_DEVICES_UPDATE_ARRAY_KEY              L"updateVirtualDevices"
#define MIDI_CONFIG_JSON_ENDPOINT_VIRTUAL_DEVICES_REMOVE_ARRAY_KEY              L"removeVirtualDevices"

#define MIDI_CONFIG_JSON_ENDPOINT_VIRTUAL_DEVICE_ASSOCIATION_ID_PROPERTY_KEY    L"associationIdentifier"
#define MIDI_CONFIG_JSON_ENDPOINT_VIRTUAL_DEVICE_UNIQUE_ID_MAX_LEN  32


#define MIDI_CONFIG_JSON_ENDPOINT_VIRTUAL_DEVICE_RESPONSE_SUCCESS_PROPERTY_KEY          L"success"
#define MIDI_CONFIG_JSON_ENDPOINT_VIRTUAL_DEVICE_RESPONSE_CREATED_DEVICES_ARRAY_KEY     L"createdDevices"
#define MIDI_CONFIG_JSON_ENDPOINT_VIRTUAL_DEVICE_RESPONSE_CREATED_ID_PROPERTY_KEY       L"id"

// loopback MIDI (here because these can also be created via the client API)

#define MIDI_CONFIG_JSON_ENDPOINT_LOOPBACK_DEVICES_CREATE_ARRAY_KEY              L"createLoopbackDevices"
#define MIDI_CONFIG_JSON_ENDPOINT_LOOPBACK_DEVICES_UPDATE_ARRAY_KEY              L"updateLoopbackDevices"
#define MIDI_CONFIG_JSON_ENDPOINT_LOOPBACK_DEVICES_REMOVE_ARRAY_KEY              L"removeLoopbackDevices"
#define MIDI_CONFIG_JSON_ENDPOINT_LOOPBACK_DEVICE_ASSOCIATION_ID_PROPERTY_KEY    L"associationIdentifier"

#define MIDI_CONFIG_JSON_ENDPOINT_LOOPBACK_DEVICE_RESPONSE_SUCCESS_PROPERTY_KEY          L"success"
#define MIDI_CONFIG_JSON_ENDPOINT_LOOPBACK_DEVICE_RESPONSE_CREATED_DEVICES_ARRAY_KEY     L"createdDevices"
#define MIDI_CONFIG_JSON_ENDPOINT_LOOPBACK_DEVICE_RESPONSE_CREATED_ID_PROPERTY_KEY       L"id"



// Session tracker. These are used in the service and in the client API

#define MIDI_SESSION_TRACKER_JSON_RESULT_SESSION_ARRAY_PROPERTY_KEY             L"sessions"

#define MIDI_SESSION_TRACKER_JSON_RESULT_SESSION_ID_PROPERTY_KEY                L"id"
#define MIDI_SESSION_TRACKER_JSON_RESULT_SESSION_NAME_PROPERTY_KEY              L"name"
#define MIDI_SESSION_TRACKER_JSON_RESULT_PROCESS_ID_PROPERTY_KEY                L"clientProcessId"
#define MIDI_SESSION_TRACKER_JSON_RESULT_PROCESS_NAME_PROPERTY_KEY              L"processName"
#define MIDI_SESSION_TRACKER_JSON_RESULT_SESSION_TIME_PROPERTY_KEY              L"startTime"


#define MIDI_SESSION_TRACKER_JSON_RESULT_CONNECTION_ARRAY_PROPERTY_KEY          L"connections"

#define MIDI_SESSION_TRACKER_JSON_RESULT_CONNECTION_TIME_PROPERTY_KEY           L"earliestStartTime"
#define MIDI_SESSION_TRACKER_JSON_RESULT_CONNECTION_COUNT_PROPERTY_KEY          L"instanceCount"
#define MIDI_SESSION_TRACKER_JSON_RESULT_CONNECTION_ENDPOINT_ID_PROPERTY_KEY    L"endpointId"



namespace Windows::Devices::Midi2::Internal
{
    bool JsonObjectFromBSTR(_In_ BSTR* const bstr, _Out_ json::JsonObject& obj) noexcept;

    bool JsonStringifyObjectToOutParam(_In_ json::JsonObject const& obj, _Out_ BSTR** outParam) noexcept;

    json::JsonObject JsonCreateSingleWStringPropertyObject(_In_ std::wstring const& key, _In_ std::wstring const& value) noexcept;


    json::JsonArray JsonGetArrayProperty(_In_ json::JsonObject const& parent, _In_ std::wstring const& key) noexcept;
    bool JsonSetArrayProperty(_In_ json::JsonObject const& parent, _In_ std::wstring const& key, _In_ json::JsonArray const& value) noexcept;

    std::wstring JsonGetWStringProperty(_In_ json::JsonObject const& parent, _In_ std::wstring const& key, std::wstring const& defaultValue) noexcept;
    bool JsonSetWStringProperty(_In_ json::JsonObject const& parent, _In_ std::wstring const& key, _In_ std::wstring const& value) noexcept;

    // this handles accuracy of seconds and no finer
    std::chrono::time_point<std::chrono::system_clock> 
    JsonGetDateTimeProperty(
        _In_ json::JsonObject const& parent, 
        _In_ std::wstring const& key, 
        _In_ std::chrono::time_point<std::chrono::system_clock> const defaultValue) noexcept;

    // this handles accuracy of seconds and no finer
    bool JsonSetDateTimeProperty(
        _In_ json::JsonObject const& parent, 
        _In_ std::wstring const& key, 
        _In_ std::chrono::time_point<std::chrono::system_clock> const value) noexcept;

    GUID JsonGetGuidProperty(_In_ json::JsonObject const& parent, _In_ std::wstring const& key, _In_ GUID const& defaultValue) noexcept;
    bool JsonSetGuidProperty(_In_ json::JsonObject const& parent, _In_ std::wstring const& key, _In_ GUID const& value) noexcept;

    bool JsonGetBoolProperty(_In_ json::JsonObject const& parent, _In_ std::wstring const& key, _In_ bool const defaultValue) noexcept;
    bool JsonSetBoolProperty(_In_ json::JsonObject const& parent, _In_ std::wstring const& key, _In_ bool const value) noexcept;

    double JsonGetDoubleProperty(_In_ json::JsonObject const& parent, _In_ std::wstring const& key, _In_ double const defaultValue) noexcept;
    bool JsonSetDoubleProperty(_In_ json::JsonObject const& parent, _In_ std::wstring const& key, _In_ double const value) noexcept;

    long JsonGetLongProperty(_In_ json::JsonObject const& parent, _In_ std::wstring const& key, _In_ long const defaultValue) noexcept;
    bool JsonSetLongProperty(_In_ json::JsonObject const& parent, _In_ std::wstring const& key, _In_ long const value) noexcept;

    json::JsonObject JsonGetObjectProperty(_In_ json::JsonObject const& parent, _In_ std::wstring const& key, json::JsonObject const& defaultValue) noexcept;
    bool JsonSetObjectProperty(_In_ json::JsonObject const& parent, _In_ std::wstring const& key, _In_ json::JsonObject const& value) noexcept;


}

#endif