// Copyright (c) Microsoft Corporation. All rights reserved.

#pragma once

// Registered parameters as MIDI 1.0 data entry sends them, with both bytes (MIDI 1.0 Detailed
// Specification, "Registered and Non-Registered Parameter Numbers"), and pitch bend sensitivity as
// the MIDI 2.0 Registered Controller (M2-104-UM 7.4.7.1). These render from the sound set Windows
// installed, and skip when there is none.
class MidiSynthDataEntryTests : public WEX::TestClass<MidiSynthDataEntryTests>
{
public:
    BEGIN_TEST_CLASS(MidiSynthDataEntryTests)
        TEST_CLASS_PROPERTY(L"TestClassification:Unit", L"Unit")
    END_TEST_CLASS()

    TEST_METHOD(TestFineTuningTakesTheDataEntryLsb);
    TEST_METHOD(TestPitchBendRangeTakesCents);
    TEST_METHOD(TestMidi2BendRangeLeavesDataEntryAlone);
};
