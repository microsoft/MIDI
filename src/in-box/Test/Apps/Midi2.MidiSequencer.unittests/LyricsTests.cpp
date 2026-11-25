// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#include "LyricsTests.h"

#include "Lyrics.h"
#include "MidiClipFile.h"
#include "SequenceRender.h"
#include "SequenceSerializer.h"
#include "StandardMidiFileBridge.h"

#include "midi_file_smf_reader.h"

#include <span>
#include <string>
#include <string_view>

using namespace WEX::Common;
using namespace WEX::Logging;
using namespace WEX::TestExecution;
using namespace midisequencer;

namespace
{
    constexpr int64_t Beat = TicksPerQuarterNote;

    void VerifySyllable(Lyric const& syllable, int64_t tick, std::wstring_view text, LyricEnd end)
    {
        VERIFY_ARE_EQUAL(tick, syllable.Tick);
        VERIFY_ARE_EQUAL(std::wstring{ text }, syllable.Text);
        VERIFY_ARE_EQUAL(static_cast<int>(end), static_cast<int>(syllable.End));
    }

    Clip NotesClip(std::wstring id, std::initializer_list<int64_t> ticks, int64_t length, bool loop)
    {
        Clip clip{};
        clip.Id = std::move(id);
        clip.Name = clip.Id;
        clip.Length = length;
        clip.Loop = loop;

        for (auto const tick : ticks)
        {
            Note note{};
            note.Tick = tick;
            note.Length = Beat / 4;
            note.Number = 60;
            note.Velocity = 0xC000;
            clip.Notes.push_back(note);
        }

        return clip;
    }

    Track PlacedTrack(std::wstring id, std::wstring const& clipId, int64_t tick, int64_t length = 0)
    {
        Track track{};
        track.Id = std::move(id);
        track.Name = track.Id;
        track.Timeline.push_back(Placement{ clipId, tick, length });
        return track;
    }

    // A voice whose notes start at 0, half a beat, a beat (twice, a chord), a beat and a half and
    // two beats, placed a bar in, and a drum track that plays on the off beats.
    Sequence SungSequence()
    {
        Sequence sequence{};
        sequence.Name = L"Song";
        sequence.Clips.push_back(NotesClip(L"c-voice", { 0, Beat / 2, Beat, Beat, Beat * 3 / 2, Beat * 2 }, Beat * 4, false));
        sequence.Clips.push_back(NotesClip(L"c-drums", { Beat / 4, Beat * 3 / 4 }, Beat * 4, false));
        sequence.Tracks.push_back(PlacedTrack(L"t-voice", L"c-voice", Beat * 4));
        sequence.Tracks.push_back(PlacedTrack(L"t-drums", L"c-drums", 0));
        return sequence;
    }

    std::vector<Lyric> HappyBirthday()
    {
        return {
            Lyric{ 0, L"Hap", LyricEnd::None },
            Lyric{ Beat / 2, L"py", LyricEnd::Word },
            Lyric{ Beat, L"birth", LyricEnd::None },
            Lyric{ Beat * 3 / 2, L"day", LyricEnd::Line },
            Lyric{ Beat * 2, L"to", LyricEnd::Word },
            Lyric{ Beat * 5 / 2, L"you", LyricEnd::Paragraph },
        };
    }

    size_t CountLyricMessages(std::vector<RenderedMessage> const& messages, uint32_t status)
    {
        size_t count{ 0 };

        for (auto const& message : messages)
        {
            if ((message.Words[0] >> 28) == 0xD && (message.Words[0] & 0xFFFF) == (0x0200 | status))
            {
                ++count;
            }
        }

        return count;
    }
}

void LyricsTests::TypedWordsSplitIntoSyllables()
{
    std::wstring_view const typed{ L"Hap-py birth-day to you\nHap-py\n\nNext" };
    auto const syllables = ParseLyricsText(typed, 1000);

    VERIFY_ARE_EQUAL(size_t{ 9 }, syllables.size());
    VerifySyllable(syllables[0], 0, L"Hap", LyricEnd::None);
    VerifySyllable(syllables[1], 0, L"py", LyricEnd::Word);
    VerifySyllable(syllables[3], 0, L"day", LyricEnd::Word);
    VerifySyllable(syllables[5], 0, L"you", LyricEnd::Line);
    VerifySyllable(syllables[7], 0, L"py", LyricEnd::Paragraph);
    VerifySyllable(syllables[8], 0, L"Next", LyricEnd::None);

    VERIFY_ARE_EQUAL(std::wstring{ typed }, LyricsToText(syllables));

    // Windows line ends and extra spaces change nothing, and neither do a TextBox's, which are a
    // carriage return alone.
    VERIFY_IS_TRUE(syllables == ParseLyricsText(L"Hap-py  birth-day to you \r\nHap-py\r\n \r\nNext", 1000));
    VERIFY_IS_TRUE(syllables == ParseLyricsText(L"Hap-py birth-day to you\rHap-py\r\rNext", 1000));

    // No more than asked for.
    VERIFY_ARE_EQUAL(size_t{ 2 }, ParseLyricsText(L"a b c d", 2).size());
}

