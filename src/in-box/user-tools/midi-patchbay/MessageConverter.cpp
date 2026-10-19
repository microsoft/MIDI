// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

// Pure: no precompiled header, so the unit tests compile this file exactly as it ships.

#include "MessageConverter.h"
#include "LogicSteps.h"

#include <algorithm>
#include <cmath>
#include <iterator>

namespace midipatchbay
{
    namespace
    {
        constexpr uint32_t TypeSystem = 0x1;
        constexpr uint32_t TypeMidi1 = 0x2;
        constexpr uint32_t TypeMidi2 = 0x4;
        constexpr uint32_t TypeStream = 0xF;

        constexpr uint8_t StatusRegistered = 0x2;
        constexpr uint8_t StatusAssignable = 0x3;
        constexpr uint8_t StatusNoteOff = 0x8;
        constexpr uint8_t StatusNoteOn = 0x9;
        constexpr uint8_t StatusPolyPressure = 0xA;
        constexpr uint8_t StatusControlChange = 0xB;
        constexpr uint8_t StatusProgramChange = 0xC;
        constexpr uint8_t StatusChannelPressure = 0xD;
        constexpr uint8_t StatusPitchBend = 0xE;

        constexpr uint8_t StatusStart = 0xFA;
        constexpr uint8_t StatusContinue = 0xFB;
        constexpr uint8_t StatusStop = 0xFC;

        struct InputInfo
        {
            ConverterInput Input;
            wchar_t const* Key;
        };

        constexpr InputInfo Inputs[] =
        {
            { ConverterInput::Note, L"note" },
            { ConverterInput::ControlChange, L"controlChange" },
            { ConverterInput::ProgramChange, L"programChange" },
            { ConverterInput::PitchBend, L"pitchBend" },
            { ConverterInput::ChannelPressure, L"channelPressure" },
            { ConverterInput::PolyPressure, L"polyPressure" },
        };

        static_assert(std::size(Inputs) == ConverterInputCount);

        constexpr wchar_t const* ReleaseKeys[] = { L"send", L"nothing", L"toggle" };
        constexpr wchar_t const* ProgramStepKeys[] = { L"this", L"next", L"previous" };

        constexpr wchar_t KeyRows[] = L"rows";
        constexpr wchar_t KeyScale[] = L"valueScale";
        constexpr wchar_t KeyFrom[] = L"from";
        constexpr wchar_t KeyTo[] = L"to";
        constexpr wchar_t KeyMessage[] = L"message";
        constexpr wchar_t KeyChannel[] = L"channel";
        constexpr wchar_t KeyNumber[] = L"number";
        constexpr wchar_t KeyLowest[] = L"lowest";
        constexpr wchar_t KeyHighest[] = L"highest";
        constexpr wchar_t KeyButton[] = L"button";
        constexpr wchar_t KeyPressPercent[] = L"pressPercent";
        constexpr wchar_t KeyVelocityPercent[] = L"velocityPercent";
        constexpr wchar_t KeyValuePercent[] = L"valuePercent";
        constexpr wchar_t KeyMidi1[] = L"midi1";
        constexpr wchar_t KeyRelease[] = L"release";
        constexpr wchar_t KeyReleasePercent[] = L"releasePercent";
        constexpr wchar_t KeyStep[] = L"step";
        constexpr wchar_t KeyWraps[] = L"wraps";
        constexpr wchar_t KeyMessageWords[] = L"messageWords";

        constexpr wchar_t ScaleSevenBit[] = L"sevenBit";
        constexpr wchar_t ScalePercent[] = L"percent";

        // ----------------------------------------------------------------------- the file

        json::JsonValue GetValue(_In_ json::JsonObject const& object, _In_ std::wstring_view key) noexcept
        {
            try
            {
                if (object != nullptr && object.HasKey(key))
                {
                    return object.GetNamedValue(key);
                }
            }
            catch (...)
            {
            }

            return nullptr;
        }

