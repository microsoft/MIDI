// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

#include <WexTestClass.h>

class ThumbnailLayoutTests : public WEX::TestClass<ThumbnailLayoutTests>
{
public:

    BEGIN_TEST_CLASS(ThumbnailLayoutTests)
        TEST_CLASS_PROPERTY(L"TestClassification", L"Unit")
    END_TEST_CLASS()

    TEST_METHOD(FillsTheImageWhenTheAspectMatches);
    TEST_METHOD(LetterboxesAPortraitPageIntoALandscapeCard);
    TEST_METHOD(LetterboxesALandscapePageIntoASquareCard);
    TEST_METHOD(NeverStretchesThePage);
    TEST_METHOD(PlacesAControlWhereThePagePutsIt);
    TEST_METHOD(LeavesOffPageControlsOutOfTheCard);
    TEST_METHOD(KeepsTinyControlsVisible);
    TEST_METHOD(ResolvesTheHueFromTheThemeSlot);
    TEST_METHOD(SurvivesAPageWithNoSize);
    TEST_METHOD(SurvivesAPageIndexThatDoesNotExist);
};
