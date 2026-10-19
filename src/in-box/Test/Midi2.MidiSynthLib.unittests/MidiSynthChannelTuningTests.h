// Copyright (c) Microsoft Corporation. All rights reserved.

#pragma once

// MIDI 2.0 Registered Controllers 0,1 and 0,2: channel fine and coarse tuning (M2-104-UM section
// 7.4.7.1, with the ranges from the MIDI 1.0 specification). These render from the sound set
// Windows installed, and skip when there is none.
class MidiSynthChannelTuningTests : public WEX::TestClass<MidiSynthChannelTuningTests>
{
public:
    BEGIN_TEST_CLASS(MidiSynthChannelTuningTests)
        TEST_CLASS_PROPERTY(L"TestClassification:Unit", L"Unit")
    END_TEST_CLASS()

    TEST_METHOD(TestFineTuningMovesThePitch);
    TEST_METHOD(TestCoarseTuningMovesThePitch);
    TEST_METHOD(TestMidi2TuningMatchesMidi1);
    TEST_METHOD(TestTuningRetunesSoundingNotes);
    TEST_METHOD(TestTuningAddsToExactPitch);

    // General MIDI 2 sections 3.4.2 and 3.4.3: a channel playing a drum kit ignores channel tuning.
    TEST_METHOD(TestDrumKitIgnoresChannelTuning);
    TEST_METHOD(TestKeptTuningLeavesDrumsAlone);
    TEST_METHOD(TestHeldNoteKeepsItsTuning);
};
