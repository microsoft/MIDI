// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

#include <WexTestClass.h>

// Following the Key and chords row: notes moved by scale steps, Chord tones, generators in the
// song's key, clips that follow the key, the arpeggiator's Chords source, and saving them.
class HarmonyFollowTests : public WEX::TestClass<HarmonyFollowTests>
{
public:

    BEGIN_TEST_CLASS(HarmonyFollowTests)
        TEST_CLASS_PROPERTY(L"TestClassification", L"Unit")
    END_TEST_CLASS()

    TEST_METHOD(ScaleStepsBetweenKeys);
    TEST_METHOD(ScaleStepsKeepAlterationsAndRange);
    TEST_METHOD(NearestChordTones);
    TEST_METHOD(NotesClipFollowsTheKey);
    TEST_METHOD(DrumPatternsNeverFollow);
    TEST_METHOD(GeneratorPlaysInTheSongKey);
    TEST_METHOD(ChordTonesOnTheBeat);
    TEST_METHOD(ArpeggiatorPlaysTheChords);
    TEST_METHOD(ArpeggiatorHearsTheKeyMove);
    TEST_METHOD(SettingsRoundTrip);
};
