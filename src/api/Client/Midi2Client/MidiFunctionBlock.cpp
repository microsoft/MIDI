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


using namespace winrt::Windows::Data::Json;


// TEMP
//#include <iostream>


namespace winrt::Windows::Devices::Midi2::implementation
{
    _Use_decl_annotations_
    bool MidiFunctionBlock::UpdateFromMessages(collections::IIterable<midi2::IMidiUniversalPacket> messages) noexcept
    {
        auto nameMessages{ winrt::single_threaded_vector<midi2::IMidiUniversalPacket>() };

        //std::cout << __FUNCTION__ << " enter" << std::endl;

        for (auto message : messages)
        {
            if (MidiMessageUtility::GetMessageTypeFromMessageFirstWord(message.PeekFirstWord()) != MidiMessageType::Stream128)
            {
                // list contains non-stream messages. Abort.
                return false;
            }

            switch (MidiMessageUtility::GetStatusFromStreamMessageFirstWord(message.PeekFirstWord()))
            {
            case (MIDI_STREAM_MESSAGE_STATUS_FUNCTION_BLOCK_INFO_NOTIFICATION):

                //std::cout << __FUNCTION__ << " info block notification" << std::endl;

                if (MidiMessageUtility::GetFormFromStreamMessageFirstWord(message.PeekFirstWord()) == MIDI_STREAM_MESSAGE_STANDARD_FORM0)
                {
                    auto ump128 = message.as<midi2::MidiMessage128>();

                    //std::cout << __FUNCTION__ << " form is correct. Setting info." << std::endl;

                    // word 0: active function block or not
                    m_isActive = internal::GetFunctionBlockActiveFlagFromInfoNotificationFirstWord(ump128.Word0());

                    // word 0: function block number
                    m_number = internal::GetFunctionBlockNumberFromInfoNotificationFirstWord(ump128.Word0());

                    // word 0: ui hint
                    m_uiHint = (midi2::MidiFunctionBlockUIHint)internal::GetFunctionBlockUIHintFromInfoNotificationFirstWord(ump128.Word0());

                    // word 0: MIDI 1.0 settings
                    m_midi10Connection = (midi2::MidiFunctionBlockMidi10)internal::GetFunctionBlockMidi10FromInfoNotificationFirstWord(ump128.Word0());

                    // word 0: direction
                    m_direction = (midi2::MidiFunctionBlockDirection)internal::GetFunctionBlockDirectionFromInfoNotificationFirstWord(ump128.Word0());



                    // word 1: first group
                    m_firstGroupIndex = internal::GetFunctionBlockFirstGroupFromInfoNotificationSecondWord(ump128.Word1());

                    // word 1: number of groups spanned
                    m_numberOfGroupsSpanned = internal::GetFunctionBlockNumberOfGroupsFromInfoNotificationSecondWord(ump128.Word1());



                    // word 2: MIDI CI Message Version / Form
                    m_midiCIMessageVersionFormat = internal::GetFunctionBlockMidiCIVersionFromInfoNotificationSecondWord(ump128.Word2());

                    // word 2: maximum number of SysEx8 streams
                    m_maxSysEx8Streams = internal::GetFunctionBlockMaxSysex8StreamsFromInfoNotificationSecondWord(ump128.Word2());
                }
                else
                {
                    //std::cout << __FUNCTION__ << " Form not recognized." << std::endl;
                    // we don't recognize this form. Abort.
                    return false;
                }
                break;

            case (MIDI_STREAM_MESSAGE_STATUS_FUNCTION_BLOCK_NAME_NOTIFICATION):
                //std::cout << __FUNCTION__ << " name notification" << std::endl;
                nameMessages.Append(message);
                break;

            default:
                // we don't recognize the message status, so abort
                return false;
            }

            if (nameMessages.Size() > 0)
            {
                m_name = MidiStreamMessageBuilder::ParseFunctionBlockNameNotificationMessages(nameMessages);
            }
        }

        return true;
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
        jsonObject.SetNamedValue(JSON_KEY_FB_NUMGROUPSSPANNED, JsonValue::CreateNumberValue(GroupCount()));
        jsonObject.SetNamedValue(JSON_KEY_FB_MIDICIFORMAT, JsonValue::CreateNumberValue(MidiCIMessageVersionFormat()));
        
        return jsonObject.Stringify();
    }


    _Use_decl_annotations_
    bool MidiFunctionBlock::UpdateFromDevPropertyStruct(MidiFunctionBlockProperty prop)
    {
        m_number = prop.BlockNumber;
        m_isActive = prop.IsActive;
        m_direction = (midi2::MidiFunctionBlockDirection)prop.Direction;
        m_firstGroupIndex = prop.FirstGroup;
        m_numberOfGroupsSpanned = prop.NumberOfGroupsSpanned;
        m_maxSysEx8Streams = prop.MaxSysEx8Streams;
        m_uiHint = (midi2::MidiFunctionBlockUIHint)prop.UIHint;
        m_midi10Connection = (midi2::MidiFunctionBlockMidi10)prop.Midi1;
        m_midiCIMessageVersionFormat = prop.MidiCIMessageVersionFormat;

        return true;
    }

}
