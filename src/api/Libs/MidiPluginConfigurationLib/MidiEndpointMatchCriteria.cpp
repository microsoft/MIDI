// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================


#include <windows.h>
//#include <Devpropdef.h>

#include <atlbase.h>
#include <atlcom.h>
#include <atlctl.h>
#include <atlcoll.h>
#include <atlsync.h>

#include <atlconv.h>
#include <string>

#include <setupapi.h>
#include <initguid.h>
#include <Devpkey.h>

#include "MidiDefs.h"
#include "hstring_util.h"
#include "json_defs.h"

#include "MidiEndpointMatchCriteria.h"

#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Foundation.Collections.h>


#ifdef max
#pragma push_macro("max")
#undef max



#define MIDI_CONFIG_JSON_ENDPOINT_COMMON_MATCH_OBJECT_KEY L"match"




namespace json = ::winrt::Windows::Data::Json;

namespace internal = ::WindowsMidiServicesInternal;

winrt::hstring MidiEndpointMatchCriteria::PropertyKey{ MIDI_CONFIG_JSON_ENDPOINT_COMMON_MATCH_OBJECT_KEY };


void MidiEndpointMatchCriteria::Normalize()
{
    EndpointDeviceId = internal::NormalizeEndpointInterfaceIdHStringCopy(EndpointDeviceId);

    DeviceInstanceId = internal::NormalizeDeviceInstanceIdHStringCopy(DeviceInstanceId);

    UsbSerialNumber = internal::TrimmedHStringCopy(UsbSerialNumber);

    DeviceManufacturerName = internal::TrimmedHStringCopy(DeviceManufacturerName);
    DeviceProductInstanceId = internal::TrimmedHStringCopy(DeviceProductInstanceId);

    NetworkStaticIPAddress = internal::TrimmedHStringCopy(NetworkStaticIPAddress);

    TransportSuppliedEndpointName = internal::TrimmedHStringCopy(TransportSuppliedEndpointName);

    ParentDeviceName = internal::TrimmedHStringCopy(ParentDeviceName);
}

//_Use_decl_annotations_
//std::shared_ptr<MidiEndpointMatchCriteria> MidiEndpointMatchCriteria::FromValues(std::map<winrt::hstring, winrt::hstring> matchValues)
//{
//
//}


_Use_decl_annotations_
std::shared_ptr<MidiEndpointMatchCriteria> MidiEndpointMatchCriteria::FromJson(json::JsonObject const& matchObject)
{
    try
    {
        auto match = std::make_shared<MidiEndpointMatchCriteria>();

        // SWD
        match->EndpointDeviceId = matchObject.GetNamedString(MIDI_CONFIG_JSON_ENDPOINT_COMMON_MATCH_PROPERTY_KEY_ENDPOINT_DEVICE_ID, L"");

        // device instance Id
        match->DeviceInstanceId = matchObject.GetNamedString(MIDI_CONFIG_JSON_ENDPOINT_COMMON_MATCH_PROPERTY_KEY_DEVICE_INSTANCE_ID, L"");

        // USB VID
        auto vid = matchObject.GetNamedNumber(MIDI_CONFIG_JSON_ENDPOINT_COMMON_MATCH_PROPERTY_KEY_VID, 0);
        if (vid < 0 || vid > std::numeric_limits<uint16_t>::max()) vid = 0;
        match->UsbVendorId = static_cast<uint16_t>(vid);

        // USB PID
        auto pid = matchObject.GetNamedNumber(MIDI_CONFIG_JSON_ENDPOINT_COMMON_MATCH_PROPERTY_KEY_PID, 0);
        if (pid < 0 || pid > std::numeric_limits<uint16_t>::max()) pid = 0;
        match->UsbProductId = static_cast<uint16_t>(pid);

        // USB Serial
        match->UsbSerialNumber = matchObject.GetNamedString(MIDI_CONFIG_JSON_ENDPOINT_COMMON_MATCH_PROPERTY_KEY_SERIAL, L"");

        // Manufacturer name and MIDI 2 product instance id
        match->DeviceManufacturerName = matchObject.GetNamedString(MIDI_CONFIG_JSON_ENDPOINT_COMMON_MATCH_PROPERTY_KEY_MANUFACTURER_NAME, L"");
        match->DeviceProductInstanceId = matchObject.GetNamedString(MIDI_CONFIG_JSON_ENDPOINT_COMMON_MATCH_PROPERTY_KEY_PRODUCT_INSTANCE_ID, L"");

        // Static IP address and port
        match->NetworkStaticIPAddress = matchObject.GetNamedString(MIDI_CONFIG_JSON_ENDPOINT_COMMON_MATCH_PROPERTY_KEY_STATIC_IP_ADDRESS, L"");

        auto port = matchObject.GetNamedNumber(MIDI_CONFIG_JSON_ENDPOINT_COMMON_MATCH_PROPERTY_KEY_NETWORK_PORT, 0);
        if (port < 0 || port > std::numeric_limits<uint16_t>::max()) port = 0;
        match->NetworkPort = static_cast<uint16_t>(port);

        // Endpoint name from the device
        match->TransportSuppliedEndpointName = matchObject.GetNamedString(MIDI_CONFIG_JSON_ENDPOINT_COMMON_MATCH_PROPERTY_KEY_TRANSPORT_SUPPLIED_NAME, L"");

        // parent device name
        match->ParentDeviceName = matchObject.GetNamedString(MIDI_CONFIG_JSON_ENDPOINT_COMMON_MATCH_PROPERTY_KEY_PARENT_DEVICE_NAME, L"");

        match->Normalize();

        return match;

    }
    catch (...)
    {
        OutputDebugString(L"Exception parsing MIDI Endpoint Match JSON.");
    }

    return nullptr;
}



