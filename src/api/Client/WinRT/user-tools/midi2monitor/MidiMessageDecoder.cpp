// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================
//
// Adding support for a new message is intended to be a local change: add a case to the
// switch for its message type and append fields with the AppendField helper. Nothing here
// allocates until a row is actually shown or exported.
//

#include "pch.h"
#include "MidiMessageDecoder.h"
#include "StringResources.h"

namespace midi2monitor
{
    namespace
    {
        // MIDI 1.0 System Common / System Real Time status bytes
        constexpr uint8_t StatusTimingClock = 0xF8;
        constexpr uint8_t StatusActiveSensing = 0xFE;
        constexpr uint8_t FirstSystemRealTimeStatus = 0xF8;

        // Utility message statuses
        constexpr uint8_t UtilityStatusNoOp = 0x0;
        constexpr uint8_t UtilityStatusJitterReductionClock = 0x1;
        constexpr uint8_t UtilityStatusJitterReductionTimestamp = 0x2;
        constexpr uint8_t UtilityStatusDeltaClockstampTicksPerQuarterNote = 0x3;
        constexpr uint8_t UtilityStatusDeltaClockstampTicksSinceLast = 0x4;

        // Field labels come from resources. They are read once and cached because the decoder
        // runs for every visible row.
        struct DecoderLabels
        {
            winrt::hstring Note;
            winrt::hstring Velocity;
            winrt::hstring Pressure;
            winrt::hstring Controller;
            winrt::hstring Value;
            winrt::hstring Program;
            winrt::hstring Bank;
            winrt::hstring Bend;
            winrt::hstring Index;
            winrt::hstring Bytes;
            winrt::hstring Status;
            winrt::hstring Song;
            winrt::hstring Position;
            winrt::hstring Frame;
            winrt::hstring Attribute;
            winrt::hstring Text;
            winrt::hstring Name;
            winrt::hstring Ticks;
            winrt::hstring Tempo;
            winrt::hstring StreamId;
            winrt::hstring FunctionBlock;
            winrt::hstring Form;
            winrt::hstring TimeSignature;
            winrt::hstring KeySignature;
            winrt::hstring PerNote;
            winrt::hstring Options;
            winrt::hstring NoteOffValue;
        };

        DecoderLabels const& Labels() noexcept
        {
            static DecoderLabels labels
            {
                resources::GetString(L"DecodeLabelNote"),
                resources::GetString(L"DecodeLabelVelocity"),
                resources::GetString(L"DecodeLabelPressure"),
                resources::GetString(L"DecodeLabelController"),
                resources::GetString(L"DecodeLabelValue"),
                resources::GetString(L"DecodeLabelProgram"),
                resources::GetString(L"DecodeLabelBank"),
                resources::GetString(L"DecodeLabelBend"),
                resources::GetString(L"DecodeLabelIndex"),
                resources::GetString(L"DecodeLabelBytes"),
                resources::GetString(L"DecodeLabelStatus"),
                resources::GetString(L"DecodeLabelSong"),
                resources::GetString(L"DecodeLabelPosition"),
                resources::GetString(L"DecodeLabelFrame"),
                resources::GetString(L"DecodeLabelAttribute"),
                resources::GetString(L"DecodeLabelText"),
                resources::GetString(L"DecodeLabelName"),
                resources::GetString(L"DecodeLabelTicks"),
                resources::GetString(L"DecodeLabelTempo"),
                resources::GetString(L"DecodeLabelStreamId"),
                resources::GetString(L"DecodeLabelFunctionBlock"),
                resources::GetString(L"DecodeLabelForm"),
                resources::GetString(L"DecodeLabelTimeSignature"),
                resources::GetString(L"DecodeLabelKeySignature"),
                resources::GetString(L"DecodeLabelPerNote"),
                resources::GetString(L"DecodeLabelOptions"),
                resources::GetString(L"DecodeLabelNoteOffValue")
            };

            return labels;
        }

        void AppendField(DecodedFields& target, winrt::hstring const& label, std::wstring const& value)
        {
            target.Add(label, value);
        }

