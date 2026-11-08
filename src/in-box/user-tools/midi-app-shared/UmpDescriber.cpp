// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================
//
// Adding support for a new message is meant to be a local change: add a case to the switch for
// its message type and append fields with AppendField. Nothing here allocates until a message is
// actually shown, copied or saved.
//

#include "pch.h"
#include "UmpDescriber.h"

#include <format>

namespace midiapp
{
    namespace midi2msg = ::winrt::Windows::Devices::Midi2::Utilities::Messages;

    namespace
    {
        // Utility message statuses
        constexpr uint8_t UtilityStatusNoOp = 0x0;
        constexpr uint8_t UtilityStatusJitterReductionClock = 0x1;
        constexpr uint8_t UtilityStatusJitterReductionTimestamp = 0x2;
        constexpr uint8_t UtilityStatusDeltaClockstampTicksPerQuarterNote = 0x3;
        constexpr uint8_t UtilityStatusDeltaClockstampTicksSinceLast = 0x4;

        std::wstring FormatWithNumber(_In_ winrt::hstring const& format, _In_ uint32_t number);

        void AppendField(_Inout_ UmpDescription& target, _In_ winrt::hstring const& label, _In_ std::wstring const& value)
        {
            target.Add(label, value);
        }

        std::wstring FormatNote(_In_ uint8_t noteIndex)
        {
            auto const name = midi2msg::MidiMessageHelper::GetNoteDisplayNameFromNoteIndex(noteIndex);
            auto const octave = midi2msg::MidiMessageHelper::GetNoteOctaveFromNoteIndex(noteIndex);

            return std::format(L"{} ({}{})", noteIndex, name.c_str(), octave);
        }

        // MIDI specification proper nouns. These are not translated, in the same way that the
        // SDK's own message names and note names are not translated.
        std::wstring_view GetControlChangeName(_In_ uint8_t controllerIndex) noexcept
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

        std::wstring FormatController(_In_ uint8_t controllerIndex)
        {
            auto const name = GetControlChangeName(controllerIndex);

            if (name.empty())
            {
                return std::format(L"{}", controllerIndex);
            }

            return std::format(L"{} ({})", controllerIndex, name);
        }

        std::wstring FormatByteList(_In_reads_(count) uint8_t const* bytes, _In_ size_t count)
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
        std::wstring DecodeUtf8Payload(_In_reads_(count) uint8_t const* bytes, _In_ size_t count)
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

        void UnpackWords(_In_ std::array<uint32_t, 4> const& words, _In_ uint8_t wordCount, _Out_writes_(wordCount * 4) uint8_t* bytes)
        {
            for (uint8_t i = 0; i < wordCount; i++)
            {
                bytes[(i * 4) + 0] = static_cast<uint8_t>((words[i] >> 24) & 0xFF);
                bytes[(i * 4) + 1] = static_cast<uint8_t>((words[i] >> 16) & 0xFF);
                bytes[(i * 4) + 2] = static_cast<uint8_t>((words[i] >> 8) & 0xFF);
                bytes[(i * 4) + 3] = static_cast<uint8_t>(words[i] & 0xFF);
            }
        }

        void DecodeUtilityMessage(_In_ std::array<uint32_t, 4> const& words, _In_ UmpDescriptionLabels const& labels, _Inout_ UmpDescription& result)
        {

            auto const status = static_cast<uint8_t>((words[0] >> 20) & 0x0F);
            auto const data = static_cast<uint16_t>(words[0] & 0xFFFF);

            switch (status)
            {
            case UtilityStatusNoOp:
                break;

            case UtilityStatusJitterReductionClock:
            case UtilityStatusJitterReductionTimestamp:
            case UtilityStatusDeltaClockstampTicksPerQuarterNote:
                AppendField(result, labels.Ticks, std::format(L"{}", data));
                break;

            // This one carries a 20 bit value, unlike the 16 bit fields above.
            case UtilityStatusDeltaClockstampTicksSinceLast:
                AppendField(result, labels.Ticks, std::format(L"{}", words[0] & 0x000FFFFF));
                break;

            default:
                break;
            }
        }

        void DecodeSystemMessage(_In_ std::array<uint32_t, 4> const& words, _In_ UmpDescriptionLabels const& labels, _Inout_ UmpDescription& result)
        {

            auto const status = static_cast<uint8_t>((words[0] >> 16) & 0xFF);
            auto const data1 = static_cast<uint8_t>((words[0] >> 8) & 0x7F);
            auto const data2 = static_cast<uint8_t>(words[0] & 0x7F);

            switch (status)
            {
            case 0xF1: // MIDI Time Code Quarter Frame
                AppendField(result, labels.Frame, std::format(L"{}", data1));
                break;

            case 0xF2: // Song Position Pointer
                AppendField(result, labels.Position,
                    std::format(L"{}", static_cast<uint16_t>((data2 << 7) | data1)));
                break;

            case 0xF3: // Song Select
                AppendField(result, labels.Song, std::format(L"{}", data1));
                break;

            default:
                break;
            }
        }

