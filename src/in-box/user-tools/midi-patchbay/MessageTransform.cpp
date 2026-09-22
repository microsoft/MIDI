// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#include "pch.h"
#include "MessageTransform.h"
#include "StringResources.h"

namespace midipatchbay
{
    namespace
    {
        constexpr wchar_t KeyActive[] = L"active";
        constexpr wchar_t KeyScale[] = L"valueScale";
        constexpr wchar_t KeyTranspose[] = L"transposeSemitones";
        constexpr wchar_t KeyNoteMap[] = L"noteMap";
        constexpr wchar_t KeyIgnoreExactPitch[] = L"ignoreExactPitchNotes";
        constexpr wchar_t KeyControlMap[] = L"controlMap";
        constexpr wchar_t KeyChannelMap[] = L"channelMap";
        constexpr wchar_t KeyProgramMap[] = L"programMap";
        constexpr wchar_t KeyBankMsbMap[] = L"bankMsbMap";
        constexpr wchar_t KeyBankLsbMap[] = L"bankLsbMap";
        constexpr wchar_t KeyCurve[] = L"velocityCurve";
        constexpr wchar_t KeyFixedVelocityPercent[] = L"fixedVelocityPercent";
        constexpr wchar_t KeyRescale[] = L"rescaleVelocity";
        constexpr wchar_t KeyMinimumVelocityPercent[] = L"minimumVelocityPercent";
        constexpr wchar_t KeyMaximumVelocityPercent[] = L"maximumVelocityPercent";
        constexpr wchar_t KeyFrom[] = L"from";
        constexpr wchar_t KeyTo[] = L"to";

        // Written by the preview that only ever had 0 to 127 velocities, and still written
        // beside the percentages so rolling a preview back does not lose the range.
        constexpr wchar_t KeyLegacyMinimumVelocity[] = L"minimumVelocity";
        constexpr wchar_t KeyLegacyMaximumVelocity[] = L"maximumVelocity";

        constexpr wchar_t ScaleNameSevenBit[] = L"sevenBit";
        constexpr wchar_t ScaleNamePercent[] = L"percent";

        constexpr uint8_t StatusNoteOff = 0x8;
        constexpr uint8_t StatusNoteOn = 0x9;
        constexpr uint8_t StatusControlChange = 0xB;
        constexpr uint8_t StatusProgramChange = 0xC;

        constexpr uint8_t BankSelectMsbController = 0;
        constexpr uint8_t BankSelectLsbController = 32;

        // The MIDI 2.0 note attribute that carries an exact pitch as 7.9 fixed point.
        constexpr uint8_t PitchAttributeType = 0x03;
        constexpr int32_t PitchUnitsPerSemitone = 512;

        constexpr uint32_t ChannelFieldMask = 0x000F0000u;
        constexpr uint32_t NoteFieldMask = 0x7Fu << 8;

        bool HasAnyEntry(_In_ int16_t const* map, _In_ size_t count) noexcept
        {
            for (size_t i = 0; i < count; i++)
            {
                if (map[i] >= 0 && map[i] != static_cast<int16_t>(i))
                {
                    return true;
                }
            }

            return false;
        }

        size_t CountEntries(_In_ int16_t const* map, _In_ size_t count) noexcept
        {
            size_t total{ 0 };

            for (size_t i = 0; i < count; i++)
            {
                if (map[i] >= 0 && map[i] != static_cast<int16_t>(i))
                {
                    total++;
                }
            }

            return total;
        }

        json::JsonArray MapToJson(_In_ int16_t const* map, _In_ size_t count) noexcept
        {
            json::JsonArray array{};

            try
            {
                for (size_t i = 0; i < count; i++)
                {
                    if (map[i] < 0 || map[i] == static_cast<int16_t>(i))
                    {
                        continue;
                    }

                    json::JsonObject entry{};

                    entry.SetNamedValue(KeyFrom, json::JsonValue::CreateNumberValue(static_cast<double>(i)));
                    entry.SetNamedValue(KeyTo, json::JsonValue::CreateNumberValue(map[i]));

                    array.Append(entry);
                }
            }
            catch (...)
            {
            }

            return array;
        }

