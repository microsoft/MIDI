// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

// Pure: no precompiled header, so the unit tests compile this file exactly as it ships.

#include "LogicSteps.h"

#include <algorithm>
#include <cmath>
#include <iterator>

namespace midipatchbay
{
    namespace
    {
        constexpr uint32_t TypeMidi1 = 0x2;
        constexpr uint32_t TypeSysEx7 = 0x3;
        constexpr uint32_t TypeMidi2 = 0x4;
        constexpr uint32_t TypeData128 = 0x5;
        constexpr uint32_t TypeFlexData = 0xD;
        constexpr uint32_t TypeStream = 0xF;

        constexpr uint8_t StatusRegisteredPerNote = 0x0;
        constexpr uint8_t StatusAssignablePerNote = 0x1;
        constexpr uint8_t StatusPerNotePitchBend = 0x6;
        constexpr uint8_t StatusNoteOff = 0x8;
        constexpr uint8_t StatusNoteOn = 0x9;
        constexpr uint8_t StatusPolyPressure = 0xA;
        constexpr uint8_t StatusControlChange = 0xB;
        constexpr uint8_t StatusProgramChange = 0xC;
        constexpr uint8_t StatusChannelPressure = 0xD;
        constexpr uint8_t StatusPitchBend = 0xE;
        constexpr uint8_t StatusPerNoteManagement = 0xF;

        constexpr uint8_t ControllerBankMsb = 0;
        constexpr uint8_t ControllerBankLsb = 32;
        constexpr uint8_t ControllerAllSoundOff = 120;
        constexpr uint8_t ControllerAllNotesOff = 123;

        // Sustain, sostenuto and soft, in that order.
        constexpr uint8_t PedalControllers[]{ 64, 66, 67 };

        // The top bit of a remembered packet's high word: something is remembered.
        constexpr uint64_t KnownMark = 1ull << 63;

        // Above this many ways there are no bits left.
        constexpr int32_t HighestWay = 64;

        uint32_t MessageType(_In_ uint32_t word) noexcept { return word >> 28; }
        uint8_t GroupOf(_In_ uint32_t word) noexcept { return static_cast<uint8_t>((word >> 24) & 0x0F); }
        uint8_t StatusOf(_In_ uint32_t word) noexcept { return static_cast<uint8_t>((word >> 20) & 0x0F); }
        uint8_t ChannelOf(_In_ uint32_t word) noexcept { return static_cast<uint8_t>((word >> 16) & 0x0F); }
        uint8_t Data1Of(_In_ uint32_t word) noexcept { return static_cast<uint8_t>((word >> 8) & 0x7F); }
        uint8_t Data2Of(_In_ uint32_t word) noexcept { return static_cast<uint8_t>(word & 0x7F); }

        bool IsChannelVoice(_In_ uint32_t word) noexcept
        {
            return MessageType(word) == TypeMidi1 || MessageType(word) == TypeMidi2;
        }

        // Utility and stream messages have no group.
        bool HasGroup(_In_ uint32_t word) noexcept
        {
            return MessageType(word) != 0x0 && MessageType(word) != TypeStream;
        }

        bool CarriesNote(_In_ uint32_t word) noexcept
        {
            auto const status = StatusOf(word);

            if (status == StatusNoteOff || status == StatusNoteOn || status == StatusPolyPressure)
            {
                return true;
            }

            return MessageType(word) == TypeMidi2 &&
                (status == StatusRegisteredPerNote || status == StatusAssignablePerNote ||
                 status == StatusPerNotePitchBend || status == StatusPerNoteManagement);
        }

        // A MIDI 1.0 note on at velocity zero is a note off. A MIDI 2.0 one is a quiet note.
        bool IsNoteOff(_In_ uint32_t word) noexcept
        {
            return StatusOf(word) == StatusNoteOff ||
                (MessageType(word) == TypeMidi1 && StatusOf(word) == StatusNoteOn && Data2Of(word) == 0);
        }