        // A whole number from 0 to highest, or nothing when it's missing or isn't one.
        std::optional<uint32_t> ReadWhole(_In_ json::JsonObject const& object, _In_ std::wstring_view key, _In_ uint32_t highest) noexcept
        {
            try
            {
                auto const value = GetValue(object, key);

                if (value != nullptr && value.ValueType() == json::JsonValueType::Number)
                {
                    auto const number = value.GetNumber();

                    if (std::isfinite(number) && number >= 0 && number <= highest && std::floor(number) == number)
                    {
                        return static_cast<uint32_t>(number);
                    }
                }
            }
            catch (...)
            {
            }

            return std::nullopt;
        }

        // A percent from 0 to 100, in hundredths.
        std::optional<int32_t> ReadPercent(_In_ json::JsonObject const& object, _In_ std::wstring_view key) noexcept
        {
            try
            {
                auto const value = GetValue(object, key);

                if (value != nullptr && value.ValueType() == json::JsonValueType::Number)
                {
                    auto const percent = value.GetNumber();

                    if (std::isfinite(percent) && percent >= 0 && percent <= 100)
                    {
                        return static_cast<int32_t>(std::lround(percent * 100.0));
                    }
                }
            }
            catch (...)
            {
            }

            return std::nullopt;
        }

        std::optional<bool> ReadBool(_In_ json::JsonObject const& object, _In_ std::wstring_view key) noexcept
        {
            try
            {
                auto const value = GetValue(object, key);

                if (value != nullptr && value.ValueType() == json::JsonValueType::Boolean)
                {
                    return value.GetBoolean();
                }
            }
            catch (...)
            {
            }

            return std::nullopt;
        }

        std::wstring ReadString(_In_ json::JsonObject const& object, _In_ std::wstring_view key) noexcept
        {
            try
            {
                auto const value = GetValue(object, key);

                if (value != nullptr && value.ValueType() == json::JsonValueType::String)
                {
                    return std::wstring{ value.GetString() };
                }
            }
            catch (...)
            {
            }

            return {};
        }

        json::JsonObject ReadObject(_In_ json::JsonObject const& object, _In_ std::wstring_view key) noexcept
        {
            try
            {
                auto const value = GetValue(object, key);

                if (value != nullptr && value.ValueType() == json::JsonValueType::Object)
                {
                    return value.GetObject();
                }
            }
            catch (...)
            {
            }

            return nullptr;
        }

        void SetNumber(_In_ json::JsonObject const& object, _In_ std::wstring_view key, _In_ double value)
        {
            object.SetNamedValue(key, json::JsonValue::CreateNumberValue(value));
        }

        void SetBool(_In_ json::JsonObject const& object, _In_ std::wstring_view key, _In_ bool value)
        {
            object.SetNamedValue(key, json::JsonValue::CreateBooleanValue(value));
        }

        void SetString(_In_ json::JsonObject const& object, _In_ std::wstring_view key, _In_ std::wstring_view value)
        {
            object.SetNamedValue(key, json::JsonValue::CreateStringValue(value));
        }

        void SetPercent(_In_ json::JsonObject const& object, _In_ std::wstring_view key, _In_ int32_t hundredths)
        {
            SetNumber(object, key, std::clamp(hundredths, 0, FullScaleHundredths) / 100.0);
        }

        bool IsParameter(_In_ ControllerMessageKind kind) noexcept
        {
            return kind == ControllerMessageKind::RegisteredController || kind == ControllerMessageKind::AssignableController;
        }

