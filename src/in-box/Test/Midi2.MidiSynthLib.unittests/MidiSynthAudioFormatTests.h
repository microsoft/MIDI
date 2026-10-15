// Copyright (c) Microsoft Corporation. All rights reserved.

#pragma once

// The sample formats an exclusive mode stream can be opened with, and the bytes written for each.
// Exclusive mode skips the Windows mixer, so a wrong byte here reaches the speakers as noise.
class MidiSynthAudioFormatTests : public WEX::TestClass<MidiSynthAudioFormatTests>
{
public:
    BEGIN_TEST_CLASS(MidiSynthAudioFormatTests)
        TEST_CLASS_PROPERTY(L"TestClassification:Unit", L"Unit")
    END_TEST_CLASS()

    TEST_METHOD(TestFormatsThatCanBeWritten);
    TEST_METHOD(TestFormatsThatCannotBeWritten);
    TEST_METHOD(TestStereoBytesForEveryKind);
    TEST_METHOD(TestExtraChannelsAreSilentAndMonoIsBothSides);
};
