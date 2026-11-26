// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

#include <WexTestClass.h>

// Files that ask for more than a person ever would: they open and play without hanging the app.
class HostileFileTests : public WEX::TestClass<HostileFileTests>
{
public:

    BEGIN_TEST_CLASS(HostileFileTests)
        TEST_CLASS_PROPERTY(L"TestClassification", L"Unit")
    END_TEST_CLASS()

    TEST_METHOD(AnEmptyOneTickLoopRendersQuickly);
    TEST_METHOD(AOneTickLoopWithANoteStopsAtTheLimit);
    TEST_METHOD(LyricsFromAHugeFileStopAtTheLimit);
};
