// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

#include <WexTestClass.h>

// The generator code in midi-app-shared that MIDI Patchbay and MIDI Glass both send with.
class SharedGeneratorTests : public WEX::TestClass<SharedGeneratorTests>
{
public:

    BEGIN_TEST_CLASS(SharedGeneratorTests)
        TEST_CLASS_PROPERTY(L"TestClassification", L"Unit")
    END_TEST_CLASS()

    TEST_METHOD(ValueMessagesAreBuiltForEachKind);
    TEST_METHOD(KindsAndWavesReadBackFromTheirKeys);
    TEST_METHOD(ASweepKeepsItsPlaceWhenItsTempoChanges);
    TEST_METHOD(ASweepStartsAgainAfterALongStall);
};