        void MapFromJson(
            _In_ json::JsonObject const& object,
            _In_ std::wstring_view key,
            _Out_writes_(count) int16_t* map,
            _In_ size_t count) noexcept
        {
            for (size_t i = 0; i < count; i++)
            {
                map[i] = -1;
            }

            try
            {
                if (!object.HasKey(key))
                {
                    return;
                }

                auto const value = object.GetNamedValue(key);

                if (value == nullptr || value.ValueType() != json::JsonValueType::Array)
                {
                    return;
                }

                size_t added{ 0 };

                for (auto const& item : value.GetArray())
                {
                    if (added >= MaximumMapEntries)
                    {
                        break;
                    }

                    if (item == nullptr || item.ValueType() != json::JsonValueType::Object)
                    {
                        continue;
                    }

                    auto const entry = item.GetObject();

                    if (!entry.HasKey(KeyFrom) || !entry.HasKey(KeyTo))
                    {
                        continue;
                    }

                    auto const fromValue = entry.GetNamedValue(KeyFrom);
                    auto const toValue = entry.GetNamedValue(KeyTo);

                    if (fromValue == nullptr || fromValue.ValueType() != json::JsonValueType::Number ||
                        toValue == nullptr || toValue.ValueType() != json::JsonValueType::Number)
                    {
                        continue;
                    }

                    auto const from = fromValue.GetNumber();
                    auto const to = toValue.GetNumber();
                    auto const limit = static_cast<double>(count) - 1;

                    if (!std::isfinite(from) || !std::isfinite(to) ||
                        from < 0 || from > limit || to < 0 || to > limit)
                    {
                        continue;
                    }

                    map[static_cast<size_t>(from)] = static_cast<int16_t>(to);
                    added++;
                }
            }
            catch (...)
            {
            }
        }
    }

    _Use_decl_annotations_
    int32_t HundredthsFromSevenBit(int32_t value) noexcept
    {
        auto const clamped = std::clamp(value, 0, 127);

        return static_cast<int32_t>((clamped * FullScaleHundredths + 63) / 127);
    }

    _Use_decl_annotations_
    int32_t SevenBitFromHundredths(int32_t hundredths) noexcept
    {
        auto const clamped = std::clamp(hundredths, 0, FullScaleHundredths);

        return static_cast<int32_t>((clamped * 127 + FullScaleHundredths / 2) / FullScaleHundredths);
    }

    _Use_decl_annotations_
    double DisplayFromHundredths(int32_t hundredths, ValueScale scale) noexcept
    {
        return scale == ValueScale::SevenBit
            ? static_cast<double>(SevenBitFromHundredths(hundredths))
            : std::clamp(hundredths, 0, FullScaleHundredths) / 100.0;
    }

    _Use_decl_annotations_
    int32_t HundredthsFromDisplay(double value, ValueScale scale) noexcept
    {
        if (!std::isfinite(value))
        {
            return 0;
        }

        if (scale == ValueScale::SevenBit)
        {
            return HundredthsFromSevenBit(static_cast<int32_t>(std::lround(value)));
        }

        return std::clamp(static_cast<int32_t>(std::lround(value * 100.0)), 0, FullScaleHundredths);
    }

    _Use_decl_annotations_
    winrt::hstring DescribeScaledValue(int32_t hundredths, ValueScale scale) noexcept
    {
        try
        {
            if (scale == ValueScale::SevenBit)
            {
                return winrt::hstring{ std::to_wstring(SevenBitFromHundredths(hundredths)) };
            }

            auto const clamped = std::clamp(hundredths, 0, FullScaleHundredths);

            return resources::FormatString(L"TransformPercentFormat",
                clamped / 100, clamped % 100);
        }
        catch (...)
        {
        }

        return {};
    }

    MessageTransform::MessageTransform() noexcept
    {
        Reset();
    }

    void MessageTransform::Reset() noexcept
    {
        IsActive = false;
        Scale = ValueScale::Percent;
        TransposeSemitones = 0;

        ChannelMap.fill(-1);
        NoteMap.fill(-1);
        ControlMap.fill(-1);
        ProgramMap.fill(-1);
        BankMsbMap.fill(-1);
        BankLsbMap.fill(-1);

        IgnoreExactPitchNotes = false;

        Curve = VelocityCurve::Unchanged;
        FixedVelocityHundredths = HundredthsFromSevenBit(100);
        RescaleVelocity = false;
        MinimumVelocityHundredths = 0;
        MaximumVelocityHundredths = FullScaleHundredths;
    }

