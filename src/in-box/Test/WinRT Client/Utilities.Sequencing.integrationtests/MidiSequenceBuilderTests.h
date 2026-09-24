// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

class MidiSequenceBuilderTests
    : public WEX::TestClass<MidiSequenceBuilderTests>
{
    BEGIN_TEST_CLASS(MidiSequenceBuilderTests)
        TEST_CLASS_PROPERTY(L"TestClassification", L"Integration")
        TEST_CLASS_PROPERTY(L"Description", L"Building a sequence in memory and playing it")
    END_TEST_CLASS()

    // ---- building ----
    TEST_METHOD(BuildsAnEmptySequence);
    TEST_METHOD(AddsTracksAndReportsTheirIndexes);
    TEST_METHOD(AddNoteWritesBothHalves);
    TEST_METHOD(AddNoteWithNoDurationStillEnds);
    TEST_METHOD(CarriesTempoAndTimeSignature);
    TEST_METHOD(ReportsTheTimingModeItWasBuiltWith);
    TEST_METHOD(RejectsMalformedInput);
    TEST_METHOD(ClearEmptiesTheBuilder);

    // ---- what reaches the wire ----
    TEST_METHOD(PlaysABuiltSequence);
    TEST_METHOD(CarriesUniversalPacketsThroughUnconverted);
    TEST_METHOD(StampsTheGroupOnUniversalPackets);
    TEST_METHOD(AbsoluteTimingWaitsInRealTime);
};
