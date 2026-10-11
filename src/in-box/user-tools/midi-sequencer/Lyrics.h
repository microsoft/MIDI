// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

// The lyrics track (design section 23): typing lyrics and putting them on notes, the lines a
// display shows, and lyrics in and out of Standard MIDI Files, MIDI 2.0 Lyric Data messages, LRC
// files and the sequence file. No XAML here, so the tests compile it unchanged.

#include <sal.h>

#include <array>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "SequenceModel.h"
#include "SequenceRender.h"
#include "SequenceUndo.h"

namespace midisequencer
{
    // ---- typing ----

    // The text the lyrics editor shows: a hyphen between a word's syllables, a space between
    // words, a line per line, a blank line between verses, and _ for a melisma. A hyphen that
    // belongs to the word, as in "six-ty--four", is typed twice.
    std::wstring LyricsToText(_In_ std::vector<Lyric> const& syllables);

    // The syllables some typed text holds, in order. Their ticks are all 0 until they're placed.
    std::vector<Lyric> ParseLyricsText(_In_ std::wstring_view text, _In_ size_t maximumSyllables);

    // Where a track's notes start, in order, as its timeline plays them, counting notes that start
    // together once. Only starts at or after fromTick, and at most limit of them.
    std::vector<int64_t> TrackNoteStarts(
        _In_ Sequence const& sequence,
        _In_ Track const& track,
        _In_ int64_t fromTick,
        _In_ size_t limit);

    // Puts each syllable on the next note of the track that sings, starting at fromTick. With no
    // such track, or once its notes run out, the syllables go a beat apart.
    void PlaceLyrics(
        _Inout_ std::vector<Lyric>& syllables,
        _In_ Sequence const& sequence,
        _In_ std::wstring_view trackId,
        _In_ int64_t fromTick);

    // The track whose notes start where the most syllables do. A file doesn't say which track
    // sings its lyrics, and players put the words on the melody, so this is the best guess.
    std::wstring TrackThatSings(
        _In_ std::vector<Track> const& tracks,
        _In_ std::vector<Clip> const& clips,
        _In_ std::vector<Lyric> const& syllables);

    // Sorted syllables, ticks in range, no control characters and nothing over the limits.
    void NormalizeLyrics(_Inout_ LyricsTrack& lyrics, _In_ SequenceLimits const& limits);

    std::unique_ptr<SequenceChange> MakeLyricsChange(
        _In_ std::optional<LyricsTrack> before,
        _In_ std::optional<LyricsTrack> after);

    // ---- showing ----

    // A line of lyrics as a display shows it.
    struct LyricLine
    {
        size_t First{ 0 };              // its first syllable
        size_t Count{ 0 };
        int64_t StartTick{ 0 };
        int64_t EndTick{ 0 };           // the next line's start, or a bar after its last syllable
        bool EndsVerse{ false };
    };

    // Lines end where the lyrics say, and also at a word's end once a line is long enough to
    // need it, since a file that never says still has to fit on a screen.
    std::vector<LyricLine> LyricLines(_In_ std::vector<Lyric> const& syllables);

    // A line as it reads, split where the syllables sung by the tick end: what's been sung and
    // what hasn't.
    std::pair<std::wstring, std::wstring> LyricLineText(
        _In_ std::vector<Lyric> const& syllables,
        _In_ LyricLine const& line,
        _In_ int64_t tick);

    // The index of the line at a tick: the last one that has started, or the first when none has.
    size_t LyricLineAtTick(_In_ std::vector<LyricLine> const& lines, _In_ int64_t tick) noexcept;

    // ---- Standard MIDI Files ----

    struct LyricFileText
    {
        int64_t Tick{ 0 };
        std::string Text{};             // UTF-8
    };

    // Syllables from a file's lyric events (RP-017, with RP-026's escapes, tags and ruby taken
    // out), or from a Soft Karaoke file's text events when karaoke is true: there / starts a line,
    // \ starts a verse, a leading space starts a word, and lines that start with @ are about the
    // file rather than words to sing.
    std::vector<Lyric> LyricsFromFileText(
        _In_ std::vector<LyricFileText> const& events,
        _In_ bool karaoke,
        _In_ SequenceLimits const& limits);

    // Lyric event text as RP-017 writes it: a space after a word's last syllable, a carriage
    // return after a line's and a line feed after a verse's, and an empty event for a melisma.
    // The characters RP-026 reserves are escaped.
    std::vector<LyricFileText> LyricsToFileText(_In_ std::vector<Lyric> const& syllables);

    // ---- MIDI 2.0 ----

    // A syllable as Lyric Data (M2-104 section 7.5.10: Flex Data, status bank 2, status 1) sent to
    // the whole group: one Complete message, or Start, Continue and End messages for longer text.
    // A melisma is a Complete message of nothing, followed by the end of the word, line or verse
    // when it ends one.
    std::vector<std::array<uint32_t, 4>> LyricDataPackets(_In_ Lyric const& syllable, _In_ uint8_t group);

    // The Lyrics Language message (status 2) for a BCP 47 tag. Empty for no tag.
    std::vector<std::array<uint32_t, 4>> LyricsLanguagePackets(_In_ std::wstring_view language, _In_ uint8_t group);

    // The Lyric Data messages a track sends in [fromTick, toTick) when it's the track that sings.
    // The language goes before the first syllable, and at fromTick too when sendLanguage is true,
    // for a start, a jump or the loop going round. Groups are 0; the destination sets them.
    void RenderLyrics(
        _In_ Sequence const& sequence,
        _In_ Track const& track,
        _In_ int64_t fromTick,
        _In_ int64_t toTick,
        _In_ bool sendLanguage,
        _In_ size_t maximum,
        _Inout_ std::vector<RenderedMessage>& messages);

    // ---- LRC ----

    // An LRC file: one [mm:ss.xx] line per line of lyrics, as karaoke apps, music players and
    // streaming services read them. UTF-8, with a byte order mark so every reader knows it.
    std::string LyricsToLrc(
        _In_ LyricsTrack const& lyrics,
        _In_ std::vector<TempoPoint> const& tempo,
        _In_ std::wstring_view title);

    // ---- the sequence file ----

    inline constexpr wchar_t LyricsKey[] = L"lyrics";

    // The value of "lyrics", indented to sit in the sequence file's top-level object.
    std::wstring LyricsToJsonText(_In_ LyricsTrack const& lyrics);

    // The "lyrics" value of a sequence file, if it has one. Anything malformed or over the limits
    // is counted in skipped and left out.
    std::optional<LyricsTrack> ReadLyricsJson(
        _In_ winrt::Windows::Data::Json::JsonObject const& root,
        _In_ SequenceLimits const& limits,
        _Inout_ size_t& skipped);
}
