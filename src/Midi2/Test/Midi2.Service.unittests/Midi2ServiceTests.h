// Copyright (c) Microsoft Corporation. All rights reserved.
#pragma once

#include <WexTestClass.h>

class Midi2ServiceTests
    : public WEX::TestClass<Midi2ServiceTests>
{
public:

    BEGIN_TEST_CLASS(Midi2ServiceTests)
        TEST_CLASS_PROPERTY(L"ThreadingModel", L"MTA")
        TEST_CLASS_PROPERTY(L"TestClassification", L"Unit")
        TEST_CLASS_PROPERTY(L"BinaryUnderTest", L"Midi2.KSAbstraction.dll")
        TEST_CLASS_PROPERTY(L"BinaryUnderTest", L"Minmidi.sys")
        TEST_CLASS_PROPERTY(L"BinaryUnderTest", L"MidiSrv.exe")
    END_TEST_CLASS()

    TEST_CLASS_SETUP(ClassSetup);
    TEST_CLASS_CLEANUP(ClassCleanup);
    
    //Generic Tests
    TEST_METHOD(TestMidiServiceRPC);

    Midi2ServiceTests()
    {}

private:

};

