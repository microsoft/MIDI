// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#include "SmfTextEncodingTests.h"
#include "SmfTestFileBuilder.h"

#include "midi_file_smf_reader.h"

#include <windows.h>

#include <initializer_list>
#include <string>
#include <vector>

using namespace WEX::Common;
using namespace WEX::Logging;
using namespace WEX::TestExecution;

using namespace midifile;
using namespace smftest;

namespace
{
    constexpr uint16_t TicksPerQuarterNote = 480;

    constexpr uint8_t MetaText = 0x01;
    constexpr uint8_t MetaCopyright = 0x02;
    constexpr uint8_t MetaTrackName = 0x03;
    constexpr uint8_t MetaInstrumentName = 0x04;
    constexpr uint8_t MetaLyric = 0x05;

    // Shift-JIS (code page 932) bytes, written out by hand so the test doesn't depend on the
    // encoder it is checking.
    // 東京の夜
    std::initializer_list<uint8_t> const ShiftJisTokyoNight{ 0x93, 0x8C, 0x8B, 0x9E, 0x82, 0xCC, 0x96, 0xE9 };
    // ピアノ: katakana only, and every trail byte is in the ASCII range
    std::initializer_list<uint8_t> const ShiftJisPiano{ 0x83, 0x73, 0x83, 0x41, 0x83, 0x6D };
    // (C) 山田 太郎
    std::initializer_list<uint8_t> const ShiftJisCopyright{ 0x28, 0x43, 0x29, 0x20, 0x8E, 0x52, 0x93, 0x63, 0x20, 0x91, 0xBE, 0x98, 0x59 };
    // さく and ら
    std::initializer_list<uint8_t> const ShiftJisSaku{ 0x82, 0xB3, 0x82, 0xAD };
    std::initializer_list<uint8_t> const ShiftJisRa{ 0x82, 0xE7 };
    // ｱｲｳ: half-width katakana, one byte each, and the same bytes as Latin-1 "±²³"
    std::initializer_list<uint8_t> const ShiftJisHalfWidthAiu{ 0xB1, 0xB2, 0xB3 };

    std::string Bytes(std::initializer_list<uint8_t> bytes)
    {
        return std::string(bytes.begin(), bytes.end());
    }

    std::wstring Wide(std::string const& utf8)
    {
        if (utf8.empty())
        {
            return {};
        }

        auto const length = ::MultiByteToWideChar(
            CP_UTF8, MB_ERR_INVALID_CHARS, utf8.data(), static_cast<int>(utf8.size()), nullptr, 0);

        VERIFY_IS_TRUE(length > 0, L"the reader produced valid UTF-8");

        std::wstring wide(static_cast<size_t>(length), L'\0');

        ::MultiByteToWideChar(
            CP_UTF8, MB_ERR_INVALID_CHARS, utf8.data(), static_cast<int>(utf8.size()), wide.data(), length);

        return wide;
    }

    void VerifyText(wchar_t const* expected, std::string const& actual)
    {
        VERIFY_ARE_EQUAL(String(expected), String(Wide(actual).c_str()));
    }

    TrackBuilder& AddNote(TrackBuilder& track)
    {
        return track.NoteOn(0, 0, 60, 100).NoteOff(480, 0, 60, 0);
    }

    MidiSequence ParseOneTrack(TrackBuilder& track, ReadLimits const& limits = {})
    {
        AddNote(track).EndOfTrack();

        auto const file = BuildFile(0, TicksPerQuarterNote, { track });

        MidiSequence sequence{};
        auto const result = ParseStandardMidiFile(std::span<uint8_t const>{ file.data(), file.size() }, sequence, limits);

        VERIFY_IS_TRUE(result.Succeeded());

        return sequence;
    }
}

