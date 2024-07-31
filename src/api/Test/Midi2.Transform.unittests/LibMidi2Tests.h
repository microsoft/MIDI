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

class LibMidi2Tests
    : public WEX::TestClass<LibMidi2Tests>
{
public:

    BEGIN_TEST_CLASS(LibMidi2Tests)
        TEST_CLASS_PROPERTY(L"TestClassification", L"Unit")
        END_TEST_CLASS()

        //TEST_CLASS_SETUP(ClassSetup);
        //TEST_CLASS_CLEANUP(ClassCleanup);

        //TEST_METHOD_SETUP(TestSetup);
        //TEST_METHOD_CLEANUP(TestCleanup);

        TEST_METHOD(TestTranslateFromBytesWithSysEx7);
        TEST_METHOD(TestTranslateFromBytesWithEmbeddedStartStopSysEx7);
        TEST_METHOD(TestTranslateFromBytesWithEmbeddedRealTimeAndSysEx7);
        TEST_METHOD(TestTranslateFromBytesWithEmptySysEx7);
        TEST_METHOD(TestTranslateFromBytesWithShortSysEx7);
        TEST_METHOD(TestTranslateFromBytesWithLongSysEx7);

    void InternalTestSysEx(_In_ uint8_t const sysexBytes[], _In_ uint32_t const byteCount, _In_ std::vector<uint32_t> const expectedWords);

private:

};

