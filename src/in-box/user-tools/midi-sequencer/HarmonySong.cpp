// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#include "HarmonySong.h"
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
#include <cwctype>

namespace midisequencer
{
    namespace
    {
        namespace json = winrt::Windows::Data::Json;

        constexpr wchar_t KeyFormat[] = L"format";
        constexpr wchar_t KeyVersion[] = L"version";
        constexpr wchar_t KeyMeter[] = L"meter";
        constexpr wchar_t KeyKey[] = L"key";
        constexpr wchar_t KeyRoot[] = L"root";
        constexpr wchar_t KeyScale[] = L"scale";
        constexpr wchar_t KeySections[] = L"sections";
        constexpr wchar_t KeyName[] = L"name";
        constexpr wchar_t KeyBars[] = L"bars";
        constexpr wchar_t KeyChords[] = L"chords";
        constexpr wchar_t KeyChord[] = L"chord";
        constexpr wchar_t KeyBeats[] = L"beats";

        SongReadResult Refuse(SongProblem problem, std::wstring section = {}, std::vector<int64_t> values = {}, std::wstring text = {})
        {
            SongReadResult result{};
            result.Problem = problem;
            result.Section = std::move(section);
            result.Values = std::move(values);
            result.Text = std::move(text);
            return result;
        }

        std::wstring Trim(std::wstring_view text)
        {
            auto const first = text.find_first_not_of(L" \t\r\n");

            if (first == std::wstring_view::npos)
            {
                return {};
            }

            auto const last = text.find_last_not_of(L" \t\r\n");
            return std::wstring{ text.substr(first, last - first + 1) };
        }

        std::optional<std::wstring> StringAt(json::JsonObject const& object, wchar_t const* key)
        {
            if (!object.HasKey(key))
            {
                return std::nullopt;
            }

            auto const value = object.GetNamedValue(key);

            if (value.ValueType() != json::JsonValueType::String)
            {
                return std::nullopt;
            }

            return std::wstring{ value.GetString() };
        }

        // A whole number, or nothing when it's missing, not a number, or has a fraction.
        std::optional<int64_t> WholeNumberAt(json::JsonObject const& object, wchar_t const* key)
        {
            if (!object.HasKey(key))
            {
                return std::nullopt;
            }

            auto const value = object.GetNamedValue(key);

            if (value.ValueType() != json::JsonValueType::Number)
            {
                return std::nullopt;
            }

            auto const number = value.GetNumber();

            if (!std::isfinite(number) || number != std::floor(number) || std::abs(number) > 1.0e9)
            {
                return std::nullopt;
            }

            return static_cast<int64_t>(number);
        }

        // There and not null: null counts as left out.
        bool HasValue(json::JsonObject const& object, wchar_t const* key)
        {
            return object.HasKey(key) && object.GetNamedValue(key).ValueType() != json::JsonValueType::Null;
        }

        std::optional<KeyChange> ReadKey(json::JsonObject const& object)
        {
            if (!HasValue(object, KeyKey) || object.GetNamedValue(KeyKey).ValueType() != json::JsonValueType::Object)
            {
                return std::nullopt;
            }

            auto const key = object.GetNamedObject(KeyKey);
            auto const root = StringAt(key, KeyRoot);
            auto const scaleId = StringAt(key, KeyScale);

            if (!root || !scaleId)
            {
                return std::nullopt;
            }

            // A root is a chord name with nothing after it.
            auto const parsed = ParseChordName(*root);
            auto const scale = ScaleFromId(*scaleId);

            if (!parsed || parsed->NoChord || parsed->Chord.Type != ChordType::Major || parsed->Chord.Bass.has_value() || !scale)
            {
                return std::nullopt;
            }

            return KeyChange{ 0, parsed->Chord.Root, *scale };
        }

        std::optional<MeterChange> ParseMeter(std::wstring_view text)
        {
            auto const slash = text.find(L'/');

            if (slash == std::wstring_view::npos || slash == 0 || slash + 1 >= text.size() || slash > 2 || text.size() - slash - 1 > 2)
            {
                return std::nullopt;
            }

            auto const number = [](std::wstring_view digits) -> int
            {
                int value{ 0 };

                for (auto const ch : digits)
                {
                    if (ch < L'0' || ch > L'9')
                    {
                        return -1;
                    }

                    value = value * 10 + (ch - L'0');
                }

                return value;
            };

            auto const numerator = number(text.substr(0, slash));
            auto const denominator = number(text.substr(slash + 1));

            if (numerator < 1 || numerator > 32 || (denominator != 1 && denominator != 2 && denominator != 4 && denominator != 8 && denominator != 16 && denominator != 32))
            {
                return std::nullopt;
            }

            MeterChange meter{};
            meter.Numerator = static_cast<uint8_t>(numerator);
            meter.Denominator = static_cast<uint8_t>(denominator);
            return meter;
        }

