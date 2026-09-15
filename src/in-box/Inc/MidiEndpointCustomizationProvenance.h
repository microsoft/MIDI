// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

#ifndef MIDI_ENDPOINT_CUSTOMIZATION_PROVENANCE_H
#define MIDI_ENDPOINT_CUSTOMIZATION_PROVENANCE_H

// this def messes with json so we need to undef it here
#pragma push_macro("GetObject")
#undef GetObject
#include <winrt/Windows.Data.Json.h>
#pragma pop_macro("GetObject")

#include <string>
#include <memory>

// Describes the device a stored customization was created for, so that an entry which no longer
// matches anything can still be recognized by the person who made it. None of it takes part in
// matching: a customization is matched by the "match" object and nothing else, deliberately, and
// these values would match the wrong device if they were ever allowed to.
//
//         "provenance":
//         {
//           "createdFor": "Tascam Model 12",
//           "created": "2026-04-11T18:15:03Z",
//           "usbVendorId": 1604,
//           "usbProductId": 32863,
//           "transportSuppliedName": "Model 12",
//           "latencySource": "measured",
//           "latencyMeasured": "2026-09-12T14:03:11Z"
//         },

#define MIDI_CONFIG_JSON_ENDPOINT_COMMON_PROVENANCE_PROPERTY_KEY                        L"provenance"

#define MIDI_CONFIG_JSON_PROVENANCE_PROPERTY_KEY_CREATED_FOR                            L"createdFor"
#define MIDI_CONFIG_JSON_PROVENANCE_PROPERTY_KEY_CREATED                                L"created"
#define MIDI_CONFIG_JSON_PROVENANCE_PROPERTY_KEY_VID                                    L"usbVendorId"
#define MIDI_CONFIG_JSON_PROVENANCE_PROPERTY_KEY_PID                                    L"usbProductId"
#define MIDI_CONFIG_JSON_PROVENANCE_PROPERTY_KEY_SERIAL                                 L"usbSerialNumber"
#define MIDI_CONFIG_JSON_PROVENANCE_PROPERTY_KEY_MANUFACTURER_NAME                      L"manufacturerName"
#define MIDI_CONFIG_JSON_PROVENANCE_PROPERTY_KEY_TRANSPORT_SUPPLIED_NAME                L"transportSuppliedName"
#define MIDI_CONFIG_JSON_PROVENANCE_PROPERTY_KEY_PARENT_DEVICE_INSTANCE_ID              L"parentDeviceInstanceId"
#define MIDI_CONFIG_JSON_PROVENANCE_PROPERTY_KEY_LATENCY_SOURCE                         L"latencySource"
#define MIDI_CONFIG_JSON_PROVENANCE_PROPERTY_KEY_LATENCY_MEASURED                       L"latencyMeasured"

#define MIDI_CONFIG_JSON_PROVENANCE_LATENCY_SOURCE_VALUE_ENTERED                        L"entered"
#define MIDI_CONFIG_JSON_PROVENANCE_LATENCY_SOURCE_VALUE_MEASURED                       L"measured"

namespace WindowsMidiServicesPluginConfigurationLib
{
    enum class MidiCustomizationLatencySource
    {
        Unspecified = 0,
        Entered = 1,
        Measured = 2,
    };

    class MidiEndpointCustomizationProvenance
    {
    public:
        static winrt::hstring PropertyKey;

        MidiEndpointCustomizationProvenance() = default;

        static std::shared_ptr<MidiEndpointCustomizationProvenance> FromJson(
            _In_ ::winrt::Windows::Data::Json::JsonObject const& provenanceObject);

        _Success_(return == true)
        bool WriteJson(_In_ ::winrt::Windows::Data::Json::JsonObject& provenanceObject);

        bool IsEmpty() const noexcept;
        void Normalize();

        // What the customer would call the device. The transport-supplied name at the time the
        // customization was created, or the customer's own name for it if they had already set one.
        winrt::hstring CreatedFor{};
        winrt::hstring Created{};

        uint16_t UsbVendorId{ 0 };
        uint16_t UsbProductId{ 0 };
        winrt::hstring UsbSerialNumber{};

        winrt::hstring ManufacturerName{};
        winrt::hstring TransportSuppliedName{};
        winrt::hstring ParentDeviceInstanceId{};

        // Whether a stored latency was typed in or measured. A measured value needs a loopback
        // cable to reproduce, so an interface which offers to discard a customization should say
        // which kind it is about to throw away.
        MidiCustomizationLatencySource LatencySource{ MidiCustomizationLatencySource::Unspecified };
        winrt::hstring LatencyMeasured{};
    };
}

#endif
