// Copyright (c) Microsoft Corporation. All rights reserved.

#pragma once

// MIDI 2.0 Note On with attribute #3, Pitch 7.9 (M2-104-UM section 7.4.15.3). With that attribute
// the note number is only an index, and a sender can rotate it through all 128 values to play any
// pitch (Appendix C.3). These render from the sound set Windows installed, and skip when there is none.
class MidiSynthExactPitchTests : public WEX::TestClass<MidiSynthExactPitchTests>
{
public:
    BEGIN_TEST_CLASS(MidiSynthExactPitchTests)
        TEST_CLASS_PROPERTY(L"TestClassification:Unit", L"Unit")
    END_TEST_CLASS()

    TEST_METHOD(TestNoteIndexDoesNotChooseTheSample);
    TEST_METHOD(TestDrumKitKeepsTheNoteNumber);
    TEST_METHOD(TestNoteOffAddressesTheNoteIndex);
    TEST_METHOD(TestTopOfThePitchRange);
};