        std::wstring MeterText(MeterChange const& meter)
        {
            return std::to_wstring(meter.Numerator) + L"/" + std::to_wstring(meter.Denominator);
        }

        // The block after a fence's opening line, up to its closing fence.
        std::optional<std::wstring> FencedBlock(std::wstring_view answer, std::wstring_view info)
        {
            size_t at{ 0 };

            while ((at = answer.find(L"```", at)) != std::wstring_view::npos)
            {
                auto const lineEnd = answer.find(L'\n', at);

                if (lineEnd == std::wstring_view::npos)
                {
                    return std::nullopt;
                }

                auto const label = Trim(answer.substr(at + 3, lineEnd - at - 3));
                auto const close = answer.find(L"```", lineEnd + 1);

                if (close == std::wstring_view::npos)
                {
                    return std::nullopt;
                }

                if (_wcsicmp(label.c_str(), std::wstring{ info }.c_str()) == 0)
                {
                    return std::wstring{ answer.substr(lineEnd + 1, close - lineEnd - 1) };
                }

                at = close + 3;
            }

            return std::nullopt;
        }

        std::wstring LowerLetters(std::wstring_view text)
        {
            std::wstring lower{};

            for (auto const ch : text)
            {
                if (std::iswalpha(ch))
                {
                    lower += static_cast<wchar_t>(std::towlower(ch));
                }
            }

            return lower;
        }
    }

    _Use_decl_annotations_
    std::wstring ExtractSongJson(std::wstring_view answer)
    {
        if (auto block = FencedBlock(answer, SongFenceName); block)
        {
            return Trim(*block);
        }

        if (auto block = FencedBlock(answer, L"json"); block)
        {
            return Trim(*block);
        }

        auto const first = answer.find(L'{');
        auto const last = answer.rfind(L'}');

        if (first == std::wstring_view::npos || last == std::wstring_view::npos || last < first)
        {
            return {};
        }

        return std::wstring{ answer.substr(first, last - first + 1) };
    }

