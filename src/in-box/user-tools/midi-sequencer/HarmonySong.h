// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

// Ask an AI assistant on the Key and chords row (proposal revision 2, section 7): reading the
// song plan an assistant writes back, checking it, and laying it out on the row and the Tempo
// and meter row's tags. The format is documented for assistants in
// docs/kb/midi-sequencer-key-and-chords-for-agents.md; change both together. No XAML here, so
// the tests compile it unchanged.

#include <sal.h>

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "SequenceModel.h"

namespace midisequencer
{
    inline constexpr wchar_t SongFormatName[] = L"windows-midi-sequencer-song";
    inline constexpr wchar_t SongFenceName[] = L"midi-sequencer-song";
    inline constexpr uint32_t SongFormatVersion = 1;

    inline constexpr size_t MaximumSongSections = 256;
    inline constexpr int64_t MaximumSongBars = 999;
    inline constexpr size_t MaximumSongChords = 4096;
    inline constexpr size_t MaximumSectionNameLength = 40;

    // Answers bigger than this are refused before they're read.
    inline constexpr size_t MaximumSongAnswerLength = 1024 * 1024;

    struct SongChord
    {
        bool NoChord{ false };
        ChordEvent Chord{};             // root, type and bass; tick and length are set when laid out
        int64_t Beats{ 0 };
    };

    struct SongSection
    {
        std::wstring Name{};
        int64_t Bars{ 0 };
        std::optional<KeyChange> Key{};
        std::vector<SongChord> Chords{};
    };

    struct SongPlan
    {
        std::optional<MeterChange> Meter{};     // as written; the sequence's is used when it's left out
        KeyChange Key{};
        std::vector<SongSection> Sections{};
    };

    // Why an answer was refused. Section is the section's name and Values the numbers in the
    // message, so the app can say it plainly in its own words.
    enum class SongProblem : uint8_t
    {
        None = 0,
        Empty,                  // nothing pasted
        TooLong,
        NoJson,                 // no JSON object found
        BadJson,                // it doesn't read as JSON
        WrongFormat,            // "format" missing or not ours
        NewerVersion,           // Values[0] is the version
        BadVersion,             // "version" missing or not a whole number
        BadMeter,               // "meter" isn't N/D
        MeterDiffers,           // Values: the answer's N and D, the sequence's N and D
        BadKey,                 // Section empty for the top level
        NoSections,
        TooManySections,
        BadSectionName,         // Values[0] is the section's number, from 1
        BadBars,                // Section; Values[0] is the bars as written, when a number
        TooManyBars,            // Values[0] is the total
        NoChords,               // Section
        TooManyChords,          // Values[0] is the total
        BadChord,               // Section; Text is the chord as written
        BadBeats,               // Section; Text is the chord
        BeatsDontAddUp,         // Section; Values: beats written, bars, beats per bar; Text the meter "4/4"
    };

    struct SongReadResult
    {
        SongProblem Problem{ SongProblem::None };
        std::wstring Section{};
        std::wstring Text{};
        std::vector<int64_t> Values{};
        std::optional<SongPlan> Plan{};

        bool Succeeded() const noexcept { return Problem == SongProblem::None && Plan.has_value(); }
    };

    // The JSON from an assistant's answer: the block fenced as midi-sequencer-song, else one fenced
    // as json, else the text from its first "{" to its last "}". Empty when there's none.
    std::wstring ExtractSongJson(_In_ std::wstring_view answer);

    // Reads and checks an answer against the sequence's meter at the place it goes: the format,
    // the version, the meter, the keys, the sections and their chords, the limits, and that each
    // section's chords fill its bars exactly. Nothing is half read: the plan is there only when
    // every check passes. Keys it doesn't know are ignored.
    SongReadResult ReadSongAnswer(_In_ std::wstring_view answer, _In_ MeterChange const& meter);

    // What a plan holds, for the summary the dialog shows.
    struct SongSummary
    {
        size_t Sections{ 0 };
        int64_t Bars{ 0 };
        size_t Chords{ 0 };                     // "N.C." isn't counted
        size_t KeyChanges{ 0 };                 // keys after the first that differ from the one before
    };

    SongSummary SummarizeSong(_In_ SongPlan const& plan) noexcept;

    // A plan on the timeline from a tick at the start of a bar: its keys, its chords (each for its
    // beats of the meter there; "N.C." leaves a gap), and a tag for each section at its first bar,
    // colored by its kind. End is the tick after its last bar.
    struct SongLayout
    {
        std::vector<KeyChange> Keys{};
        std::vector<ChordEvent> Chords{};
        std::vector<Tag> Tags{};
        int64_t Start{ 0 };
        int64_t End{ 0 };
    };

    SongLayout LayOutSong(_In_ SongPlan const& plan, _In_ std::vector<MeterChange> const& meter, _In_ int64_t startTick);

    // Whether a song of this many bars from startTick's bar runs over a meter change. Its beats
    // are checked against one meter, so the app refuses a song that does.
    bool SongCrossesMeterChange(_In_ std::vector<MeterChange> const& meter, _In_ int64_t startTick, _In_ int64_t bars) noexcept;

    // Clears [from, to) on the row for a song added there, so its "N.C." beats are silent: chords
    // inside go, one that starts before is cut at from, and one that runs past to keeps its part
    // after it.
    void ClearChordRange(_Inout_ HarmonyTrack& harmony, _In_ int64_t from, _In_ int64_t to);

    // The color a section's tag gets: Intro, Verse, Pre-chorus, Chorus, Bridge, Solo, Breakdown
    // and Outro each have their own, matched by how the name starts and ignoring case and
    // numbers ("Verse 2"). Other names get one picked from the name, the same every time.
    uint32_t SectionColor(_In_ std::wstring_view name) noexcept;
}