        uint32_t BitsOfRange(_In_ uint32_t range) noexcept
        {
            switch (range)
            {
            case SevenBitRange:     return 7;
            case FourteenBitRange:  return 14;
            case SixteenBitRange:   return 16;
            case ThirtyTwoBitRange: return 32;
            default:                return 0;
            }
        }

        uint32_t MaximumOfBits(_In_ uint32_t bits) noexcept
        {
            return bits >= 32 ? 0xFFFFFFFFu : ((1u << bits) - 1u);
        }

        // The MIDI 2.0 way of moving a value between resolutions: up by the min-center-max rule,
        // so the top of one range is the top of the other, and down by dropping the low bits.
        uint32_t Rescale(_In_ uint32_t value, _In_ uint32_t fromBits, _In_ uint32_t toBits) noexcept
        {
            if (fromBits == 0 || toBits == 0 || fromBits > 32 || toBits > 32)
            {
                return 0;
            }

            value &= MaximumOfBits(fromBits);

            if (fromBits == toBits)
            {
                return value;
            }

            if (fromBits > toBits)
            {
                return value >> (fromBits - toBits);
            }

            auto const scaleBits = toBits - fromBits;
            auto scaled = value << scaleBits;
            auto const center = 1u << (fromBits - 1);

            if (value <= center)
            {
                return scaled;
            }

            // Above the center the low bits repeat the value's own, which keeps the top at the top.
            auto const repeatBits = fromBits - 1;
            auto repeatValue = value & ((1u << repeatBits) - 1u);

            repeatValue = scaleBits > repeatBits
                ? repeatValue << (scaleBits - repeatBits)
                : repeatValue >> (repeatBits - scaleBits);

            while (repeatValue != 0)
            {
                scaled |= repeatValue;
                repeatValue >>= repeatBits;
            }

            return scaled;
        }

        uint32_t ClampWhole(_In_ LogicValue const& value, _In_ uint32_t highest) noexcept
        {
            return (std::min)(WholeOf(value), highest);
        }

        size_t NoteIndex(_In_ uint32_t word) noexcept
        {
            return (static_cast<size_t>(GroupOf(word)) * 16 + ChannelOf(word)) * 128 + Data1Of(word);
        }

        WaySet WaysFromMark(_In_ uint8_t mark, _In_ WaySet const& unknown) noexcept
        {
            if (mark == 0)
            {
                return unknown;
            }

            if (mark == KeptOutMark)
            {
                return {};
            }

            if (mark == EveryWayMark)
            {
                return WaySet::Every();
            }

            return WaySet::One(mark - 1);
        }

        uint8_t MarkFromWays(_In_ WaySet const& ways) noexcept
        {
            if (ways.IsEmpty())
            {
                return KeptOutMark;
            }

            if (ways.IsEvery())
            {
                return EveryWayMark;
            }

            for (int32_t way = 0; way <= HighestWay; way++)
            {
                if (ways.Contains(way))
                {
                    return static_cast<uint8_t>(way + 1);
                }
            }

            return EveryWayMark;
        }

        WaySet Unreadable(_In_ UnreadableWay way, _In_ int32_t first, _In_ int32_t last) noexcept
        {
            switch (way)
            {
            case UnreadableWay::KeepOut:  return {};
            case UnreadableWay::FirstWay: return WaySet::One(first);
            case UnreadableWay::LastWay:  return WaySet::One(last);
            default:                      return WaySet::Every();
            }
        }

        // Which of the long messages this is, and where it is in it. Kind is -1 for every other.
        struct PacketPlace
        {
            int32_t Kind{ -1 };
            bool Starts{ false };
            bool Continues{ false };
            bool Ends{ false };
        };