    _Use_decl_annotations_
    SongReadResult ReadSongAnswer(std::wstring_view answer, MeterChange const& meter)
    {
        if (Trim(answer).empty())
        {
            return Refuse(SongProblem::Empty);
        }

        if (answer.size() > MaximumSongAnswerLength)
        {
            return Refuse(SongProblem::TooLong);
        }

        auto const text = ExtractSongJson(answer);

        if (text.empty())
        {
            return Refuse(SongProblem::NoJson);
        }

        json::JsonObject root{ nullptr };

        try
        {
            if (!json::JsonObject::TryParse(text, root) || root == nullptr)
            {
                return Refuse(SongProblem::BadJson);
            }
        }
        catch (...)
        {
            return Refuse(SongProblem::BadJson);
        }

        if (StringAt(root, KeyFormat).value_or(L"") != SongFormatName)
        {
            return Refuse(SongProblem::WrongFormat);
        }

        auto const version = WholeNumberAt(root, KeyVersion);

        if (!version || *version < 1)
        {
            return Refuse(SongProblem::BadVersion);
        }

        if (*version > SongFormatVersion)
        {
            return Refuse(SongProblem::NewerVersion, {}, { *version });
        }

        SongPlan plan{};

        if (HasValue(root, KeyMeter))
        {
            auto const written = StringAt(root, KeyMeter);
            auto const parsed = written ? ParseMeter(Trim(*written)) : std::nullopt;

            if (!parsed)
            {
                return Refuse(SongProblem::BadMeter, {}, {}, written.value_or(L""));
            }

            if (parsed->Numerator != meter.Numerator || parsed->Denominator != meter.Denominator)
            {
                return Refuse(SongProblem::MeterDiffers, {}, { parsed->Numerator, parsed->Denominator, meter.Numerator, meter.Denominator });
            }

            plan.Meter = parsed;
        }

        auto const key = ReadKey(root);

        if (!key)
        {
            return Refuse(SongProblem::BadKey);
        }

        plan.Key = *key;

        if (!root.HasKey(KeySections) || root.GetNamedValue(KeySections).ValueType() != json::JsonValueType::Array)
        {
            return Refuse(SongProblem::NoSections);
        }

        auto const sections = root.GetNamedArray(KeySections);

        if (sections.Size() == 0)
        {
            return Refuse(SongProblem::NoSections);
        }

        if (sections.Size() > MaximumSongSections)
        {
            return Refuse(SongProblem::TooManySections, {}, { static_cast<int64_t>(sections.Size()) });
        }

        auto const beatsPerBar = static_cast<int64_t>(std::max<uint8_t>(1, meter.Numerator));
        int64_t totalBars{ 0 };
        size_t totalChords{ 0 };

        for (uint32_t i = 0; i < sections.Size(); ++i)
        {
            auto const value = sections.GetAt(i);

            if (value.ValueType() != json::JsonValueType::Object)
            {
                return Refuse(SongProblem::BadSectionName, {}, { static_cast<int64_t>(i) + 1 });
            }

            auto const object = value.GetObject();
            SongSection section{};

            section.Name = Trim(StringAt(object, KeyName).value_or(L""));

            if (section.Name.empty() || section.Name.size() > MaximumSectionNameLength)
            {
                return Refuse(SongProblem::BadSectionName, {}, { static_cast<int64_t>(i) + 1 });
            }

            auto const bars = WholeNumberAt(object, KeyBars);

            if (!bars || *bars < 1 || *bars > MaximumSongBars)
            {
                return Refuse(SongProblem::BadBars, section.Name, bars ? std::vector<int64_t>{ *bars } : std::vector<int64_t>{});
            }

            section.Bars = *bars;
            totalBars += *bars;

            if (totalBars > MaximumSongBars)
            {
                return Refuse(SongProblem::TooManyBars, {}, { totalBars });
            }

            if (HasValue(object, KeyKey))
            {
                auto const sectionKey = ReadKey(object);

                if (!sectionKey)
                {
                    return Refuse(SongProblem::BadKey, section.Name);
                }

                section.Key = sectionKey;
            }

            if (!object.HasKey(KeyChords) || object.GetNamedValue(KeyChords).ValueType() != json::JsonValueType::Array ||
                object.GetNamedArray(KeyChords).Size() == 0)
            {
                return Refuse(SongProblem::NoChords, section.Name);
            }

            auto const chords = object.GetNamedArray(KeyChords);
            int64_t beats{ 0 };

            for (uint32_t c = 0; c < chords.Size(); ++c)
            {
                if (++totalChords > MaximumSongChords)
                {
                    return Refuse(SongProblem::TooManyChords, {}, { static_cast<int64_t>(totalChords) });
                }

                auto const chordValue = chords.GetAt(c);

                if (chordValue.ValueType() != json::JsonValueType::Object)
                {
                    return Refuse(SongProblem::BadChord, section.Name, {}, std::wstring{});
                }

                auto const chordObject = chordValue.GetObject();
                auto const name = StringAt(chordObject, KeyChord).value_or(L"");
                auto const parsed = ParseChordName(name);

                if (!parsed)
                {
                    return Refuse(SongProblem::BadChord, section.Name, {}, name);
                }

                auto const chordBeats = WholeNumberAt(chordObject, KeyBeats);

                if (!chordBeats || *chordBeats < 1 || *chordBeats > MaximumSongBars * 32)
                {
                    return Refuse(SongProblem::BadBeats, section.Name, {}, name);
                }

                SongChord chord{};
                chord.NoChord = parsed->NoChord;
                chord.Chord = parsed->Chord;
                chord.Beats = *chordBeats;
                section.Chords.push_back(chord);
                beats += *chordBeats;
            }

            if (beats != section.Bars * beatsPerBar)
            {
                return Refuse(SongProblem::BeatsDontAddUp, section.Name, { beats, section.Bars, beatsPerBar }, MeterText(meter));
            }

            plan.Sections.push_back(std::move(section));
        }

        SongReadResult result{};
        result.Plan = std::move(plan);
        return result;
    }

    _Use_decl_annotations_
    SongSummary SummarizeSong(SongPlan const& plan) noexcept
    {
        SongSummary summary{};
        summary.Sections = plan.Sections.size();
        auto current = plan.Key;

        for (auto const& section : plan.Sections)
        {
            summary.Bars += section.Bars;

            for (auto const& chord : section.Chords)
            {
                summary.Chords += chord.NoChord ? 0 : 1;
            }

            if (section.Key && (section.Key->Root != current.Root || section.Key->Scale != current.Scale))
            {
                ++summary.KeyChanges;
                current = *section.Key;
            }
        }

        return summary;
    }

