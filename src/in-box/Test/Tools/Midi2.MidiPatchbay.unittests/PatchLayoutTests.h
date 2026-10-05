// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

#include <WexTestClass.h>

class PatchLayoutTests : public WEX::TestClass<PatchLayoutTests>
{
public:

    BEGIN_TEST_CLASS(PatchLayoutTests)
        TEST_CLASS_PROPERTY(L"TestClassification", L"Unit")
    END_TEST_CLASS()

    TEST_METHOD(MessagesFlowLeftToRight);
    TEST_METHOD(NothingLandsOnAnythingElse);
    TEST_METHOD(ALoopStillGetsALayout);
};