void SmfTextEncodingTests::DecodesShiftJisNamesAndCopyright()
{
    TrackBuilder track{};

    track
        .MetaText(0, MetaTrackName, Bytes(ShiftJisTokyoNight))
        .MetaText(0, MetaInstrumentName, Bytes(ShiftJisPiano))
        .MetaText(0, MetaCopyright, Bytes(ShiftJisCopyright))
        .MetaText(0, MetaLyric, Bytes(ShiftJisSaku));

    auto const sequence = ParseOneTrack(track);

    VerifyText(L"東京の夜", sequence.Tracks[0].Name);
    VerifyText(L"ピアノ", sequence.Tracks[0].InstrumentName);
    VerifyText(L"(C) 山田 太郎", sequence.Copyright);
    VerifyText(L"東京の夜", sequence.Title);

    bool foundLyric = false;

    for (auto const& text : sequence.TextEvents)
    {
        if (text.Kind == TextKind::TrackName)
        {
            VerifyText(L"東京の夜", text.Text);
        }
        else if (text.Kind == TextKind::Lyric)
        {
            VerifyText(L"さく", text.Text);
            foundLyric = true;
        }
    }

    VERIFY_IS_TRUE(foundLyric);
}

void SmfTextEncodingTests::DecodesShiftJisKaraokeLyrics()
{
    TrackBuilder track{};

    track
        .MetaText(0, MetaText, "@KMIDI KARAOKE FILE")
        .MetaText(0, MetaText, Bytes(ShiftJisSaku))
        .MetaText(120, MetaText, Bytes(ShiftJisRa));

    auto const sequence = ParseOneTrack(track);

    VERIFY_IS_TRUE(sequence.IsKaraoke);

    std::vector<std::wstring> words{};

    for (auto const& text : sequence.TextEvents)
    {
        if (text.Kind == TextKind::Text)
        {
            words.push_back(Wide(text.Text));
        }
    }

    VERIFY_ARE_EQUAL(size_t{ 3 }, words.size());
    VERIFY_ARE_EQUAL(String(L"さく"), String(words[1].c_str()));
    VERIFY_ARE_EQUAL(String(L"ら"), String(words[2].c_str()));
}

void SmfTextEncodingTests::DecodesShiftJisNextToAsciiText()
{
    TrackBuilder conductor{};

    conductor
        .MetaText(0, MetaTrackName, "Piano")
        .MetaText(0, MetaCopyright, "(C) 2026 Contoso")
        .Tempo(0, 500000)
        .EndOfTrack(960);

    TrackBuilder music{};

    music.MetaText(0, MetaTrackName, Bytes(ShiftJisPiano));
    AddNote(music).EndOfTrack();

    auto const file = BuildFile(1, TicksPerQuarterNote, { conductor, music });

    MidiSequence sequence{};
    VERIFY_IS_TRUE(ParseStandardMidiFile(std::span<uint8_t const>{ file.data(), file.size() }, sequence).Succeeded());

    VerifyText(L"Piano", sequence.Tracks[0].Name);
    VerifyText(L"ピアノ", sequence.Tracks[1].Name);
    VerifyText(L"(C) 2026 Contoso", sequence.Copyright);
    VerifyText(L"Piano", sequence.Title);
}

void SmfTextEncodingTests::DecodesHalfWidthKatakanaWhenTheRestOfTheFileIsShiftJis()
{
    // On its own the track name could be Latin-1. The copyright settles it.
    TrackBuilder track{};

    track
        .MetaText(0, MetaTrackName, Bytes(ShiftJisHalfWidthAiu))
        .MetaText(0, MetaCopyright, Bytes(ShiftJisCopyright));

    auto const sequence = ParseOneTrack(track);

    VerifyText(L"ｱｲｳ", sequence.Tracks[0].Name);
    VerifyText(L"(C) 山田 太郎", sequence.Copyright);
}

void SmfTextEncodingTests::KeepsUtf8JapaneseAsUtf8()
{
    // The source is compiled as UTF-8, so these literals are UTF-8 bytes.
    TrackBuilder track{};

    track
        .MetaText(0, MetaTrackName, "東京の夜")
        .MetaText(0, MetaCopyright, "(C) 山田 太郎")
        .MetaText(0, MetaLyric, "さく");

    auto const sequence = ParseOneTrack(track);

    VerifyText(L"東京の夜", sequence.Tracks[0].Name);
    VerifyText(L"(C) 山田 太郎", sequence.Copyright);
}