    bool MessageTransform::ChangesNothing() const noexcept
    {
        if (!IsActive)
        {
            return true;
        }

        return TransposeSemitones == 0 &&
            Curve == VelocityCurve::Unchanged &&
            !RescaleVelocity &&
            !HasAnyEntry(ChannelMap.data(), ChannelMap.size()) &&
            !HasAnyEntry(NoteMap.data(), NoteMap.size()) &&
            !HasAnyEntry(ControlMap.data(), ControlMap.size()) &&
            !HasAnyEntry(ProgramMap.data(), ProgramMap.size()) &&
            !HasAnyEntry(BankMsbMap.data(), BankMsbMap.size()) &&
            !HasAnyEntry(BankLsbMap.data(), BankLsbMap.size());
    }

    _Use_decl_annotations_
    uint8_t MessageTransform::ResultingNote(uint8_t note) const noexcept
    {
        if (note > 127)
        {
            return note;
        }

        auto const mapped = NoteMap[note];

        if (mapped >= 0)
        {
            return static_cast<uint8_t>(std::clamp<int32_t>(mapped, 0, 127));
        }

        return static_cast<uint8_t>(std::clamp(static_cast<int32_t>(note) + TransposeSemitones, 0, 127));
    }

    _Use_decl_annotations_
    double MessageTransform::ShapeUnit(double value) const noexcept
    {
        if (Curve == VelocityCurve::Fixed)
        {
            // A fixed velocity ignores what was played, and so ignores the range as well.
            return std::clamp(FixedVelocityHundredths / static_cast<double>(FullScaleHundredths), 0.0, 1.0);
        }

        auto shaped = std::clamp(value, 0.0, 1.0);

        switch (Curve)
        {
        case VelocityCurve::LinearToCurved:
            shaped = shaped * shaped;
            break;

        case VelocityCurve::CurvedToLinear:
            shaped = std::sqrt(shaped);
            break;

        default:
            break;
        }

        if (RescaleVelocity)
        {
            auto low = MinimumVelocityHundredths / static_cast<double>(FullScaleHundredths);
            auto high = MaximumVelocityHundredths / static_cast<double>(FullScaleHundredths);

            if (high < low)
            {
                std::swap(low, high);
            }

            shaped = low + shaped * (high - low);
        }

        return std::clamp(shaped, 0.0, 1.0);
    }

    _Use_decl_annotations_
    uint8_t MessageTransform::ShapeVelocity7(uint8_t velocity) const noexcept
    {
        // A MIDI 1.0 note on with velocity zero IS a note off. Shaping it to anything else would
        // leave the note sounding forever.
        if (velocity == 0)
        {
            return 0;
        }

        // Full scale is 127 here and in the dialog, so a velocity typed as a MIDI 1.0 step comes
        // out as exactly that step. The floor of 1 is what keeps a quiet note from becoming a
        // note off.
        auto const shaped = ShapeUnit(velocity / 127.0);

        return static_cast<uint8_t>(std::clamp(std::lround(shaped * 127.0), 1L, 127L));
    }

    _Use_decl_annotations_
    uint16_t MessageTransform::ShapeVelocity16(uint16_t velocity) const noexcept
    {
        auto const shaped = ShapeUnit(velocity / 65535.0);

        return static_cast<uint16_t>(std::clamp(std::lround(shaped * 65535.0), 0L, 65535L));
    }

