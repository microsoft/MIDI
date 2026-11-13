// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#include "Harmony.h"

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

// windows.h renames GetObject, which Windows.Data.Json uses.
#undef GetObject

#include <algorithm>
#include <array>
#include <cmath>
#include <format>
#include <utility>

namespace midisequencer
{
    namespace
    {
        namespace json = winrt::Windows::Data::Json;

        constexpr wchar_t KeyHidden[] = L"hidden";
        constexpr wchar_t KeyKeys[] = L"keys";
        constexpr wchar_t KeyChords[] = L"chords";
        constexpr wchar_t KeyTick[] = L"tick";
        constexpr wchar_t KeyLength[] = L"length";
        constexpr wchar_t KeyRoot[] = L"root";
        constexpr wchar_t KeyScale[] = L"scale";
        constexpr wchar_t KeyChord[] = L"chord";

        constexpr int64_t MaximumTick = SequenceLimits{}.MaximumTick;

        std::wstring Quote(_In_ std::wstring_view text)
        {
            return std::wstring{ json::JsonValue::CreateStringValue(text).Stringify() };
        }

        std::optional<double> NumberMember(_In_ json::JsonObject const& object, _In_ std::wstring_view key)
        {
            if (!object.HasKey(key))
            {
                return std::nullopt;
            }

            auto const value = object.Lookup(key);

            if (value == nullptr || value.ValueType() != json::JsonValueType::Number)
            {
                return std::nullopt;
            }

            auto const number = value.GetNumber();

            if (!std::isfinite(number))
            {
                return std::nullopt;
            }

            return number;
        }

        std::optional<std::wstring> StringMember(_In_ json::JsonObject const& object, _In_ std::wstring_view key)
        {
            if (!object.HasKey(key))
            {
                return std::nullopt;
            }

            auto const value = object.Lookup(key);

            if (value == nullptr || value.ValueType() != json::JsonValueType::String)
            {
                return std::nullopt;
            }

            return std::wstring{ value.GetString() };
        }

        int64_t TickFrom(_In_ double number) noexcept
        {
            return static_cast<int64_t>(std::clamp(std::round(number), 0.0, static_cast<double>(MaximumTick)));
        }

        // The array a member holds, or nothing. One that's there and isn't an array is counted.
        json::JsonArray ArrayMember(_In_ json::JsonObject const& object, _In_ std::wstring_view key, _Inout_ size_t& skipped)
        {
            if (!object.HasKey(key))
            {
                return nullptr;
            }

            auto const value = object.Lookup(key);

            if (value == nullptr || value.ValueType() != json::JsonValueType::Array)
            {
                ++skipped;
                return nullptr;
            }

            return value.GetArray();
        }

        void CleanChord(_Inout_ ChordEvent& chord) noexcept
        {
            chord.Tick = std::clamp<int64_t>(chord.Tick, 0, MaximumTick);
            chord.Length = std::clamp<int64_t>(chord.Length, 1, std::max<int64_t>(1, MaximumTick - chord.Tick));
            chord.Root = static_cast<uint8_t>(chord.Root % 12);

            if (static_cast<size_t>(chord.Type) >= midiapp::ChordTypeCount)
            {
                chord.Type = ChordType::Major;
            }

            if (chord.Bass)
            {
                chord.Bass = static_cast<uint8_t>(*chord.Bass % 12);

                if (*chord.Bass == chord.Root)
                {
                    chord.Bass.reset();
                }
            }
        }
    }

    // ---- names ----

    _Use_decl_annotations_
    KeySignature KeySignatureOf(KeyChange const& key) noexcept
    {
        // Sharps or flats of each major key by its root, spelled as KeyUsesFlats spells them.
        static constexpr std::array<int8_t, 12> MajorAccidentals{ 0, -5, 2, -3, 4, -1, -6, 1, -4, 3, -2, 5 };

        KeySignature signature{};
        signature.Accidentals = MajorAccidentals[midiapp::ParentMajorRoot(MusicKeyOf(key))];
        signature.Minor = TonicChordType(key.Scale) == ChordType::Minor;
        return signature;
    }

    _Use_decl_annotations_
    KeyChange KeyFromSignature(KeySignature signature, int64_t tick) noexcept
    {
        auto const accidentals = std::clamp<int>(signature.Accidentals, -7, 7);
        auto const majorRoot = static_cast<uint8_t>(((accidentals * 7) % 12 + 12) % 12);

        KeyChange key{};
        key.Tick = std::max<int64_t>(0, tick);
        key.Root = signature.Minor ? static_cast<uint8_t>((majorRoot + 9) % 12) : majorRoot;
        key.Scale = signature.Minor ? ScaleKind::NaturalMinor : ScaleKind::Major;
        return key;
    }