        json::JsonObject RowToJson(_In_ ConverterRow const& row)
        {
            json::JsonObject from{};

            SetString(from, KeyMessage, ConverterInputKey(row.From));

            if (row.FromChannel >= 0 && row.FromChannel < 16)
            {
                SetNumber(from, KeyChannel, row.FromChannel);
            }

            if (ConverterInputHasNumber(row.From))
            {
                auto const lowest = (std::min)(row.Lowest, row.Highest);
                auto const highest = (std::max)(row.Lowest, row.Highest);

                if (lowest == highest)
                {
                    SetNumber(from, KeyNumber, lowest);
                }
                else if (lowest != 0 || highest != 127)
                {
                    SetNumber(from, KeyLowest, lowest);
                    SetNumber(from, KeyHighest, highest);
                }
            }

            if (ConverterInputIsValue(row.From) && row.AsButton)
            {
                SetBool(from, KeyButton, true);
                SetPercent(from, KeyPressPercent, row.PressHundredths);
            }

            auto const& message = row.To;
            auto const isNote = message.Kind == ControllerMessageKind::Note;

            json::JsonObject to{};

            SetString(to, KeyMessage, ControllerMessageKindKey(message.Kind));

            if (IsChannelMessageKind(message.Kind))
            {
                if (!row.SameChannel)
                {
                    SetNumber(to, KeyChannel, message.Channel & 0x0F);
                }

                // An RPN or an NRPN always goes as MIDI 2.0.
                if (!IsParameter(message.Kind) && !row.SameProtocol)
                {
                    SetBool(to, KeyMidi1, message.Midi1Protocol);
                }
            }

            auto const maximum = ControllerMessageNumberMaximum(message.Kind);
            auto const numbered = maximum != 0 &&
                (message.Kind != ControllerMessageKind::ProgramChange || message.Step == ProgramStep::This);

            // A message with no number of its own always has one written.
            if (numbered && (!row.SameNumber || !ConverterInputHasNumber(row.From)))
            {
                SetNumber(to, KeyNumber, (std::min)(message.Number, maximum));
            }

            if ((isNote || IsValueMessageKind(message.Kind)) && (!row.SameValue || !ConverterInputHasValue(row.From)))
            {
                SetPercent(to, isNote ? KeyVelocityPercent : KeyValuePercent, message.ValueHundredths);
            }

            if (message.Kind == ControllerMessageKind::ProgramChange)
            {
                auto const step = (std::min)(static_cast<size_t>(message.Step), std::size(ProgramStepKeys) - 1);

                SetString(to, KeyStep, ProgramStepKeys[step]);

                if (message.Step != ProgramStep::This)
                {
                    SetNumber(to, KeyLowest, message.LowestProgram);
                    SetNumber(to, KeyHighest, message.HighestProgram);
                    SetBool(to, KeyWraps, message.Wraps);
                }
            }

            if (message.Kind == ControllerMessageKind::Words)
            {
                json::JsonArray list{};

                for (uint8_t i = 0; i < message.WordCount && i < MaximumControllerWords; i++)
                {
                    list.Append(json::JsonValue::CreateNumberValue(message.Words[i]));
                }

                to.SetNamedValue(KeyMessageWords, list);
            }

            if (ConverterRowCanLetGo(row) && ConverterOutputCanLetGo(message.Kind))
            {
                auto const release = (std::min)(static_cast<size_t>(row.Release), std::size(ReleaseKeys) - 1);

                SetString(to, KeyRelease, ReleaseKeys[release]);

                if (!isNote && row.Release != ConverterRelease::Nothing)
                {
                    SetPercent(to, KeyReleasePercent, message.ReleaseHundredths);
                }
            }

            json::JsonObject item{};

            item.SetNamedValue(KeyFrom, from);
            item.SetNamedValue(KeyTo, to);

            return item;
        }

        // Up to four whole words, or none when they aren't.
        uint8_t ReadWords(
            _In_ json::JsonObject const& object,
            _Out_writes_to_(MaximumControllerWords, return) uint32_t* words) noexcept
        {
            uint8_t count{ 0 };

            try
            {
                auto const value = GetValue(object, KeyMessageWords);

                if (value == nullptr || value.ValueType() != json::JsonValueType::Array)
                {
                    return 0;
                }

                for (auto const& word : value.GetArray())
                {
                    if (count >= MaximumControllerWords || word == nullptr || word.ValueType() != json::JsonValueType::Number)
                    {
                        break;
                    }

                    auto const number = word.GetNumber();

                    if (!std::isfinite(number) || number < 0 || number > 4294967295.0 || std::floor(number) != number)
                    {
                        break;
                    }

                    words[count++] = static_cast<uint32_t>(number);
                }
            }
            catch (...)
            {
            }

            return count;
        }