        std::wstring FormatNote(uint8_t noteIndex)
        {
            auto const name = midi2msg::MidiMessageHelper::GetNoteDisplayNameFromNoteIndex(noteIndex);
            auto const octave = midi2msg::MidiMessageHelper::GetNoteOctaveFromNoteIndex(noteIndex);

            return std::format(L"{} ({}{})", noteIndex, name.c_str(), octave);
        }

        // MIDI specification proper nouns. These are not translated, in the same way that the
        // SDK's own message names and note names are not translated.
        std::wstring_view GetControlChangeName(uint8_t controllerIndex) noexcept
        {
            switch (controllerIndex)
            {
            case 0: return L"Bank Select MSB";
            case 1: return L"Modulation Wheel MSB";
            case 2: return L"Breath Controller MSB";
            case 4: return L"Foot Controller MSB";
            case 5: return L"Portamento Time MSB";
            case 6: return L"Data Entry MSB";
            case 7: return L"Channel Volume MSB";
            case 8: return L"Balance MSB";
            case 10: return L"Pan MSB";
            case 11: return L"Expression MSB";
            case 32: return L"Bank Select LSB";
            case 33: return L"Modulation Wheel LSB";
            case 38: return L"Data Entry LSB";
            case 39: return L"Channel Volume LSB";
            case 64: return L"Damper Pedal (Sustain)";
            case 65: return L"Portamento";
            case 66: return L"Sostenuto";
            case 67: return L"Soft Pedal";
            case 68: return L"Legato Footswitch";
            case 69: return L"Hold 2";
            case 70: return L"Sound Controller 1 (Variation)";
            case 71: return L"Sound Controller 2 (Timbre)";
            case 72: return L"Sound Controller 3 (Release Time)";
            case 73: return L"Sound Controller 4 (Attack Time)";
            case 74: return L"Sound Controller 5 (Brightness)";
            case 84: return L"Portamento Control";
            case 91: return L"Reverb Send";
            case 92: return L"Tremolo Depth";
            case 93: return L"Chorus Send";
            case 94: return L"Celeste Depth";
            case 95: return L"Phaser Depth";
            case 96: return L"Data Increment";
            case 97: return L"Data Decrement";
            case 98: return L"NRPN LSB";
            case 99: return L"NRPN MSB";
            case 100: return L"RPN LSB";
            case 101: return L"RPN MSB";
            case 120: return L"All Sound Off";
            case 121: return L"Reset All Controllers";
            case 122: return L"Local Control";
            case 123: return L"All Notes Off";
            case 124: return L"Omni Mode Off";
            case 125: return L"Omni Mode On";
            case 126: return L"Mono Mode On";
            case 127: return L"Poly Mode On";
            default: return {};
            }
        }

        std::wstring FormatController(uint8_t controllerIndex)
        {
            auto const name = GetControlChangeName(controllerIndex);

            if (name.empty())
            {
                return std::format(L"{}", controllerIndex);
            }

            return std::format(L"{} ({})", controllerIndex, name);
        }

        std::wstring FormatByteList(uint8_t const* bytes, size_t count)
        {
            std::wstring result{};
            result.reserve(count * 3);

            for (size_t i = 0; i < count; i++)
            {
                if (i > 0)
                {
                    result.append(L" ");
                }

                result.append(std::format(L"{:02X}", bytes[i]));
            }

            return result;
        }

        // Payload text in FlexData and Stream messages is UTF-8, padded with nulls
        std::wstring DecodeUtf8Payload(uint8_t const* bytes, size_t count)
        {
            size_t length{ 0 };

            while (length < count && bytes[length] != 0)
            {
                length++;
            }

            if (length == 0)
            {
                return {};
            }

            auto const required = ::MultiByteToWideChar(CP_UTF8, 0,
                reinterpret_cast<char const*>(bytes), static_cast<int>(length), nullptr, 0);

            if (required <= 0)
            {
                return {};
            }

            std::wstring result(static_cast<size_t>(required), L'\0');

            if (::MultiByteToWideChar(CP_UTF8, 0, reinterpret_cast<char const*>(bytes),
                static_cast<int>(length), result.data(), required) <= 0)
            {
                return {};
            }

            return result;
        }