        void DecodeMidi1ChannelVoiceMessage(_In_ std::array<uint32_t, 4> const& words, _In_ UmpDescriptionLabels const& labels, _Inout_ UmpDescription& result)
        {

            auto const status = midi2msg::MidiMessageHelper::GetStatusFromMidi1ChannelVoiceMessage(words[0]);
            auto const data1 = static_cast<uint8_t>((words[0] >> 8) & 0x7F);
            auto const data2 = static_cast<uint8_t>(words[0] & 0x7F);

            switch (status)
            {
            case midi2msg::Midi1ChannelVoiceMessageStatus::NoteOff:
            case midi2msg::Midi1ChannelVoiceMessageStatus::NoteOn:
                AppendField(result, labels.Note, FormatNote(data1));
                AppendField(result, labels.Velocity, std::format(L"{}", data2));
                break;

            case midi2msg::Midi1ChannelVoiceMessageStatus::PolyPressure:
                AppendField(result, labels.Note, FormatNote(data1));
                AppendField(result, labels.Pressure, std::format(L"{}", data2));
                break;

            case midi2msg::Midi1ChannelVoiceMessageStatus::ControlChange:
                AppendField(result, labels.Controller, FormatController(data1));
                AppendField(result, labels.Value, std::format(L"{}", data2));
                break;

            case midi2msg::Midi1ChannelVoiceMessageStatus::ProgramChange:
                AppendField(result, labels.Program, std::format(L"{}", data1));
                break;

            case midi2msg::Midi1ChannelVoiceMessageStatus::ChannelPressure:
                AppendField(result, labels.Pressure, std::format(L"{}", data1));
                break;

            case midi2msg::Midi1ChannelVoiceMessageStatus::PitchBend:
            {
                auto const raw = static_cast<int32_t>((data2 << 7) | data1);
                AppendField(result, labels.Bend, std::format(L"{} ({:+})", raw, raw - 8192));
                break;
            }

            default:
                break;
            }
        }

