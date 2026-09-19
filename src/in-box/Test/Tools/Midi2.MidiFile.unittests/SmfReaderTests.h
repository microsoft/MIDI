// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

#include <WexTestClass.h>

class SmfReaderTests : public WEX::TestClass<SmfReaderTests>
{
public:

    BEGIN_TEST_CLASS(SmfReaderTests)
        TEST_CLASS_PROPERTY(L"TestClassification", L"Unit")
    END_TEST_CLASS()

    // ---- the shape of a file ----
    TEST_METHOD(ReadsHeaderAndASingleNote);
    TEST_METHOD(MergesTracksIntoOneTimeline);
    TEST_METHOD(SkipsUnknownChunksBetweenTracks);
    TEST_METHOD(UnwrapsRiffWrappedFiles);
    TEST_METHOD(LaysMultiSequenceFilesEndToEnd);

    // ---- message decoding ----
    TEST_METHOD(ExpandsRunningStatus);
    TEST_METHOD(TreatsNoteOnWithZeroVelocityAsANoteOff);
    TEST_METHOD(PairsOverlappingCopiesOfTheSameNote);
    TEST_METHOD(HoldsANoteWhoseEndIsMissing);
    TEST_METHOD(KeepsSystemExclusiveWhole);
    TEST_METHOD(ReassemblesSplitSystemExclusive);
    TEST_METHOD(RecordsBankWithProgramChange);
    TEST_METHOD(ReadsChordSymbolsFromSystemExclusive);
    TEST_METHOD(IgnoresSystemExclusiveThatIsNotAChordSymbol);

    // ---- timing ----
    TEST_METHOD(ConvertsTicksToTimeAtOneTempo);
    TEST_METHOD(AccumulatesMultipleTempoChanges);
    TEST_METHOD(RoundTripsTicksAndTime);
    TEST_METHOD(CountsBarsAndBeats);
    TEST_METHOD(UsesRealTimeForSmpteDivision);

    // ---- text ----
    TEST_METHOD(TakesTrackNameFromMetaEvent);
    TEST_METHOD(KeepsLyricsInOrder);
    TEST_METHOD(FindsTheChordSymbolInEffectAtATick);
    TEST_METHOD(BuildsLyricLinesFromTheMarkersAFileCarries);
    TEST_METHOD(BuildsLyricLinesFromTimingWhenNoMarkersExist);
    TEST_METHOD(ReadsSoftKaraokeLyricsFromPlainTextEvents);
    TEST_METHOD(FindsNoLyricLinesInAFileWithOnlyPlainText);

    // ---- what is sounding ----
    TEST_METHOD(CountsNotesSoundingPerTrack);

    // ---- the bar and beat grid on the note display ----
    TEST_METHOD(PlacesBarAndBeatLinesForASimpleMeter);
    TEST_METHOD(RealignsBarLinesAfterAMeterChange);
    TEST_METHOD(DropsBeatLinesWhenOnlyBarsAreWanted);
    TEST_METHOD(StopsAtTheGridLineLimit);

    // ---- system messages inside a track ----
    TEST_METHOD(KeepsItsPlaceAcrossASystemRealTimeByte);
    TEST_METHOD(ReadsTheSystemCommonMessageLengths);
    TEST_METHOD(CancelsRunningStatusOnSystemCommon);

    // ---- untrusted input ----
    TEST_METHOD(RejectsSomethingThatIsNotAMidiFile);
    TEST_METHOD(RejectsATruncatedHeader);
    TEST_METHOD(ClampsATrackLengthThatOverrunsTheFile);
    TEST_METHOD(KeepsWhatItReadFromATruncatedTrack);
    TEST_METHOD(StopsOnADataByteWithNoStatus);
    TEST_METHOD(StopsWhenADeltaTimeIsTooLargeToBeMusic);
    TEST_METHOD(ClampsAnOutOfRangeDataByteInsteadOfGivingUp);
    TEST_METHOD(RejectsAnOverlongVariableLengthQuantity);
    TEST_METHOD(RejectsAFileWithNoPlayableData);
    TEST_METHOD(HonorsTheEventCountLimit);
    TEST_METHOD(SurvivesEveryTruncationOfAValidFile);
    TEST_METHOD(SurvivesCorruptedBytesInAValidFile);

    // ---- real files ----
    TEST_METHOD(ReadsTheSampleFilesShippedWithWindows);

    // Opt in: point MIDI_PLAYER_TEST_CORPUS at a folder of real files. Inert without it, so a
    // normal test pass does not depend on what happens to be installed on the machine.
    TEST_METHOD(ReadsACorpusOfRealFiles);

    // Opt in: point MIDI_PLAYER_TEST_FILE at one file. For looking at whatever the corpus run
    // reported as an outlier.
    TEST_METHOD(ReadsOneNamedFile);
};
