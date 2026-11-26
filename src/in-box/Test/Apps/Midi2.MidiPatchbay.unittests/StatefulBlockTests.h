// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

#include <WexTestClass.h>

// The (N)RPN filter and transform, the note distributor and the gate.
class StatefulBlockTests : public WEX::TestClass<StatefulBlockTests>
{
public:

    BEGIN_TEST_CLASS(StatefulBlockTests)
        TEST_CLASS_PROPERTY(L"TestClassification", L"Unit")
    END_TEST_CLASS()

    TEST_METHOD(AGateOpensAndClosesOnStartAndStop);
    TEST_METHOD(AGateWithOneTriggerTurnsEachTime);
    TEST_METHOD(AGateComparesMidi2ValuesAtSevenBits);
    TEST_METHOD(AGateCanMatchExactWords);
    TEST_METHOD(AParameterFilterJudgesMidi2ByItsAddress);
    TEST_METHOD(AParameterFilterFollowsMidi1Selection);
    TEST_METHOD(AParameterTransformMovesMidi2Parameters);
    TEST_METHOD(AParameterTransformPutsMidi1SelectionRight);
    TEST_METHOD(ADistributorTakesTurns);
    TEST_METHOD(ADistributorCanKeepTheHighestNotes);
    TEST_METHOD(ADistributorSendsTheRestWhereItBelongs);
    TEST_METHOD(ADistributorSendsEachNoteToOneDestination);
};