        void UnpackWords(std::array<uint32_t, 4> const& words, uint8_t wordCount, uint8_t* bytes)
        {
            for (uint8_t i = 0; i < wordCount; i++)
            {
                bytes[(i * 4) + 0] = static_cast<uint8_t>((words[i] >> 24) & 0xFF);
                bytes[(i * 4) + 1] = static_cast<uint8_t>((words[i] >> 16) & 0xFF);
                bytes[(i * 4) + 2] = static_cast<uint8_t>((words[i] >> 8) & 0xFF);
                bytes[(i * 4) + 3] = static_cast<uint8_t>(words[i] & 0xFF);
            }
        }

        void DecodeUtilityMessage(MessageRecord const& record, DecodedFields& result)
        {

            auto const status = static_cast<uint8_t>((record.Words[0] >> 20) & 0x0F);
            auto const data = static_cast<uint16_t>(record.Words[0] & 0xFFFF);

            switch (status)
            {
            case UtilityStatusNoOp:
                break;

            case UtilityStatusJitterReductionClock:
            case UtilityStatusJitterReductionTimestamp:
                AppendField(result, Labels().Ticks, std::format(L"{}", data));
                break;

            case UtilityStatusDeltaClockstampTicksPerQuarterNote:
            case UtilityStatusDeltaClockstampTicksSinceLast:
                AppendField(result, Labels().Ticks, std::format(L"{}", data));
                break;

            default:
                break;
            }
        }

        void DecodeSystemMessage(MessageRecord const& record, DecodedFields& result)
        {

            auto const status = static_cast<uint8_t>((record.Words[0] >> 16) & 0xFF);
            auto const data1 = static_cast<uint8_t>((record.Words[0] >> 8) & 0x7F);
            auto const data2 = static_cast<uint8_t>(record.Words[0] & 0x7F);

            switch (status)
            {
            case 0xF1: // MIDI Time Code Quarter Frame
                AppendField(result, Labels().Frame, std::format(L"{}", data1));
                break;

            case 0xF2: // Song Position Pointer
                AppendField(result, Labels().Position,
                    std::format(L"{}", static_cast<uint16_t>((data2 << 7) | data1)));
                break;

            case 0xF3: // Song Select
                AppendField(result, Labels().Song, std::format(L"{}", data1));
                break;

            default:
                break;
            }
        }

        void DecodeMidi1ChannelVoiceMessage(MessageRecord const& record, DecodedFields& result)
        {

            auto const status = midi2msg::MidiMessageHelper::GetStatusFromMidi1ChannelVoiceMessage(record.Words[0]);
            auto const data1 = static_cast<uint8_t>((record.Words[0] >> 8) & 0x7F);
            auto const data2 = static_cast<uint8_t>(record.Words[0] & 0x7F);

            switch (status)
            {
            case midi2msg::Midi1ChannelVoiceMessageStatus::NoteOff:
            case midi2msg::Midi1ChannelVoiceMessageStatus::NoteOn:
                AppendField(result, Labels().Note, FormatNote(data1));
                AppendField(result, Labels().Velocity, std::format(L"{}", data2));
                break;

            case midi2msg::Midi1ChannelVoiceMessageStatus::PolyPressure:
                AppendField(result, Labels().Note, FormatNote(data1));
                AppendField(result, Labels().Pressure, std::format(L"{}", data2));
                break;

            case midi2msg::Midi1ChannelVoiceMessageStatus::ControlChange:
                AppendField(result, Labels().Controller, FormatController(data1));
                AppendField(result, Labels().Value, std::format(L"{}", data2));
                break;

            case midi2msg::Midi1ChannelVoiceMessageStatus::ProgramChange:
                AppendField(result, Labels().Program, std::format(L"{}", data1));
                break;

            case midi2msg::Midi1ChannelVoiceMessageStatus::ChannelPressure:
                AppendField(result, Labels().Pressure, std::format(L"{}", data1));
                break;

            case midi2msg::Midi1ChannelVoiceMessageStatus::PitchBend:
            {
                auto const raw = static_cast<int32_t>((data2 << 7) | data1);
                AppendField(result, Labels().Bend, std::format(L"{} ({:+})", raw, raw - 8192));
                break;
            }

            default:
                break;
            }
        }