void SmfTextEncodingTests::KeepsLatin1Text()
{
    TrackBuilder track{};

    track
        .MetaText(0, MetaTrackName, Bytes({ 0x43, 0x61, 0x66, 0xE9 }))                  // Café
        .MetaText(0, MetaCopyright, Bytes({ 0x28, 0x43, 0x29, 0x20, 0x4D, 0xFC, 0x6C, 0x6C, 0x65, 0x72 }))   // (C) Müller
        .MetaText(0, MetaInstrumentName, Bytes({ 0xC0, 0xC9, 0xCE }));                  // ÀÉÎ

    auto const sequence = ParseOneTrack(track);

    VerifyText(L"Café", sequence.Tracks[0].Name);
    VerifyText(L"(C) Müller", sequence.Copyright);
    VerifyText(L"ÀÉÎ", sequence.Tracks[0].InstrumentName);
}

void SmfTextEncodingTests::KeepsLatin1TextThatIsAlsoValidShiftJis()
{
    // Both decode cleanly as Shift-JIS, to kanji, because a lowercase accented letter in Latin-1
    // is also a Shift-JIS lead byte. Nothing in them looks like Japanese text, though.
    TrackBuilder track{};

    track
        .MetaText(0, MetaTrackName, Bytes({ 0x44, 0xE9, 0x73, 0x69, 0x72, 0xE9, 0x65 }))     // Désirée
        .MetaText(0, MetaCopyright, Bytes({ 0x43, 0x61, 0x6E, 0xE7, 0xE3, 0x6F }));          // Canção

    auto const sequence = ParseOneTrack(track);

    VerifyText(L"Désirée", sequence.Tracks[0].Name);
    VerifyText(L"Canção", sequence.Copyright);
}

void SmfTextEncodingTests::KeepsShortCapitalAccentedLatin1AsLatin1()
{
    // These bytes are also valid half-width katakana (ﾀﾉﾎ). With nothing else in the file to say
    // it is Japanese, Latin-1 wins.
    TrackBuilder track{};

    track.MetaText(0, MetaTrackName, Bytes({ 0xC0, 0xC9, 0xCE }));

    auto const sequence = ParseOneTrack(track);

    VerifyText(L"ÀÉÎ", sequence.Tracks[0].Name);
}

void SmfTextEncodingTests::KeepsLatin1WhenAnyTextIsNotValidShiftJis()
{
    // "Café" ends in a Shift-JIS lead byte with no trail byte, so the file can't be Shift-JIS,
    // and every text keeps the decoding the reader has always given it.
    TrackBuilder track{};

    track
        .MetaText(0, MetaTrackName, Bytes(ShiftJisTokyoNight))
        .MetaText(0, MetaCopyright, Bytes({ 0x43, 0x61, 0x66, 0xE9 }));

    auto const sequence = ParseOneTrack(track);

    VerifyText(L"\x0093\x008C\x008B\x009E\x0082\x00CC\x0096\x00E9", sequence.Tracks[0].Name);
    VerifyText(L"Café", sequence.Copyright);
}

void SmfTextEncodingTests::KeepsDosAccentedLettersOutOfShiftJis()
{
    // German from an old DOS file (code page 437, where ü is 0x81 and Ü is 0x9A). Every word is
    // also valid Shift-JIS, but each pair sits inside a word, so none of it counts as Japanese
    // and the reader decodes these the way it always has.
    TrackBuilder track{};

    track
        .MetaText(0, MetaTrackName, Bytes({ 0x4D, 0x81, 0x64, 0x65 }))                  // Müde
        .MetaText(0, MetaInstrumentName, Bytes({ 0x46, 0x6C, 0x81, 0x67, 0x65, 0x6C })) // Flügel
        .MetaText(0, MetaCopyright, Bytes({ 0x9A, 0x62, 0x65, 0x72 }));                 // Über

    auto const sequence = ParseOneTrack(track);

    VerifyText(L"M\x0081" L"de", sequence.Tracks[0].Name);
    VerifyText(L"Fl\x0081" L"gel", sequence.Tracks[0].InstrumentName);
    VerifyText(L"\x009A" L"ber", sequence.Copyright);
}