        PacketPlace PacketPlaceOf(_In_ uint32_t word) noexcept
        {
            PacketPlace place{};

            switch (MessageType(word))
            {
            case TypeSysEx7:
            case TypeData128:
            {
                auto const status = StatusOf(word);

                place.Kind = MessageType(word) == TypeSysEx7 ? 0 : 1;

                // 1 start, 2 continue, 3 end. A mixed data set has a header (8) and then payloads (9).
                place.Starts = status == 0x1 || (place.Kind == 1 && status == 0x8);
                place.Continues = status == 0x2 || status == 0x3 || (place.Kind == 1 && status == 0x9);
                place.Ends = status == 0x3;
                break;
            }

            case TypeFlexData:
            {
                auto const form = (word >> 22) & 0x3;

                place.Kind = 2;
                place.Starts = form == 0x1;
                place.Continues = form == 0x2 || form == 0x3;
                place.Ends = form == 0x3;
                break;
            }

            default:
                break;
            }

            return place;
        }
    }

    _Use_decl_annotations_
    LogicValue PlainValue(uint32_t number) noexcept
    {
        return LogicValue{ number, PlainRange, true };
    }

    _Use_decl_annotations_
    LogicValue ScaledValue(uint32_t raw, uint32_t range) noexcept
    {
        switch (range)
        {
        case SevenBitRange:
        case FourteenBitRange:
        case SixteenBitRange:
        case ThirtyTwoBitRange:
        case HundredthsRange:
            return LogicValue{ (std::min)(raw, range), range, true };

        default:
            return PlainValue(raw);
        }
    }

    _Use_decl_annotations_
    LogicValue ValueFromUnit(uint32_t number, LogicUnit unit) noexcept
    {
        return unit == LogicUnit::Value ? ScaledValue(number, HundredthsRange) : PlainValue(number);
    }

    _Use_decl_annotations_
    int32_t HundredthsOf(LogicValue const& value) noexcept
    {
        switch (value.Range)
        {
        case PlainRange:
            return HundredthsFromSevenBit(static_cast<int32_t>((std::min)(value.Raw, 127u)));

        case SevenBitRange:
            return HundredthsFromSevenBit(static_cast<int32_t>(value.Raw));

        case HundredthsRange:
            return static_cast<int32_t>((std::min)(value.Raw, HundredthsRange));

        default:
            return static_cast<int32_t>(std::llround(
                static_cast<double>(value.Raw) * FullScaleHundredths / static_cast<double>(value.Range)));
        }
    }

    _Use_decl_annotations_
    uint32_t WholeOf(LogicValue const& value) noexcept
    {
        switch (value.Range)
        {
        case PlainRange:
        case SevenBitRange:
            return value.Raw;

        case HundredthsRange:
            return static_cast<uint32_t>(SevenBitFromHundredths(static_cast<int32_t>((std::min)(value.Raw, HundredthsRange))));

        default:
            return Rescale(value.Raw, BitsOfRange(value.Range), 7);
        }
    }

    _Use_decl_annotations_
    uint32_t ValueInUnit(LogicValue const& value, LogicUnit unit) noexcept
    {
        return unit == LogicUnit::Value ? static_cast<uint32_t>(HundredthsOf(value)) : WholeOf(value);
    }

    _Use_decl_annotations_
    uint32_t FieldValueOf(LogicValue const& value, uint32_t bits) noexcept
    {
        if (bits == 0 || bits > 32)
        {
            return 0;
        }

        switch (value.Range)
        {
        case PlainRange:
            return Rescale((std::min)(value.Raw, 127u), 7, bits);

        case HundredthsRange:
            if (bits == 7)
            {
                return static_cast<uint32_t>(SevenBitFromHundredths(static_cast<int32_t>((std::min)(value.Raw, HundredthsRange))));
            }

            return static_cast<uint32_t>(std::llround(
                static_cast<double>((std::min)(value.Raw, HundredthsRange)) / FullScaleHundredths * MaximumOfBits(bits)));

        default:
            return Rescale(value.Raw, BitsOfRange(value.Range), bits);
        }
    }