        ConverterRow RowFromJson(_In_ json::JsonObject const& item) noexcept
        {
            ConverterRow row{};

            auto const from = ReadObject(item, KeyFrom);
            auto const to = ReadObject(item, KeyTo);

            row.From = ConverterInputFromKey(ReadString(from, KeyMessage)).value_or(ConverterInput::Note);

            if (auto const channel = ReadWhole(from, KeyChannel, 15))
            {
                row.FromChannel = static_cast<int8_t>(*channel);
            }

            if (auto const number = ReadWhole(from, KeyNumber, 127))
            {
                row.Lowest = static_cast<uint8_t>(*number);
                row.Highest = static_cast<uint8_t>(*number);
            }
            else
            {
                row.Lowest = static_cast<uint8_t>(ReadWhole(from, KeyLowest, 127).value_or(0));
                row.Highest = static_cast<uint8_t>(ReadWhole(from, KeyHighest, 127).value_or(127));
            }

            row.AsButton = ConverterInputIsValue(row.From) && ReadBool(from, KeyButton).value_or(false);
            row.PressHundredths = ReadPercent(from, KeyPressPercent).value_or(DefaultConverterPressHundredths);

            // A converter always sends something, or it would be a filter.
            auto const kind = ControllerMessageKindFromKey(ReadString(to, KeyMessage));
            auto& message = row.To;

            message = ControllerMessageFor(kind.has_value() && *kind != ControllerMessageKind::Nothing
                ? *kind
                : ControllerMessageKind::ControlChange);

            if (auto const channel = ReadWhole(to, KeyChannel, 15))
            {
                row.SameChannel = false;
                message.Channel = static_cast<uint8_t>(*channel);
            }

            auto const maximum = ControllerMessageNumberMaximum(message.Kind);

            if (auto const number = maximum == 0 ? std::optional<uint32_t>{} : ReadWhole(to, KeyNumber, maximum))
            {
                row.SameNumber = false;
                message.Number = *number;
            }

            auto const isNote = message.Kind == ControllerMessageKind::Note;

            if (auto const value = ReadPercent(to, isNote ? KeyVelocityPercent : KeyValuePercent))
            {
                row.SameValue = false;
                message.ValueHundredths = *value;
            }

            if (auto const midi1 = IsParameter(message.Kind) ? std::optional<bool>{} : ReadBool(to, KeyMidi1))
            {
                row.SameProtocol = false;
                message.Midi1Protocol = *midi1;
            }

            message.ReleaseHundredths = ReadPercent(to, KeyReleasePercent).value_or(message.ReleaseHundredths);

            auto const release = ReadString(to, KeyRelease);

            row.Release = release == ReleaseKeys[1] ? ConverterRelease::Nothing
                : release == ReleaseKeys[2] ? ConverterRelease::Toggle
                : ConverterRelease::Send;

            if (message.Kind == ControllerMessageKind::ProgramChange)
            {
                auto const step = ReadString(to, KeyStep);

                message.Step = step == ProgramStepKeys[1] ? ProgramStep::Next
                    : step == ProgramStepKeys[2] ? ProgramStep::Previous
                    : ProgramStep::This;

                // Not put in order: SteppedProgram does, and the file keeps what was typed.
                message.LowestProgram = static_cast<uint8_t>(ReadWhole(to, KeyLowest, 127).value_or(message.LowestProgram));
                message.HighestProgram = static_cast<uint8_t>(ReadWhole(to, KeyHighest, 127).value_or(message.HighestProgram));
                message.Wraps = ReadBool(to, KeyWraps).value_or(message.Wraps);
            }

            if (message.Kind == ControllerMessageKind::Words)
            {
                message.WordCount = (std::max)(ReadWords(to, message.Words.data()), uint8_t{ 1 });
            }

            return row;
        }

        // ----------------------------------------------------------------------- the messages

        // A message as a converter reads it.
        struct Incoming
        {
            ConverterInput Input{ ConverterInput::Note };
            bool Midi1{ true };
            uint8_t Group{ 0 };
            uint8_t Channel{ 0 };
            uint8_t Number{ 0 };
            LogicValue Value{};

            // A note off, or a MIDI 1.0 note on at velocity 0.
            bool NoteOff{ false };
        };

