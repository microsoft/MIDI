// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

#include <WexTestClass.h>

// The "who made it" fields in the editor and in the details dialogs. They write as somebody
// types, so the rule that matters most is that looking changes nothing.
class ProvenanceFieldsTests
{
    BEGIN_TEST_CLASS(ProvenanceFieldsTests)
        TEST_CLASS_PROPERTY(L"TestClassification:Type", L"Unit")
    END_TEST_CLASS()

    TEST_METHOD(ReadingTheFieldsBackChangesNothing);
    TEST_METHOD(NothingTypedAddsNoBlock);
    TEST_METHOD(TypingStartsABlockWithAnIdAndNoGuesses);
    TEST_METHOD(WhatIsTypedIsCleanedAndCut);
    TEST_METHOD(OnlyAPlainHttpsWebsiteIsTaken);
    TEST_METHOD(TheAppsOwnPartsAreKept);
    TEST_METHOD(PickingAPersonDropsTheAiDetails);
    TEST_METHOD(PickingAiBackBringsTheFieldsBack);
    TEST_METHOD(AFileThatSaysBothKeepsBothUntilAPick);
    TEST_METHOD(NoPickLeavesTheTermAlone);
    TEST_METHOD(ATermThisBuildDoesNotKnowKeepsItsAiDetails);
};
