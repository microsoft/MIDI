// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================

#pragma once

#include <WexTestClass.h>

class MidiSequenceNumberTests
    : public WEX::TestClass<MidiSequenceNumberTests>
{
public:

    BEGIN_TEST_CLASS(MidiSequenceNumberTests)
        TEST_CLASS_PROPERTY(L"TestClassification", L"Unit")
        TEST_CLASS_PROPERTY(L"BinaryUnderTest", L"Midi2.NetworkMidiTransport.dll")
    END_TEST_CLASS()


    TEST_METHOD(TestTypeAssignment);
    TEST_METHOD(TestUInt16Assignment);
    TEST_METHOD(TestEqualsComparison);
    TEST_METHOD(TestSimpleUnequalComparison);
    TEST_METHOD(TestWrappedComparison);
    TEST_METHOD(TestSimpleIncrement);
    TEST_METHOD(TestSimpleDecrement);
    TEST_METHOD(TestWrappedIncrement);
    TEST_METHOD(TestWrappedDecrement);
    TEST_METHOD(TestInitializedComparison);
    TEST_METHOD(TestBasicMath);

private:

};

