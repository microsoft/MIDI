// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

#include <WexTestClass.h>

// Generators that listen to another clip or a track (design section 29).
class GeneratorListenTests : public WEX::TestClass<GeneratorListenTests>
{
public:

    BEGIN_TEST_CLASS(GeneratorListenTests)
        TEST_CLASS_PROPERTY(L"TestClassification", L"Unit")
    END_TEST_CLASS()

    TEST_METHOD(AClipIsHeardSideBySide);
    TEST_METHOD(LearnPlaysWhatItHeard);
    TEST_METHOD(ATrackIsHeardWhereItPlays);
    TEST_METHOD(LoopsPlayTheSameColdOrInOrder);
    TEST_METHOD(LearnsOnceFromAGeneratorOrATrack);
    TEST_METHOD(ListeningRoundTripsThroughTheFile);
    TEST_METHOD(RingingRingsWhatItHeard);
    TEST_METHOD(LifeDropsWhatItHears);
    TEST_METHOD(LifeKeepsListening);
    TEST_METHOD(LifeLoopsPlayTheSameColdOrInOrder);
    TEST_METHOD(EvolveSoundsLikeWhatItHears);
    TEST_METHOD(EvolveAimsAtWhatItHeardEachPass);
    TEST_METHOD(ATrackListenerKeepsOneRunEveryPass);
    TEST_METHOD(ListenersKeepWhatTheyLearnedInTheFile);
};
