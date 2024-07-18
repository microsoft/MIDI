// Copyright (c) Microsoft Corporation. All rights reserved.

#include "pch.h"
//#include "bytestreamToUMP.h"
//#include "midi2.BS2UMPtransform.h"

_Use_decl_annotations_
HRESULT
CMidi2UmpProtocolDownscalerMidiTransform::Initialize(
    LPCWSTR Device,
    PTRANSFORMCREATIONPARAMS CreationParams,
    DWORD *,
    IMidiCallback * Callback,
    LONGLONG Context,
    IUnknown* /*MidiDeviceManager*/
)
{
    TraceLoggingWrite(
        MidiUmpProtocolDownscalerTransformTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this")
    );

    // this only converts UMP to UMP
    RETURN_HR_IF(ERROR_UNSUPPORTED_TYPE, CreationParams->DataFormatIn != MidiDataFormat_UMP);
    RETURN_HR_IF(ERROR_UNSUPPORTED_TYPE, CreationParams->DataFormatOut != MidiDataFormat_UMP);

    m_Device = Device;
    m_Callback = Callback;
    m_Context = Context;

    return S_OK;
}

HRESULT
CMidi2UmpProtocolDownscalerMidiTransform::Cleanup()
{
    TraceLoggingWrite(
        MidiUmpProtocolDownscalerTransformTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this")
        );

    return S_OK;
}

#define USE_LIBMIDI2_FOR_UMP_TO_MIDI1_PROTOCOL

#ifndef USE_LIBMIDI2_FOR_UMP_TO_MIDI1_PROTOCOL

#define SCALE_DOWN_FROM_16_BIT_VALUE    16
#define SCALE_DOWN_FROM_32_BIT_VALUE    32
#define SCALE_TO_7_BIT_VALUE            7
#define SCALE_TO_14_BIT_VALUE           14

#endif

//// TEMP due to missing libmidi2 function impl
//void umpToMIDI1Protocol::increaseWrite()
//{
//    bufferLength++;
//    writeIndex++;
//    if (writeIndex == UMPTOPROTO1_BUFFER) {
//        writeIndex = 0;
//    }
//}

