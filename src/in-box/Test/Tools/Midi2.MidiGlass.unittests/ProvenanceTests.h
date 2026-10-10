// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

#include <WexTestClass.h>

// The provenance block: who made a layout or theme, with what, and from what. It is
// self-stated, so what matters is that it round-trips exactly, that a newer build's fields
// survive, and that nothing in it can disguise a name or smuggle in a link.
class ProvenanceTests
{
    BEGIN_TEST_CLASS(ProvenanceTests)
        TEST_CLASS_PROPERTY(L"TestClassification:Type", L"Unit")
    END_TEST_CLASS()

    TEST_METHOD(EveryFieldRoundTrips);
    TEST_METHOD(UnknownFieldsSurviveAtEveryLevel);
    TEST_METHOD(TheBlockIsWrittenInAFixedOrder);
    TEST_METHOD(AStringBasedOnIsTheName);
    TEST_METHOD(AFullIptcAddressIsShortened);
    TEST_METHOD(EveryTermTheToolsWriteIsKnown);
    TEST_METHOD(DirectionOverridesAndZeroWidthCharactersAreRemoved);
    TEST_METHOD(ALongNameIsCutWithoutSplittingACharacter);
    TEST_METHOD(OnlyAPlainHttpsAddressIsALink);
    TEST_METHOD(VersionsCompareNumberByNumber);
    TEST_METHOD(DatesAndTimesAreRead);
    TEST_METHOD(ACopyCreditsTheOriginal);
    TEST_METHOD(ACopyOfAiWorkSaysSo);

    // ---- in a layout and in a theme ----

    TEST_METHOD(ALayoutKeepsItsProvenance);
    TEST_METHOD(AFavoriteIsReadButNeverWritten);
    TEST_METHOD(AThemeKeepsItsProvenance);
    TEST_METHOD(AThemeKeepsKeysFromANewerBuild);
};
