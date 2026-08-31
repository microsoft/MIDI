// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================

#include "pch.h"



void MidiSequenceNumberTests::TestTypeAssignment()
{
    uint16_t val{ 100 };

    MidiSequenceNumber a{ val };
    MidiSequenceNumber b;

    b = a;

    VERIFY_ARE_EQUAL(a.Value(), val);
    VERIFY_ARE_EQUAL(a.Value(), b.Value());
}

void MidiSequenceNumberTests::TestUInt16Assignment()
{
    uint16_t val{ 100 };

    MidiSequenceNumber a{ };

    a = val;

    VERIFY_ARE_EQUAL(a.Value(), val);
}


void MidiSequenceNumberTests::TestEqualsComparison()
{
    uint16_t val{ 100 };

    MidiSequenceNumber a{ };
    MidiSequenceNumber b{ };

    a = val;
    b = val;

    VERIFY_ARE_EQUAL(a.Value(), val);

    VERIFY_IS_TRUE(a == b);
}

void MidiSequenceNumberTests::TestSimpleUnequalComparison()
{
    uint16_t val1{ 100 };
    uint16_t val2{ 200 };

    MidiSequenceNumber a{ val1 };
    MidiSequenceNumber b{ val2 };

    VERIFY_IS_TRUE(b > a);
    VERIFY_IS_TRUE(b > val1);

    VERIFY_IS_TRUE(a < b);
    VERIFY_IS_TRUE(a < val2);

    VERIFY_IS_FALSE(a > a);
    VERIFY_IS_FALSE(b > b);

}

void MidiSequenceNumberTests::TestWrappedComparison()
{
    uint16_t val1{ 65534 };
    uint16_t val2{ 50 };

    MidiSequenceNumber a{ val1 };
    MidiSequenceNumber b{ val2 };

    VERIFY_IS_TRUE(a < b);
    VERIFY_IS_FALSE(a > b);

    VERIFY_IS_TRUE(b > a);
    VERIFY_IS_FALSE(b < a);

    VERIFY_IS_FALSE(a > a);
    VERIFY_IS_FALSE(b > b);

}



void MidiSequenceNumberTests::TestSimpleIncrement()
{
    uint16_t val{ 5000 };

    MidiSequenceNumber a{ val };

    VERIFY_ARE_EQUAL(a.Value(), val);

    a++;
    VERIFY_ARE_EQUAL(a.Value(), val + 1);

    a = val;
    a.Increment();
    VERIFY_ARE_EQUAL(a.Value(), val + 1);

    a = val;
    ++a;
    VERIFY_ARE_EQUAL(a.Value(), val + 1);
}


void MidiSequenceNumberTests::TestSimpleDecrement()
{
    uint16_t val{ 5000 };

    MidiSequenceNumber a{ val };
    VERIFY_ARE_EQUAL(a.Value(), val);

    a--;
    VERIFY_ARE_EQUAL(a.Value(), val - 1);

    a = val;
    a.Decrement();
    VERIFY_ARE_EQUAL(a.Value(), val - 1);

    a = val;
    --a;
    VERIFY_ARE_EQUAL(a.Value(), val - 1);
}

void MidiSequenceNumberTests::TestWrappedIncrement()
{
    uint16_t val{ 65535 };

    MidiSequenceNumber a{ val };
    VERIFY_ARE_EQUAL(a.Value(), val);

    a++;
    VERIFY_ARE_EQUAL(a.Value(), 0);

    a = val;
    a.Increment();
    VERIFY_ARE_EQUAL(a.Value(), 0);

    a = val;
    ++a;
    VERIFY_ARE_EQUAL(a.Value(), 0);
}

void MidiSequenceNumberTests::TestWrappedDecrement()
{
    uint16_t val{ 0 };

    MidiSequenceNumber a{ val };
    VERIFY_ARE_EQUAL(a.Value(), val);

    a--;
    VERIFY_ARE_EQUAL(a.Value(), 65535);

    a = val;
    a.Decrement();
    VERIFY_ARE_EQUAL(a.Value(), 65535);

    a = val;
    --a;
    VERIFY_ARE_EQUAL(a.Value(), 65535);
}

void MidiSequenceNumberTests::TestInitializedComparison()
{
    MidiSequenceNumber a{ 0 };
    MidiSequenceNumber b{ 0 };

    a--;    // gets us to max, which will be considered less than the 0 of b

    VERIFY_IS_TRUE(b > a);
    VERIFY_ARE_EQUAL(a.Value(), MidiSequenceNumber::Max());
    VERIFY_ARE_EQUAL(b.Value(), (a + 1).Value());
    VERIFY_ARE_EQUAL(a.Value(), (b - 1).Value());
}

void MidiSequenceNumberTests::TestBasicMath()
{
    uint16_t val{ 100 };
    MidiSequenceNumber a{ val };

    a = val;
    a = a - 50;
    VERIFY_ARE_EQUAL(a.Value(), 50);

    a = val;
    a = a + 50;
    VERIFY_ARE_EQUAL(a.Value(), 150);

    a = val;
    a = a - 101;
    VERIFY_ARE_EQUAL(a.Value(), MidiSequenceNumber::Max());

}
