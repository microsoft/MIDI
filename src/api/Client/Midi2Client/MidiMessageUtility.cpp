// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================

#include "pch.h"
#include "MidiMessageUtility.h"
#include "MidiMessageUtility.g.cpp"

namespace winrt::Windows::Devices::Midi2::implementation
{
    _Use_decl_annotations_
    bool MidiMessageUtility::ValidateMessage32MessageType(uint32_t const word0) noexcept
    {
        return internal::GetUmpLengthInMidiWordsFromFirstWord(word0) == 1;
    }

    _Use_decl_annotations_
    bool MidiMessageUtility::ValidateMessage64MessageType(uint32_t const word0) noexcept
    {
        return internal::GetUmpLengthInMidiWordsFromFirstWord(word0) == 2;
    }

    _Use_decl_annotations_
    bool MidiMessageUtility::ValidateMessage96MessageType(uint32_t const word0) noexcept
    {
        return internal::GetUmpLengthInMidiWordsFromFirstWord(word0) == 3;
    }

    _Use_decl_annotations_
    bool MidiMessageUtility::ValidateMessage128MessageType(uint32_t const word0) noexcept
    {
        return internal::GetUmpLengthInMidiWordsFromFirstWord(word0) == 4;
    }

    _Use_decl_annotations_
    midi2::MidiMessageType MidiMessageUtility::GetMessageTypeFromMessageFirstWord(uint32_t const word0) noexcept
    {
        return (midi2::MidiMessageType)internal::GetUmpMessageTypeFromFirstWord(word0);
    }

    _Use_decl_annotations_
    midi2::MidiPacketType MidiMessageUtility::GetPacketTypeFromMessageFirstWord(uint32_t const word0) noexcept
    {
        return (midi2::MidiPacketType)internal::GetUmpLengthInMidiWordsFromFirstWord(word0);
    }

    _Use_decl_annotations_
    midi2::MidiGroup MidiMessageUtility::GetGroupFromMessageFirstWord(_In_ uint32_t const word0)
    {
        auto group = winrt::make<MidiGroup>(internal::GetGroupIndexFromFirstWord(word0));
        return group;
    }


    // It's expected for the user to check to see if the message type has a group field before calling this
    _Use_decl_annotations_
    uint32_t MidiMessageUtility::ReplaceGroupInMessageFirstWord(uint32_t const word0, midi2::MidiGroup const newGroup) noexcept
    {
        return internal::GetFirstWordWithNewGroup(word0, newGroup.Index());
    }

    _Use_decl_annotations_
    bool MidiMessageUtility::MessageTypeHasGroupField(midi2::MidiMessageType const messageType) noexcept
    {
        return internal::MessageTypeHasGroupField((uint8_t)messageType);
    }


    _Use_decl_annotations_
    midi2::MidiChannel MidiMessageUtility::GetChannelFromMessageFirstWord(_In_ uint32_t const word0)
    {
        auto channel = winrt::make<MidiChannel>(internal::GetChannelIndexFromFirstWord(word0));
        return channel;
    }


    // It's expected for the user to check to see if the message type has a channel field before calling this
    _Use_decl_annotations_
    uint32_t MidiMessageUtility::ReplaceChannelInMessageFirstWord(uint32_t const word0, midi2::MidiChannel const newChannel) noexcept
    {
        return internal::GetFirstWordWithNewChannel(word0, newChannel.Index());
    }

    _Use_decl_annotations_
    bool MidiMessageUtility::MessageTypeHasChannelField(midi2::MidiMessageType const messageType) noexcept
    {
        return internal::MessageTypeHasChannelField((uint8_t)messageType);
    }


    _Use_decl_annotations_
    uint8_t MidiMessageUtility::GetStatusFromUtilityMessage(uint32_t const word0) noexcept
    {
        return internal::GetStatusFromUmp32FirstWord(word0);
    }

    _Use_decl_annotations_
    Midi1ChannelVoiceMessageStatus MidiMessageUtility::GetStatusFromMidi1ChannelVoiceMessage(uint32_t const word0) noexcept
    {
        return (Midi1ChannelVoiceMessageStatus)internal::GetStatusFromChannelVoiceMessage(word0);
    }