        void DecodeMidi2ChannelVoiceMessage(_In_ std::array<uint32_t, 4> const& words, _In_ UmpDescriptionLabels const& labels, _Inout_ UmpDescription& result)
        {

            auto const status = midi2msg::MidiMessageHelper::GetStatusFromMidi2ChannelVoiceMessageFirstWord(words[0]);
            auto const index1 = static_cast<uint8_t>((words[0] >> 8) & 0xFF);
            auto const index2 = static_cast<uint8_t>(words[0] & 0xFF);
            auto const word1 = words[1];

            switch (status)
            {
            case midi2msg::Midi2ChannelVoiceMessageStatus::NoteOff:
            case midi2msg::Midi2ChannelVoiceMessageStatus::NoteOn:
            {
                AppendField(result, labels.Note, FormatNote(index1 & 0x7F));
                AppendField(result, labels.Velocity, std::format(L"{}", static_cast<uint16_t>(word1 >> 16)));

                if (index2 != 0)
                {
                    AppendField(result, labels.Attribute,
                        std::format(L"{} / {}", index2, static_cast<uint16_t>(word1 & 0xFFFF)));
                }
                break;
            }

            case midi2msg::Midi2ChannelVoiceMessageStatus::PolyPressure:
                AppendField(result, labels.Note, FormatNote(index1 & 0x7F));
                AppendField(result, labels.Pressure, std::format(L"{}", word1));
                break;

            case midi2msg::Midi2ChannelVoiceMessageStatus::ChannelPressure:
                AppendField(result, labels.Pressure, std::format(L"{}", word1));
                break;

            case midi2msg::Midi2ChannelVoiceMessageStatus::ControlChange:
                AppendField(result, labels.Controller, FormatController(index1 & 0x7F));
                AppendField(result, labels.Value, std::format(L"{}", word1));
                break;

            case midi2msg::Midi2ChannelVoiceMessageStatus::ProgramChange:
            {
                AppendField(result, labels.Program, std::format(L"{}", static_cast<uint8_t>((word1 >> 24) & 0x7F)));

                if ((index2 & 0x01) != 0)
                {
                    auto const bankMsb = static_cast<uint8_t>((word1 >> 8) & 0x7F);
                    auto const bankLsb = static_cast<uint8_t>(word1 & 0x7F);

                    AppendField(result, labels.Bank, std::format(L"{} / {}", bankMsb, bankLsb));
                }
                break;
            }

            case midi2msg::Midi2ChannelVoiceMessageStatus::PitchBend:
                AppendField(result, labels.Bend,
                    std::format(L"{} ({:+})", word1, static_cast<int64_t>(word1) - 0x80000000LL));
                break;

            case midi2msg::Midi2ChannelVoiceMessageStatus::PerNotePitchBend:
                AppendField(result, labels.Note, FormatNote(index1 & 0x7F));
                AppendField(result, labels.Bend,
                    std::format(L"{} ({:+})", word1, static_cast<int64_t>(word1) - 0x80000000LL));
                break;

            case midi2msg::Midi2ChannelVoiceMessageStatus::RegisteredPerNoteController:
            case midi2msg::Midi2ChannelVoiceMessageStatus::AssignablePerNoteController:
                AppendField(result, labels.Note, FormatNote(index1 & 0x7F));
                AppendField(result, labels.Index, std::format(L"{}", index2));
                AppendField(result, labels.Value, std::format(L"{}", word1));
                break;

            case midi2msg::Midi2ChannelVoiceMessageStatus::PerNoteManagement:
                AppendField(result, labels.Note, FormatNote(index1 & 0x7F));
                AppendField(result, labels.Options, std::format(L"0x{:02X}", index2));
                break;

            case midi2msg::Midi2ChannelVoiceMessageStatus::RegisteredController:
            case midi2msg::Midi2ChannelVoiceMessageStatus::AssignableController:
            case midi2msg::Midi2ChannelVoiceMessageStatus::RelativeRegisteredController:
            case midi2msg::Midi2ChannelVoiceMessageStatus::RelativeAssignableController:
                AppendField(result, labels.Bank, std::format(L"{}", index1 & 0x7F));
                AppendField(result, labels.Index, std::format(L"{}", index2 & 0x7F));
                AppendField(result, labels.Value, std::format(L"{}", word1));
                break;

            default:
                break;
            }
        }

        void DecodeData64Message(_In_ std::array<uint32_t, 4> const& words, _In_ UmpDescriptionLabels const& labels, _Inout_ UmpDescription& result)
        {

            auto const byteCount = std::min<uint8_t>(
                midi2msg::MidiMessageHelper::GetNumberOfBytesFromDataMessage64FirstWord(words[0]), 6);

            std::array<uint8_t, 8> raw{};
            UnpackWords(words, 2, raw.data());

            // payload starts at the third byte of the first word
            AppendField(result, labels.Bytes, FormatByteList(raw.data() + 2, byteCount));
        }

        void DecodeData128Message(_In_ std::array<uint32_t, 4> const& words, _In_ UmpDescriptionLabels const& labels, _Inout_ UmpDescription& result)
        {

            auto const byteCount = std::min<uint8_t>(
                midi2msg::MidiMessageHelper::GetNumberOfBytesFromDataMessage128FirstWord(words[0]), 13);

            std::array<uint8_t, 16> raw{};
            UnpackWords(words, 4, raw.data());

            AppendField(result, labels.StreamId, std::format(L"{}", raw[2]));
            AppendField(result, labels.Bytes, FormatByteList(raw.data() + 3, byteCount));
        }

        void DecodeFlexDataMessage(_In_ std::array<uint32_t, 4> const& words, _In_ UmpDescriptionLabels const& labels, _Inout_ UmpDescription& result)
        {

            auto const statusBank = midi2msg::MidiMessageHelper::GetStatusBankFromFlexDataMessageFirstWord(words[0]);
            auto const status = midi2msg::MidiMessageHelper::GetStatusFromFlexDataMessageFirstWord(words[0]);

            std::array<uint8_t, 16> raw{};
            UnpackWords(words, 4, raw.data());

            if (statusBank == 0x00)
            {
                switch (status)
                {
                case 0x00: // Set Tempo, in 10ns per quarter note
                {
                    auto const nanosecondsPerQuarterNote = words[1];

                    if (nanosecondsPerQuarterNote > 0)
                    {
                        auto const beatsPerMinute = 6000000000.0 / static_cast<double>(nanosecondsPerQuarterNote);
                        AppendField(result, labels.Tempo, std::format(L"{:.3f} BPM", beatsPerMinute));
                    }
                    break;
                }

                case 0x01: // Set Time Signature
                {
                    // The denominator is sent as a negative power of two: 2 is a quarter note.
                    auto const numerator = raw[4];
                    auto const denominatorPower = raw[5];

                    if (denominatorPower > 0 && denominatorPower < 32)
                    {
                        AppendField(result, labels.TimeSignature,
                            std::format(L"{}/{}", numerator, 1u << denominatorPower));
                    }
                    else
                    {
                        AppendField(result, labels.TimeSignature,
                            FormatWithNumber(labels.TimeSignatureNonStandardFormat, numerator));
                    }
                    break;
                }

                case 0x05: // Set Key Signature
                    AppendField(result, labels.KeySignature, std::format(L"0x{:02X}", raw[4]));
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
                    AppendField(result, labels.Text, text);
                }
            }
        }

