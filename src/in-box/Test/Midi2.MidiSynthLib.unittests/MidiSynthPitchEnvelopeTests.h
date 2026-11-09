// Copyright (c) Microsoft Corporation. All rights reserved.

#pragma once

// The DLS pitch envelope (EG2, DLS Level 1 section 1.6), which the sound set uses for a pitch that
// moves after the strike and for instruments whose samples are tuned to need it. These render from
// the sound set Windows installed, and skip when there is none.
class MidiSynthPitchEnvelopeTests : public WEX::TestClass<MidiSynthPitchEnvelopeTests>
{
public:
    BEGIN_TEST_CLASS(MidiSynthPitchEnvelopeTests)
        TEST_CLASS_PROPERTY(L"TestClassification:Unit", L"Unit")
    END_TEST_CLASS()

    TEST_METHOD(TestHeldPitchEnvelopeTunesTheInstrument);
    TEST_METHOD(TestPitchEnvelopeFallsAfterTheStrike);
};