void LyricsTests::MelismasAndHyphensSurviveARoundTrip()
{
    std::wstring_view const typed{ L"A-_-men six-ty---four Ah _" };
    auto const syllables = ParseLyricsText(typed, 1000);

    VERIFY_ARE_EQUAL(size_t{ 8 }, syllables.size());
    VerifySyllable(syllables[0], 0, L"A", LyricEnd::None);
    VerifySyllable(syllables[1], 0, L"", LyricEnd::None);
    VerifySyllable(syllables[2], 0, L"men", LyricEnd::Word);
    VerifySyllable(syllables[3], 0, L"six", LyricEnd::None);
    VerifySyllable(syllables[4], 0, L"ty-", LyricEnd::None);
    VerifySyllable(syllables[5], 0, L"four", LyricEnd::Word);
    VerifySyllable(syllables[7], 0, L"", LyricEnd::None);

    VERIFY_ARE_EQUAL(std::wstring{ typed }, LyricsToText(syllables));
    VERIFY_IS_TRUE(syllables == ParseLyricsText(LyricsToText(syllables), 1000));
}

void LyricsTests::SyllablesGoOnTheSingersNotes()
{
    auto const sequence = SungSequence();

    // Notes that start together take one syllable, and once the notes run out the rest go a
    // beat apart.
    auto syllables = ParseLyricsText(L"one two three four five six", 100);
    PlaceLyrics(syllables, sequence, L"t-voice", 0);

    VERIFY_ARE_EQUAL(Beat * 4, syllables[0].Tick);
    VERIFY_ARE_EQUAL(Beat * 4 + Beat / 2, syllables[1].Tick);
    VERIFY_ARE_EQUAL(Beat * 5, syllables[2].Tick);
    VERIFY_ARE_EQUAL(Beat * 5 + Beat / 2, syllables[3].Tick);
    VERIFY_ARE_EQUAL(Beat * 6, syllables[4].Tick);
    VERIFY_ARE_EQUAL(Beat * 7, syllables[5].Tick);

    // From the playhead.
    auto later = ParseLyricsText(L"la la", 100);
    PlaceLyrics(later, sequence, L"t-voice", Beat * 4 + Beat * 3 / 4);
    VERIFY_ARE_EQUAL(Beat * 5, later[0].Tick);
    VERIFY_ARE_EQUAL(Beat * 5 + Beat / 2, later[1].Tick);

    // A looping clip gives a note every time round, and a one-tick loop a billion ticks long
    // doesn't walk every pass to get to the playhead.
    Sequence looped{};
    looped.Clips.push_back(NotesClip(L"c-loop", { 0 }, Beat, true));
    looped.Clips.push_back(NotesClip(L"c-tiny", { 0 }, 1, true));
    looped.Tracks.push_back(PlacedTrack(L"t-loop", L"c-loop", 0, Beat * 4));
    looped.Tracks.push_back(PlacedTrack(L"t-tiny", L"c-tiny", 0, 2000000000));

    auto round = ParseLyricsText(L"a b c d", 100);
    PlaceLyrics(round, looped, L"t-loop", 0);
    VERIFY_ARE_EQUAL(Beat * 3, round[3].Tick);

    auto far = ParseLyricsText(L"a b c", 100);
    PlaceLyrics(far, looped, L"t-tiny", 1000000000);
    VERIFY_ARE_EQUAL(int64_t{ 1000000002 }, far[2].Tick);
}

void LyricsTests::WithNoSingerSyllablesGoABeatApart()
{
    auto const sequence = SungSequence();
    auto syllables = ParseLyricsText(L"a b c", 100);

    PlaceLyrics(syllables, sequence, L"", 100);
    VERIFY_ARE_EQUAL(int64_t{ 100 }, syllables[0].Tick);
    VERIFY_ARE_EQUAL(100 + Beat, syllables[1].Tick);
    VERIFY_ARE_EQUAL(100 + Beat * 2, syllables[2].Tick);

    // A folder sings nothing, and neither does a track that isn't there.
    PlaceLyrics(syllables, sequence, L"t-gone", 0);
    VERIFY_ARE_EQUAL(Beat * 2, syllables[2].Tick);
}