    _Use_decl_annotations_
    SongLayout LayOutSong(SongPlan const& plan, std::vector<MeterChange> const& meter, int64_t startTick)
    {
        SongLayout layout{};
        auto const firstBar = BarPositionAtTick(meter, std::max<int64_t>(0, startTick)).Bar;
        layout.Start = TickAtBar(meter, firstBar);

        auto bar = firstBar;
        auto current = plan.Key;
        current.Tick = layout.Start;
        layout.Keys.push_back(current);

        for (auto const& section : plan.Sections)
        {
            auto const sectionStart = TickAtBar(meter, bar);
            auto const sectionEnd = TickAtBar(meter, bar + section.Bars);

            if (section.Key && (section.Key->Root != current.Root || section.Key->Scale != current.Scale))
            {
                current = *section.Key;
                current.Tick = sectionStart;
                layout.Keys.push_back(current);
            }

            layout.Tags.push_back(Tag{ sectionStart, section.Name, SectionColor(section.Name) });

            auto tick = sectionStart;

            for (auto const& chord : section.Chords)
            {
                auto const length = chord.Beats * TicksPerBeat(MeterAtTick(meter, tick));
                auto const end = std::min(sectionEnd, tick + length);

                if (!chord.NoChord && end > tick)
                {
                    auto placed = chord.Chord;
                    placed.Tick = tick;
                    placed.Length = end - tick;
                    layout.Chords.push_back(placed);
                }

                tick = end;
            }

            bar += section.Bars;
            layout.End = sectionEnd;
        }

        return layout;
    }

    _Use_decl_annotations_
    bool SongCrossesMeterChange(std::vector<MeterChange> const& meter, int64_t startTick, int64_t bars) noexcept
    {
        auto const firstBar = BarPositionAtTick(meter, std::max<int64_t>(0, startTick)).Bar;
        auto const start = TickAtBar(meter, firstBar);
        auto const end = TickAtBar(meter, firstBar + std::max<int64_t>(0, bars));

        return std::any_of(meter.begin(), meter.end(), [start, end](MeterChange const& change) { return change.Tick > start && change.Tick < end; });
    }

    _Use_decl_annotations_
    void ClearChordRange(HarmonyTrack& harmony, int64_t from, int64_t to)
    {
        if (to <= from)
        {
            return;
        }

        std::vector<ChordEvent> kept{};
        kept.reserve(harmony.Chords.size() + 1);

        for (auto const& chord : harmony.Chords)
        {
            auto const end = chord.Tick + chord.Length;

            if (end <= from || chord.Tick >= to)
            {
                kept.push_back(chord);
                continue;
            }

            if (chord.Tick < from)
            {
                auto before = chord;
                before.Length = from - chord.Tick;
                kept.push_back(before);
            }

            if (end > to)
            {
                auto after = chord;
                after.Tick = to;
                after.Length = end - to;
                kept.push_back(after);
            }
        }

        harmony.Chords = std::move(kept);
    }

    _Use_decl_annotations_
    uint32_t SectionColor(std::wstring_view name) noexcept
    {
        try
        {
            struct Kind
            {
                wchar_t const* Start;
                uint32_t Color;
            };

            // Matched on the letters only, so "Pre-chorus 2" is "prechorus". Pre-chorus comes
            // before Chorus so it gets its own color.
            static constexpr std::array<Kind, 10> Kinds{ {
                { L"prechorus", 0x6CCB5F },
                { L"intro", 0x8E8CD8 },
                { L"verse", 0x4CC2FF },
                { L"chorus", 0xF7630C },
                { L"refrain", 0xF7630C },
                { L"bridge", 0xC239B3 },
                { L"solo", 0xFFB900 },
                { L"breakdown", 0x00B7C3 },
                { L"outro", 0x8A8886 },
                { L"coda", 0x8A8886 },
            } };

            auto const letters = LowerLetters(name);

            for (auto const& kind : Kinds)
            {
                if (letters.rfind(kind.Start, 0) == 0)
                {
                    return kind.Color;
                }
            }

            static constexpr std::array<uint32_t, 6> Others{ 0xE3008C, 0x0099BC, 0x8764B8, 0x498205, 0xCA5010, 0x038387 };

            uint32_t hash{ 2166136261u };

            for (auto const ch : letters)
            {
                hash = (hash ^ static_cast<uint32_t>(ch)) * 16777619u;
            }

            return Others[hash % Others.size()];
        }
        catch (...)
        {
            return 0x8A8886;
        }
    }
}