    _Use_decl_annotations_
    Midi2ChannelVoiceMessageStatus MidiMessageUtility::GetStatusFromMidi2ChannelVoiceMessageFirstWord(uint32_t const word0) noexcept
    {
        return (Midi2ChannelVoiceMessageStatus)internal::GetStatusFromChannelVoiceMessage(word0);
    }


    _Use_decl_annotations_
    uint8_t MidiMessageUtility::GetFormFromStreamMessageFirstWord(uint32_t const word0) noexcept
    {
        return internal::GetFormFromStreamMessageFirstWord(word0);
    }

    _Use_decl_annotations_
    uint16_t MidiMessageUtility::GetStatusFromStreamMessageFirstWord(uint32_t const word0) noexcept
    {
        return internal::GetStatusFromStreamMessageFirstWord(word0);
    }


    _Use_decl_annotations_
    uint8_t MidiMessageUtility::GetStatusBankFromFlexDataMessageFirstWord(uint32_t const word0) noexcept
    {
        return internal::GetStatusBankFromFlexDataMessageFirstWord(word0);
    }
    
    _Use_decl_annotations_
    uint8_t MidiMessageUtility::GetStatusFromFlexDataMessageFirstWord(uint32_t const word0) noexcept
    {
        return internal::GetStatusFromFlexDataMessageFirstWord(word0);
    }

    _Use_decl_annotations_
    uint8_t MidiMessageUtility::GetStatusFromSystemCommonMessage(_In_ uint32_t const word0) noexcept
    {
        return internal::GetStatusFromSystemCommonMessage(word0);
    }


    _Use_decl_annotations_
    uint8_t MidiMessageUtility::GetStatusFromDataMessage64FirstWord(_In_ uint32_t const word0) noexcept
    {
        return internal::GetStatusFromDataMessage64FirstWord(word0);
    }

    _Use_decl_annotations_
    uint8_t MidiMessageUtility::GetNumberOfBytesFromDataMessage64FirstWord(_In_ uint32_t const word0) noexcept
    {
        return internal::GetNumberOfBytesFromDataMessage64FirstWord(word0);
    }


    _Use_decl_annotations_
    uint8_t MidiMessageUtility::GetStatusFromDataMessage128FirstWord(_In_ uint32_t const word0) noexcept
    {
        return internal::GetStatusFromDataMessage128FirstWord(word0);
    }

    _Use_decl_annotations_
    uint8_t MidiMessageUtility::GetNumberOfBytesFromDataMessage128FirstWord(_In_ uint32_t const word0) noexcept
    {
        return internal::GetNumberOfBytesFromDataMessage128FirstWord(word0);
    }




