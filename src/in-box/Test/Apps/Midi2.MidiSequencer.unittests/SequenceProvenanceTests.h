// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

#include <WexTestClass.h>

// How a sequence was made, suggested from how its clips began (design section 13).
class SequenceProvenanceTests : public WEX::TestClass<SequenceProvenanceTests>
{
public:

    BEGIN_TEST_CLASS(SequenceProvenanceTests)
        TEST_CLASS_PROPERTY(L"TestClassification", L"Unit")
    END_TEST_CLASS()

    TEST_METHOD(OneKindOfClipSuggestsThatKind);
    TEST_METHOD(AMixSuggestsComposite);
    TEST_METHOD(AnAssistantsClipIsAlwaysDisclosed);
    TEST_METHOD(OnlyClipsInUseCount);
    TEST_METHOD(TheSequenceFileKeepsTheCopyrightNotice);
};
