// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

#include <windows.h>

#include <atlbase.h>
#include <atlcom.h>
#include <atlctl.h>
#include <atlcoll.h>
#include <atlsync.h>

#include <atlconv.h>
#include <string>
#include <limits>

#include "MidiDefs.h"
#include "hstring_util.h"
#include "json_defs.h"

#include "MidiEndpointCustomizationProvenance.h"

#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Foundation.Collections.h>

namespace json = ::winrt::Windows::Data::Json;
namespace internal = ::WindowsMidiServicesInternal;

namespace WindowsMidiServicesPluginConfigurationLib
{
    winrt::hstring MidiEndpointCustomizationProvenance::PropertyKey{ MIDI_CONFIG_JSON_ENDPOINT_COMMON_PROVENANCE_PROPERTY_KEY };

    namespace
    {
        uint16_t ReadUsbId(_In_ json::JsonObject const& provenanceObject, _In_ winrt::hstring const& key) noexcept
        {
            auto const value = provenanceObject.GetNamedNumber(key, 0);

            // parenthesized so the windows.h max macro cannot capture it
            if (value <= 0 || value > (std::numeric_limits<uint16_t>::max)())
            {
                return 0;
            }

            return static_cast<uint16_t>(value);
        }
    }

    _Use_decl_annotations_
    std::shared_ptr<MidiEndpointCustomizationProvenance> MidiEndpointCustomizationProvenance::FromJson(
        json::JsonObject const& provenanceObject)
    {
        if (provenanceObject == nullptr)
        {
            return nullptr;
        }

        auto provenance = std::make_shared<MidiEndpointCustomizationProvenance>();

        if (provenance == nullptr)
        {
            return nullptr;
        }

        provenance->CreatedFor = provenanceObject.GetNamedString(MIDI_CONFIG_JSON_PROVENANCE_PROPERTY_KEY_CREATED_FOR, L"");
        provenance->Created = provenanceObject.GetNamedString(MIDI_CONFIG_JSON_PROVENANCE_PROPERTY_KEY_CREATED, L"");

        provenance->UsbVendorId = ReadUsbId(provenanceObject, MIDI_CONFIG_JSON_PROVENANCE_PROPERTY_KEY_VID);
        provenance->UsbProductId = ReadUsbId(provenanceObject, MIDI_CONFIG_JSON_PROVENANCE_PROPERTY_KEY_PID);
        provenance->UsbSerialNumber = provenanceObject.GetNamedString(MIDI_CONFIG_JSON_PROVENANCE_PROPERTY_KEY_SERIAL, L"");

        provenance->ManufacturerName = provenanceObject.GetNamedString(MIDI_CONFIG_JSON_PROVENANCE_PROPERTY_KEY_MANUFACTURER_NAME, L"");
        provenance->TransportSuppliedName = provenanceObject.GetNamedString(MIDI_CONFIG_JSON_PROVENANCE_PROPERTY_KEY_TRANSPORT_SUPPLIED_NAME, L"");
        provenance->ParentDeviceInstanceId = provenanceObject.GetNamedString(MIDI_CONFIG_JSON_PROVENANCE_PROPERTY_KEY_PARENT_DEVICE_INSTANCE_ID, L"");

        auto const latencySource = provenanceObject.GetNamedString(MIDI_CONFIG_JSON_PROVENANCE_PROPERTY_KEY_LATENCY_SOURCE, L"");

        if (latencySource == MIDI_CONFIG_JSON_PROVENANCE_LATENCY_SOURCE_VALUE_MEASURED)
        {
            provenance->LatencySource = MidiCustomizationLatencySource::Measured;
        }
        else if (latencySource == MIDI_CONFIG_JSON_PROVENANCE_LATENCY_SOURCE_VALUE_ENTERED)
        {
            provenance->LatencySource = MidiCustomizationLatencySource::Entered;
        }

        provenance->LatencyMeasured = provenanceObject.GetNamedString(MIDI_CONFIG_JSON_PROVENANCE_PROPERTY_KEY_LATENCY_MEASURED, L"");

        provenance->Normalize();

        return provenance;
    }

    void MidiEndpointCustomizationProvenance::Normalize()
    {
        CreatedFor = internal::TrimmedHStringCopy(CreatedFor);
        Created = internal::TrimmedHStringCopy(Created);
        UsbSerialNumber = internal::TrimmedHStringCopy(UsbSerialNumber);
        ManufacturerName = internal::TrimmedHStringCopy(ManufacturerName);
        TransportSuppliedName = internal::TrimmedHStringCopy(TransportSuppliedName);
        ParentDeviceInstanceId = internal::TrimmedHStringCopy(ParentDeviceInstanceId);
        LatencyMeasured = internal::TrimmedHStringCopy(LatencyMeasured);
    }