        void DecodeMidi2ChannelVoiceMessage(MessageRecord const& record, DecodedFields& result)
        {

            auto const status = midi2msg::MidiMessageHelper::GetStatusFromMidi2ChannelVoiceMessageFirstWord(record.Words[0]);
            auto const index1 = static_cast<uint8_t>((record.Words[0] >> 8) & 0xFF);
            auto const index2 = static_cast<uint8_t>(record.Words[0] & 0xFF);
            auto const word1 = record.Words[1];

            switch (status)
            {
            case midi2msg::Midi2ChannelVoiceMessageStatus::NoteOff:
            case midi2msg::Midi2ChannelVoiceMessageStatus::NoteOn:
            {
                AppendField(result, Labels().Note, FormatNote(index1 & 0x7F));
                AppendField(result, Labels().Velocity, std::format(L"{}", static_cast<uint16_t>(word1 >> 16)));

                if (index2 != 0)
                {
                    AppendField(result, Labels().Attribute,
                        std::format(L"{} / {}", index2, static_cast<uint16_t>(word1 & 0xFFFF)));
                }
                break;
            }

            case midi2msg::Midi2ChannelVoiceMessageStatus::PolyPressure:
                AppendField(result, Labels().Note, FormatNote(index1 & 0x7F));
                AppendField(result, Labels().Pressure, std::format(L"{}", word1));
                break;

            case midi2msg::Midi2ChannelVoiceMessageStatus::ChannelPressure:
                AppendField(result, Labels().Pressure, std::format(L"{}", word1));
                break;

            case midi2msg::Midi2ChannelVoiceMessageStatus::ControlChange:
                AppendField(result, Labels().Controller, FormatController(index1 & 0x7F));
                AppendField(result, Labels().Value, std::format(L"{}", word1));
                break;

            case midi2msg::Midi2ChannelVoiceMessageStatus::ProgramChange:
            {
                AppendField(result, Labels().Program, std::format(L"{}", static_cast<uint8_t>((word1 >> 24) & 0x7F)));

                if ((index2 & 0x01) != 0)
                {
                    auto const bankMsb = static_cast<uint8_t>((word1 >> 8) & 0x7F);
                    auto const bankLsb = static_cast<uint8_t>(word1 & 0x7F);

                    AppendField(result, Labels().Bank, std::format(L"{} / {}", bankMsb, bankLsb));
                }
                break;
            }

            case midi2msg::Midi2ChannelVoiceMessageStatus::PitchBend:
                AppendField(result, Labels().Bend,
                    std::format(L"{} ({:+})", word1, static_cast<int64_t>(word1) - 0x80000000LL));
                break;

            case midi2msg::Midi2ChannelVoiceMessageStatus::PerNotePitchBend:
                AppendField(result, Labels().Note, FormatNote(index1 & 0x7F));
                AppendField(result, Labels().Bend,
                    std::format(L"{} ({:+})", word1, static_cast<int64_t>(word1) - 0x80000000LL));
                break;

            case midi2msg::Midi2ChannelVoiceMessageStatus::RegisteredPerNoteController:
            case midi2msg::Midi2ChannelVoiceMessageStatus::AssignablePerNoteController:
                AppendField(result, Labels().Note, FormatNote(index1 & 0x7F));
                AppendField(result, Labels().Index, std::format(L"{}", index2));
                AppendField(result, Labels().Value, std::format(L"{}", word1));
                break;

            case midi2msg::Midi2ChannelVoiceMessageStatus::PerNoteManagement:
                AppendField(result, Labels().Note, FormatNote(index1 & 0x7F));
                AppendField(result, Labels().Options, std::format(L"0x{:02X}", index2));
                break;

            case midi2msg::Midi2ChannelVoiceMessageStatus::RegisteredController:
            case midi2msg::Midi2ChannelVoiceMessageStatus::AssignableController:
            case midi2msg::Midi2ChannelVoiceMessageStatus::RelativeRegisteredController:
            case midi2msg::Midi2ChannelVoiceMessageStatus::RelativeAssignableController:
                AppendField(result, Labels().Bank, std::format(L"{}", index1 & 0x7F));
                AppendField(result, Labels().Index, std::format(L"{}", index2 & 0x7F));
                AppendField(result, Labels().Value, std::format(L"{}", word1));
                break;

            default:
                break;
            }
        }