void SmfTextEncodingTests::KeepsCurlyQuotesOutOfShiftJis()
{
    // Windows-1252 puts the curly apostrophe at 0x92, which with the next letter is also a valid
    // Shift-JIS kanji.
    TrackBuilder track{};

    track.MetaText(0, MetaTrackName, Bytes({ 0x44, 0x6F, 0x6E, 0x92, 0x74, 0x20, 0x53, 0x74, 0x6F, 0x70 })); // Don’t Stop

    auto const sequence = ParseOneTrack(track);

    VerifyText(L"Don\x0092" L"t Stop", sequence.Tracks[0].Name);
}

void SmfTextEncodingTests::DropsAShiftJisCharacterCutByTheTextLimit()
{
    ReadLimits limits{};
    limits.MaximumTextBytes = 7;

    // The limit cuts 夜 in half.
    TrackBuilder track{};

    track.MetaText(0, MetaTrackName, Bytes(ShiftJisTokyoNight));

    auto const sequence = ParseOneTrack(track, limits);

    VerifyText(L"東京の", sequence.Tracks[0].Name);
}

void SmfTextEncodingTests::DropsAUtf8CharacterCutByTheTextLimit()
{
    ReadLimits limits{};
    limits.MaximumTextBytes = 7;

    // 東京 is six bytes in UTF-8, and the limit keeps one byte of の.
    TrackBuilder track{};

    track.MetaText(0, MetaTrackName, "東京の夜");

    auto const sequence = ParseOneTrack(track, limits);

    VerifyText(L"東京", sequence.Tracks[0].Name);
}

namespace
{
    std::vector<std::wstring> Lyrics(MidiSequence const& sequence)
    {
        std::vector<std::wstring> lyrics{};

        for (auto const& text : sequence.TextEvents)
        {
            if (text.Kind == TextKind::Lyric)
            {
                lyrics.push_back(Wide(text.Text));
            }
        }

        return lyrics;
    }

    void VerifyLyrics(std::initializer_list<wchar_t const*> expected, MidiSequence const& sequence)
    {
        auto const lyrics = Lyrics(sequence);

        VERIFY_ARE_EQUAL(expected.size(), lyrics.size());

        size_t index = 0;

        for (auto const text : expected)
        {
            VERIFY_ARE_EQUAL(String(text), String(lyrics[index++].c_str()));
        }
    }
}

void SmfTextEncodingTests::ReadsLyricsAfterAJapaneseCodeSetTag()
{
    // Half-width katakana alone isn't enough evidence for a guess (see
    // KeepsShortCapitalAccentedLatin1AsLatin1), but the tag says what it is. The tag itself is
    // not a lyric.
    TrackBuilder track{};

    track
        .MetaText(0, MetaLyric, "{@JP}")
        .MetaText(0, MetaLyric, Bytes(ShiftJisHalfWidthAiu))
        .MetaText(120, MetaLyric, Bytes(ShiftJisSaku));

    auto const sequence = ParseOneTrack(track);

    VerifyLyrics({ L"ｱｲｳ", L"さく" }, sequence);
}

void SmfTextEncodingTests::ReadsATagAndTextInTheSameLyric()
{
    TrackBuilder track{};

    track.MetaText(0, MetaLyric, "{@JP}" + Bytes(ShiftJisSaku));

    auto const sequence = ParseOneTrack(track);

    VerifyLyrics({ L"さく" }, sequence);
}

void SmfTextEncodingTests::AcceptsTheCodeSetNameInAnyCase()
{
    TrackBuilder track{};

    track
        .MetaText(0, MetaLyric, "{@jp}" + Bytes(ShiftJisHalfWidthAiu))
        .MetaText(120, MetaLyric, "{@Latin}" + Bytes(ShiftJisHalfWidthAiu))
        .MetaText(120, MetaLyric, "{@Jp}" + Bytes(ShiftJisHalfWidthAiu));

    auto const sequence = ParseOneTrack(track);

    VerifyLyrics({ L"ｱｲｳ", L"±²³", L"ｱｲｳ" }, sequence);
}