    bool MidiEndpointCustomizationProvenance::IsEmpty() const noexcept
    {
        return
            CreatedFor.empty() &&
            Created.empty() &&
            UsbVendorId == 0 &&
            UsbProductId == 0 &&
            UsbSerialNumber.empty() &&
            ManufacturerName.empty() &&
            TransportSuppliedName.empty() &&
            ParentDeviceInstanceId.empty() &&
            LatencySource == MidiCustomizationLatencySource::Unspecified &&
            LatencyMeasured.empty();
    }

    _Use_decl_annotations_
    bool MidiEndpointCustomizationProvenance::WriteJson(json::JsonObject& provenanceObject)
    {
        Normalize();

        try
        {
            // Everything here is optional, so an unset field is left out rather than written empty.
            // A merge into an existing entry then cannot erase what an earlier writer knew.
            if (!CreatedFor.empty())
            {
                provenanceObject.SetNamedValue(
                    MIDI_CONFIG_JSON_PROVENANCE_PROPERTY_KEY_CREATED_FOR,
                    json::JsonValue::CreateStringValue(CreatedFor));
            }

            if (!Created.empty())
            {
                provenanceObject.SetNamedValue(
                    MIDI_CONFIG_JSON_PROVENANCE_PROPERTY_KEY_CREATED,
                    json::JsonValue::CreateStringValue(Created));
            }

            if (UsbVendorId != 0)
            {
                provenanceObject.SetNamedValue(
                    MIDI_CONFIG_JSON_PROVENANCE_PROPERTY_KEY_VID,
                    json::JsonValue::CreateNumberValue(UsbVendorId));
            }

            if (UsbProductId != 0)
            {
                provenanceObject.SetNamedValue(
                    MIDI_CONFIG_JSON_PROVENANCE_PROPERTY_KEY_PID,
                    json::JsonValue::CreateNumberValue(UsbProductId));
            }

            if (!UsbSerialNumber.empty())
            {
                provenanceObject.SetNamedValue(
                    MIDI_CONFIG_JSON_PROVENANCE_PROPERTY_KEY_SERIAL,
                    json::JsonValue::CreateStringValue(UsbSerialNumber));
            }

            if (!ManufacturerName.empty())
            {
                provenanceObject.SetNamedValue(
                    MIDI_CONFIG_JSON_PROVENANCE_PROPERTY_KEY_MANUFACTURER_NAME,
                    json::JsonValue::CreateStringValue(ManufacturerName));
            }

            if (!TransportSuppliedName.empty())
            {
                provenanceObject.SetNamedValue(
                    MIDI_CONFIG_JSON_PROVENANCE_PROPERTY_KEY_TRANSPORT_SUPPLIED_NAME,
                    json::JsonValue::CreateStringValue(TransportSuppliedName));
            }

            if (!ParentDeviceInstanceId.empty())
            {
                provenanceObject.SetNamedValue(
                    MIDI_CONFIG_JSON_PROVENANCE_PROPERTY_KEY_PARENT_DEVICE_INSTANCE_ID,
                    json::JsonValue::CreateStringValue(ParentDeviceInstanceId));
            }

            if (LatencySource != MidiCustomizationLatencySource::Unspecified)
            {
                provenanceObject.SetNamedValue(
                    MIDI_CONFIG_JSON_PROVENANCE_PROPERTY_KEY_LATENCY_SOURCE,
                    json::JsonValue::CreateStringValue(
                        LatencySource == MidiCustomizationLatencySource::Measured
                        ? MIDI_CONFIG_JSON_PROVENANCE_LATENCY_SOURCE_VALUE_MEASURED
                        : MIDI_CONFIG_JSON_PROVENANCE_LATENCY_SOURCE_VALUE_ENTERED));
            }

            if (!LatencyMeasured.empty())
            {
                provenanceObject.SetNamedValue(
                    MIDI_CONFIG_JSON_PROVENANCE_PROPERTY_KEY_LATENCY_MEASURED,
                    json::JsonValue::CreateStringValue(LatencyMeasured));
            }

            return true;
        }
        catch (...)
        {
            return false;
        }
    }
}