    _Use_decl_annotations_
    void MessageTransform::Apply(uint32_t* words, uint8_t wordCount) const noexcept
    {
        if (!IsActive || words == nullptr || wordCount == 0)
        {
            return;
        }

        auto const messageType = static_cast<uint8_t>((words[0] >> 28) & 0x0F);

        auto const isMidi1 = messageType == static_cast<uint8_t>(UmpMessageType::Midi1ChannelVoice);
        auto const isMidi2 = messageType == static_cast<uint8_t>(UmpMessageType::Midi2ChannelVoice);

        if (!isMidi1 && !isMidi2)
        {
            return;
        }

        // The channel is the outermost address, so it moves before anything addressed within it.
        {
            auto const channel = static_cast<uint8_t>((words[0] >> 16) & 0x0F);
            auto const mappedChannel = ChannelMap[channel];

            if (mappedChannel >= 0 && mappedChannel != channel)
            {
                words[0] = (words[0] & ~ChannelFieldMask) |
                    (static_cast<uint32_t>(mappedChannel & 0x0F) << 16);
            }
        }

        auto const status = static_cast<uint8_t>((words[0] >> 20) & 0x0F);

        if (status == StatusControlChange)
        {
            auto index = static_cast<uint8_t>((words[0] >> 8) & 0x7F);
            auto const mapped = ControlMap[index];

            if (mapped >= 0 && mapped != index)
            {
                index = static_cast<uint8_t>(mapped & 0x7F);
                words[0] = (words[0] & ~NoteFieldMask) | (static_cast<uint32_t>(index) << 8);
            }

            // MIDI 1.0 carries the bank in two controllers. A MIDI 2.0 program change carries it
            // in the message itself, and its controller values are 32 bit, so neither table
            // belongs here for one.
            if (isMidi1 && (index == BankSelectMsbController || index == BankSelectLsbController))
            {
                auto const& bankMap = index == BankSelectMsbController ? BankMsbMap : BankLsbMap;
                auto const value = static_cast<uint8_t>(words[0] & 0x7F);
                auto const mappedBank = bankMap[value];

                if (mappedBank >= 0 && mappedBank != value)
                {
                    words[0] = (words[0] & ~0x7Fu) | static_cast<uint32_t>(mappedBank & 0x7F);
                }
            }

            return;
        }

        if (status == StatusProgramChange)
        {
            if (isMidi1)
            {
                auto const program = static_cast<uint8_t>((words[0] >> 8) & 0x7F);
                auto const mapped = ProgramMap[program];

                if (mapped >= 0 && mapped != program)
                {
                    words[0] = (words[0] & ~NoteFieldMask) | (static_cast<uint32_t>(mapped & 0x7F) << 8);
                }

                return;
            }

            if (wordCount < 2)
            {
                return;
            }

            auto const program = static_cast<uint8_t>((words[1] >> 24) & 0x7F);
            auto const mapped = ProgramMap[program];

            if (mapped >= 0 && mapped != program)
            {
                words[1] = (words[1] & 0x00FFFFFFu) | (static_cast<uint32_t>(mapped & 0x7F) << 24);
            }

            // Bit 0 of the option flags. With no bank in the message there is nothing to match
            // against, so the tables stay out of it rather than inventing one.
            if ((words[0] & 0x01u) != 0)
            {
                auto const msb = static_cast<uint8_t>((words[1] >> 8) & 0x7F);
                auto const lsb = static_cast<uint8_t>(words[1] & 0x7F);

                auto const mappedMsb = BankMsbMap[msb];
                auto const mappedLsb = BankLsbMap[lsb];

                if (mappedMsb >= 0 && mappedMsb != msb)
                {
                    words[1] = (words[1] & ~0xFF00u) | (static_cast<uint32_t>(mappedMsb & 0x7F) << 8);
                }

                if (mappedLsb >= 0 && mappedLsb != lsb)
                {
                    words[1] = (words[1] & ~0xFFu) | static_cast<uint32_t>(mappedLsb & 0x7F);
                }
            }

            return;
        }

        if (!StatusCarriesNote(status, isMidi2))
        {
            return;
        }

        auto const isNote = status == StatusNoteOn || status == StatusNoteOff;

        // A MIDI 2.0 note can declare the exact pitch it wants, which makes its note number an
        // address rather than a pitch.
        auto const carriesExactPitch = isMidi2 && isNote && wordCount >= 2 &&
            static_cast<uint8_t>(words[0] & 0xFF) == PitchAttributeType;

        if (!carriesExactPitch || !IgnoreExactPitchNotes)
        {
            auto const note = static_cast<uint8_t>((words[0] >> 8) & 0x7F);
            auto const outgoing = ResultingNote(note);

            if (outgoing != note)
            {
                words[0] = (words[0] & ~NoteFieldMask) | (static_cast<uint32_t>(outgoing) << 8);

                if (carriesExactPitch)
                {
                    // Move the declared pitch with the note, or the message would ask for one
                    // note and the pitch of another.
                    auto const shifted = static_cast<int32_t>(words[1] & 0xFFFF) +
                        (static_cast<int32_t>(outgoing) - static_cast<int32_t>(note)) * PitchUnitsPerSemitone;

                    words[1] = (words[1] & 0xFFFF0000u) |
                        static_cast<uint32_t>(std::clamp(shifted, 0, 0xFFFF));
                }
            }
        }

        if (status != StatusNoteOn || (Curve == VelocityCurve::Unchanged && !RescaleVelocity))
        {
            return;
        }

        if (isMidi1)
        {
            words[0] = (words[0] & ~0x7Fu) | ShapeVelocity7(static_cast<uint8_t>(words[0] & 0x7F));
        }
        else if (wordCount >= 2)
        {
            auto const velocity = static_cast<uint16_t>((words[1] >> 16) & 0xFFFF);

            words[1] = (words[1] & 0x0000FFFFu) |
                (static_cast<uint32_t>(ShapeVelocity16(velocity)) << 16);
        }
    }

