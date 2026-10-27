// Copyright (c) Microsoft Corporation. All rights reserved.

#pragma once

// The reset messages a file or a player sends to put the synthesizer back to its power-up state:
// GM1 System On and GM2 System On (General MIDI 2 section 4.9), and the Roland GS Reset and Yamaha
// XG System On that files written for those synthesizers open with. These render from the sound
// set Windows installed, and skip when there is none.
class MidiSynthSystemOnTests : public WEX::TestClass<MidiSynthSystemOnTests>
{
public:
    BEGIN_TEST_CLASS(MidiSynthSystemOnTests)
        TEST_CLASS_PROPERTY(L"TestClassification:Unit", L"Unit")
    END_TEST_CLASS()

    TEST_METHOD(TestEveryResetRestoresChannelDefaults);
    TEST_METHOD(TestResetRestoresMasterVolume);
    TEST_METHOD(TestResetFadesInsteadOfCutting);
    TEST_METHOD(TestGmSystemOffIsIgnored);
};
