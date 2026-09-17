// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

class MidiSequenceReaderTests
    : public WEX::TestClass<MidiSequenceReaderTests>
{
    BEGIN_TEST_CLASS(MidiSequenceReaderTests)
        TEST_CLASS_PROPERTY(L"TestClassification", L"Integration")
        TEST_CLASS_PROPERTY(L"Description", L"Reading Standard MIDI Files through the public API")
    END_TEST_CLASS()

    // ---- what a file contains ----
    TEST_METHOD(ReadsASimpleFile);
    TEST_METHOD(ReadsAMultiTrackFile);
    TEST_METHOD(ReportsTheTempoMap);
    TEST_METHOD(ReportsTheTimeSignatureMapWithRealDenominators);
    TEST_METHOD(ConvertsBetweenTicksAndMicroseconds);
    TEST_METHOD(ReportsBarAndBeatAcrossMeterChanges);

    // ---- text, lyrics and chords ----
    TEST_METHOD(BuildsLyricLinesWhenAFileHasThem);
    TEST_METHOD(OffersNoLyricLinesWhenAFileHasNone);
    TEST_METHOD(ReadsSoftKaraokeWordsFromTextEvents);
    TEST_METHOD(FindsChordSymbolsInEffect);
    TEST_METHOD(OffersNoChordsWhenAFileHasNone);

    // ---- the windowed note path ----
    TEST_METHOD(CountsAndFillsNotesInAWindow);
    TEST_METHOD(FillRespectsTheCallersArraySize);
    TEST_METHOD(FillHonorsTheStartIndex);
    TEST_METHOD(CountsSoundingNotesPerTrack);
    TEST_METHOD(ReturnsNothingForAnInvertedWindow);

    // ---- timing shapes ----
    TEST_METHOD(ReadsSmpteTimedFiles);
    TEST_METHOD(ReadsRunningStatus);
    TEST_METHOD(ReadsSystemMessagesWithoutLosingItsPlace);
    TEST_METHOD(ReadsALongSystemExclusive);

    // ---- refusing and salvaging ----
    TEST_METHOD(RefusesSomethingThatIsNotAMidiFile);
    TEST_METHOD(RefusesAFileWithNothingToPlay);
    TEST_METHOD(SalvagesATruncatedFile);
    TEST_METHOD(SurvivesALyingTrackLength);
    TEST_METHOD(SurvivesALyingTrackCount);
    TEST_METHOD(SurvivesAnAbsurdDeltaTime);
    TEST_METHOD(RefusesAFileLargerThanTheLimit);
    TEST_METHOD(HonorsTheEventCountLimit);
    TEST_METHOD(FailsOnUnreadableDataWhenAsked);
    TEST_METHOD(ReadsFromAStreamAsWellAsAFile);
};
