// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

#include <WexTestClass.h>

class GeneratorTests : public WEX::TestClass<GeneratorTests>
{
public:

    BEGIN_TEST_CLASS(GeneratorTests)
        TEST_CLASS_PROPERTY(L"TestClassification", L"Unit")
    END_TEST_CLASS()

    // ---- the LFO's shapes ----

    TEST_METHOD(ASineStartsInTheMiddleAndPeaksAtAQuarter);
    TEST_METHOD(ATriangleTurnsAtTheSamePlacesTheSineDoes);
    TEST_METHOD(ASquareIsHighForTheFirstHalf);
    TEST_METHOD(TheTwoRampsAreMirrorImages);
    TEST_METHOD(PhaseWrapsRatherThanClamping);
    TEST_METHOD(NoiseHasNoShapeToReadAtAPhase);
    TEST_METHOD(OnlyThePeriodicShapesRepeat);

    // ---- the sweep's two ends ----

    TEST_METHOD(AFullSweepReachesBothEnds);
    TEST_METHOD(ANarrowSweepStaysInsideItsEnds);
    TEST_METHOD(ALowEndAboveTheHighOneTurnsTheWaveOver);
    TEST_METHOD(ANoiseSampleIsScaledByTheSameEnds);

    // ---- noise ----

    TEST_METHOD(EveryNoiseStaysInRange);
    TEST_METHOD(WhiteNoiseDoesNotRepeatItself);
    TEST_METHOD(BrownNoiseMovesLessThanWhite);
    TEST_METHOD(ResettingANoiseSourceRepeatsIt);

    // ---- the platter ----

    TEST_METHOD(StraightUpIsZeroDegrees);
    TEST_METHOD(AngleRunsClockwise);
    TEST_METHOD(APlatterPushedPastTheTopKeepsGoing);
    TEST_METHOD(AnAngleDeltaTakesTheShortWayRound);
    TEST_METHOD(OnlyThePlatterIsTurnedByHand);
};
