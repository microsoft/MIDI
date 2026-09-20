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
        constexpr wchar_t KeyTranspose[] = L"transposeSemitones";
        constexpr wchar_t KeyNoteMap[] = L"noteMap";
        constexpr wchar_t KeyControlMap[] = L"controlMap";
        constexpr wchar_t KeyCurve[] = L"velocityCurve";
        constexpr wchar_t KeyRescale[] = L"rescaleVelocity";
        constexpr wchar_t KeyMinimumVelocity[] = L"minimumVelocity";
        constexpr wchar_t KeyMaximumVelocity[] = L"maximumVelocity";
        constexpr wchar_t KeyFrom[] = L"from";
        constexpr wchar_t KeyTo[] = L"to";

        constexpr uint8_t StatusNoteOn = 0x9;
        constexpr uint8_t StatusControlChange = 0xB;

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

                    if (!std::isfinite(from) || !std::isfinite(to) ||
                        from < 0 || from >= static_cast<double>(count) || to < 0 || to > 127)
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

    MessageTransform::MessageTransform() noexcept
    {
        Reset();
    }

    void MessageTransform::Reset() noexcept
    {
        IsActive = false;
        TransposeSemitones = 0;

        NoteMap.fill(-1);
        ControlMap.fill(-1);

        Curve = VelocityCurve::Unchanged;
        RescaleVelocity = false;
        MinimumVelocity = 1;
        MaximumVelocity = 127;
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
            !HasAnyEntry(NoteMap.data(), NoteMap.size()) &&
            !HasAnyEntry(ControlMap.data(), ControlMap.size());
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
            auto low = MinimumVelocity / 127.0;
            auto high = MaximumVelocity / 127.0;

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

        // 1 to 127 rather than 0 to 127, so the quietest playable velocity stays playable.
        auto const shaped = ShapeUnit((velocity - 1) / 126.0);

        return static_cast<uint8_t>(std::clamp(std::lround(1.0 + shaped * 126.0), 1L, 127L));
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

        auto const status = static_cast<uint8_t>((words[0] >> 20) & 0x0F);

        if (status == StatusControlChange)
        {
            auto const index = static_cast<uint8_t>((words[0] >> 8) & 0x7F);
            auto const mapped = ControlMap[index];

            if (mapped >= 0 && mapped != index)
            {
                words[0] = (words[0] & ~NoteFieldMask) | (static_cast<uint32_t>(mapped & 0x7F) << 8);
            }

            return;
        }

        if (!StatusCarriesNote(status, isMidi2))
        {
            return;
        }

        auto const note = static_cast<uint8_t>((words[0] >> 8) & 0x7F);
        auto const outgoing = ResultingNote(note);

        if (outgoing != note)
        {
            words[0] = (words[0] & ~NoteFieldMask) | (static_cast<uint32_t>(outgoing) << 8);
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

            if (transform.RescaleVelocity)
            {
                parts.push_back(std::wstring{ resources::FormatString(L"TransformSummaryVelocityRangeFormat",
                    static_cast<int>(transform.MinimumVelocity), static_cast<int>(transform.MaximumVelocity)) });
            }

            auto const controls = CountEntries(transform.ControlMap.data(), transform.ControlMap.size());

            if (controls > 0)
            {
                parts.push_back(std::wstring{ controls == 1
                    ? resources::GetString(L"TransformSummaryOneControlMap")
                    : resources::FormatString(L"TransformSummaryControlMapFormat", static_cast<int>(controls)) });
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
            object.SetNamedValue(KeyTranspose, json::JsonValue::CreateNumberValue(transform.TransposeSemitones));
            object.SetNamedValue(KeyCurve, json::JsonValue::CreateNumberValue(static_cast<int32_t>(transform.Curve)));
            object.SetNamedValue(KeyRescale, json::JsonValue::CreateBooleanValue(transform.RescaleVelocity));
            object.SetNamedValue(KeyMinimumVelocity, json::JsonValue::CreateNumberValue(transform.MinimumVelocity));
            object.SetNamedValue(KeyMaximumVelocity, json::JsonValue::CreateNumberValue(transform.MaximumVelocity));

            object.SetNamedValue(KeyNoteMap, MapToJson(transform.NoteMap.data(), transform.NoteMap.size()));
            object.SetNamedValue(KeyControlMap, MapToJson(transform.ControlMap.data(), transform.ControlMap.size()));
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

            transform.TransposeSemitones = static_cast<int32_t>(
                readNumber(KeyTranspose, MinimumTranspose, MaximumTranspose, 0));

            auto const curve = static_cast<int32_t>(readNumber(KeyCurve, 0, 2, 0));
            transform.Curve = static_cast<VelocityCurve>(curve);

            transform.RescaleVelocity = readBool(KeyRescale, false);
            transform.MinimumVelocity = static_cast<uint8_t>(readNumber(KeyMinimumVelocity, 1, 127, 1));
            transform.MaximumVelocity = static_cast<uint8_t>(readNumber(KeyMaximumVelocity, 1, 127, 127));

            if (transform.MinimumVelocity > transform.MaximumVelocity)
            {
                std::swap(transform.MinimumVelocity, transform.MaximumVelocity);
            }

            MapFromJson(object, KeyNoteMap, transform.NoteMap.data(), transform.NoteMap.size());
            MapFromJson(object, KeyControlMap, transform.ControlMap.data(), transform.ControlMap.size());
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
            (transform.RescaleVelocity
                ? std::to_wstring(transform.MinimumVelocity) + L'-' + std::to_wstring(transform.MaximumVelocity)
                : std::wstring{ L"n" });

        for (size_t i = 0; i < transform.NoteMap.size(); i++)
        {
            if (transform.NoteMap[i] >= 0)
            {
                signature += L'.' + std::to_wstring(i) + L'>' + std::to_wstring(transform.NoteMap[i]);
            }
        }

        for (size_t i = 0; i < transform.ControlMap.size(); i++)
        {
            if (transform.ControlMap[i] >= 0)
            {
                signature += L".c" + std::to_wstring(i) + L'>' + std::to_wstring(transform.ControlMap[i]);
            }
        }

        return signature;
    }
}