    _Use_decl_annotations_
    std::wstring ChordName(ChordEvent const& chord, KeyChange const* key)
    {
        if (key == nullptr)
        {
            return midiapp::ChordName(ChordSymbolOf(chord), nullptr);
        }

        auto const musicKey = MusicKeyOf(*key);
        return midiapp::ChordName(ChordSymbolOf(chord), &musicKey);
    }

    _Use_decl_annotations_
    std::optional<ParsedChord> ParseChordName(std::wstring_view text)
    {
        auto const parsed = midiapp::ParseChordName(text);

        if (!parsed)
        {
            return std::nullopt;
        }

        ParsedChord chord{};
        chord.NoChord = parsed->NoChord;
        chord.Chord.Root = parsed->Chord.Root;
        chord.Chord.Type = parsed->Chord.Type;
        chord.Chord.Bass = parsed->Chord.Bass;
        return chord;
    }

    _Use_decl_annotations_
    std::wstring RomanNumeral(ChordEvent const& chord, KeyChange const& key)
    {
        return midiapp::RomanNumeral(ChordSymbolOf(chord), MusicKeyOf(key));
    }

    _Use_decl_annotations_
    bool ChordIsInKey(ChordEvent const& chord, KeyChange const& key)
    {
        return midiapp::ChordIsInKey(ChordSymbolOf(chord), MusicKeyOf(key));
    }

    // ---- notes ----

    _Use_decl_annotations_
    std::vector<uint8_t> ChordPitchClasses(ChordEvent const& chord)
    {
        return midiapp::ChordPitchClasses(ChordSymbolOf(chord));
    }

    _Use_decl_annotations_
    std::vector<uint8_t> ChordNotes(ChordEvent const& chord)
    {
        return midiapp::ChordNotes(ChordSymbolOf(chord));
    }

    // ---- the row ----

    _Use_decl_annotations_
    KeyChange const* KeyAt(HarmonyTrack const& harmony, int64_t tick) noexcept
    {
        if (harmony.Keys.empty())
        {
            return nullptr;
        }

        auto const after = std::upper_bound(harmony.Keys.begin(), harmony.Keys.end(), tick,
            [](int64_t t, KeyChange const& key) { return t < key.Tick; });

        return after == harmony.Keys.begin() ? &harmony.Keys.front() : &*(after - 1);
    }

    _Use_decl_annotations_
    ChordEvent const* ChordAt(HarmonyTrack const& harmony, int64_t tick) noexcept
    {
        auto const after = std::upper_bound(harmony.Chords.begin(), harmony.Chords.end(), tick,
            [](int64_t t, ChordEvent const& chord) { return t < chord.Tick; });

        if (after == harmony.Chords.begin())
        {
            return nullptr;
        }

        auto const& chord = *(after - 1);
        return tick - chord.Tick < chord.Length ? &chord : nullptr;
    }

    _Use_decl_annotations_
    void NormalizeHarmony(HarmonyTrack& harmony)
    {
        if (harmony.Keys.size() > MaximumHarmonyEvents)
        {
            harmony.Keys.resize(MaximumHarmonyEvents);
        }

        if (harmony.Chords.size() > MaximumHarmonyEvents)
        {
            harmony.Chords.resize(MaximumHarmonyEvents);
        }

        for (auto& key : harmony.Keys)
        {
            key.Tick = std::clamp<int64_t>(key.Tick, 0, MaximumTick);
            key.Root = static_cast<uint8_t>(key.Root % 12);

            if (static_cast<size_t>(key.Scale) >= midiapp::ScaleKindCount)
            {
                key.Scale = ScaleKind::Major;
            }
        }

        std::stable_sort(harmony.Keys.begin(), harmony.Keys.end(),
            [](KeyChange const& a, KeyChange const& b) { return a.Tick < b.Tick; });

        std::vector<KeyChange> keys{};
        keys.reserve(harmony.Keys.size());

        for (auto const& key : harmony.Keys)
        {
            if (!keys.empty() && keys.back().Tick == key.Tick)
            {
                keys.back() = key;
            }
            else
            {
                keys.push_back(key);
            }
        }

        harmony.Keys = std::move(keys);

        for (auto& chord : harmony.Chords)
        {
            CleanChord(chord);
        }

        std::stable_sort(harmony.Chords.begin(), harmony.Chords.end(),
            [](ChordEvent const& a, ChordEvent const& b) { return a.Tick < b.Tick; });

        std::vector<ChordEvent> chords{};
        chords.reserve(harmony.Chords.size());

        for (auto const& chord : harmony.Chords)
        {
            if (!chords.empty() && chords.back().Tick + chords.back().Length > chord.Tick)
            {
                if (chords.back().Tick == chord.Tick)
                {
                    chords.pop_back();
                }
                else
                {
                    chords.back().Length = chord.Tick - chords.back().Tick;
                }
            }

            chords.push_back(chord);
        }

        harmony.Chords = std::move(chords);
    }

