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


// TODO: DeviceIdentity information like sysex id, device family, etc

using namespace winrt::Windows::Data::Json;

namespace winrt::Windows::Devices::Midi2::implementation
{
    _Use_decl_annotations_
    bool MidiEndpointInformation::UpdateFromMessages(collections::IIterable<midi2::MidiMessage128> messages) noexcept
    {
        try
        {
            auto nameMessages{ winrt::single_threaded_vector<midi2::MidiMessage128>() };

            //std::cout << __FUNCTION__ << " enter" << std::endl;

            for (auto message : messages)
            {
                if (MidiMessageUtility::GetMessageTypeFromFirstMessageWord(message.Word0()) != MidiMessageType::Stream128)
                {
                    // list contains non-stream messages. Abort.
                    return false;
                }

                switch (MidiMessageUtility::GetStatusFromStreamMessageFirstWord(message.Word0()))
                {
                case (MIDI_STREAM_MESSAGE_STATUS_FUNCTION_BLOCK_INFO_NOTIFICATION):
                    return UpdateFromInfoNotificationMessage(message);

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
        catch (...)
        {
            internal::LogGeneralError(__FUNCTION__, L" exception updating from message list.");

            return false;
        }

    }

    _Use_decl_annotations_
    bool MidiEndpointInformation::UpdateFromInfoNotificationMessage(midi2::MidiMessage128 message) noexcept
    {
        try
        {
            if (MidiMessageUtility::GetStatusFromStreamMessageFirstWord(message.Word0()) != MIDI_STREAM_MESSAGE_STATUS_FUNCTION_BLOCK_INFO_NOTIFICATION)
            {
                return false;
            }

            if (MidiMessageUtility::GetFormFromStreamMessageFirstWord(message.Word0()) == MIDI_STREAM_MESSAGE_STANDARD_FORM0)
            {

                m_umpVersionMajor = internal::GetEndpointInfoNotificationUmpVersionMajorFirstWord(message.Word0());
                m_umpVersionMinor = internal::GetEndpointInfoNotificationUmpVersionMinorFirstWord(message.Word0());

                m_hasStaticFunctionBlocks = internal::GetEndpointInfoNotificationStaticFunctionBlocksFlagFromSecondWord(message.Word1());
                m_functionBlockCount = internal::GetEndpointInfoNotificationNumberOfFunctionBlocksFromSecondWord(message.Word1());

                m_supportsMidi10Protocol = internal::GetEndpointInfoNotificationMidi1ProtocolCapabilityFromSecondWord(message.Word1());
                m_supportsMidi20Protocol = internal::GetEndpointInfoNotificationMidi2ProtocolCapabilityFromSecondWord(message.Word1());

                m_supportsReceivingJRTimestamps = internal::GetEndpointInfoNotificationReceiveJRTimestampCapabilityFromSecondWord(message.Word1());
                m_supportsSendingJRTimestamps = internal::GetEndpointInfoNotificationTransmitJRTimestampCapabilityFromSecondWord(message.Word1());

                return true;
            }
            else
            {
                //std::cout << __FUNCTION__ << " Form not recognized." << std::endl;
                // we don't recognize this form. Abort.
                return false;
            }
        }
        catch (...)
        {
            internal::LogGeneralError(__FUNCTION__, L" exception updating from notification message.");

            return false;
        }
    }


    _Use_decl_annotations_
    bool MidiEndpointInformation::UpdateFromJson(winrt::Windows::Data::Json::JsonObject const json) noexcept
    {
        try
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
        catch (...)
        {
            internal::LogGeneralError(__FUNCTION__, L" exception updating from json object.");

            return false;
        }
    }


    _Use_decl_annotations_
    bool MidiEndpointInformation::UpdateFromJsonString(winrt::hstring json) noexcept
    {
        try
        {
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
        catch (...)
        {
            internal::LogGeneralError(__FUNCTION__, L" exception updating from json string.");

            return false;
        }
    }


    _Use_decl_annotations_
    winrt::hstring MidiEndpointInformation::GetJsonString() noexcept
    {
        try
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
        catch (...)
        {
            internal::LogGeneralError(__FUNCTION__, L" exception getting json string.");

            return L"";
        }
    }

}
