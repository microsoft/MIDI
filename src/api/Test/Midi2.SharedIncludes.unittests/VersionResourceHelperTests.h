// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

#include "midi_service_plugin_version.h"

#include <WexTestClass.h>

//MODULE_SETUP(ModuleSetup);
//bool ModuleSetup()
//{
//    winrt::init_apartment();
//    return true;
//}

class VersionResourceHelperTests
    : public WEX::TestClass<VersionResourceHelperTests>
{
public:

    BEGIN_TEST_CLASS(VersionResourceHelperTests)
        TEST_CLASS_PROPERTY(L"TestClassification", L"Unit")
        END_TEST_CLASS()


        TEST_METHOD(TestGetVersion);
        TEST_METHOD(TestGetCompanyName);

private:

};