    _Use_decl_annotations_
    winrt::hstring SummarizeTransform(MessageTransform const& transform) noexcept
    {
        try
        {
            if (transform.ChangesNothing())
            {
                return resources::GetString(L"TransformSummaryNothing");
            }

            std::vector<std::wstring> parts{};

            auto const channels = CountEntries(transform.ChannelMap.data(), transform.ChannelMap.size());

            if (channels > 0)
            {
                parts.push_back(std::wstring{ channels == 1
                    ? resources::GetString(L"TransformSummaryOneChannelMap")
                    : resources::FormatString(L"TransformSummaryChannelMapFormat", static_cast<int>(channels)) });
            }

            if (transform.TransposeSemitones != 0)
            {
                parts.push_back(std::wstring{ resources::FormatString(L"TransformSummaryTransposeFormat",
                    transform.TransposeSemitones > 0
                        ? std::wstring{ L"+" } + std::to_wstring(transform.TransposeSemitones)
                        : std::to_wstring(transform.TransposeSemitones)) });
            }

            auto const notes = CountEntries(transform.NoteMap.data(), transform.NoteMap.size());

            if (notes > 0)
            {
                parts.push_back(std::wstring{ notes == 1
                    ? resources::GetString(L"TransformSummaryOneNoteMap")
                    : resources::FormatString(L"TransformSummaryNoteMapFormat", static_cast<int>(notes)) });
            }

            if (transform.Curve == VelocityCurve::LinearToCurved)
            {
                parts.push_back(std::wstring{ resources::GetString(L"TransformSummaryCurved") });
            }
            else if (transform.Curve == VelocityCurve::CurvedToLinear)
            {
                parts.push_back(std::wstring{ resources::GetString(L"TransformSummaryLinear") });
            }
            else if (transform.Curve == VelocityCurve::Fixed)
            {
                parts.push_back(std::wstring{ resources::FormatString(L"TransformSummaryFixedVelocityFormat",
                    DescribeScaledValue(transform.FixedVelocityHundredths, transform.Scale)) });
            }

            if (transform.RescaleVelocity && transform.Curve != VelocityCurve::Fixed)
            {
                parts.push_back(std::wstring{ resources::FormatString(L"TransformSummaryVelocityRangeFormat",
                    DescribeScaledValue(transform.MinimumVelocityHundredths, transform.Scale),
                    DescribeScaledValue(transform.MaximumVelocityHundredths, transform.Scale)) });
            }

            auto const controls = CountEntries(transform.ControlMap.data(), transform.ControlMap.size());

            if (controls > 0)
            {
                parts.push_back(std::wstring{ controls == 1
                    ? resources::GetString(L"TransformSummaryOneControlMap")
                    : resources::FormatString(L"TransformSummaryControlMapFormat", static_cast<int>(controls)) });
            }

            auto const programs = CountEntries(transform.ProgramMap.data(), transform.ProgramMap.size());

            if (programs > 0)
            {
                parts.push_back(std::wstring{ programs == 1
                    ? resources::GetString(L"TransformSummaryOneProgramMap")
                    : resources::FormatString(L"TransformSummaryProgramMapFormat", static_cast<int>(programs)) });
            }

            auto const banks =
                CountEntries(transform.BankMsbMap.data(), transform.BankMsbMap.size()) +
                CountEntries(transform.BankLsbMap.data(), transform.BankLsbMap.size());

            if (banks > 0)
            {
                parts.push_back(std::wstring{ banks == 1
                    ? resources::GetString(L"TransformSummaryOneBankMap")
                    : resources::FormatString(L"TransformSummaryBankMapFormat", static_cast<int>(banks)) });
            }

            std::wstring text{};

            for (auto const& part : parts)
            {
                if (!text.empty())
                {
                    text += L". ";
                }

                text += part;
            }

            return text.empty() ? resources::GetString(L"TransformSummaryNothing") : winrt::hstring{ text };
        }
        catch (...)
        {
        }

        return resources::GetString(L"TransformSummaryNothing");
    }

