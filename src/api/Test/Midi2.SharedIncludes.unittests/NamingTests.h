// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

#include <WexTestClass.h>

//MODULE_SETUP(ModuleSetup);
//bool ModuleSetup()
//{
//    winrt::init_apartment();
//    return true;
//}

class NamingTests
    : public WEX::TestClass<NamingTests>
{
public:

    BEGIN_TEST_CLASS(NamingTests)
        TEST_CLASS_PROPERTY(L"TestClassification", L"Unit")
    END_TEST_CLASS()

        //TEST_CLASS_SETUP(ClassSetup);
        //TEST_CLASS_CLEANUP(ClassCleanup);

        //TEST_METHOD_SETUP(TestSetup);
        //TEST_METHOD_CLEANUP(TestCleanup);

    TEST_METHOD(TestCheckForEndingNumber);
    TEST_METHOD(TestFullyCleanupBlockName);
    TEST_METHOD(TestFullyCleanupKSPinName);
    TEST_METHOD(TestRemoveJustKSPinGeneratedSuffix);
    TEST_METHOD(TestAddGroupNumberToNameIfNeeded);


    TEST_METHOD(TestGenerateWinMMLegacyName);
    TEST_METHOD(TestGeneratePinNameBasedMidi1PortName);
    TEST_METHOD(TestGenerateFilterPlusBlockMidi1PortName);


private:

};