    // Names used in this function are those used in the MIDI 2.0 Specification
    // This will need to grow over time as new messages are added
    _Use_decl_annotations_
    winrt::hstring MidiMessageUtility::GetMessageFriendlyNameFromFirstWord(uint32_t const word0) noexcept
    {
        switch (GetMessageTypeFromMessageFirstWord(word0))
        {
        case MidiMessageType::UtilityMessage32:
            switch (GetStatusFromUtilityMessage(word0))
            {
            case 0x0:
                return internal::ResourceManager::GetHString(IDS_MESSAGE_DESC_MT0_NOOP);
            case 0x1:
                return internal::ResourceManager::GetHString(IDS_MESSAGE_DESC_MT0_JR_CLOCK);
            case 0x2:
                return internal::ResourceManager::GetHString(IDS_MESSAGE_DESC_MT0_JR_TS);
            case 0x3:
                return internal::ResourceManager::GetHString(IDS_MESSAGE_DESC_MT0_TICKS_PQN);
            case 0x4:
                return internal::ResourceManager::GetHString(IDS_MESSAGE_DESC_MT0_DELTA_TICKS);

            default:
                return internal::ResourceManager::GetHString(IDS_MESSAGE_DESC_MT0_UNKNOWN);
            }
            break;

        case MidiMessageType::SystemCommon32:
            switch (GetStatusFromSystemCommonMessage(word0))
            {
            case 0xF0:
                return internal::ResourceManager::GetHString(IDS_MESSAGE_DESC_MT1_F0_RESERVED); //L"Reserved 0xF0";
            case 0xF1:
                return internal::ResourceManager::GetHString(IDS_MESSAGE_DESC_MT1_F1_TIME_CODE); //L"MIDI Time Code";
            case 0xF2:
                return internal::ResourceManager::GetHString(IDS_MESSAGE_DESC_MT1_F2_SONG_POSITION); //L"Song Position Pointer";
            case 0xF3:
                return internal::ResourceManager::GetHString(IDS_MESSAGE_DESC_MT1_F3_SONG_SELECT); //L"Song Select";
            case 0xF4:
                return internal::ResourceManager::GetHString(IDS_MESSAGE_DESC_MT1_F4_RESERVED); //L"Reserved 0xF4";
            case 0xF5:
                return internal::ResourceManager::GetHString(IDS_MESSAGE_DESC_MT1_F5_RESERVED); //L"Reserved 0xF5";
            case 0xF6:
                return internal::ResourceManager::GetHString(IDS_MESSAGE_DESC_MT1_F6_TUNE_REQUEST); //L"Tune Request";
            case 0xF7:
                return internal::ResourceManager::GetHString(IDS_MESSAGE_DESC_MT1_F7_RESERVED); //L"Reserved 0xF7";
            case 0xF8:
                return internal::ResourceManager::GetHString(IDS_MESSAGE_DESC_MT1_F8_TIMING_CLOCK); //L"Timing Clock";
            case 0xF9:
                return internal::ResourceManager::GetHString(IDS_MESSAGE_DESC_MT1_F9_RESERVED); //L"Reserved 0xF9";
            case 0xFA:
                return internal::ResourceManager::GetHString(IDS_MESSAGE_DESC_MT1_FA_START); //L"Start";
            case 0xFB:
                return internal::ResourceManager::GetHString(IDS_MESSAGE_DESC_MT1_FB_CONTINUE); //L"Continue";
            case 0xFC:
                return internal::ResourceManager::GetHString(IDS_MESSAGE_DESC_MT1_FC_STOP); //L"Stop";
            case 0xFD:
                return internal::ResourceManager::GetHString(IDS_MESSAGE_DESC_MT1_FD_RESERVED); //L"Reserved 0xFD";
            case 0xFE:
                return internal::ResourceManager::GetHString(IDS_MESSAGE_DESC_MT1_FE_ACTIVE_SENSE); //L"Active Sensing";
            case 0xFF:
                return internal::ResourceManager::GetHString(IDS_MESSAGE_DESC_MT1_FF_RESET); //L"Reset";

            default:
                return internal::ResourceManager::GetHString(IDS_MESSAGE_DESC_MT1_UNKNOWN); //L"System Common/Realtime Unknown";
            }
            break;

        case MidiMessageType::Midi1ChannelVoice32:
            switch (GetStatusFromMidi1ChannelVoiceMessage(word0))
            {
            case Midi1ChannelVoiceMessageStatus::NoteOff:
                return internal::ResourceManager::GetHString(IDS_MESSAGE_DESC_MT2_8_NOTE_OFF); //L"MIDI 1.0 Note Off";
            case Midi1ChannelVoiceMessageStatus::NoteOn:
                return internal::ResourceManager::GetHString(IDS_MESSAGE_DESC_MT2_9_NOTE_ON); //L"MIDI 1.0 Note On";
            case Midi1ChannelVoiceMessageStatus::PolyPressure:
                return internal::ResourceManager::GetHString(IDS_MESSAGE_DESC_MT2_A_POLY_PRESSURE); //L"MIDI 1.0 Poly Pressure";
            case Midi1ChannelVoiceMessageStatus::ControlChange:
                return internal::ResourceManager::GetHString(IDS_MESSAGE_DESC_MT2_B_CONTROL_CHANGE); //L"MIDI 1.0 Control Change";
            case Midi1ChannelVoiceMessageStatus::ProgramChange:
                return internal::ResourceManager::GetHString(IDS_MESSAGE_DESC_MT2_C_PROGRAM_CHANGE); //L"MIDI 1.0 Program Change";
            case Midi1ChannelVoiceMessageStatus::ChannelPressure:
                return internal::ResourceManager::GetHString(IDS_MESSAGE_DESC_MT2_D_CHANNEL_PRESSURE); //L"MIDI 1.0 Channel Pressure";
            case Midi1ChannelVoiceMessageStatus::PitchBend:
                return internal::ResourceManager::GetHString(IDS_MESSAGE_DESC_MT2_E_PITCH_BEND); //L"MIDI 1.0 Pitch Bend";

            default:
                return internal::ResourceManager::GetHString(IDS_MESSAGE_DESC_MT2_UNKNOWN); //L"MIDI 1.0 Channel Voice";
            }
            break;

        case MidiMessageType::DataMessage64:
            switch (GetStatusFromDataMessage64FirstWord(word0))
            {
            case 0x0:
                return internal::ResourceManager::GetHString(IDS_MESSAGE_DESC_MT3_0_SYSEX7_COMPLETE); //L"SysEx 7-bit Complete";
            case 0x1:
                return internal::ResourceManager::GetHString(IDS_MESSAGE_DESC_MT3_1_SYSEX7_START); //L"SysEx 7-bit Start";
            case 0x2:
                return internal::ResourceManager::GetHString(IDS_MESSAGE_DESC_MT3_2_SYSEX7_CONTINUE); //L"SysEx 7-bit Continue";
            case 0x3:
                return internal::ResourceManager::GetHString(IDS_MESSAGE_DESC_MT3_3_SYSEX7_END); //L"SysEx 7-bit End";

            default:
                return internal::ResourceManager::GetHString(IDS_MESSAGE_DESC_MT3_DATA64_UNKNOWN); //L"Data Message 64";
            }
            break;

        case MidiMessageType::Midi2ChannelVoice64:
            switch (GetStatusFromMidi2ChannelVoiceMessageFirstWord(word0))
            {
            case Midi2ChannelVoiceMessageStatus::RegisteredPerNoteController:
                return internal::ResourceManager::GetHString(IDS_MESSAGE_DESC_MT4_0_RPNC); //L"MIDI 2.0 Registered Per-Note Controller";
            case Midi2ChannelVoiceMessageStatus::AssignablePerNoteController:
                return internal::ResourceManager::GetHString(IDS_MESSAGE_DESC_MT4_1_APNC); //L"MIDI 2.0 Assignable Per-Note Controller";
            case Midi2ChannelVoiceMessageStatus::RegisteredController:
                return internal::ResourceManager::GetHString(IDS_MESSAGE_DESC_MT4_2_RC); //L"MIDI 2.0 Registered Controller";
            case Midi2ChannelVoiceMessageStatus::AssignableController:
                return internal::ResourceManager::GetHString(IDS_MESSAGE_DESC_MT4_3_AC); //L"MIDI 2.0 Assignable Controller";
            case Midi2ChannelVoiceMessageStatus::RelativeRegisteredController:
                return internal::ResourceManager::GetHString(IDS_MESSAGE_DESC_MT4_4_REL_RC); //L"MIDI 2.0 Relative Registered Controller";
            case Midi2ChannelVoiceMessageStatus::RelativeAssignableController:
                return internal::ResourceManager::GetHString(IDS_MESSAGE_DESC_MT4_5_REL_AC); //L"MIDI 2.0 Relative Assignable Controller";
            case Midi2ChannelVoiceMessageStatus::PerNotePitchBend:
                return internal::ResourceManager::GetHString(IDS_MESSAGE_DESC_MT4_6_PER_NOTE_BEND); //L"MIDI 2.0 Per-Note Pitch Bend";
            case Midi2ChannelVoiceMessageStatus::NoteOff:
                return internal::ResourceManager::GetHString(IDS_MESSAGE_DESC_MT4_8_NOTE_OFF); //L"MIDI 2.0 Note Off";
            case Midi2ChannelVoiceMessageStatus::NoteOn:
                return internal::ResourceManager::GetHString(IDS_MESSAGE_DESC_MT4_9_NOTE_ON); //L"MIDI 2.0 Note On";
            case Midi2ChannelVoiceMessageStatus::PolyPressure:
                return internal::ResourceManager::GetHString(IDS_MESSAGE_DESC_MT4_A_POLY_PRESSURE); //L"MIDI 2.0 Poly Pressure";
            case Midi2ChannelVoiceMessageStatus::ControlChange:
                return internal::ResourceManager::GetHString(IDS_MESSAGE_DESC_MT4_B_CONTROL_CHANGE); //L"MIDI 2.0 Control Change";
            case Midi2ChannelVoiceMessageStatus::ProgramChange:
                return internal::ResourceManager::GetHString(IDS_MESSAGE_DESC_MT4_C_PROGRAM_CHANGE); //L"MIDI 2.0 Program Change";
            case Midi2ChannelVoiceMessageStatus::ChannelPressure:
                return internal::ResourceManager::GetHString(IDS_MESSAGE_DESC_MT4_D_CHANNEL_PRESSURE); //L"MIDI 2.0 Channel Pressure";
            case Midi2ChannelVoiceMessageStatus::PitchBend:
                return internal::ResourceManager::GetHString(IDS_MESSAGE_DESC_MT4_E_PITCH_BEND); //L"MIDI 2.0 Pitch Bend";

            default:
                return internal::ResourceManager::GetHString(IDS_MESSAGE_DESC_MT4_MIDI2_CV_UNKNOWN); //L"MIDI 2.0 Channel Voice";
            }
            break;

        case MidiMessageType::DataMessage128:
            switch (GetStatusFromDataMessage128FirstWord(word0))
            {
            case 0x0:
                return internal::ResourceManager::GetHString(IDS_MESSAGE_DESC_MT5_0_SYSEX8_COMPLETE); //L"SysEx 8-bit Complete";
            case 0x1:
                return internal::ResourceManager::GetHString(IDS_MESSAGE_DESC_MT5_1_SYSEX8_START); //L"SysEx 8-bit Start";
            case 0x2:
                return internal::ResourceManager::GetHString(IDS_MESSAGE_DESC_MT5_2_SYSEX8_CONTINUE); //L"SysEx 8-bit Continue";
            case 0x3:
                return internal::ResourceManager::GetHString(IDS_MESSAGE_DESC_MT5_3_SYSEX8_END); //L"SysEx 8-bit End";

            case 0x8:
                return internal::ResourceManager::GetHString(IDS_MESSAGE_DESC_MT5_8_MIXED_DATA_HEADER); //L"Mixed Data Set Header";
            case 0x9:
                return internal::ResourceManager::GetHString(IDS_MESSAGE_DESC_MT5_9_MIXED_DATA_PAYLOAD); //L"Mixed Data Set Payload";

            default:
                return internal::ResourceManager::GetHString(IDS_MESSAGE_DESC_MT5_UNKNOWN); //L"Data Message Unknown";
            }
            break;

        case MidiMessageType::FutureReserved632:
            return internal::ResourceManager::GetHString(IDS_MESSAGE_DESC_MT6_RESERVED); //L"Unknown Type 6";

        case MidiMessageType::FutureReserved732:
            return internal::ResourceManager::GetHString(IDS_MESSAGE_DESC_MT7_RESERVED); //L"Unknown Type 7";

        case MidiMessageType::FutureReserved864:
            return internal::ResourceManager::GetHString(IDS_MESSAGE_DESC_MT8_RESERVED); //L"Unknown Type 8";

        case MidiMessageType::FutureReserved964:
            return internal::ResourceManager::GetHString(IDS_MESSAGE_DESC_MT9_RESERVED); //L"Unknown Type 9";

        case MidiMessageType::FutureReservedA64:
            return internal::ResourceManager::GetHString(IDS_MESSAGE_DESC_MTA_RESERVED); //L"Unknown Type A";

        case MidiMessageType::FutureReservedB96:
            return internal::ResourceManager::GetHString(IDS_MESSAGE_DESC_MTB_RESERVED); //L"Unknown Type B";

        case MidiMessageType::FutureReservedC96:
            return internal::ResourceManager::GetHString(IDS_MESSAGE_DESC_MTC_RESERVED); //L"Unknown Type C";

        case MidiMessageType::FlexData128:
            {
                uint8_t statusBank = GetStatusBankFromFlexDataMessageFirstWord(word0);
                uint8_t status = GetStatusFromFlexDataMessageFirstWord(word0);

                switch (statusBank)
                {
                case 0x01:
                    switch (status)
                    {
                    case 0x00:
                        return internal::ResourceManager::GetHString(IDS_MESSAGE_DESC_MTD_01_00_METADATA_TEXT); //L"Metadata Text Event Status 0x00";
                    case 0x01:
                        return internal::ResourceManager::GetHString(IDS_MESSAGE_DESC_MTD_01_01_PROJECT_NAME); //L"Project Name";
                    case 0x02:
                        return internal::ResourceManager::GetHString(IDS_MESSAGE_DESC_MTD_01_02_SONG_NAME); //L"Composition (Song) Name";
                    case 0x03:
                        return internal::ResourceManager::GetHString(IDS_MESSAGE_DESC_MTD_01_03_CLIP_NAME); //L"MIDI Clip Name";
                    case 0x04:
                        return internal::ResourceManager::GetHString(IDS_MESSAGE_DESC_MTD_01_04_COPYRIGHT); //L"Copyright Notice";
                    case 0x05:
                        return internal::ResourceManager::GetHString(IDS_MESSAGE_DESC_MTD_01_05_COMPOSER_NAME); //L"Composer Name";
                    case 0x06:
                        return internal::ResourceManager::GetHString(IDS_MESSAGE_DESC_MTD_01_06_LYRICIST_NAME); //L"Lyricist Name";
                    case 0x07:
                        return internal::ResourceManager::GetHString(IDS_MESSAGE_DESC_MTD_01_07_ARRANGER_NAME); //L"Arranger Name";
                    case 0x08:
                        return internal::ResourceManager::GetHString(IDS_MESSAGE_DESC_MTD_01_08_PUBLISHER_NAME); //L"Publisher Name";
                    case 0x09:
                        return internal::ResourceManager::GetHString(IDS_MESSAGE_DESC_MTD_01_09_PRIMARY_PERFORMER_NAME); //L"Primary Performer Name";
                    case 0x0A:
                        return internal::ResourceManager::GetHString(IDS_MESSAGE_DESC_MTD_01_0A_ACCOMPANY_PERFORMER_NAME); //L"Accompanying Performer Name";
                    case 0x0B:
                        return internal::ResourceManager::GetHString(IDS_MESSAGE_DESC_MTD_01_0B_RECORDING_DATE); //L"Recording / Concert Date";
                    case 0x0C:
                        return internal::ResourceManager::GetHString(IDS_MESSAGE_DESC_MTD_01_0C_RECORDING_LOCATION); //L"Recording / Concert Location";
                    default:
                        return internal::ResourceManager::GetHString(IDS_MESSAGE_DESC_MTD_01_UNKNOWN); //L"Flex Data with Bank 0x01";
                    }
                    break;
                case 0x02:
                    switch (status)
                    {
                    case 0x00:
                        return internal::ResourceManager::GetHString(IDS_MESSAGE_DESC_MTD_02_00_PERF_TEXT_EVENT); //L"Performance Text Event Status 0x00";
                    case 0x01:
                        return internal::ResourceManager::GetHString(IDS_MESSAGE_DESC_MTD_02_01_LYRICS); //L"Lyrics";
                    case 0x02:
                        return internal::ResourceManager::GetHString(IDS_MESSAGE_DESC_MTD_02_02_LYRICS_LANGUAGE); //L"Lyrics Language";
                    case 0x03:
                        return internal::ResourceManager::GetHString(IDS_MESSAGE_DESC_MTD_02_03_RUBY); //L"Ruby";
                    case 0x04:
                        return internal::ResourceManager::GetHString(IDS_MESSAGE_DESC_MTD_02_04_RUBY_LANGUAGE); //L"Ruby Language";
                    default:
                        return internal::ResourceManager::GetHString(IDS_MESSAGE_DESC_MTD_02_UNKNOWN); //L"Flex Data with Bank 0x02";
                    }
                    break;
                default:
                    return internal::ResourceManager::GetHString(IDS_MESSAGE_DESC_MTD_UNKNOWN); //L"Flex Data Unknown";

                }
            }

        case MidiMessageType::FutureReservedE128:
            return internal::ResourceManager::GetHString(IDS_MESSAGE_DESC_MTE_RESERVED); //L"Type E Unknown";

        case MidiMessageType::Stream128:
            switch (GetStatusFromStreamMessageFirstWord(word0))
            {
            case MIDI_STREAM_MESSAGE_STATUS_ENDPOINT_DISCOVERY:
                return internal::ResourceManager::GetHString(IDS_MESSAGE_DESC_MTF_00_DISCOVERY); //L"Endpoint Discovery";
            case MIDI_STREAM_MESSAGE_STATUS_ENDPOINT_INFO_NOTIFICATION:
                return internal::ResourceManager::GetHString(IDS_MESSAGE_DESC_MTF_01_ENDPOINT_INFO); //L"Endpoint Info Notification";
            case MIDI_STREAM_MESSAGE_STATUS_DEVICE_IDENTITY_NOTIFICATION:
                return internal::ResourceManager::GetHString(IDS_MESSAGE_DESC_MTF_02_DEVICE_IDENTITY); //L"Device Identity Notification";
            case MIDI_STREAM_MESSAGE_STATUS_ENDPOINT_NAME_NOTIFICATION:
                return internal::ResourceManager::GetHString(IDS_MESSAGE_DESC_MTF_03_ENDPOINT_NAME); //L"Endpoint Name Notification";
            case MIDI_STREAM_MESSAGE_STATUS_ENDPOINT_PRODUCT_INSTANCE_ID_NOTIFICATION:
                return internal::ResourceManager::GetHString(IDS_MESSAGE_DESC_MTF_04_PRODUCT_INSTANCE_ID); //L"Product Instance Id Notification";
            case MIDI_STREAM_MESSAGE_STATUS_STREAM_CONFIGURATION_REQUEST:
                return internal::ResourceManager::GetHString(IDS_MESSAGE_DESC_MTF_05_CONFIG_REQUEST); //L"Stream Configuration Request";
            case MIDI_STREAM_MESSAGE_STATUS_STREAM_CONFIGURATION_NOTIFICATION:
                return internal::ResourceManager::GetHString(IDS_MESSAGE_DESC_MTF_06_CONFIG_NOTIFICATION); //L"Stream Configuration Notification";

            case MIDI_STREAM_MESSAGE_STATUS_FUNCTION_BLOCK_INFO_NOTIFICATION:
                return internal::ResourceManager::GetHString(IDS_MESSAGE_DESC_MTF_11_FUNCTION_BLOCK_INFO); //L"Function Block Info Notification";
            case MIDI_STREAM_MESSAGE_STATUS_FUNCTION_BLOCK_NAME_NOTIFICATION:
                return internal::ResourceManager::GetHString(IDS_MESSAGE_DESC_MTF_12_FUNCTION_BLOCK_NAME); //L"Function Block Name Notification";
            default:
                return internal::ResourceManager::GetHString(IDS_MESSAGE_DESC_MTF_UNKNOWN); //L"Stream Message Unknown";
            }
            break;
        default:
            // this is here just to satisfy the compiler because it doesn't understand 4-bit values
            return internal::ResourceManager::GetHString(IDS_MESSAGE_DESC_MT_UNKNOWN); //L"Unknown";
        };


    }

}

