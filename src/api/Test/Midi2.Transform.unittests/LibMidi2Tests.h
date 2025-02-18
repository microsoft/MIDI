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
        TEST_METHOD(TestTranslateFromBytesWithSysEx7AndNonZeroGroup);    
        TEST_METHOD(TestTranslateFromBytesWithEmbeddedStartStopSysEx7);
        TEST_METHOD(TestTranslateFromBytesWithEmbeddedRealTimeAndSysEx7);
        TEST_METHOD(TestTranslateFromBytesWithEmptySysEx7);
        TEST_METHOD(TestTranslateFromBytesWithShortSysEx7);
        TEST_METHOD(TestTranslateFromBytesWithLongSysEx7);

        TEST_METHOD(TestTranslateToBytesWithSysEx7);
        TEST_METHOD(TestTranslateToBytesWithInterruptedSysEx7);
        TEST_METHOD(TestTranslateToBytesWithCanceledSysEx7);

        TEST_METHOD(TestProgramChangeFromBytes);
        TEST_METHOD(TestProgramChangeToBytes);
        TEST_METHOD(TestSelectedChannelMessagesToBytes);

    void InternalTranslateMidi1BytesToUmpWords(
        _In_ uint8_t const groupIndex, 
        _In_reads_bytes_(byteCount) uint8_t const sysexBytes[], 
        _In_ uint32_t const byteCount, 
        _In_ std::vector<uint32_t> const expectedWords);

    void InternalTranslateUmpWordsToMidi1Bytes(
        _In_ std::vector<uint32_t> const messageWords,
        _In_ std::vector<uint8_t> const expectedBytes);



private:

};