    _Use_decl_annotations_
    LogicValue ReadPart(PartPlace const& place, uint32_t const* words, uint8_t wordCount) noexcept
    {
        if (words == nullptr || wordCount == 0)
        {
            return {};
        }

        auto const word = words[0];

        if (place.Part == MessagePart::Group)
        {
            return HasGroup(word) ? PlainValue(GroupOf(word)) : LogicValue{};
        }

        if (place.Part == MessagePart::Bits)
        {
            if (place.Word >= wordCount || place.HighBit > 31 || place.LowBit > place.HighBit)
            {
                return {};
            }

            return PlainValue((words[place.Word] >> place.LowBit) & MaximumOfBits(place.HighBit - place.LowBit + 1u));
        }

        if (!IsChannelVoice(word))
        {
            return {};
        }

        auto const isMidi2 = MessageType(word) == TypeMidi2;
        auto const status = StatusOf(word);

        // The second word of a MIDI 2.0 message, which carries its value.
        auto const data = isMidi2 && wordCount >= 2 ? std::optional<uint32_t>{ words[1] } : std::nullopt;

        switch (place.Part)
        {
        case MessagePart::Channel:
            return PlainValue(ChannelOf(word));

        case MessagePart::Note:
            return CarriesNote(word) ? PlainValue(Data1Of(word)) : LogicValue{};

        case MessagePart::Velocity:
            if (status != StatusNoteOn && status != StatusNoteOff)
            {
                return {};
            }

            if (isMidi2)
            {
                return data.has_value() ? ScaledValue(*data >> 16, SixteenBitRange) : LogicValue{};
            }

            return ScaledValue(Data2Of(word), SevenBitRange);

        case MessagePart::ControllerNumber:
            return status == StatusControlChange ? PlainValue(Data1Of(word)) : LogicValue{};

        case MessagePart::ControllerValue:
            if (status != StatusControlChange)
            {
                return {};
            }

            if (isMidi2)
            {
                return data.has_value() ? ScaledValue(*data, ThirtyTwoBitRange) : LogicValue{};
            }

            return ScaledValue(Data2Of(word), SevenBitRange);

        case MessagePart::Program:
            if (status != StatusProgramChange)
            {
                return {};
            }

            if (isMidi2)
            {
                return data.has_value() ? PlainValue((*data >> 24) & 0x7F) : LogicValue{};
            }

            return PlainValue(Data1Of(word));

        case MessagePart::BankMsb:
        case MessagePart::BankLsb:
        {
            auto const msb = place.Part == MessagePart::BankMsb;

            // MIDI 1.0 selects a bank with control changes 0 and 32, MIDI 2.0 inside the program
            // change, where bit 0 says whether there is one.
            if (!isMidi2)
            {
                return status == StatusControlChange && Data1Of(word) == (msb ? ControllerBankMsb : ControllerBankLsb)
                    ? PlainValue(Data2Of(word))
                    : LogicValue{};
            }

            if (status != StatusProgramChange || !data.has_value() || (word & 0x1) == 0)
            {
                return {};
            }

            return PlainValue(msb ? ((*data >> 8) & 0x7F) : (*data & 0x7F));
        }

        case MessagePart::Pressure:
            if (status == StatusChannelPressure)
            {
                if (isMidi2)
                {
                    return data.has_value() ? ScaledValue(*data, ThirtyTwoBitRange) : LogicValue{};
                }

                return ScaledValue(Data1Of(word), SevenBitRange);
            }

            if (status == StatusPolyPressure)
            {
                if (isMidi2)
                {
                    return data.has_value() ? ScaledValue(*data, ThirtyTwoBitRange) : LogicValue{};
                }

                return ScaledValue(Data2Of(word), SevenBitRange);
            }

            return {};

        case MessagePart::PitchBend:
            if (status == StatusPitchBend)
            {
                if (isMidi2)
                {
                    return data.has_value() ? ScaledValue(*data, ThirtyTwoBitRange) : LogicValue{};
                }

                // Low seven bits first.
                return ScaledValue((static_cast<uint32_t>(Data2Of(word)) << 7) | Data1Of(word), FourteenBitRange);
            }

            if (isMidi2 && status == StatusPerNotePitchBend)
            {
                return data.has_value() ? ScaledValue(*data, ThirtyTwoBitRange) : LogicValue{};
            }

            return {};

        default:
            return {};
        }
    }