void LyricsTests::LyricEventsReadTheWayRp017Says()
{
    std::vector<LyricFileText> const events{
        { 0, "{@LATIN}{#Title=Song}" },     // RP-026 tags only: not a syllable
        { 0, "Hap" },
        { Beat / 2, "py " },
        { Beat, "birth" },
        { Beat * 3 / 2, "day\r" },
        { Beat * 2, "to " },
        { Beat * 5 / 2, "you" },
        { Beat * 3, "" },                   // a melisma
        { Beat * 3 + 10, "\n" },            // a verse ends, on its own
        { Beat * 7 / 2, "Ne[ne]xt" },       // ruby, kept apart from the words
        { Beat * 4, "a\\[b\\] " },          // escapes
    };

    auto const syllables = LyricsFromFileText(events, false, SequenceLimits{});

    VERIFY_ARE_EQUAL(size_t{ 9 }, syllables.size());
    VerifySyllable(syllables[0], 0, L"Hap", LyricEnd::None);
    VerifySyllable(syllables[1], Beat / 2, L"py", LyricEnd::Word);
    VerifySyllable(syllables[3], Beat * 3 / 2, L"day", LyricEnd::Line);
    VerifySyllable(syllables[5], Beat * 5 / 2, L"you", LyricEnd::None);
    VerifySyllable(syllables[6], Beat * 3, L"", LyricEnd::Paragraph);
    VerifySyllable(syllables[7], Beat * 7 / 2, L"Next", LyricEnd::None);
    VERIFY_ARE_EQUAL(std::wstring{ L"ne" }, syllables[7].Ruby);
    VerifySyllable(syllables[8], Beat * 4, L"a[b]", LyricEnd::Word);

    // Written out and read back, nothing changes.
    auto const written = LyricsToFileText(syllables);
    VERIFY_ARE_EQUAL(std::string{ "day\r" }, written[3].Text);
    VERIFY_ARE_EQUAL(std::string{ "a\\[b\\] " }, written.back().Text);
    VERIFY_IS_TRUE(syllables == LyricsFromFileText(written, false, SequenceLimits{}));
}

void LyricsTests::KaraokeFilesRead()
{
    std::vector<LyricFileText> const events{
        { 0, "@KMIDI KARAOKE FILE" },
        { 0, "@TTitle" },
        { 0, "\\Hap" },
        { Beat / 2, "py" },
        { Beat, " birth" },
        { Beat * 3 / 2, "day" },
        { Beat * 2, "/to" },
        { Beat * 5 / 2, " you" },
    };

    auto const syllables = LyricsFromFileText(events, true, SequenceLimits{});

    VERIFY_ARE_EQUAL(size_t{ 6 }, syllables.size());
    VerifySyllable(syllables[0], 0, L"Hap", LyricEnd::None);
    VerifySyllable(syllables[1], Beat / 2, L"py", LyricEnd::Word);
    VerifySyllable(syllables[3], Beat * 3 / 2, L"day", LyricEnd::Line);
    VerifySyllable(syllables[4], Beat * 2, L"to", LyricEnd::Word);
    VerifySyllable(syllables[5], Beat * 5 / 2, L"you", LyricEnd::None);
}

void LyricsTests::OlderCharacterSetsRead()
{
    // ANSI, which RP-026 assumes when nothing says otherwise.
    auto const ansi = LyricsFromFileText({ { 0, "Caf\xE9 " }, { Beat, "\x93Hi\x94" } }, false, SequenceLimits{});
    VERIFY_ARE_EQUAL(size_t{ 2 }, ansi.size());
    VerifySyllable(ansi[0], 0, L"Caf\u00E9", LyricEnd::Word);
    VerifySyllable(ansi[1], Beat, L"\u201CHi\u201D", LyricEnd::None);

    // Shift-JIS once {@JP} says so.
    auto const japanese = LyricsFromFileText({ { 0, "{@JP}" }, { Beat, "\x82\xA0" } }, false, SequenceLimits{});
    VERIFY_ARE_EQUAL(size_t{ 1 }, japanese.size());
    VerifySyllable(japanese[0], Beat, L"\u3042", LyricEnd::None);

    // Byte order marks: UTF-16 as RP-026 describes, and UTF-8.
    auto const marked = LyricsFromFileText({ { 0, std::string{ "\xFF\xFE" "A\0B\0", 6 } }, { Beat, "\xEF\xBB\xBF" "\xC3\xA9t\xC3\xA9" } }, false, SequenceLimits{});
    VERIFY_ARE_EQUAL(size_t{ 2 }, marked.size());
    VerifySyllable(marked[0], 0, L"AB", LyricEnd::None);
    VerifySyllable(marked[1], Beat, L"\u00E9t\u00E9", LyricEnd::None);
}