        bool Read(_In_reads_(wordCount) uint32_t const* words, _In_ uint8_t wordCount, _Out_ Incoming& in) noexcept
        {
            in = Incoming{};

            if (words == nullptr || wordCount == 0)
            {
                return false;
            }

            auto const word = words[0];
            auto const type = word >> 28;

            if (type != TypeMidi1 && type != TypeMidi2)
            {
                return false;
            }

            auto const midi2 = type == TypeMidi2;

            if (midi2 && wordCount < 2)
            {
                return false;
            }

            auto const data = midi2 ? words[1] : 0u;
            auto const status = static_cast<uint8_t>((word >> 20) & 0x0F);
            auto const data1 = static_cast<uint8_t>((word >> 8) & 0x7F);
            auto const data2 = static_cast<uint8_t>(word & 0x7F);

            in.Midi1 = !midi2;
            in.Group = static_cast<uint8_t>((word >> 24) & 0x0F);
            in.Channel = static_cast<uint8_t>((word >> 16) & 0x0F);

            switch (status)
            {
            case StatusNoteOff:
            case StatusNoteOn:
                in.Input = ConverterInput::Note;
                in.Number = data1;
                in.Value = midi2 ? ScaledValue(data >> 16, SixteenBitRange) : ScaledValue(data2, SevenBitRange);

                // A MIDI 2.0 note on at velocity 0 is a quiet note, not a note off.
                in.NoteOff = status == StatusNoteOff || (!midi2 && data2 == 0);
                return true;

            case StatusPolyPressure:
            case StatusControlChange:
                in.Input = status == StatusControlChange ? ConverterInput::ControlChange : ConverterInput::PolyPressure;
                in.Number = data1;
                in.Value = midi2 ? ScaledValue(data, ThirtyTwoBitRange) : ScaledValue(data2, SevenBitRange);
                return true;

            case StatusProgramChange:
                in.Input = ConverterInput::ProgramChange;
                in.Number = midi2 ? static_cast<uint8_t>((data >> 24) & 0x7F) : data1;
                return true;

            case StatusChannelPressure:
                in.Input = ConverterInput::ChannelPressure;
                in.Value = midi2 ? ScaledValue(data, ThirtyTwoBitRange) : ScaledValue(data1, SevenBitRange);
                return true;

            case StatusPitchBend:
                in.Input = ConverterInput::PitchBend;

                // Low seven bits first.
                in.Value = midi2
                    ? ScaledValue(data, ThirtyTwoBitRange)
                    : ScaledValue((static_cast<uint32_t>(data2) << 7) | data1, FourteenBitRange);
                return true;

            default:
                return false;
            }
        }

        bool Matches(_In_ ConverterRow const& row, _In_ Incoming const& in) noexcept
        {
            if (row.From != in.Input || (row.FromChannel >= 0 && row.FromChannel != in.Channel))
            {
                return false;
            }

            return !ConverterInputHasNumber(in.Input) ||
                (in.Number >= (std::min)(row.Lowest, row.Highest) && in.Number <= (std::max)(row.Lowest, row.Highest));
        }

        // Where an input's bits are: by its kind, group, channel and number.
        size_t SlotOf(_In_ Incoming const& in) noexcept
        {
            auto const number = ConverterInputHasNumber(in.Input) ? in.Number : uint8_t{ 0 };

            return ((static_cast<size_t>(in.Input) * 16 + in.Group) * 16 + in.Channel) * 128 + number;
        }

        uint8_t WordsForType(_In_ uint32_t firstWord) noexcept
        {
            constexpr uint8_t sizes[16] = { 1, 1, 1, 2, 2, 4, 1, 1, 2, 2, 2, 3, 3, 4, 4, 4 };

            return sizes[(firstWord >> 28) & 0x0F];
        }

        uint8_t NextProgram(
            _Inout_ ConverterMemory& memory,
            _In_ uint8_t group,
            _In_ uint8_t channel,
            _In_ ControllerMessage const& to) noexcept
        {
            auto& place = memory.Programs[static_cast<size_t>(group & 0x0F) * 16 + (channel & 0x0F)];
            auto stored = place.load(std::memory_order_relaxed);
            uint8_t program{ 0 };

            do
            {
                program = SteppedProgram(static_cast<int32_t>(stored) - 1, to.Step, to.LowestProgram, to.HighestProgram, to.Wraps);
            }
            while (!place.compare_exchange_weak(stored, program + 1u, std::memory_order_relaxed));

            return program;
        }

