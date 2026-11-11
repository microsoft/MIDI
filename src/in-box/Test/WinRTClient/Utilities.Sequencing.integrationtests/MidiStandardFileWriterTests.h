// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

class MidiStandardFileWriterTests
    : public WEX::TestClass<MidiStandardFileWriterTests>
{
    BEGIN_TEST_CLASS(MidiStandardFileWriterTests)
        TEST_CLASS_PROPERTY(L"TestClassification", L"Integration")
        TEST_CLASS_PROPERTY(L"Description", L"Writing a sequence back out as a Standard MIDI File")
    END_TEST_CLASS()

    // ---- round trip through the reader ----
    TEST_METHOD(RoundTripsEveryTestFile);
    TEST_METHOD(RoundTripsNotesExactly);
    TEST_METHOD(RoundTripsTempoAndMeterChanges);
    TEST_METHOD(RoundTripsTrackNamesAndInstruments);
    TEST_METHOD(RoundTripsLyricsAndMarkers);
    TEST_METHOD(RoundTripsChordSymbolsCarriedInSystemExclusive);
    TEST_METHOD(RoundTripsSmpteTiming);
    TEST_METHOD(WritingTwiceGivesTheSameBytes);

    // ---- options ----
    TEST_METHOD(WriteSingleTrackMergesEverythingOntoOneTrack);
    TEST_METHOD(RunningStatusIsSmallerAndReadsTheSame);
    TEST_METHOD(RefusesToGoOverTheMaximumFileBytes);

    // ---- sequences built in memory ----
    TEST_METHOD(WritesASequenceBuiltInMemory);
    TEST_METHOD(TranslatesMidi2NoteOnToMidi1);
    TEST_METHOD(NeverTurnsAMidi2NoteOnIntoANoteOff);
    TEST_METHOD(TranslatesMidi2ControlChangeToMidi1);
    TEST_METHOD(CountsMessagesMidi1CannotExpress);
    TEST_METHOD(ReassemblesSystemExclusiveFromUniversalPackets);
    TEST_METHOD(AbsoluteTimingIsLaidOutOnAMusicalTimeline);

    // ---- refusals and the disk ----
    TEST_METHOD(ReportsNothingToWriteForAnEmptySequence);
    TEST_METHOD(ReportsAnErrorForANullSequence);
    TEST_METHOD(WritesToAFileOnDisk);
};
