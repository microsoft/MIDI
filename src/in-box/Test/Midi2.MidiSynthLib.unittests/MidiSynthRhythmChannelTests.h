// Copyright (c) Microsoft Corporation. All rights reserved.

#pragma once

// General MIDI 2 rhythm channels (sections 2.4, 3.3.1, 3.3.2, 3.3.7, 3.4.1 and 3.6): what a channel
// playing a drum kit ignores, and how bank select moves a channel between drums and melody. These
// render from the sound set Windows installed, and skip when there is none.
class MidiSynthRhythmChannelTests : public WEX::TestClass<MidiSynthRhythmChannelTests>
{
public:
    BEGIN_TEST_CLASS(MidiSynthRhythmChannelTests)
        TEST_CLASS_PROPERTY(L"TestClassification:Unit", L"Unit")
    END_TEST_CLASS()

    TEST_METHOD(TestRhythmChannelIgnoresPerformanceMessages);
    TEST_METHOD(TestDrumNotesIgnoreWhatTheChannelHeld);
    TEST_METHOD(TestRhythmChannelStillLiftsTheDamper);
    TEST_METHOD(TestBank121MakesChannel10Melodic);
    TEST_METHOD(TestChannelListNamesWhatPlays);
};