void SmfTextEncodingTests::SwitchesCodeSetsPartWayThrough()
{
    TrackBuilder track{};

    track
        .MetaText(0, MetaLyric, "{@JP}")
        .MetaText(0, MetaLyric, Bytes(ShiftJisSaku))
        .MetaText(120, MetaLyric, "{@LATIN}")
        .MetaText(0, MetaLyric, Bytes({ 0x43, 0x61, 0x66, 0xE9 }))                  // Café
        .MetaText(120, MetaLyric, "{@JP}")
        .MetaText(0, MetaLyric, Bytes(ShiftJisRa));

    auto const sequence = ParseOneTrack(track);

    VerifyLyrics({ L"さく", L"Café", L"ら" }, sequence);
}

void SmfTextEncodingTests::ReadsLatinTaggedLyricsAsWindows1252()
{
    // ANSI puts curly quotes at 0x93 and 0x94, where Latin-1 has control codes.
    TrackBuilder track{};

    track
        .MetaText(0, MetaLyric, "{@LATIN}")
        .MetaText(0, MetaLyric, Bytes({ 0x93, 0x48, 0x69, 0x94 }));

    auto const sequence = ParseOneTrack(track);

    VerifyLyrics({ L"\x201CHi\x201D" }, sequence);
}

void SmfTextEncodingTests::UsesAJapaneseTagForNamesWithoutOtherEvidence()
{
    // Untagged, a track name of half-width katakana stays Latin-1. A {@JP} lyric tag settles it.
    TrackBuilder track{};

    track
        .MetaText(0, MetaTrackName, Bytes(ShiftJisHalfWidthAiu))
        .MetaText(0, MetaLyric, "{@JP}")
        .MetaText(0, MetaLyric, Bytes(ShiftJisSaku));

    auto const sequence = ParseOneTrack(track);

    VerifyText(L"ｱｲｳ", sequence.Tracks[0].Name);
}

void SmfTextEncodingTests::UsesALatinTagForNamesThatLookLikeShiftJis()
{
    // 0x93 0x8C 0x8B 0x9E is 東京 in Shift-JIS and would be guessed as such without a tag.
    TrackBuilder track{};

    track
        .MetaText(0, MetaTrackName, Bytes({ 0x93, 0x8C, 0x8B, 0x9E }))
        .MetaText(0, MetaLyric, "{@LATIN}la");

    auto const sequence = ParseOneTrack(track);

    VerifyText(L"\x201C\x0152\x2039\x017E", sequence.Tracks[0].Name);
    VerifyLyrics({ L"la" }, sequence);
}

void SmfTextEncodingTests::KeepsUtf8NamesInATaggedFile()
{
    TrackBuilder track{};

    track
        .MetaText(0, MetaTrackName, "東京の夜")
        .MetaText(0, MetaLyric, "{@JP}" + Bytes(ShiftJisSaku));

    auto const sequence = ParseOneTrack(track);

    VerifyText(L"東京の夜", sequence.Tracks[0].Name);
    VerifyLyrics({ L"さく" }, sequence);
}

void SmfTextEncodingTests::DropsNonAsciiLyricsInAnUndefinedCodeSet()
{
    // RP-026: lyrics in an undefined code set are ignored until a defined one appears. Plain
    // ASCII reads the same in any code set, so it stays.
    TrackBuilder track{};

    track
        .MetaText(0, MetaLyric, "{@KR}")
        .MetaText(0, MetaLyric, Bytes({ 0xC7, 0xD1, 0xB1, 0xDB }))
        .MetaText(120, MetaLyric, "la")
        .MetaText(120, MetaLyric, "{@JP}" + Bytes(ShiftJisRa));

    auto const sequence = ParseOneTrack(track);

    VerifyLyrics({ L"la", L"ら" }, sequence);
}

void SmfTextEncodingTests::LeavesSomethingThatOnlyLooksLikeATagAlone()
{
    TrackBuilder track{};

    track
        .MetaText(0, MetaLyric, "{@JP")
        .MetaText(0, MetaTrackName, "{@JP}");

    auto const sequence = ParseOneTrack(track);

    VerifyLyrics({ L"{@JP" }, sequence);
    VerifyText(L"{@JP}", sequence.Tracks[0].Name);
}