void LyricsTests::LyricDataFollowsTheSpec()
{
    // Flex Data, the whole group, status bank 2, status 1, the text padded with 0.
    auto const one = LyricDataPackets(Lyric{ 0, L"Hap", LyricEnd::None }, 0);
    VERIFY_ARE_EQUAL(size_t{ 1 }, one.size());
    VERIFY_ARE_EQUAL(0xD0100201u, one[0][0]);
    VERIFY_ARE_EQUAL(0x48617000u, one[0][1]);
    VERIFY_ARE_EQUAL(0u, one[0][2]);
    VERIFY_ARE_EQUAL(0u, one[0][3]);

    auto const grouped = LyricDataPackets(Lyric{ 0, L"a", LyricEnd::Word }, 5);
    VERIFY_ARE_EQUAL(0xD5100201u, grouped[0][0]);
    VERIFY_ARE_EQUAL(0x61200000u, grouped[0][1]);

    // Longer text: a full Start message, then an End.
    auto const longer = LyricDataPackets(Lyric{ 0, L"supercalifragilistic", LyricEnd::Word }, 0);
    VERIFY_ARE_EQUAL(size_t{ 2 }, longer.size());
    VERIFY_ARE_EQUAL(0xD0500201u, longer[0][0]);
    VERIFY_ARE_EQUAL(0x73757065u, longer[0][1]);
    VERIFY_ARE_EQUAL(0xD0D00201u, longer[1][0]);
    VERIFY_ARE_EQUAL(0x20000000u, longer[1][3]);

    // A Start message is never short, even when that splits a character.
    auto const split = LyricDataPackets(Lyric{ 0, L"abcdefghijk\u00E9", LyricEnd::None }, 0);
    VERIFY_ARE_EQUAL(size_t{ 2 }, split.size());
    VERIFY_ARE_EQUAL(0x696A6BC3u, split[0][3]);
    VERIFY_ARE_EQUAL(0xA9000000u, split[1][1]);

    // A melisma that ends a word: a Complete message of nothing, then a space.
    auto const melisma = LyricDataPackets(Lyric{ 0, L"", LyricEnd::Word }, 0);
    VERIFY_ARE_EQUAL(size_t{ 2 }, melisma.size());
    VERIFY_ARE_EQUAL(0xD0100201u, melisma[0][0]);
    VERIFY_ARE_EQUAL(0u, melisma[0][1]);
    VERIFY_ARE_EQUAL(0x20000000u, melisma[1][1]);

    // The end of a verse is a carriage return and a line feed.
    auto const verse = LyricDataPackets(Lyric{ 0, L"end", LyricEnd::Paragraph }, 0);
    VERIFY_ARE_EQUAL(0x656E640Du, verse[0][1]);
    VERIFY_ARE_EQUAL(0x0A000000u, verse[0][2]);

    auto const language = LyricsLanguagePackets(L"en", 0);
    VERIFY_ARE_EQUAL(size_t{ 1 }, language.size());
    VERIFY_ARE_EQUAL(0xD0100202u, language[0][0]);
    VERIFY_ARE_EQUAL(0x656E0000u, language[0][1]);
    VERIFY_IS_TRUE(LyricsLanguagePackets(L"", 0).empty());
}