    _Use_decl_annotations_
    bool WritePart(PartPlace const& place, LogicValue const& value, uint32_t* words, uint8_t wordCount) noexcept
    {
        if (words == nullptr || wordCount == 0 || !value.HasValue)
        {
            return false;
        }

        auto& word = words[0];

        if (place.Part == MessagePart::Group)
        {
            if (!HasGroup(word))
            {
                return false;
            }

            word = (word & ~0x0F000000u) | (ClampWhole(value, 15) << 24);
            return true;
        }

        if (place.Part == MessagePart::Bits)
        {
            if (place.Word >= wordCount || place.HighBit > 31 || place.LowBit > place.HighBit)
            {
                return false;
            }

            auto const bits = place.HighBit - place.LowBit + 1u;
            auto const mask = MaximumOfBits(bits);

            // A value is fitted to the field. A number goes in as it is, as much of it as fits.
            auto const field = value.Range == PlainRange ? (value.Raw & mask) : FieldValueOf(value, bits);

            auto& target = words[place.Word];
            target = (target & ~(mask << place.LowBit)) | (field << place.LowBit);
            return true;
        }

        if (!IsChannelVoice(word))
        {
            return false;
        }

        auto const isMidi2 = MessageType(word) == TypeMidi2;
        auto const status = StatusOf(word);
        auto const hasData = isMidi2 && wordCount >= 2;

        switch (place.Part)
        {
        case MessagePart::Channel:
            word = (word & ~0x000F0000u) | (ClampWhole(value, 15) << 16);
            return true;

        case MessagePart::Note:
            if (!CarriesNote(word))
            {
                return false;
            }

            word = (word & ~0x00007F00u) | (ClampWhole(value, 127) << 8);
            return true;

        case MessagePart::Velocity:
            if (status != StatusNoteOn && status != StatusNoteOff)
            {
                return false;
            }

            if (isMidi2)
            {
                if (!hasData)
                {
                    return false;
                }

                words[1] = (words[1] & 0x0000FFFFu) | (FieldValueOf(value, 16) << 16);
                return true;
            }

            // A MIDI 1.0 note on at velocity zero is a note off: one stays a note off, and nothing
            // else becomes one.
            if (status == StatusNoteOn)
            {
                if (Data2Of(word) == 0)
                {
                    return false;
                }

                word = (word & ~0x7Fu) | (std::max)(FieldValueOf(value, 7), 1u);
                return true;
            }

            word = (word & ~0x7Fu) | FieldValueOf(value, 7);
            return true;

        case MessagePart::ControllerNumber:
            if (status != StatusControlChange)
            {
                return false;
            }

            word = (word & ~0x00007F00u) | (ClampWhole(value, 127) << 8);
            return true;

        case MessagePart::ControllerValue:
            if (status != StatusControlChange)
            {
                return false;
            }

            if (isMidi2)
            {
                if (!hasData)
                {
                    return false;
                }

                words[1] = FieldValueOf(value, 32);
                return true;
            }

            word = (word & ~0x7Fu) | FieldValueOf(value, 7);
            return true;

        case MessagePart::Program:
            if (status != StatusProgramChange)
            {
                return false;
            }

            if (isMidi2)
            {
                if (!hasData)
                {
                    return false;
                }

                words[1] = (words[1] & 0x00FFFFFFu) | (ClampWhole(value, 127) << 24);
                return true;
            }

            word = (word & ~0x00007F00u) | (ClampWhole(value, 127) << 8);
            return true;

        case MessagePart::BankMsb:
        case MessagePart::BankLsb:
        {
            auto const msb = place.Part == MessagePart::BankMsb;

            if (!isMidi2)
            {
                if (status != StatusControlChange || Data1Of(word) != (msb ? ControllerBankMsb : ControllerBankLsb))
                {
                    return false;
                }

                word = (word & ~0x7Fu) | ClampWhole(value, 127);
                return true;
            }

            if (status != StatusProgramChange || !hasData || (word & 0x1) == 0)
            {
                return false;
            }

            words[1] = msb
                ? (words[1] & ~0x00007F00u) | (ClampWhole(value, 127) << 8)
                : (words[1] & ~0x0000007Fu) | ClampWhole(value, 127);
            return true;
        }

        case MessagePart::Pressure:
            if (status != StatusChannelPressure && status != StatusPolyPressure)
            {
                return false;
            }

            if (isMidi2)
            {
                if (!hasData)
                {
                    return false;
                }

                words[1] = FieldValueOf(value, 32);
                return true;
            }

            word = status == StatusChannelPressure
                ? (word & ~0x00007F00u) | (FieldValueOf(value, 7) << 8)
                : (word & ~0x7Fu) | FieldValueOf(value, 7);
            return true;

        case MessagePart::PitchBend:
            if (isMidi2 && (status == StatusPitchBend || status == StatusPerNotePitchBend))
            {
                if (!hasData)
                {
                    return false;
                }

                words[1] = FieldValueOf(value, 32);
                return true;
            }

            if (!isMidi2 && status == StatusPitchBend)
            {
                auto const bend = FieldValueOf(value, 14);

                word = (word & ~0x00007F7Fu) | ((bend & 0x7F) << 8) | ((bend >> 7) & 0x7F);
                return true;
            }

            return false;

        default:
            return false;
        }
    }