_Use_decl_annotations_
bool MidiEndpointMatchCriteria::WriteJson(json::JsonObject& matchObject)
{
    Normalize();

    try
    {
        if (!EndpointDeviceId.empty())
        {
            matchObject.SetNamedValue(
                MIDI_CONFIG_JSON_ENDPOINT_COMMON_MATCH_PROPERTY_KEY_ENDPOINT_DEVICE_ID,
                json::JsonValue::CreateStringValue(EndpointDeviceId)
            );
        }

        // device instance id, for bring-up before the SWD is created
        if (!DeviceInstanceId.empty())
        {
            matchObject.SetNamedValue(
                MIDI_CONFIG_JSON_ENDPOINT_COMMON_MATCH_PROPERTY_KEY_DEVICE_INSTANCE_ID,
                json::JsonValue::CreateStringValue(DeviceInstanceId)
            );
        }


        // VID, PID, Serial
        if (UsbVendorId > 0)
        {
            matchObject.SetNamedValue(
                MIDI_CONFIG_JSON_ENDPOINT_COMMON_MATCH_PROPERTY_KEY_VID,
                json::JsonValue::CreateNumberValue(UsbVendorId)
            );
        }

        if (UsbProductId > 0)
        {
            matchObject.SetNamedValue(
                MIDI_CONFIG_JSON_ENDPOINT_COMMON_MATCH_PROPERTY_KEY_PID,
                json::JsonValue::CreateNumberValue(UsbProductId)
            );
        }

        if (!UsbSerialNumber.empty())
        {
            matchObject.SetNamedValue(
                MIDI_CONFIG_JSON_ENDPOINT_COMMON_MATCH_PROPERTY_KEY_SERIAL,
                json::JsonValue::CreateStringValue(UsbSerialNumber)
            );
        }

        // Manufacturer and Product Instance Id
        if (!DeviceManufacturerName.empty())
        {
            matchObject.SetNamedValue(
                MIDI_CONFIG_JSON_ENDPOINT_COMMON_MATCH_PROPERTY_KEY_MANUFACTURER_NAME,
                json::JsonValue::CreateStringValue(DeviceManufacturerName)
            );
        }

        if (!DeviceProductInstanceId.empty())
        {
            matchObject.SetNamedValue(
                MIDI_CONFIG_JSON_ENDPOINT_COMMON_MATCH_PROPERTY_KEY_PRODUCT_INSTANCE_ID,
                json::JsonValue::CreateStringValue(DeviceProductInstanceId)
            );
        }

        // IP Address and Port
        if (!NetworkStaticIPAddress.empty())
        {
            matchObject.SetNamedValue(
                MIDI_CONFIG_JSON_ENDPOINT_COMMON_MATCH_PROPERTY_KEY_STATIC_IP_ADDRESS,
                json::JsonValue::CreateStringValue(NetworkStaticIPAddress)
            );
        }

        if(NetworkPort > 0)
        {
            matchObject.SetNamedValue(
                MIDI_CONFIG_JSON_ENDPOINT_COMMON_MATCH_PROPERTY_KEY_NETWORK_PORT,
                json::JsonValue::CreateNumberValue(NetworkPort)
            );
        }

        // Transport supplied name
        if (!TransportSuppliedEndpointName.empty())
        {
            matchObject.SetNamedValue(
                MIDI_CONFIG_JSON_ENDPOINT_COMMON_MATCH_PROPERTY_KEY_TRANSPORT_SUPPLIED_NAME,
                json::JsonValue::CreateStringValue(TransportSuppliedEndpointName)
            );
        }

        // Parent Device name
        if (!ParentDeviceName.empty())
        {
            matchObject.SetNamedValue(
                MIDI_CONFIG_JSON_ENDPOINT_COMMON_MATCH_PROPERTY_KEY_PARENT_DEVICE_NAME,
                json::JsonValue::CreateStringValue(ParentDeviceName)
            );
        }

        return true;
    }
    catch (...)
    {
        OutputDebugString(L"Exception writing MIDI Endpoint Match JSON.");
    }

    return false;
}


