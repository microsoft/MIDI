// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App SDK and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

#include "pch.h"
#include "MidiServiceConfigEndpointMatchCriteria.h"
#include "ServiceConfig.MidiServiceConfigEndpointMatchCriteria.g.cpp"

namespace winrt::Microsoft::Windows::Devices::Midi2::ServiceConfig::implementation
{
    winrt::hstring MidiServiceConfigEndpointMatchCriteria::GetConfigJson() const noexcept
    {
        json::JsonObject matchObject;

        if (!m_endpointDeviceId.empty())
        {
            matchObject.SetNamedValue(MIDI_CONFIG_JSON_ENDPOINT_COMMON_SEARCH_PROPERTY_KEY_SWD, json::JsonValue::CreateStringValue(m_endpointDeviceId));
        }

        if (!m_usbSerialNumber.empty() && m_usbVendorId > 0 && m_usbProductId > 0)
        {
            matchObject.SetNamedValue(MIDI_CONFIG_JSON_ENDPOINT_COMMON_SEARCH_PROPERTY_KEY_SERIAL, json::JsonValue::CreateStringValue(m_usbSerialNumber));
            matchObject.SetNamedValue(MIDI_CONFIG_JSON_ENDPOINT_COMMON_SEARCH_PROPERTY_KEY_VID, json::JsonValue::CreateNumberValue(m_usbVendorId));
            matchObject.SetNamedValue(MIDI_CONFIG_JSON_ENDPOINT_COMMON_SEARCH_PROPERTY_KEY_PID, json::JsonValue::CreateNumberValue(m_usbProductId));
        }

        if (!m_staticIPAddress.empty() && m_port > 0)
        {
            matchObject.SetNamedValue(MIDI_CONFIG_JSON_ENDPOINT_COMMON_SEARCH_PROPERTY_KEY_STATIC_IP_ADDRESS, json::JsonValue::CreateStringValue(m_staticIPAddress));
            matchObject.SetNamedValue(MIDI_CONFIG_JSON_ENDPOINT_COMMON_SEARCH_PROPERTY_KEY_UDP_PORT, json::JsonValue::CreateNumberValue(m_port));
        }

        if (!m_transportSuppliedEndpointName.empty())
        {
            matchObject.SetNamedValue(MIDI_CONFIG_JSON_ENDPOINT_COMMON_SEARCH_PROPERTY_KEY_TRANSPORT_SUPPLIED_NAME, json::JsonValue::CreateStringValue(m_transportSuppliedEndpointName));
        }

        if (!m_parentDeviceName.empty())
        {
            matchObject.SetNamedValue(MIDI_CONFIG_JSON_ENDPOINT_COMMON_SEARCH_PROPERTY_KEY_PARENT_DEVICE_NAME, json::JsonValue::CreateStringValue(m_parentDeviceName));
        }

        return matchObject.Stringify();
    }
}