void SmfTextEncodingTests::ReadsTextThatStartsWithAByteOrderMark()
{
    TrackBuilder track{};

    track
        .MetaText(0, MetaTrackName, Bytes({ 0xFE, 0xFF, 0x30, 0x42, 0x30, 0x44 }))      // あい, big endian
        .MetaText(0, MetaCopyright, Bytes({ 0xEF, 0xBB, 0xBF, 0x43, 0x61, 0x66, 0xC3, 0xA9 }))   // Café, UTF-8
        .MetaText(0, MetaLyric, "{@JP}")
        .MetaText(0, MetaLyric, Bytes({ 0xFF, 0xFE, 0x42, 0x30 }));                     // あ, little endian

    auto const sequence = ParseOneTrack(track);

    VerifyText(L"あい", sequence.Tracks[0].Name);
    VerifyText(L"Café", sequence.Copyright);
    VerifyLyrics({ L"あ" }, sequence);
}

namespace
{
    // The bytes of three real files written in GBK, and of one written in Shift-JIS, rebuilt
    // here rather than shipping the files. Each GBK file carries the same text event and an ASCII
    // copyright alongside its title.
    // 【主页】 URL
    std::initializer_list<uint8_t> const GbkHomePage{ 0xA1, 0xBE, 0xD6, 0xF7, 0xD2, 0xB3, 0xA1, 0xBF, 0x20, 0x55, 0x52, 0x4C };
    // アンノウン · マザーグース
    std::initializer_list<uint8_t> const GbkUnknownMotherGoose{
        0xA5, 0xA2, 0xA5, 0xF3, 0xA5, 0xCE, 0xA5, 0xA6, 0xA5, 0xF3, 0x20, 0xA1, 0xA4, 0x20,
        0xA5, 0xDE, 0xA5, 0xB6, 0xA9, 0x60, 0xA5, 0xB0, 0xA9, 0x60, 0xA5, 0xB9 };
    // 千本桜, which Shift-JIS reads as ﾇｧｱｾ諭
    std::initializer_list<uint8_t> const GbkSenbonzakura{ 0xC7, 0xA7, 0xB1, 0xBE, 0x97, 0x40 };
    // 紅一葉, which Shift-JIS reads as ｼtﾒｻﾈ~
    std::initializer_list<uint8_t> const GbkKurenaiHitoha{ 0xBC, 0x74, 0xD2, 0xBB, 0xC8, 0x7E };

    // 【サイト名】 URL and ---- 全体設定 ----
    std::initializer_list<uint8_t> const ShiftJisSiteName{ 0x81, 0x79, 0x83, 0x54, 0x83, 0x43, 0x83, 0x67, 0x96, 0xBC, 0x81, 0x7A, 0x20, 0x55, 0x52, 0x4C };
    std::initializer_list<uint8_t> const ShiftJisOverallSettings{ 0x2D, 0x2D, 0x2D, 0x2D, 0x20, 0x91, 0x53, 0x91, 0xCC, 0x90, 0xDD, 0x92, 0xE8, 0x20, 0x2D, 0x2D, 0x2D, 0x2D };

    MidiSequence ParseGbkStyleFile(std::initializer_list<uint8_t> title)
    {
        TrackBuilder track{};

        track
            .MetaText(0, MetaTrackName, Bytes(title))
            .MetaText(0, MetaText, Bytes(GbkHomePage))
            .MetaText(0, MetaCopyright, "2025 Nikiyiki Music Media No Corporation");

        return ParseOneTrack(track);
    }
}

void SmfTextEncodingTests::DecodesAGbkTitleOfKatakana()
{
    // Not valid Shift-JIS at all.
    auto const sequence = ParseGbkStyleFile(GbkUnknownMotherGoose);

    VerifyText(L"アンノウン · マザーグース", sequence.Tracks[0].Name);
    VerifyText(L"2025 Nikiyiki Music Media No Corporation", sequence.Copyright);
}

