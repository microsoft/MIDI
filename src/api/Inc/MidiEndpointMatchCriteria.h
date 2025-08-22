// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

#ifndef MIDI_ENDPOINTMATCH_CRITERIA_H
#define MIDI_ENDPOINTMATCH_CRITERIA_H

#pragma push_macro("GetObject")
#undef GetObject
#include <winrt/Windows.Data.Json.h>
#pragma pop_macro("GetObject")


#include <windows.h>


#include "MidiDefs.h"
#include "wstring_util.h"
#include "json_defs.h"

#define MIDI_CONFIG_JSON_ENDPOINT_COMMON_MATCH_PROPERTY_KEY_ENDPOINT_DEVICE_ID              L"endpointDeviceId"

#define MIDI_CONFIG_JSON_ENDPOINT_COMMON_MATCH_PROPERTY_KEY_DEVICE_INSTANCE_ID              L"endpointDeviceInstanceId"

#define MIDI_CONFIG_JSON_ENDPOINT_COMMON_MATCH_PROPERTY_KEY_VID                             L"usbVendorId"
#define MIDI_CONFIG_JSON_ENDPOINT_COMMON_MATCH_PROPERTY_KEY_PID                             L"usbProductId"
#define MIDI_CONFIG_JSON_ENDPOINT_COMMON_MATCH_PROPERTY_KEY_SERIAL                          L"usbSerialNumber"

#define MIDI_CONFIG_JSON_ENDPOINT_COMMON_MATCH_PROPERTY_KEY_MANUFACTURER_NAME               L"manufacturerName"
#define MIDI_CONFIG_JSON_ENDPOINT_COMMON_MATCH_PROPERTY_KEY_PRODUCT_INSTANCE_ID             L"productInstanceId"

#define MIDI_CONFIG_JSON_ENDPOINT_COMMON_MATCH_PROPERTY_KEY_STATIC_IP_ADDRESS               L"staticIPAddress"
#define MIDI_CONFIG_JSON_ENDPOINT_COMMON_MATCH_PROPERTY_KEY_NETWORK_PORT                    L"port"

#define MIDI_CONFIG_JSON_ENDPOINT_COMMON_MATCH_PROPERTY_KEY_TRANSPORT_SUPPLIED_NAME         L"transportSuppliedEndpointName"

#define MIDI_CONFIG_JSON_ENDPOINT_COMMON_MATCH_PROPERTY_KEY_PARENT_DEVICE_NAME              L"parentDeviceName"


//         "match" :
//         {
//           "endpointDeviceId" : "..."
//         },

class MidiEndpointMatchCriteria
{
public:
    static winrt::hstring PropertyKey;  // main property key for this type. defined in impl file

    MidiEndpointMatchCriteria() = default;

    _Success_(return != nullptr)
    static std::shared_ptr<MidiEndpointMatchCriteria> FromJson(_In_::winrt::Windows::Data::Json::JsonObject const& matchObject);
//    static std::shared_ptr<MidiEndpointMatchCriteria> FromValues(_In_ std::map<winrt::hstring, winrt::hstring> matchValues);

    _Success_(return == true)
    bool WriteJson(_In_::winrt::Windows::Data::Json::JsonObject& matchObject);

    bool IsMatch(_In_ MidiEndpointMatchCriteria const& matchValues);


    winrt::hstring EndpointDeviceId{};

    winrt::hstring DeviceInstanceId{};    // used when setting properties before the endpoint has been created. This will break if a device has > 1 UMP endpoint exposed

    uint16_t UsbVendorId{};
    uint16_t UsbProductId{};
    winrt::hstring UsbSerialNumber{};

    winrt::hstring DeviceManufacturerName{};
    winrt::hstring DeviceProductInstanceId{};   // this can be the same across manufacturers so manufacturer is required. Even that isn't really sufficient

    winrt::hstring NetworkStaticIPAddress{};
    uint16_t NetworkPort{};

    winrt::hstring TransportSuppliedEndpointName{};

    winrt::hstring ParentDeviceName{};


private:

    void Normalize();


};



















#endif