    _Use_decl_annotations_
    bool PassesCondition(LogicCondition const& condition, LogicUnit unit, LogicValue const& value) noexcept
    {
        switch (condition.Test)
        {
        case LogicTest::Anything:   return true;
        case LogicTest::HasValue:   return value.HasValue;
        case LogicTest::IsEmpty:    return !value.HasValue;
        default:                    break;
        }

        if (!value.HasValue)
        {
            return false;
        }

        auto const number = ValueInUnit(value, unit);

        switch (condition.Test)
        {
        case LogicTest::Is:         return number == condition.Value;
        case LogicTest::IsNot:      return number != condition.Value;
        case LogicTest::AtLeast:    return number >= condition.Value;
        case LogicTest::Below:      return number < condition.Value;

        case LogicTest::Between:
            return number >= (std::min)(condition.Lowest, condition.Highest) &&
                number <= (std::max)(condition.Lowest, condition.Highest);

        case LogicTest::OneOf:
            return std::find(condition.Values.begin(), condition.Values.end(), number) != condition.Values.end();

        default:
            return false;
        }
    }

    _Use_decl_annotations_
    LogicValue FindTag(TagLink const* tags, uint32_t slot) noexcept
    {
        for (auto const* link = tags; link != nullptr; link = link->Previous)
        {
            if (link->Slot == slot)
            {
                return link->Value;
            }
        }

        return {};
    }

    _Use_decl_annotations_
    uint64_t PackLogicValue(LogicValue const& value) noexcept
    {
        if (!value.HasValue)
        {
            return 0;
        }

        uint64_t code{ 0 };

        switch (value.Range)
        {
        case SevenBitRange:     code = 1; break;
        case FourteenBitRange:  code = 2; break;
        case SixteenBitRange:   code = 3; break;
        case ThirtyTwoBitRange: code = 4; break;
        case HundredthsRange:   code = 5; break;
        default:                code = 0; break;
        }

        return static_cast<uint64_t>(value.Raw) | (code << 32) | (1ull << 35);
    }

    _Use_decl_annotations_
    LogicValue UnpackLogicValue(uint64_t packed) noexcept
    {
        if ((packed & (1ull << 35)) == 0)
        {
            return {};
        }

        constexpr uint32_t ranges[]{ PlainRange, SevenBitRange, FourteenBitRange, SixteenBitRange, ThirtyTwoBitRange, HundredthsRange };

        auto const code = static_cast<size_t>((packed >> 32) & 0x7);

        return LogicValue{ static_cast<uint32_t>(packed & 0xFFFFFFFFu), code < std::size(ranges) ? ranges[code] : PlainRange, true };
    }