void SmfTextEncodingTests::DecodesAGbkTitleThatIsAlsoValidShiftJis()
{
    auto const sequence = ParseGbkStyleFile(GbkSenbonzakura);

    VerifyText(L"千本桜", sequence.Tracks[0].Name);
}

void SmfTextEncodingTests::DecodesAGbkTitleThatShiftJisReadsAsHalfWidthKatakana()
{
    auto const sequence = ParseGbkStyleFile(GbkKurenaiHitoha);

    VerifyText(L"紅一葉", sequence.Tracks[0].Name);
}

void SmfTextEncodingTests::KeepsAShiftJisFileThatIsAlsoValidGbk()
{
    // Every one of these bytes is also clean GBK, as rare characters from its extension.
    TrackBuilder track{};

    track
        .MetaText(0, MetaText, Bytes(ShiftJisSiteName))
        .MetaText(0, 0x06, Bytes(ShiftJisOverallSettings));

    auto const sequence = ParseOneTrack(track);

    bool foundMarker = false;

    for (auto const& text : sequence.TextEvents)
    {
        if (text.Kind == TextKind::Marker)
        {
            VerifyText(L"---- 全体設定 ----", text.Text);
            foundMarker = true;
        }
    }

    VERIFY_IS_TRUE(foundMarker);
}

void SmfTextEncodingTests::KeepsAShortLatin1PairThatIsAlsoValidGbk()
{
    // ÀÉ is also the GBK hanzi 郎. One character is not enough to believe GBK.
    TrackBuilder track{};

    track.MetaText(0, MetaTrackName, Bytes({ 0xC0, 0xC9 }));

    auto const sequence = ParseOneTrack(track);

    VerifyText(L"ÀÉ", sequence.Tracks[0].Name);
}

void SmfTextEncodingTests::LetsAJapaneseTagWinOverGbk()
{
    TrackBuilder track{};

    track
        .MetaText(0, MetaTrackName, Bytes(GbkSenbonzakura))
        .MetaText(0, MetaLyric, "{@JP}la");

    auto const sequence = ParseOneTrack(track);

    VerifyText(L"ﾇｧｱｾ諭", sequence.Tracks[0].Name);
}

void SmfTextEncodingTests::KeepsHalfWidthKatakanaWithVoicingMarksAsShiftJis()
{
    // Older Japanese files often use only half-width katakana. Every byte here is also clean
    // GBK, as common hanzi, but ﾌﾟ and ﾄﾞ only make sense in Japanese. From a real file.
    TrackBuilder track{};

    track.MetaText(0, MetaTrackName,
        "FINAL FANTASY " + Bytes({ 0xA2, 0xCC, 0xDF, 0xDA, 0xD8, 0xAD, 0xB0, 0xC4, 0xDE, 0xA3 }));

    auto const sequence = ParseOneTrack(track);

    VerifyText(L"FINAL FANTASY ｢ﾌﾟﾚﾘｭｰﾄﾞ｣", sequence.Tracks[0].Name);

    TrackBuilder town{};

    // ｢ﾏﾁ｣ has no voicing mark, but the brackets around kana are just as Japanese.
    town.MetaText(0, MetaTrackName, "FINAL FANTASY " + Bytes({ 0xA2, 0xCF, 0xC1, 0xA3 }));

    auto const townSequence = ParseOneTrack(town);

    VerifyText(L"FINAL FANTASY ｢ﾏﾁ｣", townSequence.Tracks[0].Name);
}

void SmfTextEncodingTests::KeepsAKoreanCopyrightSignOutOfGbk()
{
    // The © of Korean code page 949 is 0xA8 0xCF, which GBK reads as the bopomofo ㄏ. This
    // reader doesn't read Korean, so the text stays as it always was.
    TrackBuilder track{};

    track.MetaText(0, MetaCopyright, "Copyright " + Bytes({ 0xA8, 0xCF }) + " 1999 by Contoso");

    auto const sequence = ParseOneTrack(track);

    VerifyText(L"Copyright ¨Ï 1999 by Contoso", sequence.Copyright);
}
