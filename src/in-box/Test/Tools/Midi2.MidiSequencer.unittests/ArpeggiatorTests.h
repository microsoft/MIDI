// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

#include <WexTestClass.h>

// Arpeggiator clips: the orders, keys held on the source, another track's chords, the file, and
// the engine stepping through what's played.
class ArpeggiatorTests : public WEX::TestClass<ArpeggiatorTests>
{
public:

    BEGIN_TEST_CLASS(ArpeggiatorTests)
        TEST_CLASS_PROPERTY(L"TestClassification", L"Unit")
    END_TEST_CLASS()

    TEST_METHOD(OrdersStepThroughTheChord);
    TEST_METHOD(HeldKeysPlayFromTheFirstStep);
    TEST_METHOD(LatchKeepsTheLastKeys);
    TEST_METHOD(AnotherTracksChordsPlay);
    TEST_METHOD(MakeNotesKeepsWhatItPlayed);
    TEST_METHOD(ArpeggiatorSettingsSurviveTheFile);
    TEST_METHOD(TheEngineArpeggiatesKeysAsTheyArePlayed);
};
