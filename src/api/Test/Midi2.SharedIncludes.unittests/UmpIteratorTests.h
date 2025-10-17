// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

#include "ump_iterator.h"

#include <WexTestClass.h>

//MODULE_SETUP(ModuleSetup);
//bool ModuleSetup()
//{
//    winrt::init_apartment();
//    return true;
//}

class UmpIteratorTests
    : public WEX::TestClass<UmpIteratorTests>
{
public:

    BEGIN_TEST_CLASS(UmpIteratorTests)
        TEST_CLASS_PROPERTY(L"TestClassification", L"Unit")
        END_TEST_CLASS()

        
        TEST_METHOD(TestBasicIteration);
    TEST_METHOD(TestMixedIteration);
    TEST_METHOD(TestIncompleteBuffer);
    TEST_METHOD(TestValidateIncompleteBufferHasCompleteUmps);
    TEST_METHOD(TestValidateCompleteBufferHasCompleteUmps);
    TEST_METHOD(TestGetMessageType);
    TEST_METHOD(TestCopyWordsToVector);


private:

};

