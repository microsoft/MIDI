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

        //TEST_METHOD(TestGetPreferredName);

        //TEST_METHOD(TestGetSourceEntry);
        //TEST_METHOD(TestGetDestinationEntry);

        //TEST_METHOD(TestUpdateSourceEntryCustomName);
        //TEST_METHOD(TestUpdateDestinationEntryCustomName);


        //TEST_METHOD(TestGetSourceEntryCustomName);
        //TEST_METHOD(TestGetDestinationEntryCustomName);


        //TEST_METHOD(TestPopulateAllEntriesForNativeUmpDevice);
        //TEST_METHOD(TestPopulateAllEntriesForMidi1DeviceUsingUmpDriver);

        TEST_METHOD(TestPopulateEntryForNativeUmpDevice);
        TEST_METHOD(TestPopulateEntryForMidi1DeviceUsingUmpDriver);
        TEST_METHOD(TestPopulateEntryForMidi1DeviceUsingMidi1Driver);

        // TODO: Need to test reading/writing from property vectors


private:

};

