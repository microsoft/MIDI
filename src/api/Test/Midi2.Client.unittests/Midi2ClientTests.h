// Copyright (c) Microsoft Corporation. All rights reserved.
#pragma once

#include <winrt/base.h>
#include "WexTestClass.h"

MODULE_SETUP(ModuleSetup);
bool ModuleSetup()
{
    winrt::init_apartment();
    return true;
}

class Midi2ClientTests
    : public WEX::TestClass<Midi2ClientTests>
{
public:

    BEGIN_TEST_CLASS(Midi2ClientTests)
        TEST_CLASS_PROPERTY(L"TestClassification", L"Unit")
        TEST_CLASS_PROPERTY(L"BinaryUnderTest", L"Midi2.KSAbstraction.dll")
        TEST_CLASS_PROPERTY(L"BinaryUnderTest", L"Minmidi.sys")
    END_TEST_CLASS()

    TEST_CLASS_SETUP(ClassSetup);
    TEST_CLASS_CLEANUP(ClassCleanup);
    
    //Generic Tests
    TEST_METHOD(TestMidiClient);

    Midi2ClientTests()
    {}

private:

};

