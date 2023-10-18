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
    midi2::MidiMessageType MidiMessageUtility::GetMessageTypeFromFirstMessageWord(uint32_t const word0) noexcept
    {
        return (midi2::MidiMessageType)internal::GetUmpMessageTypeFromFirstWord(word0);
    }

    _Use_decl_annotations_
    midi2::MidiPacketType MidiMessageUtility::GetPacketTypeFromFirstMessageWord(uint32_t const word0) noexcept
    {
        return (midi2::MidiPacketType)internal::GetUmpLengthInMidiWordsFromFirstWord(word0);
    }

    _Use_decl_annotations_
    midi2::MidiGroup MidiMessageUtility::GetGroup(_In_ uint32_t const word0)
    {
        auto group = winrt::make<MidiGroup>(internal::GetGroupIndexFromFirstWord(word0));
        return group;
    }


    // It's expected for the user to check to see if the message type has a group field before calling this
    _Use_decl_annotations_
    uint32_t MidiMessageUtility::ReplaceGroup(uint32_t const word0, midi2::MidiGroup const newGroup) noexcept
    {
        return internal::GetFirstWordWithNewGroup(word0, newGroup.Index());
    }

    _Use_decl_annotations_
    bool MidiMessageUtility::MessageTypeHasGroupField(midi2::MidiMessageType const messageType) noexcept
    {
        return internal::MessageTypeHasGroupField((uint8_t)messageType);
    }


    _Use_decl_annotations_
    midi2::MidiChannel MidiMessageUtility::GetChannel(_In_ uint32_t const word0)
    {
        auto channel = winrt::make<MidiChannel>(internal::GetChannelIndexFromFirstWord(word0));
        return channel;
    }


    // It's expected for the user to check to see if the message type has a channel field before calling this
    _Use_decl_annotations_
    uint32_t MidiMessageUtility::ReplaceChannel(uint32_t const word0, midi2::MidiChannel const newChannel) noexcept
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
        switch (GetMessageTypeFromFirstMessageWord(word0))
        {
        case MidiMessageType::UtilityMessage32:
            switch (GetStatusFromUtilityMessage(word0))
            {
            case 0x0:
                return L"NOOP";
            case 0x1:
                return L"Jitter Reduction Clock Time";
            case 0x2:
                return L"Jitter Reduction Timestamp";
            case 0x3:
                return L"Delta Ticks Per Quarter Note";
            case 0x4:
                return L"Delta Ticks Since Last Event";

            default:
                return L"Utility Unknown";
            }
            break;

        case MidiMessageType::SystemCommon32:
            switch (GetStatusFromSystemCommonMessage(word0))
            {
            case 0xF0:
                return L"Reserved 0xF0";
            case 0xF1:
                return L"MIDI Time Code";
            case 0xF2:
                return L"Song Position Pointer";
            case 0xF3:
                return L"Song Select";
            case 0xF4:
                return L"Reserved 0xF4";
            case 0xF5:
                return L"Reserved 0xF5";
            case 0xF6:
                return L"Tune Request";
            case 0xF7:
                return L"Reserved 0xF7";
            case 0xF8:
                return L"Timing Clock";
            case 0xF9:
                return L"Reserved 0xF9";
            case 0xFA:
                return L"Start";
            case 0xFB:
                return L"Continue";
            case 0xFC:
                return L"Stop";
            case 0xFD:
                return L"Reserved 0xFD";
            case 0xFE:
                return L"Active Sensing";
            case 0xFF:
                return L"Reset";

            default:
                return L"System Common/Realtime Unknown";
            }
            break;

        case MidiMessageType::Midi1ChannelVoice32:
            switch (GetStatusFromMidi1ChannelVoiceMessage(word0))
            {
            case Midi1ChannelVoiceMessageStatus::NoteOff:
                return L"MIDI 1.0 Note Off";
            case Midi1ChannelVoiceMessageStatus::NoteOn:
                return L"MIDI 1.0 Note On";
            case Midi1ChannelVoiceMessageStatus::PolyPressure:
                return L"MIDI 1.0 Poly Pressure";
            case Midi1ChannelVoiceMessageStatus::ControlChange:
                return L"MIDI 1.0 Control Change";
            case Midi1ChannelVoiceMessageStatus::ProgramChange:
                return L"MIDI 1.0 Program Change";
            case Midi1ChannelVoiceMessageStatus::ChannelPressure:
                return L"MIDI 1.0 Channel Pressure";
            case Midi1ChannelVoiceMessageStatus::PitchBend:
                return L"MIDI 1.0 Pitch Bend";

            default:
                return L"MIDI 1.0 Channel Voice";
            }
            break;

        case MidiMessageType::DataMessage64:
            switch (GetStatusFromDataMessage64FirstWord(word0))
            {
            case 0x0:
                return L"SysEx 7-bit Complete";
            case 0x1:
                return L"SysEx 7-bit Start";
            case 0x2:
                return L"SysEx 7-bit Continue";
            case 0x3:
                return L"SysEx 7-bit End";

            default:
                return L"Data Message 64";
            }
            break;

        case MidiMessageType::Midi2ChannelVoice64:
            switch (GetStatusFromMidi2ChannelVoiceMessageFirstWord(word0))
            {
            case Midi2ChannelVoiceMessageStatus::RegisteredPerNoteController:
                return L"MIDI 2.0 Registered Per-Note Controller";
            case Midi2ChannelVoiceMessageStatus::AssignablePerNoteController:
                return L"MIDI 2.0 Assignable Per-Note Controller";
            case Midi2ChannelVoiceMessageStatus::RegisteredController:
                return L"MIDI 2.0 Registered Controller";
            case Midi2ChannelVoiceMessageStatus::AssignableController:
                return L"MIDI 2.0 Assignable Controller";
            case Midi2ChannelVoiceMessageStatus::RelativeRegisteredController:
                return L"MIDI 2.0 Relative Registered Controller";
            case Midi2ChannelVoiceMessageStatus::RelativeAssignableController:
                return L"MIDI 2.0 Relative Assignable Controller";
            case Midi2ChannelVoiceMessageStatus::PerNotePitchBend:
                return L"MIDI 2.0 Per-Note Pitch Bend";
            case Midi2ChannelVoiceMessageStatus::NoteOff:
                return L"MIDI 2.0 Note Off";
            case Midi2ChannelVoiceMessageStatus::NoteOn:
                return L"MIDI 2.0 Note On";
            case Midi2ChannelVoiceMessageStatus::PolyPressure:
                return L"MIDI 2.0 Poly Pressure";
            case Midi2ChannelVoiceMessageStatus::ControlChange:
                return L"MIDI 2.0 Control Change";
            case Midi2ChannelVoiceMessageStatus::ProgramChange:
                return L"MIDI 2.0 Program Change";
            case Midi2ChannelVoiceMessageStatus::ChannelPressure:
                return L"MIDI 2.0 Channel Pressure";
            case Midi2ChannelVoiceMessageStatus::PitchBend:
                return L"MIDI 2.0 Pitch Bend";

            default:
                return L"MIDI 2.0 Channel Voice";
            }
            break;

        case MidiMessageType::DataMessage128:
            switch (GetStatusFromDataMessage128FirstWord(word0))
            {
            case 0x0:
                return L"SysEx 8-bit Complete";
            case 0x1:
                return L"SysEx 8-bit Start";
            case 0x2:
                return L"SysEx 8-bit Continue";
            case 0x3:
                return L"SysEx 8-bit End";

            case 0x8:
                return L"Mixed Data Set Header";
            case 0x9:
                return L"Mixed Data Set Payload";

            default:
                return L"Data Message Unknown";
            }
            break;

        case MidiMessageType::FutureReserved632:
            return L"Unknown Type 6";

        case MidiMessageType::FutureReserved732:
            return L"Unknown Type 7";

        case MidiMessageType::FutureReserved864:
            return L"Unknown Type 8";

        case MidiMessageType::FutureReserved964:
            return L"Unknown Type 9";

        case MidiMessageType::FutureReservedA64:
            return L"Unknown Type A";

        case MidiMessageType::FutureReservedB96:
            return L"Unknown Type B";

        case MidiMessageType::FutureReservedC96:
            return L"Unknown Type C";

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
                        return L"Metadata Text Event Status 0x00";
                    case 0x01:
                        return L"Project Name";
                    case 0x02:
                        return L"Composition (Song) Name";
                    case 0x03:
                        return L"MIDI Clip Name";
                    case 0x04:
                        return L"Copyright Notice";
                    case 0x05:
                        return L"Composer Name";
                    case 0x06:
                        return L"Lyricist Name";
                    case 0x07:
                        return L"Arranger Name";
                    case 0x08:
                        return L"Publisher Name";
                    case 0x09:
                        return L"Primary Performer Name";
                    case 0x0A:
                        return L"Accompanying Performer Name";
                    case 0x0B:
                        return L"Recording / Concert Date";
                    case 0x0C:
                        return L"Recording / Concert Location";
                    default:
                        return L"Flex Data with Bank 0x01";
                    }
                    break;
                case 0x02:
                    switch (status)
                    {
                    case 0x00:
                        return L"Performance Text Event Status 0x00";
                    case 0x01:
                        return L"Lyrics";
                    case 0x02:
                        return L"Lyrics Language";
                    case 0x03:
                        return L"Ruby";
                    case 0x04:
                        return L"Ruby Language";
                    default:
                        return L"Flex Data with Bank 0x02";
                    }
                    break;
                default:
                    return L"Flex Data Unknown";

                }
            }

        case MidiMessageType::FutureReservedE128:
            return L"Type E Unknown";

        case MidiMessageType::Stream128:
            switch (GetStatusFromStreamMessageFirstWord(word0))
            {
            case MIDI_STREAM_MESSAGE_STATUS_DEVICE_IDENTITY_NOTIFICATION:
                return L"Device Identity Notification";
            case MIDI_STREAM_MESSAGE_STATUS_ENDPOINT_DISCOVERY:
                return L"Endpoint Discovery";
            case MIDI_STREAM_MESSAGE_STATUS_ENDPOINT_INFO_NOTIFICATION:
                return L"Endpoint Info Notification";
            case MIDI_STREAM_MESSAGE_STATUS_ENDPOINT_NAME_NOTIFICATION:
                return L"Endpoint Name Notification";
            case MIDI_STREAM_MESSAGE_STATUS_ENDPOINT_PRODUCT_INSTANCE_ID_NOTIFICATION:
                return L"Product Instance Id Notification";
            case MIDI_STREAM_MESSAGE_STATUS_FUNCTION_BLOCK_INFO_NOTIFICATION:
                return L"Function Block Info Notification";
            case MIDI_STREAM_MESSAGE_STATUS_FUNCTION_BLOCK_NAME_NOTIFICATION:
                return L"Function Block Name Notification";
            case MIDI_STREAM_MESSAGE_STATUS_STREAM_CONFIGURATION_NOTIFICATION:
                return L"Stream Configuration Notification";
            case MIDI_STREAM_MESSAGE_STATUS_STREAM_CONFIGURATION_REQUEST:
                return L"Stream Configuration Request";
            default:
                return L"Stream Message Unknown";
            }
            break;
        default:
            // this is here just to satisfy the compiler because it doesn't understand 4-bit values
            return L"Unknown";
        };


    }

}

