// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================


#include "pch.h"
#include "MidiFunctionBlock.h"
#include "MidiFunctionBlock.g.cpp"

#define JSON_KEY_FB_NUMBER L"functionBlockNumber"
#define JSON_KEY_FB_NAME L"name"
#define JSON_KEY_FB_ACTIVE L"active"
#define JSON_KEY_FB_UIHINT L"uiHint"
#define JSON_KEY_FB_MIDI10 L"midi10"
#define JSON_KEY_FB_DIRECTION L"direction"
#define JSON_KEY_FB_FIRSTGROUP L"firstGroupIndex"
#define JSON_KEY_FB_NUMGROUPSSPANNED L"numberOfGroupsSpanned"
#define JSON_KEY_FB_MIDICIFORMAT L"midiCIMessageFormat"

using namespace winrt::Windows::Data::Json;

namespace winrt::Windows::Devices::Midi2::implementation
{
    _Use_decl_annotations_
    bool MidiFunctionBlock::UpdateFromMessages(winrt::array_view<midi2::MidiUmp128 const> /*messages*/) noexcept
    {
        return false;
    }

    _Use_decl_annotations_
    bool MidiFunctionBlock::UpdateFromJson(winrt::Windows::Data::Json::JsonObject const json) noexcept
    {
        // this needs to do some data validation, like checking group index, enums, etc.

        // this should only update the fields that exist, which is why all the checks

        if (json.HasKey(JSON_KEY_FB_NUMBER)) m_number = (uint8_t)json.GetNamedNumber(JSON_KEY_FB_NUMBER, 0);
        if (json.HasKey(JSON_KEY_FB_NAME)) m_name = json.GetNamedString(JSON_KEY_FB_NAME, L"");
        if (json.HasKey(JSON_KEY_FB_ACTIVE)) m_isActive = json.GetNamedBoolean(JSON_KEY_FB_ACTIVE, false);
        if (json.HasKey(JSON_KEY_FB_UIHINT)) m_uiHint = (MidiFunctionBlockUIHint)json.GetNamedNumber(JSON_KEY_FB_UIHINT, (int)MidiFunctionBlockUIHint::Unknown);
        if (json.HasKey(JSON_KEY_FB_MIDI10)) m_midi10Connection = (MidiFunctionBlockMidi10)json.GetNamedNumber(JSON_KEY_FB_MIDI10, (int)MidiFunctionBlockMidi10::Not10);
        if (json.HasKey(JSON_KEY_FB_DIRECTION)) m_direction = (MidiFunctionBlockDirection)json.GetNamedNumber(JSON_KEY_FB_DIRECTION, (int)MidiFunctionBlockDirection::Undefined);
        if (json.HasKey(JSON_KEY_FB_FIRSTGROUP)) m_firstGroupIndex = (uint8_t)json.GetNamedNumber(JSON_KEY_FB_FIRSTGROUP, 0);
        if (json.HasKey(JSON_KEY_FB_NUMGROUPSSPANNED)) m_numberOfGroupsSpanned = (uint8_t)json.GetNamedNumber(JSON_KEY_FB_NUMGROUPSSPANNED, 0);
        if (json.HasKey(JSON_KEY_FB_MIDICIFORMAT)) m_midiCIMessageVersionFormat = (uint8_t)json.GetNamedNumber(JSON_KEY_FB_MIDICIFORMAT, 0);


        // todo: return false on error

        return true;
    }

    _Use_decl_annotations_
    bool MidiFunctionBlock::UpdateFromJsonString(winrt::hstring const json) noexcept
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
    winrt::hstring MidiFunctionBlock::GetJsonString() noexcept
    {
        JsonObject jsonObject;

        jsonObject.SetNamedValue(JSON_KEY_FB_NUMBER, JsonValue::CreateNumberValue(Number()));
        jsonObject.SetNamedValue(JSON_KEY_FB_NAME, JsonValue::CreateStringValue(Name()));
        jsonObject.SetNamedValue(JSON_KEY_FB_ACTIVE, JsonValue::CreateBooleanValue(IsActive()));
        jsonObject.SetNamedValue(JSON_KEY_FB_UIHINT, JsonValue::CreateNumberValue((int)UIHint()));
        jsonObject.SetNamedValue(JSON_KEY_FB_MIDI10, JsonValue::CreateNumberValue((int)Midi10Connection()));
        jsonObject.SetNamedValue(JSON_KEY_FB_DIRECTION, JsonValue::CreateNumberValue((int)Direction()));
        jsonObject.SetNamedValue(JSON_KEY_FB_FIRSTGROUP, JsonValue::CreateNumberValue(FirstGroupIndex()));
        jsonObject.SetNamedValue(JSON_KEY_FB_NUMGROUPSSPANNED, JsonValue::CreateNumberValue(NumberOfGroupsSpanned()));
        jsonObject.SetNamedValue(JSON_KEY_FB_MIDICIFORMAT, JsonValue::CreateNumberValue(MidiCIMessageVersionFormat()));
        
        return jsonObject.Stringify();
    }
}
