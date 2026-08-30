// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

#include <WexTestClass.h>

class Utf8StringTests
    : public WEX::TestClass<Utf8StringTests>
{
public:

    BEGIN_TEST_CLASS(Utf8StringTests)
        TEST_CLASS_PROPERTY(L"TestClassification", L"Unit")
    END_TEST_CLASS()

    TEST_METHOD(TestByteCountAscii);
    TEST_METHOD(TestByteCountMultiByte);
    TEST_METHOD(TestTruncateNotNeeded);
    TEST_METHOD(TestTruncateAscii);
    TEST_METHOD(TestTruncateOnExactCharacterBoundary);
    TEST_METHOD(TestTruncateSplitsTwoByteCharacter);
    TEST_METHOD(TestTruncateSplitsThreeByteCharacter);
    TEST_METHOD(TestTruncateSplitsSurrogatePair);
    TEST_METHOD(TestTruncateToZero);
    TEST_METHOD(TestTruncateEmptyString);
    TEST_METHOD(TestTruncateSingleCharacterTooLarge);
    TEST_METHOD(TestTruncatedResultAlwaysWithinLimit);
};
