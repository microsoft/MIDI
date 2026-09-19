// Copyright (c) Microsoft Corporation. All rights reserved.

#pragma once

// The DLS parser, driven with files no shipped sound set would contain.
//
// gm.dls lives in System32 and is protected, but the parser is what stands between a replaced file
// and the renderer, so it is tested as though the file were hostile. Every file here is built in
// memory: nothing reads or writes the installed sound set.
class MidiSynthDlsTests : public WEX::TestClass<MidiSynthDlsTests>
{
public:
    BEGIN_TEST_CLASS(MidiSynthDlsTests)
        TEST_CLASS_PROPERTY(L"TestClassification:Unit", L"Unit")
    END_TEST_CLASS()

    TEST_METHOD(TestMinimalValidFileIsAccepted);
    TEST_METHOD(TestEightBitMonoIsRejected);
    TEST_METHOD(TestUnsupportedBitDepthsAreRejected);
    TEST_METHOD(TestNonPcmFormatIsRejected);
    TEST_METHOD(TestZeroChannelsIsRejected);
    TEST_METHOD(TestEveryTruncationIsRejected);
    TEST_METHOD(TestNotRiffIsRejected);
    TEST_METHOD(TestWrongFormTypeIsRejected);
    TEST_METHOD(TestOversizedChunkLengthIsRejected);
    TEST_METHOD(TestInvalidWaveIndexIsRejected);
};