        void DecodeData64Message(MessageRecord const& record, DecodedFields& result)
        {

            auto const byteCount = std::min<uint8_t>(
                midi2msg::MidiMessageHelper::GetNumberOfBytesFromDataMessage64FirstWord(record.Words[0]), 6);

            std::array<uint8_t, 8> raw{};
            UnpackWords(record.Words, 2, raw.data());

            // payload starts at the third byte of the first word
            AppendField(result, Labels().Bytes, FormatByteList(raw.data() + 2, byteCount));
        }

        void DecodeData128Message(MessageRecord const& record, DecodedFields& result)
        {

            auto const byteCount = std::min<uint8_t>(
                midi2msg::MidiMessageHelper::GetNumberOfBytesFromDataMessage128FirstWord(record.Words[0]), 13);

            std::array<uint8_t, 16> raw{};
            UnpackWords(record.Words, 4, raw.data());

            AppendField(result, Labels().StreamId, std::format(L"{}", raw[2]));
            AppendField(result, Labels().Bytes, FormatByteList(raw.data() + 3, byteCount));
        }

        void DecodeFlexDataMessage(MessageRecord const& record, DecodedFields& result)
        {

            auto const statusBank = midi2msg::MidiMessageHelper::GetStatusBankFromFlexDataMessageFirstWord(record.Words[0]);
            auto const status = midi2msg::MidiMessageHelper::GetStatusFromFlexDataMessageFirstWord(record.Words[0]);

            std::array<uint8_t, 16> raw{};
            UnpackWords(record.Words, 4, raw.data());

            if (statusBank == 0x00)
            {
                switch (status)
                {
                case 0x00: // Set Tempo, in 10ns per quarter note
                {
                    auto const nanosecondsPerQuarterNote = record.Words[1];

                    if (nanosecondsPerQuarterNote > 0)
                    {
                        auto const beatsPerMinute = 6000000000.0 / static_cast<double>(nanosecondsPerQuarterNote);
                        AppendField(result, Labels().Tempo, std::format(L"{:.3f} BPM", beatsPerMinute));
                    }
                    break;
                }

                case 0x01: // Set Time Signature
                    AppendField(result, Labels().TimeSignature,
                        std::format(L"{}/{}", raw[4], raw[5]));
                    break;

                case 0x05: // Set Key Signature
                    AppendField(result, Labels().KeySignature, std::format(L"0x{:02X}", raw[4]));
                    break;

                default:
                    break;
                }
            }
            else if (statusBank == 0x01 || statusBank == 0x02)
            {
                // metadata text and performance text banks carry UTF-8 in words 1-3
                auto const text = DecodeUtf8Payload(raw.data() + 4, 12);

                if (!text.empty())
                {
                    AppendField(result, Labels().Text, text);
                }
            }
        }

        void DecodeStreamMessage(MessageRecord const& record, DecodedFields& result)
        {

            auto const status = midi2msg::MidiMessageHelper::GetStatusFromStreamMessageFirstWord(record.Words[0]);

            std::array<uint8_t, 16> raw{};
            UnpackWords(record.Words, 4, raw.data());

            switch (status)
            {
            case 0x03: // Endpoint Name Notification
            case 0x04: // Product Instance Id Notification
            {
                auto const text = DecodeUtf8Payload(raw.data() + 2, 14);

                if (!text.empty())
                {
                    AppendField(result, Labels().Name, text);
                }
                break;
            }

            case 0x12: // Function Block Name Notification
            {
                AppendField(result, Labels().FunctionBlock, std::format(L"{}", raw[2]));

                auto const text = DecodeUtf8Payload(raw.data() + 3, 13);

                if (!text.empty())
                {
                    AppendField(result, Labels().Name, text);
                }
                break;
            }

            case 0x10: // Function Block Discovery
            case 0x11: // Function Block Info Notification
                AppendField(result, Labels().FunctionBlock, std::format(L"{}", raw[2]));
                break;

            default:
                break;
            }
        }
    }

