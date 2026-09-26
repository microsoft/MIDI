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

    // ---- overrides of the theme ----

    TEST_METHOD(AControlThatAgreesWithItsThemeWritesNoOverrides);
    TEST_METHOD(OverridesOfTheThemeSurviveARoundTrip);
    TEST_METHOD(ALabelBoxSurvivesARoundTrip);
    TEST_METHOD(ALabelWithNoBoxWritesNoBox);
    TEST_METHOD(ACustomPlacementWithNoBoxFallsBackToTheTheme);
    TEST_METHOD(ALabelBoxFromAFileIsBounded);

    // ---- the forward rule ----
    TEST_METHOD(KeepsFieldsFromANewerVersion);
    TEST_METHOD(SaysWhenAFileIsFromANewerVersion);
    TEST_METHOD(UnknownFieldsSurviveAtEveryLevel);

    // ---- untrusted input ----
    TEST_METHOD(RejectsSomethingThatIsNotJson);
    TEST_METHOD(SurvivesAHostileFile);
    TEST_METHOD(BoundsStringsFromAFile);
    TEST_METHOD(RejectsSystemExclusiveThatIsNotHex);

    // ---- the background picture ----

    TEST_METHOD(ABackgroundPictureSurvivesARoundTrip);
    TEST_METHOD(AControlPictureSurvivesARoundTrip);
    TEST_METHOD(ABackgroundPictureThatIsAPathIsRefused);
    TEST_METHOD(NoBackgroundPictureWritesNothing);

    // ---- validation ----
    TEST_METHOD(AcceptsAValidDocument);
    TEST_METHOD(CatchesAMessageSentToAnUnknownDevice);
    TEST_METHOD(CatchesDuplicateControlIds);
    TEST_METHOD(CatchesAnUnclosedRepeatBlock);
    TEST_METHOD(CatchesASequenceThatDoesNotExist);

    // ---- the page and the canvas ----
    TEST_METHOD(FindsControlsOutsideThePage);

    // ---- hexadecimal in and out ----
    TEST_METHOD(HexBytesRoundTrip);
    TEST_METHOD(HexAcceptsWhatSomebodyWouldPaste);
    TEST_METHOD(HalfAByteIsRefusedWhole);
    TEST_METHOD(SomethingThatIsNotHexIsRefusedWhole);
    TEST_METHOD(HexIsBounded);
    TEST_METHOD(HexWordsRoundTrip);
};