void LyricsTests::TheSingingTrackSendsTheLyrics()
{
    auto sequence = SungSequence();
    sequence.Lyrics = LyricsTrack{ L"en", L"t-voice", HappyBirthday(), nullptr };

    auto const& voice = sequence.Tracks[0];
    auto const& drums = sequence.Tracks[1];

    RenderOptions options{};
    options.Lyrics = true;

    std::vector<RenderedMessage> messages{};
    RenderTrack(sequence, voice, 0, Beat * 8, messages, options);

    VERIFY_ARE_EQUAL(size_t{ 1 }, CountLyricMessages(messages, 0x02));
    VERIFY_ARE_EQUAL(size_t{ 6 }, CountLyricMessages(messages, 0x01));

    // The language goes first, before the first syllable.
    VERIFY_ARE_EQUAL(0xD0100202u, messages.front().Words[0]);
    VERIFY_ARE_EQUAL(0xD0100201u, messages[1].Words[0]);

    // A window that starts later sends what's in it, and the language only for a start or a jump.
    messages.clear();
    RenderTrack(sequence, voice, Beat, Beat * 8, messages, options);
    VERIFY_ARE_EQUAL(size_t{ 0 }, CountLyricMessages(messages, 0x02));
    VERIFY_ARE_EQUAL(size_t{ 4 }, CountLyricMessages(messages, 0x01));

    options.LaneStartValues = true;
    messages.clear();
    RenderTrack(sequence, voice, Beat, Beat * 8, messages, options);
    VERIFY_ARE_EQUAL(size_t{ 1 }, CountLyricMessages(messages, 0x02));
    options.LaneStartValues = false;

    // Not from another track, not when not asked, and not to MIDI 1.0.
    messages.clear();
    RenderTrack(sequence, drums, 0, Beat * 8, messages, options);
    VERIFY_ARE_EQUAL(size_t{ 0 }, CountLyricMessages(messages, 0x01));

    options.Lyrics = false;
    messages.clear();
    RenderTrack(sequence, voice, 0, Beat * 8, messages, options);
    VERIFY_ARE_EQUAL(size_t{ 0 }, CountLyricMessages(messages, 0x01));

    options.Lyrics = true;
    options.Midi2 = false;
    messages.clear();
    RenderTrack(sequence, voice, 0, Beat * 8, messages, options);
    VERIFY_ARE_EQUAL(size_t{ 0 }, CountLyricMessages(messages, 0x01));
}

void LyricsTests::LrcHasALinePerLine()
{
    LyricsTrack const lyrics{ L"en", L"t-voice", HappyBirthday(), nullptr };

    auto const lrc = LyricsToLrc(lyrics, { TempoPoint{ 0, 120.0, false } }, L"Song");

    VERIFY_ARE_EQUAL(
        std::string{ "\xEF\xBB\xBF[ti:Song]\n[la:en]\n[00:00.00]Happy birthday\n[00:01.00]to you\n[00:02.25]\n" },
        lrc);
}

void LyricsTests::TheSequenceFileKeepsLyrics()
{
    auto original = SungSequence();
    original.Lyrics = LyricsTrack{ L"en", L"t-voice", HappyBirthday(), nullptr };
    original.Lyrics->Syllables[4].Text = L"\"to\"";
    original.Lyrics->Syllables.push_back(Lyric{ Beat * 3, L"", LyricEnd::None });

    winrt::Windows::Data::Json::JsonObject unknown{};
    unknown.SetNamedValue(L"future", winrt::Windows::Data::Json::JsonValue::CreateStringValue(L"kept"));
    original.Lyrics->Unknown = unknown;
    NormalizeSequence(original);

    auto const text = WriteSequenceJson(original);

    Sequence read{};
    auto const result = ReadSequenceJson(text, read);
    VERIFY_IS_TRUE(result.Succeeded());
    VERIFY_ARE_EQUAL(size_t{ 0 }, result.SkippedItems);
    VERIFY_IS_TRUE(read.Lyrics.has_value());
    VERIFY_ARE_EQUAL(std::wstring{ L"en" }, read.Lyrics->Language);
    VERIFY_ARE_EQUAL(std::wstring{ L"t-voice" }, read.Lyrics->TrackId);
    VERIFY_IS_TRUE(original.Lyrics->Syllables == read.Lyrics->Syllables);
    VERIFY_IS_TRUE(read.Lyrics->Unknown != nullptr && read.Lyrics->Unknown.HasKey(L"future"));
    VERIFY_ARE_EQUAL(text, WriteSequenceJson(read));

    // Anything malformed is left out and counted.
    auto const root = winrt::Windows::Data::Json::JsonObject::Parse(
        LR"({"lyrics":{"language":"en<script>","syllables":[[0,"ok"],["bad"],[1e300,"far","line"],[10,5],"no",[-4,"before\u0007"]]}})");

    size_t skipped{ 0 };
    auto const lyrics = ReadLyricsJson(root, SequenceLimits{}, skipped);
    VERIFY_IS_TRUE(lyrics.has_value());
    VERIFY_ARE_EQUAL(size_t{ 3 }, skipped);
    VERIFY_ARE_EQUAL(std::wstring{ L"enscript" }, lyrics->Language);
    VERIFY_ARE_EQUAL(size_t{ 3 }, lyrics->Syllables.size());
    VerifySyllable(lyrics->Syllables[0], 0, L"ok", LyricEnd::None);
    VerifySyllable(lyrics->Syllables[1], 0, L"before", LyricEnd::None);
    VerifySyllable(lyrics->Syllables[2], SequenceLimits{}.MaximumTick, L"far", LyricEnd::Line);

    skipped = 0;
    VERIFY_IS_FALSE(ReadLyricsJson(winrt::Windows::Data::Json::JsonObject::Parse(LR"({"lyrics":"words"})"), SequenceLimits{}, skipped).has_value());
    VERIFY_ARE_EQUAL(size_t{ 1 }, skipped);
}

