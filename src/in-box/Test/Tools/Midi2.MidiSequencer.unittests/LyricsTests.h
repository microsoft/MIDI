// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

#include <WexTestClass.h>

// The lyrics track: typing it, placing it on notes, and carrying it in Standard MIDI Files, MIDI
// 2.0 Lyric Data messages, LRC files and the sequence file.
class LyricsTests : public WEX::TestClass<LyricsTests>
{
public:

    BEGIN_TEST_CLASS(LyricsTests)
        TEST_CLASS_PROPERTY(L"TestClassification", L"Unit")
    END_TEST_CLASS()

    TEST_METHOD(TypedWordsSplitIntoSyllables);
    TEST_METHOD(MelismasAndHyphensSurviveARoundTrip);
    TEST_METHOD(SyllablesGoOnTheSingersNotes);
    TEST_METHOD(WithNoSingerSyllablesGoABeatApart);
    TEST_METHOD(LyricEventsReadTheWayRp017Says);
    TEST_METHOD(KaraokeFilesRead);
    TEST_METHOD(OlderCharacterSetsRead);
    TEST_METHOD(LyricDataFollowsTheSpec);
    TEST_METHOD(TheSingingTrackSendsTheLyrics);
    TEST_METHOD(LrcHasALinePerLine);
    TEST_METHOD(TheSequenceFileKeepsLyrics);
    TEST_METHOD(StandardMidiFilesKeepLyrics);
    TEST_METHOD(LinesBreakWhereTheLyricsSay);
};
