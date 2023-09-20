// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================

#include "pch.h"
#include "MidiEndpointInformation.h"
#include "MidiEndpointInformation.g.cpp"


#define JSON_KEY_EP_NAME L"name"
#define JSON_KEY_EP_PID L"productInstanceId"
#define JSON_KEY_EP_UMPVERMAJ L"umpVersionMajor"
#define JSON_KEY_EP_UMPVERMIN L"umpVersionMinor"
#define JSON_KEY_EP_STATICFB L"staticFunctionBlocks"
#define JSON_KEY_EP_NUMFB L"numberOfFunctionBlocks"
#define JSON_KEY_EP_MIDI2 L"midi2ProtocolCapability"
#define JSON_KEY_EP_MIDI1 L"midi1ProtocolCapability"
#define JSON_KEY_EP_RECJR L"receiveJRTimestampCapability"
#define JSON_KEY_EP_SENDJR L"sendJRTimestampCapability"

// TODO: DeviceIdentity information like sysex id, device family, etc

using namespace winrt::Windows::Data::Json;

namespace winrt::Windows::Devices::Midi2::implementation
{
    _Use_decl_annotations_
    bool MidiEndpointInformation::UpdateFromMessages(winrt::array_view<midi2::MidiMessage128 const> /*messages*/) noexcept
    {
        return false;
    }



    _Use_decl_annotations_
    bool MidiEndpointInformation::UpdateFromJson(winrt::Windows::Data::Json::JsonObject const json) noexcept
    {
        // this should only update the fields that exist, which is why all the checks

        if (json.HasKey(JSON_KEY_EP_NAME)) m_name = json.GetNamedString(JSON_KEY_EP_NAME);
        if (json.HasKey(JSON_KEY_EP_PID)) m_productInstanceId = json.GetNamedString(JSON_KEY_EP_PID);
        if (json.HasKey(JSON_KEY_EP_UMPVERMAJ)) m_umpVersionMajor = (uint8_t)json.GetNamedNumber(JSON_KEY_EP_UMPVERMAJ);
        if (json.HasKey(JSON_KEY_EP_UMPVERMIN)) m_umpVersionMinor = (uint8_t)json.GetNamedNumber(JSON_KEY_EP_UMPVERMIN);
        if (json.HasKey(JSON_KEY_EP_STATICFB)) m_hasStaticFunctionBlocks = json.GetNamedBoolean(JSON_KEY_EP_STATICFB);
        if (json.HasKey(JSON_KEY_EP_NUMFB)) m_functionBlockCount = (uint8_t)json.GetNamedNumber(JSON_KEY_EP_NUMFB);
        if (json.HasKey(JSON_KEY_EP_MIDI2)) m_supportsMidi20Protocol = json.GetNamedBoolean(JSON_KEY_EP_MIDI2);
        if (json.HasKey(JSON_KEY_EP_MIDI1)) m_supportsMidi10Protocol = json.GetNamedBoolean(JSON_KEY_EP_MIDI1);
        if (json.HasKey(JSON_KEY_EP_RECJR)) m_supportsReceivingJRTimestamps = json.GetNamedBoolean(JSON_KEY_EP_RECJR);
        if (json.HasKey(JSON_KEY_EP_SENDJR)) m_supportsSendingJRTimestamps = json.GetNamedBoolean(JSON_KEY_EP_SENDJR);

        // TODO: Device Identity fields


        // todo: return false on error

        return true;
    }


    _Use_decl_annotations_
    bool MidiEndpointInformation::UpdateFromJsonString(winrt::hstring json) noexcept
    {
        // TODO: Need to catch any exceptions in here

        JsonObject jsonObject{};

        if (JsonObject::TryParse(json, jsonObject))
        {
            return UpdateFromJson(jsonObject);
        }
        else
        {
            return false;
        }
    }


    _Use_decl_annotations_
    winrt::hstring MidiEndpointInformation::GetJsonString() noexcept
    {
        JsonObject jsonObject;

        jsonObject.SetNamedValue(JSON_KEY_EP_NAME, JsonValue::CreateStringValue(Name()));
        jsonObject.SetNamedValue(JSON_KEY_EP_PID, JsonValue::CreateStringValue(ProductInstanceId()));
        jsonObject.SetNamedValue(JSON_KEY_EP_UMPVERMAJ, JsonValue::CreateNumberValue(SpecificationVersionMajor()));
        jsonObject.SetNamedValue(JSON_KEY_EP_UMPVERMIN, JsonValue::CreateNumberValue(SpecificationVersionMinor()));
        jsonObject.SetNamedValue(JSON_KEY_EP_STATICFB, JsonValue::CreateBooleanValue(HasStaticFunctionBlocks()));
        jsonObject.SetNamedValue(JSON_KEY_EP_NUMFB, JsonValue::CreateNumberValue(FunctionBlockCount()));
        jsonObject.SetNamedValue(JSON_KEY_EP_MIDI2, JsonValue::CreateBooleanValue(SupportsMidi20Protocol()));
        jsonObject.SetNamedValue(JSON_KEY_EP_MIDI1, JsonValue::CreateBooleanValue(SupportsMidi10Protocol()));
        jsonObject.SetNamedValue(JSON_KEY_EP_RECJR, JsonValue::CreateBooleanValue(SupportsReceivingJRTimestamps()));
        jsonObject.SetNamedValue(JSON_KEY_EP_SENDJR, JsonValue::CreateBooleanValue(SupportsSendingJRTimestamps()));

        // TODO: Device Identity fields

        return jsonObject.Stringify();
    }

}