    _Use_decl_annotations_
    LogicValue NextMemoryValue(SetMemorySettings const& settings, LogicValue const& current, LogicValue const& source) noexcept
    {
        switch (settings.Action)
        {
        case MemoryAction::Set:
            // A source that has nothing to give leaves the memory as it was.
            return source.HasValue ? source : current;

        case MemoryAction::Toggle:
        {
            auto const isFirst = current.HasValue && ValueInUnit(current, settings.Unit) == settings.First;

            return ValueFromUnit(isFirst ? settings.Second : settings.First, settings.Unit);
        }

        case MemoryAction::StepUp:
        case MemoryAction::StepDown:
        {
            auto const lowest = (std::min)(settings.Lowest, settings.Highest);
            auto const highest = (std::max)(settings.Lowest, settings.Highest);
            auto const up = settings.Action == MemoryAction::StepUp;

            if (!current.HasValue)
            {
                return PlainValue(up ? lowest : highest);
            }

            auto const number = WholeOf(current);

            if (number < lowest || number > highest)
            {
                return PlainValue(up ? lowest : highest);
            }

            if (up)
            {
                return PlainValue(number < highest ? number + 1 : (settings.Wraps ? lowest : highest));
            }

            return PlainValue(number > lowest ? number - 1 : (settings.Wraps ? highest : lowest));
        }

        case MemoryAction::Clear:
            return {};

        default:
            return current;
        }
    }

    WaySet WaySet::Every() noexcept
    {
        return WaySet{ ~0ull, 1ull };
    }

    _Use_decl_annotations_
    WaySet WaySet::One(int32_t way) noexcept
    {
        WaySet ways{};

        if (way >= 0 && way < 64)
        {
            ways.Low = 1ull << way;
        }
        else if (way == HighestWay)
        {
            ways.High = 1ull;
        }

        return ways;
    }

    _Use_decl_annotations_
    bool WaySet::Contains(int32_t way) const noexcept
    {
        if (way >= 0 && way < 64)
        {
            return (Low & (1ull << way)) != 0;
        }

        return way == HighestWay && (High & 1ull) != 0;
    }

    bool WaySet::IsEmpty() const noexcept
    {
        return Low == 0 && (High & 1ull) == 0;
    }

    bool WaySet::IsEvery() const noexcept
    {
        return Low == ~0ull && (High & 1ull) != 0;
    }

    _Use_decl_annotations_
    WaySet& WaySet::operator|=(WaySet const& other) noexcept
    {
        Low |= other.Low;
        High |= other.High & 1ull;

        return *this;
    }

    _Use_decl_annotations_
    WaySet DecideBranch(BranchSettings const& settings, LogicValue const& subject) noexcept
    {
        auto const& condition = settings.Condition;

        if (condition.Test == LogicTest::Anything)
        {
            return WaySet::One(BranchYesWay);
        }

        // Only these two can say anything about a value that isn't there.
        if (!subject.HasValue && condition.Test != LogicTest::HasValue && condition.Test != LogicTest::IsEmpty)
        {
            return Unreadable(settings.Unreadable, BranchYesWay, BranchNoWay);
        }

        return WaySet::One(PassesCondition(condition, settings.Unit, subject) ? BranchYesWay : BranchNoWay);
    }

    _Use_decl_annotations_
    WaySet DecideSwitch(SwitchSettings const& settings, LogicValue const& subject) noexcept
    {
        auto const first = settings.Cases.empty() ? SwitchOtherwiseWay : settings.Cases.front().Id;

        for (auto const& entry : settings.Cases)
        {
            auto const test = entry.Condition.Test;

            // An empty value only matches a case that asks about it.
            if (!subject.HasValue && test != LogicTest::HasValue && test != LogicTest::IsEmpty)
            {
                continue;
            }

            if (PassesCondition(entry.Condition, settings.Unit, subject))
            {
                return WaySet::One(entry.Id);
            }
        }

        if (!subject.HasValue)
        {
            return Unreadable(settings.Unreadable, first, SwitchOtherwiseWay);
        }

        return WaySet::One(SwitchOtherwiseWay);
    }

