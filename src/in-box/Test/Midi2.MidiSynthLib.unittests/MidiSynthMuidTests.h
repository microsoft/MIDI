// Copyright (c) Microsoft Corporation. All rights reserved.

#pragma once

// The identifier the synthesizer answers MIDI-CI with, and replacing it after another device
// withdraws it or claims it too (M2-101-UM, Discovery and Invalidate MUID). The responder belongs to
// the thread that dispatches messages, so a replacement made on another thread is handed over and
// taken up there. None of this needs a sound set.
class MidiSynthMuidTests : public WEX::TestClass<MidiSynthMuidTests>
{
public:
    BEGIN_TEST_CLASS(MidiSynthMuidTests)
        TEST_CLASS_PROPERTY(L"TestClassification:Unit", L"Unit")
    END_TEST_CLASS()

    TEST_METHOD(TestWithdrawnMuidIsReplacedOnTheDispatchingThread);
    TEST_METHOD(TestCollisionAsksForOneReplacement);
};