    _Use_decl_annotations_
    json::JsonObject TransformToJson(MessageTransform const& transform) noexcept
    {
        json::JsonObject object{};

        try
        {
            object.SetNamedValue(KeyActive, json::JsonValue::CreateBooleanValue(transform.IsActive));

            object.SetNamedValue(KeyScale, json::JsonValue::CreateStringValue(
                transform.Scale == ValueScale::SevenBit ? ScaleNameSevenBit : ScaleNamePercent));

            object.SetNamedValue(KeyTranspose, json::JsonValue::CreateNumberValue(transform.TransposeSemitones));
            object.SetNamedValue(KeyIgnoreExactPitch, json::JsonValue::CreateBooleanValue(transform.IgnoreExactPitchNotes));
            object.SetNamedValue(KeyCurve, json::JsonValue::CreateNumberValue(static_cast<int32_t>(transform.Curve)));
            object.SetNamedValue(KeyRescale, json::JsonValue::CreateBooleanValue(transform.RescaleVelocity));

            object.SetNamedValue(KeyFixedVelocityPercent,
                json::JsonValue::CreateNumberValue(transform.FixedVelocityHundredths / 100.0));
            object.SetNamedValue(KeyMinimumVelocityPercent,
                json::JsonValue::CreateNumberValue(transform.MinimumVelocityHundredths / 100.0));
            object.SetNamedValue(KeyMaximumVelocityPercent,
                json::JsonValue::CreateNumberValue(transform.MaximumVelocityHundredths / 100.0));

            // Beside the percentages so a rolled back preview still reads a sensible range.
            object.SetNamedValue(KeyLegacyMinimumVelocity, json::JsonValue::CreateNumberValue(
                std::max(1, SevenBitFromHundredths(transform.MinimumVelocityHundredths))));
            object.SetNamedValue(KeyLegacyMaximumVelocity, json::JsonValue::CreateNumberValue(
                std::max(1, SevenBitFromHundredths(transform.MaximumVelocityHundredths))));

            object.SetNamedValue(KeyChannelMap, MapToJson(transform.ChannelMap.data(), transform.ChannelMap.size()));
            object.SetNamedValue(KeyNoteMap, MapToJson(transform.NoteMap.data(), transform.NoteMap.size()));
            object.SetNamedValue(KeyControlMap, MapToJson(transform.ControlMap.data(), transform.ControlMap.size()));
            object.SetNamedValue(KeyProgramMap, MapToJson(transform.ProgramMap.data(), transform.ProgramMap.size()));
            object.SetNamedValue(KeyBankMsbMap, MapToJson(transform.BankMsbMap.data(), transform.BankMsbMap.size()));
            object.SetNamedValue(KeyBankLsbMap, MapToJson(transform.BankLsbMap.data(), transform.BankLsbMap.size()));
        }
        catch (...)
        {
        }

        return object;
    }

