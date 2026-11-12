// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

#include <WexTestClass.h>

class CalculatorMathTests : public WEX::TestClass<CalculatorMathTests>
{
public:

    BEGIN_TEST_CLASS(CalculatorMathTests)
        TEST_CLASS_PROPERTY(L"TestClassification", L"Unit")
    END_TEST_CLASS()

    // ---- tap tempo (shared with the MIDI Clock app) ----
    TEST_METHOD(TapTempoNeedsTwoTaps);
    TEST_METHOD(TapTempoAveragesEvenTaps);
    TEST_METHOD(TapTempoStartsOverAfterAPause);
    TEST_METHOD(TapTempoRoundsAndClamps);
    TEST_METHOD(TapTempoReportsHowEvenTheTapsWere);

    // ---- tempo and beats ----
    TEST_METHOD(BeatCalculatorFindsTheTempo);
    TEST_METHOD(BeatCalculatorFindsTheLengthAndTheBars);
    TEST_METHOD(BeatsFollowTheMetersBottomNumber);
    TEST_METHOD(RejectsMetersAndTemposOutOfRange);
    TEST_METHOD(NoteLengthsAt120);
    TEST_METHOD(NoteLengthsAsHertzAndSamples);
    TEST_METHOD(TempoChangeGivesSpeedLengthAndPitch);

    // ---- notes and pitch ----
    TEST_METHOD(MiddleCIsC3LikeTheApi);
    TEST_METHOD(MiddleCCanBeAnotherOctave);
    TEST_METHOD(ParsesNotesWithTheChosenMiddleC);
    TEST_METHOD(EqualTemperamentFrequencies);
    TEST_METHOD(FrequencyToNearestNote);
    TEST_METHOD(EveryTemperamentKeepsTheReferenceNote);
    TEST_METHOD(JustIntonationHasPureThirdsAndFifths);
    TEST_METHOD(MeantoneHasPureMajorThirds);
    TEST_METHOD(PythagoreanAndWerckmeisterTables);
    TEST_METHOD(IntervalsBetweenNotes);

    // ---- MIDI 2.0 values ----
    TEST_METHOD(MinCenterMaxMatchesTheSpecTables);
    TEST_METHOD(MinCenterMaxDownscaleCutsBits);
    TEST_METHOD(ZeroExtensionMatchesTheSpecTables);
    TEST_METHOD(Pitch79AndPitch725);
    TEST_METHOD(Pitch79WidensTo725WithZeros);
    TEST_METHOD(PitchBendInBothProtocols);
    TEST_METHOD(BendRangeAsRpnData);
    TEST_METHOD(UmpTiming);
    TEST_METHOD(ManufacturerIds);

    // ---- Windows time values ----
    TEST_METHOD(FileTimeToOtherFormats);
    TEST_METHOD(TimestampsToSecondsKeepTheirDigits);
    TEST_METHOD(TimestampsAndClockTime);
    TEST_METHOD(SplitsSecondsIntoHoursAndMinutes);
};