void LyricsTests::StandardMidiFilesKeepLyrics()
{
    auto sequence = SungSequence();

    // On the voice's notes, which start a bar in.
    auto syllables = HappyBirthday();

    for (auto& syllable : syllables)
    {
        syllable.Tick += Beat * 4;
    }

    sequence.Lyrics = LyricsTrack{ L"", L"t-voice", syllables, nullptr };

    auto const exported = ExportStandardMidiFile(sequence, {}, ClipFileText{});
    VERIFY_IS_TRUE(exported.Succeeded);

    midifile::MidiSequence file{};
    VERIFY_IS_TRUE(midifile::ParseStandardMidiFile(std::span<uint8_t const>{ exported.Bytes }, file).Succeeded());

    size_t lyricEvents{ 0 };

    for (auto const& text : file.TextEvents)
    {
        if (text.Kind == midifile::TextKind::Lyric)
        {
            ++lyricEvents;

            // On the voice's track, the first after the tempo track.
            VERIFY_ARE_EQUAL(uint16_t{ 1 }, text.TrackIndex);
        }
    }

    VERIFY_ARE_EQUAL(syllables.size(), lyricEvents);

    auto const imported = ImportStandardMidiFile(file, L"Song.mid");
    VERIFY_IS_TRUE(imported.Lyrics.has_value());
    VERIFY_IS_TRUE(syllables == imported.Lyrics->Syllables);
    VERIFY_ARE_EQUAL(size_t{ 2 }, imported.Tracks.size());
    VERIFY_ARE_EQUAL(imported.Tracks[0].Id, imported.Lyrics->TrackId);

    // The words stay with the track that sings them when it's left out.
    auto const drumsOnly = ExportStandardMidiFile(sequence, { L"t-drums" }, ClipFileText{});
    midifile::MidiSequence drumsFile{};
    VERIFY_IS_TRUE(midifile::ParseStandardMidiFile(std::span<uint8_t const>{ drumsOnly.Bytes }, drumsFile).Succeeded());

    for (auto const& text : drumsFile.TextEvents)
    {
        VERIFY_IS_TRUE(text.Kind != midifile::TextKind::Lyric);
    }
}

void LyricsTests::LinesBreakWhereTheLyricsSay()
{
    auto syllables = HappyBirthday();
    syllables.push_back(Lyric{ Beat * 3, L"next", LyricEnd::None });

    auto const lines = LyricLines(syllables);
    VERIFY_ARE_EQUAL(size_t{ 3 }, lines.size());
    VERIFY_ARE_EQUAL(size_t{ 4 }, lines[0].Count);
    VERIFY_ARE_EQUAL(Beat * 2, lines[0].EndTick);
    VERIFY_IS_TRUE(lines[1].EndsVerse);
    VERIFY_ARE_EQUAL(Beat * 2, lines[1].StartTick);

    auto const [sung, rest] = LyricLineText(syllables, lines[0], Beat);
    VERIFY_ARE_EQUAL(std::wstring{ L"Happy birth" }, sung);
    VERIFY_ARE_EQUAL(std::wstring{ L"day" }, rest);

    VERIFY_ARE_EQUAL(size_t{ 0 }, LyricLineAtTick(lines, -5));
    VERIFY_ARE_EQUAL(size_t{ 1 }, LyricLineAtTick(lines, Beat * 2 + 1));
    VERIFY_ARE_EQUAL(size_t{ 2 }, LyricLineAtTick(lines, 99999));

    // Lyrics that never say where a line ends still break into lines that fit.
    std::vector<Lyric> endless{};

    for (int64_t i = 0; i < 20; ++i)
    {
        endless.push_back(Lyric{ i * Beat, L"word", LyricEnd::Word });
    }

    VERIFY_ARE_EQUAL(size_t{ 3 }, LyricLines(endless).size());
}