    _Use_decl_annotations_
    MessageTransform TransformFromJson(json::JsonObject const& object) noexcept
    {
        MessageTransform transform{};

        if (object == nullptr)
        {
            return transform;
        }

        try
        {
            auto const readNumber = [&object](std::wstring_view key, double low, double high, double fallback)
                {
                    if (!object.HasKey(key))
                    {
                        return fallback;
                    }

                    auto const value = object.GetNamedValue(key);

                    if (value == nullptr || value.ValueType() != json::JsonValueType::Number)
                    {
                        return fallback;
                    }

                    auto const number = value.GetNumber();

                    return std::isfinite(number) && number >= low && number <= high ? number : fallback;
                };

            auto const readBool = [&object](std::wstring_view key, bool fallback)
                {
                    if (!object.HasKey(key))
                    {
                        return fallback;
                    }

                    auto const value = object.GetNamedValue(key);

                    return value != nullptr && value.ValueType() == json::JsonValueType::Boolean
                        ? value.GetBoolean() : fallback;
                };

            transform.IsActive = readBool(KeyActive, false);

            // A patch written before the scale existed only ever meant 0 to 127, so that is what
            // its velocities are read back as.
            auto const hasScale = object.HasKey(KeyScale) &&
                object.GetNamedValue(KeyScale) != nullptr &&
                object.GetNamedValue(KeyScale).ValueType() == json::JsonValueType::String;

            transform.Scale = hasScale && object.GetNamedString(KeyScale) == ScaleNamePercent
                ? ValueScale::Percent
                : ValueScale::SevenBit;

            transform.TransposeSemitones = static_cast<int32_t>(
                readNumber(KeyTranspose, MinimumTranspose, MaximumTranspose, 0));

            transform.IgnoreExactPitchNotes = readBool(KeyIgnoreExactPitch, false);

            auto const curve = static_cast<int32_t>(readNumber(KeyCurve, 0, 3, 0));
            transform.Curve = static_cast<VelocityCurve>(curve);

            transform.RescaleVelocity = readBool(KeyRescale, false);

            if (hasScale)
            {
                transform.FixedVelocityHundredths = static_cast<int32_t>(std::lround(
                    readNumber(KeyFixedVelocityPercent, 0, 100, 100.0 * 100 / 127) * 100.0));
                transform.MinimumVelocityHundredths = static_cast<int32_t>(std::lround(
                    readNumber(KeyMinimumVelocityPercent, 0, 100, 0) * 100.0));
                transform.MaximumVelocityHundredths = static_cast<int32_t>(std::lround(
                    readNumber(KeyMaximumVelocityPercent, 0, 100, 100) * 100.0));
            }
            else
            {
                transform.FixedVelocityHundredths = HundredthsFromSevenBit(100);
                transform.MinimumVelocityHundredths = HundredthsFromSevenBit(
                    static_cast<int32_t>(readNumber(KeyLegacyMinimumVelocity, 0, 127, 1)));
                transform.MaximumVelocityHundredths = HundredthsFromSevenBit(
                    static_cast<int32_t>(readNumber(KeyLegacyMaximumVelocity, 0, 127, 127)));
            }

            if (transform.MinimumVelocityHundredths > transform.MaximumVelocityHundredths)
            {
                std::swap(transform.MinimumVelocityHundredths, transform.MaximumVelocityHundredths);
            }

            MapFromJson(object, KeyChannelMap, transform.ChannelMap.data(), transform.ChannelMap.size());
            MapFromJson(object, KeyNoteMap, transform.NoteMap.data(), transform.NoteMap.size());
            MapFromJson(object, KeyControlMap, transform.ControlMap.data(), transform.ControlMap.size());
            MapFromJson(object, KeyProgramMap, transform.ProgramMap.data(), transform.ProgramMap.size());
            MapFromJson(object, KeyBankMsbMap, transform.BankMsbMap.data(), transform.BankMsbMap.size());
            MapFromJson(object, KeyBankLsbMap, transform.BankLsbMap.data(), transform.BankLsbMap.size());
        }
        catch (...)
        {
        }

        return transform;
    }

    _Use_decl_annotations_
    std::wstring TransformSignature(MessageTransform const& transform) noexcept
    {
        if (transform.ChangesNothing())
        {
            return L"*";
        }

        std::wstring signature =
            std::to_wstring(transform.TransposeSemitones) + L'.' +
            std::to_wstring(static_cast<int32_t>(transform.Curve)) + L'.' +
            std::to_wstring(transform.FixedVelocityHundredths) + L'.' +
            (transform.IgnoreExactPitchNotes ? L'p' : L'-') +
            (transform.RescaleVelocity
                ? std::to_wstring(transform.MinimumVelocityHundredths) + L'-' +
                  std::to_wstring(transform.MaximumVelocityHundredths)
                : std::wstring{ L"n" });

        // The prefixes are what keep one table's entries from colliding with another's. They must
        // be strings: L'.c' is a multi character constant, not two characters.
        auto const appendMap = [&signature](int16_t const* map, size_t count, wchar_t const* prefix)
            {
                for (size_t i = 0; i < count; i++)
                {
                    if (map[i] >= 0)
                    {
                        signature += prefix + std::to_wstring(i) + L'>' + std::to_wstring(map[i]);
                    }
                }
            };

        appendMap(transform.NoteMap.data(), transform.NoteMap.size(), L".n");
        appendMap(transform.ControlMap.data(), transform.ControlMap.size(), L".c");
        appendMap(transform.ChannelMap.data(), transform.ChannelMap.size(), L".h");
        appendMap(transform.ProgramMap.data(), transform.ProgramMap.size(), L".g");
        appendMap(transform.BankMsbMap.data(), transform.BankMsbMap.size(), L".m");
        appendMap(transform.BankLsbMap.data(), transform.BankLsbMap.size(), L".l");

        return signature;
    }
}