_Use_decl_annotations_
bool MidiEndpointMatchCriteria::IsMatch(MidiEndpointMatchCriteria const& matchValues)
{
    Normalize();

    // in priority order so we confirm on the most important ones first.

    // Endpoint device id
    if (!matchValues.EndpointDeviceId.empty() && 
        matchValues.EndpointDeviceId == EndpointDeviceId)
    {
        return true;
    }

    // Device Instance Id 
    if (!matchValues.DeviceInstanceId.empty() && 
        matchValues.DeviceInstanceId == DeviceInstanceId)
    {
        return true;
    }

    // VID/PID/Serial
    if (matchValues.UsbVendorId > 0 &&
        matchValues.UsbProductId > 0 && 
        matchValues.UsbVendorId == UsbVendorId &&
        matchValues.UsbProductId == UsbProductId)
    {
        if (matchValues.UsbSerialNumber.empty())
        {
            // we're not matching on serial, and VID/PID match, so we're good
            return true;
        }
        else if (matchValues.UsbSerialNumber == UsbSerialNumber)
        {
            // serial isn't empty, and it matches
            return true;
        }
    }

    // Manufacturer and product instance id
    if (!matchValues.DeviceManufacturerName.empty() && 
        !matchValues.DeviceProductInstanceId.empty() &&
        matchValues.DeviceManufacturerName == DeviceManufacturerName &&
        matchValues.DeviceProductInstanceId == DeviceProductInstanceId)
    {
        return true;
    }


    // Static IP address and port
    if (!matchValues.NetworkStaticIPAddress.empty() &&
        matchValues.NetworkPort > 0 &&
        matchValues.NetworkStaticIPAddress == NetworkStaticIPAddress &&
        matchValues.NetworkPort == NetworkPort)
    {
        return true;
    }

    // Transport-supplied name
    if (!matchValues.TransportSuppliedEndpointName.empty() &&
        matchValues.TransportSuppliedEndpointName == TransportSuppliedEndpointName)
    {
        return true;
    }

    // Parent device name. This is going to restrict us to a single UMP endpoint per device
    // so it may need to be removed, or combined with other criteria in the future
    if (!matchValues.ParentDeviceName.empty() &&
        matchValues.ParentDeviceName == ParentDeviceName)
    {
        return true;
    }


    return false;
}



#pragma pop_macro("max")
#endif  // ifdef max