        // What a row sends as its input is pressed, or let go. Returns how many words, which is 0
        // when the message has nothing to send then.
        uint8_t BuildWords(
            _In_ ConverterRow const& row,
            _In_ Incoming const& in,
            _In_ bool letGo,
            _Inout_ ConverterMemory& memory,
            _Out_writes_to_(MaximumControllerWords, return) uint32_t* out) noexcept
        {
            auto const& to = row.To;
            auto const group = in.Group;
            auto const channel = row.SameChannel ? in.Channel : static_cast<uint8_t>(to.Channel & 0x0F);
            auto const number = row.SameNumber && ConverterInputHasNumber(in.Input) ? static_cast<uint32_t>(in.Number) : to.Number;
            auto const seven = static_cast<uint8_t>((std::min)(number, 127u));
            auto const midi1 = row.SameProtocol ? in.Midi1 : to.Midi1Protocol;

            auto const value = letGo
                ? ScaledValue(static_cast<uint32_t>(std::clamp(to.ReleaseHundredths, 0, FullScaleHundredths)), HundredthsRange)
                : row.SameValue && ConverterInputHasValue(in.Input)
                    ? in.Value
                    : ScaledValue(static_cast<uint32_t>(std::clamp(to.ValueHundredths, 0, FullScaleHundredths)), HundredthsRange);

            switch (to.Kind)
            {
            case ControllerMessageKind::Note:
                if (midi1)
                {
                    // A MIDI 1.0 note on at velocity 0 would be a note off.
                    out[0] = letGo
                        ? midiapp::BuildMidi1ChannelVoice(group, StatusNoteOff, channel, seven, 0)
                        : midiapp::BuildMidi1ChannelVoice(group, StatusNoteOn, channel, seven,
                            static_cast<uint8_t>((std::max)(FieldValueOf(value, 7), 1u)));
                    return 1;
                }

                midiapp::BuildMidi2ChannelVoice(group, letGo ? StatusNoteOff : StatusNoteOn, channel, seven, 0,
                    letGo ? 0u : (std::max)(FieldValueOf(value, 16), 1u) << 16, out);
                return 2;

            case ControllerMessageKind::ControlChange:
            case ControllerMessageKind::PolyPressure:
            {
                auto const status = to.Kind == ControllerMessageKind::ControlChange ? StatusControlChange : StatusPolyPressure;

                if (midi1)
                {
                    out[0] = midiapp::BuildMidi1ChannelVoice(group, status, channel, seven, static_cast<uint8_t>(FieldValueOf(value, 7)));
                    return 1;
                }

                midiapp::BuildMidi2ChannelVoice(group, status, channel, seven, 0, FieldValueOf(value, 32), out);
                return 2;
            }

            case ControllerMessageKind::ChannelPressure:
                if (midi1)
                {
                    out[0] = midiapp::BuildMidi1ChannelVoice(group, StatusChannelPressure, channel, static_cast<uint8_t>(FieldValueOf(value, 7)), 0);
                    return 1;
                }

                midiapp::BuildMidi2ChannelVoice(group, StatusChannelPressure, channel, 0, 0, FieldValueOf(value, 32), out);
                return 2;

            case ControllerMessageKind::PitchBend:
                if (midi1)
                {
                    auto const bend = FieldValueOf(value, 14);

                    // Low seven bits first.
                    out[0] = midiapp::BuildMidi1ChannelVoice(group, StatusPitchBend, channel,
                        static_cast<uint8_t>(bend & 0x7F), static_cast<uint8_t>((bend >> 7) & 0x7F));
                    return 1;
                }

                midiapp::BuildMidi2ChannelVoice(group, StatusPitchBend, channel, 0, 0, FieldValueOf(value, 32), out);
                return 2;

            // Always MIDI 2.0. Windows makes the control changes for a MIDI 1.0 device.
            case ControllerMessageKind::RegisteredController:
            case ControllerMessageKind::AssignableController:
            {
                auto const parameter = (std::min)(number, midiapp::MaximumValueMessageNumber);

                midiapp::BuildMidi2ChannelVoice(group,
                    to.Kind == ControllerMessageKind::RegisteredController ? StatusRegistered : StatusAssignable,
                    channel, static_cast<uint8_t>((parameter >> 7) & 0x7F), static_cast<uint8_t>(parameter & 0x7F),
                    FieldValueOf(value, 32), out);
                return 2;
            }

            case ControllerMessageKind::ProgramChange:
            {
                if (letGo)
                {
                    return 0;
                }

                auto const program = to.Step == ProgramStep::This ? seven : NextProgram(memory, group, channel, to);

                if (midi1)
                {
                    out[0] = midiapp::BuildMidi1ChannelVoice(group, StatusProgramChange, channel, program, 0);
                    return 1;
                }

                // No bank: the bank valid bit stays clear.
                midiapp::BuildMidi2ChannelVoice(group, StatusProgramChange, channel, 0, 0, static_cast<uint32_t>(program & 0x7F) << 24, out);
                return 2;
            }

            case ControllerMessageKind::Start:
            case ControllerMessageKind::Continue:
            case ControllerMessageKind::Stop:
            {
                if (letGo)
                {
                    return 0;
                }

                auto const status = to.Kind == ControllerMessageKind::Start ? StatusStart
                    : to.Kind == ControllerMessageKind::Continue ? StatusContinue
                    : StatusStop;

                out[0] = (TypeSystem << 28) | (static_cast<uint32_t>(group) << 24) | (static_cast<uint32_t>(status) << 16);
                return 1;
            }

            case ControllerMessageKind::Words:
            {
                auto const count = to.WordCount;
                auto const type = to.Words[0] >> 28;

                // One whole message with a group, which it takes from the message it replaces.
                if (letGo || count < 1 || count > MaximumControllerWords || count != WordsForType(to.Words[0]) ||
                    type == 0x0 || type == TypeStream)
                {
                    return 0;
                }

                std::copy_n(to.Words.begin(), count, out);
                out[0] = (out[0] & ~0x0F000000u) | (static_cast<uint32_t>(group) << 24);
                return count;
            }

            default:
                return 0;
            }
        }
    }