    _Use_decl_annotations_
    size_t PlaceChord(HarmonyTrack& harmony, ChordEvent chord)
    {
        CleanChord(chord);

        auto const start = chord.Tick;
        auto const end = chord.Tick + chord.Length;

        std::vector<ChordEvent> chords{};
        chords.reserve(harmony.Chords.size() + 2);

        for (auto const& existing : harmony.Chords)
        {
            auto const existingEnd = existing.Tick + existing.Length;

            if (existingEnd <= start || existing.Tick >= end)
            {
                chords.push_back(existing);
            }
            else if (existing.Tick < start && existingEnd > end)
            {
                auto left = existing;
                left.Length = start - existing.Tick;
                chords.push_back(left);

                auto right = existing;
                right.Tick = end;
                right.Length = existingEnd - end;
                chords.push_back(right);
            }
            else if (existing.Tick < start)
            {
                auto left = existing;
                left.Length = start - existing.Tick;
                chords.push_back(left);
            }
            else if (existingEnd > end)
            {
                auto right = existing;
                right.Tick = end;
                right.Length = existingEnd - end;
                chords.push_back(right);
            }
        }

        auto const at = std::upper_bound(chords.begin(), chords.end(), start,
            [](int64_t t, ChordEvent const& c) { return t < c.Tick; });

        auto const index = static_cast<size_t>(at - chords.begin());
        chords.insert(at, chord);

        harmony.Chords = std::move(chords);
        return index;
    }

    _Use_decl_annotations_
    void TransposeChords(HarmonyTrack& harmony, int64_t fromTick, int64_t toTick, int semitones)
    {
        auto const shift = ((semitones % 12) + 12) % 12;

        for (auto& chord : harmony.Chords)
        {
            if (chord.Tick < fromTick || chord.Tick >= toTick)
            {
                continue;
            }

            chord.Root = static_cast<uint8_t>((chord.Root % 12 + shift) % 12);

            if (chord.Bass)
            {
                chord.Bass = static_cast<uint8_t>((*chord.Bass % 12 + shift) % 12);
            }
        }
    }

    _Use_decl_annotations_
    std::unique_ptr<SequenceChange> MakeHarmonyChange(std::optional<HarmonyTrack> before, std::optional<HarmonyTrack> after)
    {
        auto const cost = [](std::optional<HarmonyTrack> const& harmony)
        {
            size_t bytes{ sizeof(HarmonyTrack) };

            if (harmony)
            {
                bytes += harmony->Keys.size() * sizeof(KeyChange) + harmony->Chords.size() * sizeof(ChordEvent);
            }

            return bytes;
        };

        auto const total = cost(before) + cost(after);

        return std::make_unique<SnapshotChange<std::optional<HarmonyTrack>>>(
            [](Sequence& sequence) { return &sequence.Harmony; },
            std::move(before),
            std::move(after),
            total);
    }

    // ---- the sequence file ----