    _Use_decl_annotations_
    UmpInfo InspectMessage(uint32_t word0) noexcept
    {
        UmpInfo info{};

        info.MessageType = GetMessageTypeFromFirstWord(word0);
        info.WordCount = GetWordCountFromMessageType(info.MessageType);
        info.HasGroup = MessageTypeHasGroup(info.MessageType);
        info.GroupIndex = info.HasGroup ? GetGroupIndexFromFirstWord(word0) : uint8_t{ 0 };

        if (info.MessageType == MessageTypeFlexData128)
        {
            info.HasChannel = FlexDataMessageAddressesChannel(word0);
        }
        else
        {
            info.HasChannel = MessageTypeHasChannel(info.MessageType);
        }

        info.ChannelIndex = info.HasChannel ? GetChannelIndexFromFirstWord(word0) : uint8_t{ 0 };

        if (info.MessageType == MessageTypeSystemCommon32)
        {
            auto const status = static_cast<uint8_t>((word0 >> 16) & 0xFF);

            if (status >= FirstSystemRealTimeStatus)
            {
                info.Traits = info.Traits | MessageTraits::SystemRealTime;
            }

            if (status == StatusTimingClock)
            {
                info.Traits = info.Traits | MessageTraits::Clock;
            }
            else if (status == StatusActiveSensing)
            {
                info.Traits = info.Traits | MessageTraits::ActiveSense;
            }
        }
        else if (info.MessageType == MessageTypeUtility32)
        {
            auto const status = static_cast<uint8_t>((word0 >> 20) & 0x0F);

            if (status == UtilityStatusJitterReductionClock ||
                status == UtilityStatusJitterReductionTimestamp)
            {
                info.Traits = info.Traits | MessageTraits::Clock;
            }
        }

        return info;
    }

    _Use_decl_annotations_
    bool FlexDataMessageAddressesChannel(uint32_t word0) noexcept
    {
        // address field is bits 23-22. 0 = channel, 1 = group
        return ((word0 >> 22) & 0x03) == 0;
    }

    _Use_decl_annotations_
    winrt::hstring GetMessageDisplayName(uint32_t word0) noexcept
    {
        try
        {
            return midi2msg::MidiMessageHelper::GetMessageDisplayNameFromFirstWord(word0);
        }
        MIDI_MONITOR_CATCH_AND_LOG(L"Unable to retrieve the message display name.")

        return {};
    }

    winrt::hstring DecodedFields::ToDisplayString() const
    {
        std::wstring text{};

        for (uint32_t i = 0; i < Count; i++)
        {
            if (!text.empty())
            {
                text.append(L"   ");
            }

            text.append(Labels[i]);
            text.append(L" ");
            text.append(Values[i]);
        }

        return winrt::hstring{ text };
    }

    _Use_decl_annotations_
    DecodedFields DecodeMessage(MessageRecord const& record) noexcept
    {
        DecodedFields fields{};

        try
        {
            if (record.Kind != RecordKind::MidiMessage || record.WordCount == 0)
            {
                return fields;
            }

            switch (GetMessageTypeFromFirstWord(record.Words[0]))
            {
            case MessageTypeUtility32:
                DecodeUtilityMessage(record, fields);
                break;
            case MessageTypeSystemCommon32:
                DecodeSystemMessage(record, fields);
                break;
            case MessageTypeMidi1ChannelVoice32:
                DecodeMidi1ChannelVoiceMessage(record, fields);
                break;
            case MessageTypeData64:
                DecodeData64Message(record, fields);
                break;
            case MessageTypeMidi2ChannelVoice64:
                DecodeMidi2ChannelVoiceMessage(record, fields);
                break;
            case MessageTypeData128:
                DecodeData128Message(record, fields);
                break;
            case MessageTypeFlexData128:
                DecodeFlexDataMessage(record, fields);
                break;
            case MessageTypeStream128:
                DecodeStreamMessage(record, fields);
                break;
            default:
                break;
            }
        }
        MIDI_MONITOR_CATCH_AND_LOG(L"Unable to decode the message contents.")

        return fields;
    }

    _Use_decl_annotations_
    uint32_t GetMessageTypeColorIndex(uint8_t messageType) noexcept
    {
        switch (messageType & 0x0F)
        {
        case MessageTypeUtility32:              return 0;
        case MessageTypeSystemCommon32:         return 1;
        case MessageTypeMidi1ChannelVoice32:    return 2;
        case MessageTypeMidi2ChannelVoice64:    return 3;
        case MessageTypeData64:
        case MessageTypeData128:                return 4;
        case MessageTypeFlexData128:            return 5;
        case MessageTypeStream128:              return 6;
        default:                                return 7;
        }
    }
}