    _Use_decl_annotations_
    std::wstring_view ConverterInputKey(ConverterInput input) noexcept
    {
        for (auto const& info : Inputs)
        {
            if (info.Input == input)
            {
                return info.Key;
            }
        }

        return Inputs[0].Key;
    }

    _Use_decl_annotations_
    std::optional<ConverterInput> ConverterInputFromKey(std::wstring_view key) noexcept
    {
        for (auto const& info : Inputs)
        {
            if (key == info.Key)
            {
                return info.Input;
            }
        }

        return std::nullopt;
    }

    _Use_decl_annotations_
    bool ConverterInputHasNumber(ConverterInput input) noexcept
    {
        return input == ConverterInput::Note || input == ConverterInput::ControlChange ||
            input == ConverterInput::ProgramChange || input == ConverterInput::PolyPressure;
    }

    _Use_decl_annotations_
    bool ConverterInputHasValue(ConverterInput input) noexcept
    {
        return input != ConverterInput::ProgramChange;
    }

    _Use_decl_annotations_
    bool ConverterInputIsValue(ConverterInput input) noexcept
    {
        return input == ConverterInput::ControlChange || input == ConverterInput::PitchBend ||
            input == ConverterInput::ChannelPressure || input == ConverterInput::PolyPressure;
    }

    ConverterRow DefaultConverterRow() noexcept
    {
        ConverterRow row{};

        row.Lowest = 60;
        row.Highest = 60;

        return row;
    }

    _Use_decl_annotations_
    bool ConverterRowCanLetGo(ConverterRow const& row) noexcept
    {
        return row.From == ConverterInput::Note || (ConverterInputIsValue(row.From) && row.AsButton);
    }

    _Use_decl_annotations_
    bool ConverterOutputCanLetGo(ControllerMessageKind kind) noexcept
    {
        return kind == ControllerMessageKind::Note || IsValueMessageKind(kind);
    }

