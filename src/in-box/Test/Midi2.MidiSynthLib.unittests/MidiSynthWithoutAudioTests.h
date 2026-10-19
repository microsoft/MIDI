// Copyright (c) Microsoft Corporation. All rights reserved.

#pragma once

// Getting from the first note to the audio device without losing anything on the way. Messages that
// arrive while no audio device is open are answered without one, except a channel voice message,
// which waits for the device; when no device can be had, notes are dropped so nothing else waits
// behind them. Once audio runs, only the render thread may change the engine, so other threads ask
// it to.
class MidiSynthWithoutAudioTests : public WEX::TestClass<MidiSynthWithoutAudioTests>
{
public:
    BEGIN_TEST_CLASS(MidiSynthWithoutAudioTests)
        TEST_CLASS_PROPERTY(L"TestClassification:Unit", L"Unit")
    END_TEST_CLASS()

    TEST_METHOD(TestPeekLeavesTheItemQueued);
    TEST_METHOD(TestChannelVoiceWaitsForAudio);
    TEST_METHOD(TestDiscardingDropsOnlyNoteOns);
    TEST_METHOD(TestRenderThreadMakesRequestedChanges);
};