_Use_decl_annotations_
HRESULT
CMidi2UmpProtocolDownscalerMidiTransform::SendMidiMessage(
    PVOID Data,
    UINT Length,
    LONGLONG Timestamp
)
{
    TraceLoggingWrite(
        MidiUmpProtocolDownscalerTransformTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this")
    );


    if (Length >= sizeof(uint32_t))
    {
#ifdef USE_LIBMIDI2_FOR_UMP_TO_MIDI1_PROTOCOL
        // using libmidi2. It has a bug it it at the moment.
        // 
        // Send the UMP(s) to the parser
        uint32_t* data = (uint32_t*)Data;
        for (UINT i = 0; i < (Length / sizeof(uint32_t)); i++)
        {
            m_umpToMidi1.UMPStreamParse(data[i]);
        }

        // retrieve the message from the parser
        // and send it on
        while (m_umpToMidi1.availableUMP())
        {
            uint32_t words[MAXIMUM_LOOPED_DATASIZE / sizeof(uint32_t)];
            UINT wordCount{ 0 };

            for (wordCount = 0; wordCount < _countof(words) && m_umpToMidi1.availableUMP(); wordCount++)
            {
                words[wordCount] = m_umpToMidi1.readUMP();
            }

            if (wordCount > 0)
            {
                RETURN_IF_FAILED(m_Callback->Callback(&(words[0]), wordCount * sizeof(uint32_t), Timestamp, m_Context));
            }
        }
#else
        auto originalWord0 = internal::MidiWord0FromVoidMessageDataPointer(Data);

        // only translate MT4 to MT2 (MIDI 2.0 protocol to MIDI 1.0 protocol)
        if (internal::GetUmpMessageTypeFromFirstWord(originalWord0) == MIDI_UMP_MESSAGE_TYPE_MIDI2_CHANNEL_VOICE_64)
        {
            // downscale MIDI 2 to MIDI 1. Note that we treat the note index as a note number no matter 
            // what. There was quite a bit of discussion about this, and the main architects in the MIDI
            // association felt that a wrong note is better than no note. So in cases where the note number
            // is an index and exact pitch is specified, this will send an incorrect note. This is by
            // consensus and therefore by design.

            //uint8_t* incomingDataPointer = (uint8_t*)Data;

            auto originalWord1 = internal::MidiWord1FromVoidMessageDataPointer(((uint8_t*)Data) + sizeof(uint32_t));

            uint8_t groupIndex = internal::GetGroupIndexFromFirstWord(originalWord0);
            uint8_t channelIndex = internal::GetChannelIndexFromFirstWord(originalWord0);
            uint8_t status = internal::GetStatusFromChannelVoiceMessage(originalWord0);

            // converting only to 32 bit messages, but some MIDI 2.0 messages convert into multiple MIDI 1.0 messages
            // a stack reference here is ok because none of these callbacks are async in any way. It's a chain.

            // first we check for messages that return more than one resulting translated message
            if (status == MIDI_UMP_MIDI2_CHANNEL_VOICE_STATUS_PROGRAM_CHANGE ||
                status == MIDI_UMP_MIDI2_CHANNEL_VOICE_STATUS_REG_CONTROLLER ||
                status == MIDI_UMP_MIDI2_CHANNEL_VOICE_STATUS_ASSIGN_CONTROLLER)
            {
                uint32_t resultingMessages[4];
                uint32_t resultingMessageCount{ 0 };

                if (status == MIDI_UMP_MIDI2_CHANNEL_VOICE_STATUS_PROGRAM_CHANGE)
                {
                    // this one needs to create three separate messages so we special case here
                    uint8_t program = MIDIWORDBYTE1(originalWord1);

                    // the bank is valid if the least significant bit in word 0 is set
                    bool bankValid = (originalWord0 & (uint32_t)0x1) == 0x1;

                    resultingMessageCount = 0;

                    if (bankValid)
                    {
                        uint8_t bankMsb = MIDIWORDBYTE3(originalWord1);
                        uint8_t bankLsb = MIDIWORDBYTE4(originalWord1);

                        resultingMessages[resultingMessageCount++] = UMPMessage::mt2CC(groupIndex, channelIndex, MIDI_UMP_MIDI1_BANK_SELECT_MSB_CC_INDEX, bankMsb);
                        resultingMessages[resultingMessageCount++] = UMPMessage::mt2CC(groupIndex, channelIndex, MIDI_UMP_MIDI1_BANK_SELECT_LSB_CC_INDEX, bankLsb);;
                    }

                    resultingMessages[resultingMessageCount++] = UMPMessage::mt2ProgramChange(groupIndex, channelIndex, program);
                }
                else if (status == MIDI_UMP_MIDI2_CHANNEL_VOICE_STATUS_REG_CONTROLLER)
                {
                    // RPN - converts to four MIDI 1.0 messages

                    uint8_t bank = internal::CleanupByte7(MIDIWORDBYTE3(originalWord0));
                    uint8_t index = internal::CleanupByte7(MIDIWORDBYTE4(originalWord0));

                    uint8_t coarse = internal::CleanupByte7(MIDIWORDBYTE1(originalWord1));
                    uint8_t fine = internal::CleanupByte7(MIDIWORDBYTE2(originalWord1));

                    resultingMessageCount = 0;

                    resultingMessages[resultingMessageCount++] = UMPMessage::mt2CC(groupIndex, channelIndex, MIDI_RPN_CC_NUMBER_BANK, bank);
                    resultingMessages[resultingMessageCount++] = UMPMessage::mt2CC(groupIndex, channelIndex, MIDI_RPN_CC_NUMBER_INDEX, index);
                    resultingMessages[resultingMessageCount++] = UMPMessage::mt2CC(groupIndex, channelIndex, MIDI_RPN_CC_NUMBER_DATA_COARSE, coarse);
                    resultingMessages[resultingMessageCount++] = UMPMessage::mt2CC(groupIndex, channelIndex, MIDI_RPN_CC_NUMBER_DATA_FINE, fine);
                }
                else if (status == MIDI_UMP_MIDI2_CHANNEL_VOICE_STATUS_ASSIGN_CONTROLLER)
                {
                    // NRPN - converts to four MIDI 1.0 messages

                    uint8_t bank = MIDIWORDBYTE3(originalWord0);
                    uint8_t index = MIDIWORDBYTE4(originalWord0);

                    uint8_t coarse = internal::CleanupByte7(MIDIWORDBYTE1(originalWord1));
                    uint8_t fine = internal::CleanupByte7(MIDIWORDBYTE2(originalWord1));

                    resultingMessageCount = 0;

                    resultingMessages[resultingMessageCount++] = UMPMessage::mt2CC(groupIndex, channelIndex, MIDI_NRPN_CC_NUMBER_BANK, bank);
                    resultingMessages[resultingMessageCount++] = UMPMessage::mt2CC(groupIndex, channelIndex, MIDI_NRPN_CC_NUMBER_INDEX, index);
                    resultingMessages[resultingMessageCount++] = UMPMessage::mt2CC(groupIndex, channelIndex, MIDI_NRPN_CC_NUMBER_DATA_COARSE, coarse);
                    resultingMessages[resultingMessageCount++] = UMPMessage::mt2CC(groupIndex, channelIndex, MIDI_NRPN_CC_NUMBER_DATA_FINE, fine);
                }

                // send all the generated messages in order
                if (resultingMessageCount > 0 && m_Callback != nullptr)
                {
                    for (uint32_t i = 0; i < resultingMessageCount; i++)
                    {
                        RETURN_IF_FAILED(m_Callback->Callback(&resultingMessages[i], sizeof(uint32_t), Timestamp + i, m_Context));
                    }
                }
            }
            else
            {
                // these messages all result in a single new message to send.
                // the one exception is the fall-through which just passes along the original data. 

                uint32_t newWord0;
                PVOID newData{ nullptr };
                UINT newLength{ 0 };

                if (status == MIDI_UMP_MIDI2_CHANNEL_VOICE_STATUS_NOTE_OFF)
                {
                    //TraceLoggingWrite(
                    //    MidiUmpProtocolDownscalerTransformTelemetryProvider::Provider(),
                    //    __FUNCTION__,
                    //    TraceLoggingLevel(WINEVENT_LEVEL_INFO),
                    //    TraceLoggingPointer(this, "this"),
                    //    TraceLoggingWideString(L"Downscaling Note Off", MIDI_TRACE_EVENT_MESSAGE_FIELD)
                    //);

                    // note number / index is third byte
                    uint8_t noteNumber = MIDIWORDBYTE3(originalWord0);
                    uint8_t newVelocity = (uint8_t)(M2Utils::scaleDown(MIDIWORDSHORT1(originalWord1), SCALE_DOWN_FROM_16_BIT_VALUE, SCALE_TO_7_BIT_VALUE));
                    newWord0 = UMPMessage::mt2NoteOff(groupIndex, channelIndex, noteNumber, newVelocity);
                    newData = &newWord0;
                    newLength = sizeof(uint32_t);
                }

                else if (status == MIDI_UMP_MIDI2_CHANNEL_VOICE_STATUS_NOTE_ON)
                {
                    //TraceLoggingWrite(
                    //    MidiUmpProtocolDownscalerTransformTelemetryProvider::Provider(),
                    //    __FUNCTION__,
                    //    TraceLoggingLevel(WINEVENT_LEVEL_INFO),
                    //    TraceLoggingPointer(this, "this"),
                    //    TraceLoggingWideString(L"Downscaling Note On", MIDI_TRACE_EVENT_MESSAGE_FIELD)
                    //);

                    uint8_t noteNumber = MIDIWORDBYTE3(originalWord0);
                    uint8_t newVelocity = (uint8_t)(M2Utils::scaleDown(MIDIWORDSHORT1(originalWord1), SCALE_DOWN_FROM_16_BIT_VALUE, SCALE_TO_7_BIT_VALUE));

                    if (newVelocity == 0)
                    {
                        // MIDI 2.0 to MIDI 1.0 conversion rules
                        newVelocity = 1;
                    }

                    newWord0 = UMPMessage::mt2NoteOn(groupIndex, channelIndex, noteNumber, newVelocity);
                    newData = &newWord0;
                    newLength = sizeof(uint32_t);
                }

                else if (status == MIDI_UMP_MIDI2_CHANNEL_VOICE_STATUS_POLY_PRESSURE)
                {
                    uint8_t noteNumber = MIDIWORDBYTE3(originalWord0);
                    // pressure is entire second word
                    uint8_t newPressure = (uint8_t)(M2Utils::scaleDown(originalWord1, SCALE_DOWN_FROM_32_BIT_VALUE, SCALE_TO_7_BIT_VALUE));
                    newWord0 = UMPMessage::mt2PolyPressure(groupIndex, channelIndex, noteNumber, newPressure);

                    newData = &newWord0;
                    newLength = sizeof(uint32_t);
                }

                else if (status == MIDI_UMP_MIDI2_CHANNEL_VOICE_STATUS_CONTROL_CHANGE)
                {
                    uint8_t controlIndex = MIDIWORDBYTE3(originalWord0);
                    // value is entire second word
                    uint8_t newValue = (uint8_t)(M2Utils::scaleDown(originalWord1, SCALE_DOWN_FROM_32_BIT_VALUE, SCALE_TO_7_BIT_VALUE));
                    newWord0 = UMPMessage::mt2CC(groupIndex, channelIndex, controlIndex, newValue);

                    newData = &newWord0;
                    newLength = sizeof(uint32_t);
                }

                else if (status == MIDI_UMP_MIDI2_CHANNEL_VOICE_STATUS_CHANNEL_PRESSURE)
                {
                    // the pressure data value is the entire second word
                    uint8_t pressureData = (uint8_t)(M2Utils::scaleDown(originalWord1, SCALE_DOWN_FROM_32_BIT_VALUE, SCALE_TO_7_BIT_VALUE));

                    newWord0 = UMPMessage::mt2ChannelPressure(groupIndex, channelIndex, pressureData);

                    newData = &newWord0;
                    newLength = sizeof(uint32_t);
                }

                else if (status == MIDI_UMP_MIDI2_CHANNEL_VOICE_STATUS_PITCH_BEND)
                {
                    // the bend value is the entire second word
                    uint16_t bend = (uint16_t)M2Utils::scaleDown(originalWord1, SCALE_DOWN_FROM_32_BIT_VALUE, SCALE_TO_14_BIT_VALUE);

                    newWord0 = UMPMessage::mt2PitchBend(groupIndex, channelIndex, bend);

                    newData = &newWord0;
                    newLength = sizeof(uint32_t);
                }

                else
                {
                    // just pass it through without any processing. We don't have a specific conversion for this CV message
                    newData = Data;
                    newLength = Length;
                }

                //TraceLoggingWrite(
                //    MidiUmpProtocolDownscalerTransformTelemetryProvider::Provider(),
                //    __FUNCTION__,
                //    TraceLoggingLevel(WINEVENT_LEVEL_INFO),
                //    TraceLoggingPointer(this, "this"),
                //    TraceLoggingWideString(L"Sending single new message", MIDI_TRACE_EVENT_MESSAGE_FIELD)
                //);

                if (m_Callback != nullptr && newLength > 0 && newData != nullptr)
                {
                    return m_Callback->Callback(newData, newLength, Timestamp, m_Context);
                }

            }
        }
        else
        {
            // pass through because it's not a message type we handle

            //TraceLoggingWrite(
            //    MidiUmpProtocolDownscalerTransformTelemetryProvider::Provider(),
            //    __FUNCTION__,
            //    TraceLoggingLevel(WINEVENT_LEVEL_INFO),
            //    TraceLoggingPointer(this, "this"),
            //    TraceLoggingWideString(L"Not a message type we recognize. No downscaling applied", MIDI_TRACE_EVENT_MESSAGE_FIELD),
            //    TraceLoggingUInt32(originalWord0, "Word 0")
            //);


            if (m_Callback != nullptr)
            {
                return m_Callback->Callback(Data, Length, Timestamp, m_Context);
            }
        }


#endif

    }
    else
    {
        // not a valid UMP

        TraceLoggingWrite(
            MidiUmpProtocolDownscalerTransformTelemetryProvider::Provider(),
            MIDI_TRACE_EVENT_ERROR,
            TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
            TraceLoggingLevel(WINEVENT_LEVEL_INFO),
            TraceLoggingPointer(this, "this"),
            TraceLoggingWideString(L"invalid UMP", MIDI_TRACE_EVENT_MESSAGE_FIELD),
            TraceLoggingUInt32(Length, "Length in Bytes")
            );
    }

    return S_OK;
}
