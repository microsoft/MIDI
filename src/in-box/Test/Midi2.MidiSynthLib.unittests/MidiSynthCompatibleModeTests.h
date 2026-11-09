// Copyright (c) Microsoft Corporation. All rights reserved.

#pragma once

// Compatible mode reproduces how the older Windows synthesizer (Microsoft GS Wavetable Synth)
// sounded, where that differs from what later standards recommend. These render from the sound set
// Windows installed, and skip when there is none.
class MidiSynthCompatibleModeTests : public WEX::TestClass<MidiSynthCompatibleModeTests>
{
public:
    BEGIN_TEST_CLASS(MidiSynthCompatibleModeTests)
        TEST_CLASS_PROPERTY(L"TestClassification:Unit", L"Unit")
    END_TEST_CLASS()

    TEST_METHOD(TestDrumsTakePerformanceControls);
    TEST_METHOD(TestDrumChannelListsWhatDrumsTake);
    TEST_METHOD(TestTuningIsFixedAtNoteOn);
    TEST_METHOD(TestChannel10HasTheHighestPriority);
    TEST_METHOD(TestSpareVoicesFadeTheLowestPriority);
    TEST_METHOD(TestStealingPrefersAReleasedNote);
    TEST_METHOD(TestPitchBendGlides);
};
