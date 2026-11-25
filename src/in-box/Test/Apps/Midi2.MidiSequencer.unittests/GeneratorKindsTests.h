// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

#include <WexTestClass.h>

// The generators of design section 28: Life, Learn, Words, Evolve, the infinity series, L-systems
// and change ringing, and the file.
class GeneratorKindsTests : public WEX::TestClass<GeneratorKindsTests>
{
public:

    BEGIN_TEST_CLASS(GeneratorKindsTests)
        TEST_CLASS_PROPERTY(L"TestClassification", L"Unit")
    END_TEST_CLASS()

    TEST_METHOD(ABlinkerBlinks);
    TEST_METHOD(AGliderWrapsAround);
    TEST_METHOD(AnRPentominoSettles);
    TEST_METHOD(LifeStartsAgainWhenAsked);
    TEST_METHOD(LifePlaysItsKeyAndJoinsCells);
    TEST_METHOD(LifeDrumsPlayTheirDrums);
    TEST_METHOD(LearnFollowsWhatItLearned);
    TEST_METHOD(LearnFindsAClipsKey);
    TEST_METHOD(KeyRulesKeepJumpsAndEnds);
    TEST_METHOD(KeepRepeatsAPass);
    TEST_METHOD(SeedsFromWords);
    TEST_METHOD(WordsAreSpelled);
    TEST_METHOD(LongWordsCarryOn);
    TEST_METHOD(EvolveChoosesTheBest);
    TEST_METHOD(EvolveScoresClimb);
    TEST_METHOD(TheInfinitySeries);
    TEST_METHOD(LSystemsGrowAndPlay);
    TEST_METHOD(RingingCourses);
    TEST_METHOD(RingingPlaysRoundsFirst);
    TEST_METHOD(NewGeneratorsPlayThroughTheEngine);
    TEST_METHOD(NewGeneratorsRoundTripThroughTheFile);
    TEST_METHOD(DamagedNewGeneratorsAreClamped);
    TEST_METHOD(VelocitiesStayInTheirRange);
    TEST_METHOD(EveryGeneratorHasTheCommonShapes);
    TEST_METHOD(NewLifeCellsPlayLoudest);
    TEST_METHOD(LearnPlaysLoudAndSoftAsLearned);
    TEST_METHOD(WordsStressTheirStarts);
    TEST_METHOD(BellsStressTheirRows);
    TEST_METHOD(LSystemsClimbAndBranch);
};