    _Use_decl_annotations_
    std::wstring HarmonyToJsonText(HarmonyTrack const& harmony)
    {
        std::wstring out{ L"{" };
        bool first{ true };

        auto const member = [&](std::wstring_view key)
        {
            out += first ? L"\n    " : L",\n    ";
            first = false;
            out += Quote(key);
            out += L": ";
        };

        if (harmony.Hidden)
        {
            member(KeyHidden);
            out += L"true";
        }

        member(KeyKeys);
        out += L"[";

        for (size_t i = 0; i < harmony.Keys.size(); ++i)
        {
            auto const& key = harmony.Keys[i];

            out += i == 0 ? L"\n      " : L",\n      ";
            out += std::format(L"{{ {}: {}, {}: {}, {}: {} }}",
                Quote(KeyTick), key.Tick,
                Quote(KeyRoot), Quote(NoteName(key.Root, KeyUsesFlats(key.Root, key.Scale))),
                Quote(KeyScale), Quote(ScaleId(key.Scale)));
        }

        out += harmony.Keys.empty() ? L"]" : L"\n    ]";

        member(KeyChords);
        out += L"[";

        for (size_t i = 0; i < harmony.Chords.size(); ++i)
        {
            auto const& chord = harmony.Chords[i];

            out += i == 0 ? L"\n      " : L",\n      ";
            out += std::format(L"{{ {}: {}, {}: {}, {}: {} }}",
                Quote(KeyTick), chord.Tick,
                Quote(KeyLength), chord.Length,
                Quote(KeyChord), Quote(ChordName(chord, KeyAt(harmony, chord.Tick))));
        }

        out += harmony.Chords.empty() ? L"]" : L"\n    ]";

        if (harmony.Unknown != nullptr)
        {
            std::vector<std::pair<std::wstring, std::wstring>> pairs{};

            for (auto const& pair : harmony.Unknown)
            {
                pairs.emplace_back(std::wstring{ pair.Key() }, std::wstring{ pair.Value().Stringify() });
            }

            std::sort(pairs.begin(), pairs.end());

            for (auto const& [key, value] : pairs)
            {
                member(key);
                out += value;
            }
        }

        out += L"\n  }";
        return out;
    }

    _Use_decl_annotations_
    std::optional<HarmonyTrack> ReadHarmonyJson(json::JsonObject const& root, size_t& skipped)
    {
        if (root == nullptr || !root.HasKey(HarmonyKey))
        {
            return std::nullopt;
        }

        auto const value = root.Lookup(HarmonyKey);

        if (value == nullptr || value.ValueType() != json::JsonValueType::Object)
        {
            ++skipped;
            return std::nullopt;
        }

        auto const object = value.GetObject();

        HarmonyTrack harmony{};

        if (object.HasKey(KeyHidden))
        {
            auto const hidden = object.Lookup(KeyHidden);

            if (hidden != nullptr && hidden.ValueType() == json::JsonValueType::Boolean)
            {
                harmony.Hidden = hidden.GetBoolean();
            }
            else
            {
                ++skipped;
            }
        }

        if (auto const keys = ArrayMember(object, KeyKeys, skipped); keys != nullptr)
        {
            for (auto const& entry : keys)
            {
                if (harmony.Keys.size() >= MaximumHarmonyEvents || entry.ValueType() != json::JsonValueType::Object)
                {
                    ++skipped;
                    continue;
                }

                auto const fields = entry.GetObject();
                auto const tick = NumberMember(fields, KeyTick);
                auto const rootName = StringMember(fields, KeyRoot);
                auto const scaleId = StringMember(fields, KeyScale);
                auto const pitch = rootName ? midiapp::ParseNoteName(*rootName) : std::nullopt;
                auto const scale = scaleId ? ScaleFromId(*scaleId) : std::nullopt;

                if (!tick || !pitch || !scale)
                {
                    ++skipped;
                    continue;
                }

                harmony.Keys.push_back(KeyChange{ TickFrom(*tick), *pitch, *scale });
            }
        }

        if (auto const chords = ArrayMember(object, KeyChords, skipped); chords != nullptr)
        {
            for (auto const& entry : chords)
            {
                if (harmony.Chords.size() >= MaximumHarmonyEvents || entry.ValueType() != json::JsonValueType::Object)
                {
                    ++skipped;
                    continue;
                }

                auto const fields = entry.GetObject();
                auto const tick = NumberMember(fields, KeyTick);
                auto const length = NumberMember(fields, KeyLength);
                auto const name = StringMember(fields, KeyChord);
                auto const parsed = name ? ParseChordName(*name) : std::nullopt;

                if (!tick || !length || !parsed || parsed->NoChord)
                {
                    ++skipped;
                    continue;
                }

                auto chord = parsed->Chord;
                chord.Tick = TickFrom(*tick);
                chord.Length = std::max<int64_t>(1, TickFrom(*length));
                harmony.Chords.push_back(chord);
            }
        }

        for (auto const& pair : object)
        {
            std::wstring_view const key{ pair.Key() };

            if (key == KeyHidden || key == KeyKeys || key == KeyChords)
            {
                continue;
            }

            if (harmony.Unknown == nullptr)
            {
                harmony.Unknown = json::JsonObject{};
            }

            harmony.Unknown.SetNamedValue(pair.Key(), pair.Value());
        }

        NormalizeHarmony(harmony);
        return harmony;
    }
}