    _Use_decl_annotations_
    bool ConvertMessage(
        MessageConverterSettings const& settings,
        ConverterMemory& memory,
        uint32_t const* words,
        uint8_t wordCount,
        ConvertedMessages& converted) noexcept
    {
        converted = ConvertedMessages{};

        Incoming in{};

        if (settings.Rows.empty() || !Read(words, wordCount, in))
        {
            return false;
        }

        auto const found = std::find_if(settings.Rows.begin(), settings.Rows.end(),
            [&in](ConverterRow const& row) { return Matches(row, in); });

        if (found == settings.Rows.end())
        {
            return false;
        }

        auto const& row = *found;
        auto const slot = SlotOf(in);
        auto const bit = 1ull << (slot % 64);
        auto& down = memory.Down[slot / 64];
        auto& on = memory.On[slot / 64];

        auto const add = [&](bool letGo)
            {
                if (converted.Count >= MaximumConvertedMessages)
                {
                    return;
                }

                auto const count = BuildWords(row, in, letGo, memory, converted.Words[converted.Count].data());

                if (count > 0)
                {
                    converted.Counts[converted.Count] = count;
                    converted.Count++;
                }
            };

        auto const canLetGo = ConverterRowCanLetGo(row);
        auto letGo{ false };

        if (in.Input == ConverterInput::Note)
        {
            letGo = in.NoteOff;
        }
        else if (canLetGo)
        {
            // Only crossing the press point counts, so a value that wanders while held sends nothing.
            auto const pressAt = (std::max)(SevenBitFromHundredths(std::clamp(row.PressHundredths, 0, FullScaleHundredths)), 1);
            auto const isDown = static_cast<int32_t>(WholeOf(in.Value)) >= pressAt;

            auto const before = isDown
                ? down.fetch_or(bit, std::memory_order_relaxed)
                : down.fetch_and(~bit, std::memory_order_relaxed);

            if (((before & bit) != 0) == isDown)
            {
                return true;
            }

            letGo = !isDown;
        }

        auto const hasOff = ConverterOutputCanLetGo(row.To.Kind);

        if (letGo)
        {
            if (row.Release == ConverterRelease::Send && hasOff)
            {
                add(true);
            }

            return true;
        }

        if (canLetGo && row.Release == ConverterRelease::Toggle && hasOff)
        {
            auto const wasOn = (on.fetch_xor(bit, std::memory_order_relaxed) & bit) != 0;

            add(wasOn);
            return true;
        }

        add(false);

        // Nothing would ever end a note started by an input that never lets go.
        if (!canLetGo && row.To.Kind == ControllerMessageKind::Note)
        {
            add(true);
        }

        return true;
    }

    _Use_decl_annotations_
    void MessageConverterToJson(MessageConverterSettings const& settings, json::JsonObject const& object)
    {
        json::JsonArray rows{};

        for (auto const& row : settings.Rows)
        {
            rows.Append(RowToJson(row));
        }

        object.SetNamedValue(KeyRows, rows);
        SetString(object, KeyScale, settings.Scale == ValueScale::Percent ? ScalePercent : ScaleSevenBit);
    }

    _Use_decl_annotations_
    MessageConverterSettings MessageConverterFromJson(json::JsonObject const& object) noexcept
    {
        MessageConverterSettings settings{};

        try
        {
            settings.Scale = ReadString(object, KeyScale) == ScalePercent ? ValueScale::Percent : ValueScale::SevenBit;

            auto const value = GetValue(object, KeyRows);

            if (value == nullptr || value.ValueType() != json::JsonValueType::Array)
            {
                return settings;
            }

            for (auto const& item : value.GetArray())
            {
                if (settings.Rows.size() >= MaximumConverterRows)
                {
                    break;
                }

                if (item != nullptr && item.ValueType() == json::JsonValueType::Object)
                {
                    settings.Rows.push_back(RowFromJson(item.GetObject()));
                }
            }
        }
        catch (...)
        {
        }

        return settings;
    }

    _Use_decl_annotations_
    std::wstring MessageConverterSignature(MessageConverterSettings const& settings)
    {
        // The file keeps exactly what the step does, and leaves out what a row doesn't use.
        json::JsonObject object{};
        MessageConverterToJson(settings, object);

        return std::wstring{ object.Stringify() };
    }
}
