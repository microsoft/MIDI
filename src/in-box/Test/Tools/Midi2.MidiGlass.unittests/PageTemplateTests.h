// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

#include <WexTestClass.h>

class PageTemplateTests : public WEX::TestClass<PageTemplateTests>
{
public:

    BEGIN_TEST_CLASS(PageTemplateTests)
        TEST_CLASS_PROPERTY(L"TestClassification", L"Unit")
    END_TEST_CLASS()

    TEST_METHOD(OffersTheTemplatesTheDesignNames);
    TEST_METHOD(MatchesTheQuotedSizesOnTheReferencePage);
    TEST_METHOD(EverySizeLandsOnTheFourPixelQuantum);
    TEST_METHOD(ASmallPageGetsChunkierControlsThanALargeOne);
    TEST_METHOD(ABiggerPageStillGetsBiggerControlsInAbsoluteTerms);
    TEST_METHOD(SurvivesANonsensePageSize);
};