        void DecodeStreamMessage(_In_ std::array<uint32_t, 4> const& words, _In_ UmpDescriptionLabels const& labels, _Inout_ UmpDescription& result)
        {

            auto const status = midi2msg::MidiMessageHelper::GetStatusFromStreamMessageFirstWord(words[0]);

            std::array<uint8_t, 16> raw{};
            UnpackWords(words, 4, raw.data());

            switch (status)
            {
            case 0x03: // Endpoint Name Notification
            case 0x04: // Product Instance Id Notification
            {
                auto const text = DecodeUtf8Payload(raw.data() + 2, 14);

                if (!text.empty())
                {
                    AppendField(result, labels.Name, text);
                }
                break;
            }

            case 0x12: // Function Block Name Notification
            {
                AppendField(result, labels.FunctionBlock, std::format(L"{}", raw[2]));

                auto const text = DecodeUtf8Payload(raw.data() + 3, 13);

                if (!text.empty())
                {
                    AppendField(result, labels.Name, text);
                }
                break;
            }

            case 0x10: // Function Block Discovery
            case 0x11: // Function Block Info Notification
                AppendField(result, labels.FunctionBlock, std::format(L"{}", raw[2]));
                break;

            default:
                break;
            }
        }

        // The format comes from an app's resources, so a bad one is logged rather than thrown.
        std::wstring FormatWithNumber(_In_ winrt::hstring const& format, _In_ uint32_t number)
        {
            try
            {
                return std::vformat(std::wstring_view{ format }, std::make_wformat_args(number));
            }
            catch (...)
            {
                LOG_CAUGHT_EXCEPTION();
                return std::to_wstring(number);
            }
        }
    }

    winrt::hstring UmpDescription::ToDisplayString() const
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
    UmpDescription DescribeUmp(std::array<uint32_t, 4> const& words, uint8_t wordCount, UmpDescriptionLabels const& labels) noexcept
    {
        UmpDescription fields{};

        try
        {
            if (wordCount == 0)
            {
                return fields;
            }

            switch (static_cast<uint8_t>((words[0] >> 28) & 0x0F))
            {
            case 0x0:
                DecodeUtilityMessage(words, labels, fields);
                break;
            case 0x1:
                DecodeSystemMessage(words, labels, fields);
                break;
            case 0x2:
                DecodeMidi1ChannelVoiceMessage(words, labels, fields);
                break;
            case 0x3:
                DecodeData64Message(words, labels, fields);
                break;
            case 0x4:
                DecodeMidi2ChannelVoiceMessage(words, labels, fields);
                break;
            case 0x5:
                DecodeData128Message(words, labels, fields);
                break;
            case 0xD:
                DecodeFlexDataMessage(words, labels, fields);
                break;
            case 0xF:
                DecodeStreamMessage(words, labels, fields);
                break;
            default:
                break;
            }
        }
        catch (...)
        {
            LOG_CAUGHT_EXCEPTION();
        }

        return fields;
    }

    _Use_decl_annotations_
    winrt::hstring UmpDisplayName(uint32_t word0) noexcept
    {
        try
        {
            return midi2msg::MidiMessageHelper::GetMessageDisplayNameFromFirstWord(word0);
        }
        catch (...)
        {
            LOG_CAUGHT_EXCEPTION();
        }

        return {};
    }

    _Use_decl_annotations_
    uint32_t UmpMessageTypeColorIndex(uint8_t messageType) noexcept
    {
        switch (messageType & 0x0F)
        {
        case 0x0: return 0;     // utility
        case 0x1: return 1;     // system
        case 0x2: return 2;     // MIDI 1.0 channel voice
        case 0x4: return 3;     // MIDI 2.0 channel voice
        case 0x3:
        case 0x5: return 4;     // data
        case 0xD: return 5;     // flex data
        case 0xF: return 6;     // stream
        default:  return 7;
        }
    }
}