void LyricsTests::RubyIsTypedInBrackets()
{
    // Sky (two syllables, read so-ra) and "wo", then "miru" with its kanji read mi, a word with
    // brackets of its own, and a held syllable that has a reading.
    std::wstring_view const typed{ L"\u7A7A[\u305D\u3089]-\u3092 \u898B[\u307F]-\u308B\n[[x]]-_[\u3089 ]]]" };
    auto const syllables = ParseLyricsText(typed, 1000);

    VERIFY_ARE_EQUAL(size_t{ 6 }, syllables.size());
    VerifySyllable(syllables[0], 0, L"\u7A7A", LyricEnd::None);
    VERIFY_ARE_EQUAL(std::wstring{ L"\u305D\u3089" }, syllables[0].Ruby);
    VerifySyllable(syllables[1], 0, L"\u3092", LyricEnd::Word);
    VERIFY_IS_TRUE(syllables[1].Ruby.empty());
    VERIFY_ARE_EQUAL(std::wstring{ L"\u307F" }, syllables[2].Ruby);
    VerifySyllable(syllables[3], 0, L"\u308B", LyricEnd::Line);
    VerifySyllable(syllables[4], 0, L"[x]", LyricEnd::None);
    VerifySyllable(syllables[5], 0, L"", LyricEnd::None);
    VERIFY_ARE_EQUAL(std::wstring{ L"\u3089 ]" }, syllables[5].Ruby);

    // What the editor shows reads back the same.
    VERIFY_IS_TRUE(syllables == ParseLyricsText(LyricsToText(syllables), 1000));

    // Ruby with nothing in front of it reads the syllable before, as RP-026 has it, and a
    // reading can have spaces in it.
    auto const before = ParseLyricsText(L"mi ca-sa [my house]", 1000);
    VERIFY_ARE_EQUAL(size_t{ 3 }, before.size());
    VerifySyllable(before[2], 0, L"sa", LyricEnd::Word);
    VERIFY_ARE_EQUAL(std::wstring{ L"my house" }, before[2].Ruby);

    // A reading that never closes ends with its line.
    auto const open = ParseLyricsText(L"a[b\nc", 1000);
    VERIFY_ARE_EQUAL(size_t{ 2 }, open.size());
    VERIFY_ARE_EQUAL(std::wstring{ L"b" }, open[0].Ruby);
    VerifySyllable(open[0], 0, L"a", LyricEnd::Line);
    VerifySyllable(open[1], 0, L"c", LyricEnd::None);
}

void LyricsTests::RubyTravelsInFilesAndMessages()
{
    // RP-026: ruby in brackets after the text it reads, or on its own after it.
    std::vector<LyricFileText> const events{
        { 0, "\xE7\xA9\xBA[\xE3\x81\x9D\xE3\x82\x89]" },
        { Beat, "\xE3\x82\x92 " },
        { Beat * 2, "casa" },
        { Beat * 2, "[house] " },
        { Beat * 3, "" },
        { Beat * 3, "[\xE3\x82\x89] " },
    };

    auto const syllables = LyricsFromFileText(events, false, SequenceLimits{});
    VERIFY_ARE_EQUAL(size_t{ 4 }, syllables.size());
    VerifySyllable(syllables[0], 0, L"\u7A7A", LyricEnd::None);
    VERIFY_ARE_EQUAL(std::wstring{ L"\u305D\u3089" }, syllables[0].Ruby);
    VerifySyllable(syllables[2], Beat * 2, L"casa", LyricEnd::Word);
    VERIFY_ARE_EQUAL(std::wstring{ L"house" }, syllables[2].Ruby);
    VerifySyllable(syllables[3], Beat * 3, L"", LyricEnd::Word);
    VERIFY_ARE_EQUAL(std::wstring{ L"\u3089" }, syllables[3].Ruby);

    auto const written = LyricsToFileText(syllables);
    VERIFY_ARE_EQUAL(std::string{ "casa[house] " }, written[2].Text);
    VERIFY_IS_TRUE(syllables == LyricsFromFileText(written, false, SequenceLimits{}));

    // Ruby Data: status 3, ending a word of its own unless its syllable ends a line or a verse.
    auto const ruby = RubyDataPackets(Lyric{ 0, L"x", LyricEnd::None, L"ab" }, 0);
    VERIFY_ARE_EQUAL(size_t{ 1 }, ruby.size());
    VERIFY_ARE_EQUAL(0xD0100203u, ruby[0][0]);
    VERIFY_ARE_EQUAL(0x61622000u, ruby[0][1]);
    VERIFY_ARE_EQUAL(0x61620D00u, RubyDataPackets(Lyric{ 0, L"x", LyricEnd::Line, L"ab" }, 0)[0][1]);
    VERIFY_IS_TRUE(RubyDataPackets(Lyric{ 0, L"x", LyricEnd::Word }, 0).empty());

    auto const language = RubyLanguagePackets(L"ja", 0);
    VERIFY_ARE_EQUAL(0xD0100204u, language[0][0]);
    VERIFY_ARE_EQUAL(0x6A610000u, language[0][1]);

    // The singing track sends each reading with its syllable, and the ruby's language with the
    // lyrics'.
    auto sequence = SungSequence();
    auto sung = HappyBirthday();
    sung[2].Ruby = L"berth";
    sequence.Lyrics = LyricsTrack{ L"en", L"t-voice", sung, nullptr, L"en-fonipa" };

    RenderOptions options{};
    options.Lyrics = true;

    std::vector<RenderedMessage> messages{};
    RenderTrack(sequence, sequence.Tracks[0], 0, Beat * 8, messages, options);
    VERIFY_ARE_EQUAL(size_t{ 1 }, CountLyricMessages(messages, 0x03));
    VERIFY_ARE_EQUAL(size_t{ 1 }, CountLyricMessages(messages, 0x04));

    // The sequence file keeps both.
    NormalizeSequence(sequence);
    auto const text = WriteSequenceJson(sequence);

    Sequence read{};
    VERIFY_IS_TRUE(ReadSequenceJson(text, read).Succeeded());
    VERIFY_IS_TRUE(read.Lyrics.has_value());
    VERIFY_ARE_EQUAL(std::wstring{ L"en-fonipa" }, read.Lyrics->RubyLanguage);
    VERIFY_IS_TRUE(sequence.Lyrics->Syllables == read.Lyrics->Syllables);
    VERIFY_ARE_EQUAL(text, WriteSequenceJson(read));
}

