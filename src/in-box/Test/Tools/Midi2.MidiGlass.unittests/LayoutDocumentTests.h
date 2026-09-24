// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

#include <WexTestClass.h>

class LayoutDocumentTests : public WEX::TestClass<LayoutDocumentTests>
{
public:

    BEGIN_TEST_CLASS(LayoutDocumentTests)
        TEST_CLASS_PROPERTY(L"TestClassification", L"Unit")
    END_TEST_CLASS()

    // ---- reading a file a person wrote ----
    TEST_METHOD(ReadsAHandAuthoredLayout);
    TEST_METHOD(ReadsMessagesAndSystemExclusive);
    TEST_METHOD(ReadsTheDeviceTableThroughTheSharedMatchCriteria);
    TEST_METHOD(ReadsSequenceSteps);

    // ---- the round trip ----
    TEST_METHOD(WritingTheSameDocumentTwiceProducesTheSameBytes);
    TEST_METHOD(ReadingBackWhatWasWrittenChangesNothing);
    TEST_METHOD(WholeNumbersDoNotGrowADecimalPoint);

    // ---- the forward rule ----
    TEST_METHOD(KeepsFieldsFromANewerVersion);
    TEST_METHOD(SaysWhenAFileIsFromANewerVersion);
    TEST_METHOD(UnknownFieldsSurviveAtEveryLevel);

    // ---- untrusted input ----
    TEST_METHOD(RejectsSomethingThatIsNotJson);
    TEST_METHOD(SurvivesAHostileFile);
    TEST_METHOD(BoundsStringsFromAFile);
    TEST_METHOD(RejectsSystemExclusiveThatIsNotHex);

    // ---- validation ----
    TEST_METHOD(AcceptsAValidDocument);
    TEST_METHOD(CatchesAMessageSentToAnUnknownDevice);
    TEST_METHOD(CatchesDuplicateControlIds);
    TEST_METHOD(CatchesAnUnclosedRepeatBlock);
    TEST_METHOD(CatchesASequenceThatDoesNotExist);

    // ---- the page and the canvas ----
    TEST_METHOD(FindsControlsOutsideThePage);
};