    _Use_decl_annotations_
    WaySet FollowWays(WayMemory& memory, uint32_t const* words, uint8_t wordCount, WaySet const& decided) noexcept
    {
        if (words == nullptr || wordCount == 0)
        {
            return decided;
        }

        auto const word = words[0];

        // The rest of a long message goes where its start went, so it reaches its destination whole.
        if (auto const packet = PacketPlaceOf(word); packet.Kind >= 0)
        {
            auto const slot = (static_cast<size_t>(GroupOf(word)) * 3 + static_cast<size_t>(packet.Kind)) * 2;
            auto& low = memory.Packets[slot];
            auto& high = memory.Packets[slot + 1];

            if (packet.Starts)
            {
                low.store(decided.Low, std::memory_order_relaxed);
                high.store((decided.High & 1ull) | KnownMark, std::memory_order_relaxed);
                return decided;
            }

            if (packet.Continues)
            {
                auto const known = high.load(std::memory_order_relaxed);

                // One that started before this step was there goes where this packet would.
                if ((known & KnownMark) == 0)
                {
                    return decided;
                }

                WaySet const remembered{ low.load(std::memory_order_relaxed), known & 1ull };

                if (packet.Ends)
                {
                    high.store(0, std::memory_order_relaxed);
                    low.store(0, std::memory_order_relaxed);
                }

                return remembered;
            }

            return decided;
        }

        if (!IsChannelVoice(word))
        {
            return decided;
        }

        auto const status = StatusOf(word);

        if (CarriesNote(word))
        {
            auto& mark = memory.Notes[NoteIndex(word)];

            if (status == StatusNoteOn && !IsNoteOff(word))
            {
                mark.store(MarkFromWays(decided), std::memory_order_relaxed);
                return decided;
            }

            // A note that started before this step was there could be anywhere, and a note off
            // where nothing is playing does no harm.
            if (IsNoteOff(word))
            {
                return WaysFromMark(mark.exchange(0, std::memory_order_relaxed), WaySet::Every());
            }

            // Pressure, and anything else about one note, follows the note while it plays.
            return WaysFromMark(mark.load(std::memory_order_relaxed), decided);
        }

        if (status != StatusControlChange)
        {
            return decided;
        }

        auto const controller = Data1Of(word);
        auto const group = GroupOf(word);
        auto const channel = ChannelOf(word);

        // Everything on the channel stops, wherever it went.
        if (controller == ControllerAllSoundOff || controller == ControllerAllNotesOff)
        {
            auto const first = (static_cast<size_t>(group) * 16 + channel) * 128;

            for (size_t i = 0; i < 128; i++)
            {
                memory.Notes[first + i].store(0, std::memory_order_relaxed);
            }

            return WaySet::Every();
        }

        auto const pedal = std::find(std::begin(PedalControllers), std::end(PedalControllers), controller);

        if (pedal == std::end(PedalControllers))
        {
            return decided;
        }

        auto const isMidi2 = MessageType(word) == TypeMidi2;

        if (isMidi2 && wordCount < 2)
        {
            return decided;
        }

        auto const down = isMidi2 ? words[1] >= 0x80000000u : Data2Of(word) >= 64;

        auto const slot = ((static_cast<size_t>(group) * 16 + channel) * 3 +
            static_cast<size_t>(pedal - std::begin(PedalControllers))) * 2;

        auto& low = memory.Pedals[slot];
        auto& high = memory.Pedals[slot + 1];

        if (down)
        {
            low.fetch_or(decided.Low, std::memory_order_relaxed);
            high.fetch_or(decided.High & 1ull, std::memory_order_relaxed);
            return decided;
        }

        // Let go everywhere it was held down, as well as where it would go now.
        WaySet released{ low.exchange(0, std::memory_order_relaxed), high.exchange(0, std::memory_order_relaxed) & 1ull };
        released |= decided;

        return released;
    }
}