void LyricsTests::ClipFilesBringTheirLyrics()
{
    // Exported from a sequence and read back, the way an imported clip file arrives.
    auto sequence = SungSequence();
    auto sung = HappyBirthday();
    sung[0].Ruby = L"hap";
    sung[3].Text = L"daaaaaaaaaaaaaaaaaay";
    sung.push_back(Lyric{ Beat * 3, L"", LyricEnd::Word });
    sequence.Lyrics = LyricsTrack{ L"en", L"t-voice", sung, nullptr, L"en" };

    auto const file = ExportTrackToClipFile(sequence, sequence.Tracks[0], ClipFileText{});
    auto const bytes = WriteClipFile(file);

    ClipFile read{};
    VERIFY_IS_TRUE(ReadClipFile(bytes, read).Succeeded());

    auto imported = ImportClipFile(read);
    auto& clip = imported.Content;

    // A message on a channel of its own stays in the clip, and so does anything else.
    auto const elsewhere = LyricDataPackets(Lyric{ 0, L"other", LyricEnd::Word }, 0)[0];
    clip.Events.push_back(ClipEvent{ 0, { (elsewhere[0] & 0xFFCFFFFFu) | 0x00020000u, elsewhere[1], elsewhere[2], elsewhere[3] }, 4 });
    SortClip(clip);

    auto const eventsBefore = clip.Events.size();
    auto const lyrics = TakeLyricsFromClip(clip, SequenceLimits{});

    VERIFY_IS_TRUE(lyrics.has_value());
    VERIFY_ARE_EQUAL(std::wstring{ L"en" }, lyrics->Language);
    VERIFY_ARE_EQUAL(std::wstring{ L"en" }, lyrics->RubyLanguage);
    VERIFY_ARE_EQUAL(sung.size(), lyrics->Syllables.size());

    for (size_t i = 0; i < sung.size(); ++i)
    {
        VerifySyllable(lyrics->Syllables[i], sung[i].Tick, sung[i].Text, sung[i].End);
        VERIFY_ARE_EQUAL(sung[i].Ruby, lyrics->Syllables[i].Ruby);
    }

    VERIFY_ARE_EQUAL(size_t{ 1 }, clip.Events.size());
    VERIFY_IS_TRUE(eventsBefore > clip.Events.size());
    VERIFY_ARE_EQUAL(0x00020000u, clip.Events[0].Words[0] & 0x003F0000u);

    // A clip with no lyrics gives none, and loses nothing.
    Clip plain{};
    plain.Events.push_back(ClipEvent{ 0, { 0x40B00700u, 0x80000000u, 0, 0 }, 2 });
    VERIFY_IS_FALSE(TakeLyricsFromClip(plain, SequenceLimits{}).has_value());
    VERIFY_ARE_EQUAL(size_t{ 1 }, plain.Events.size());

    // A Continue or an End with nothing before it is left alone.
    Clip broken{};
    broken.Events.push_back(ClipEvent{ 0, { 0xD0D00201u, 0x61000000u, 0, 0 }, 4 });
    VERIFY_IS_FALSE(TakeLyricsFromClip(broken, SequenceLimits{}).has_value());
}
