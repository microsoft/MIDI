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
