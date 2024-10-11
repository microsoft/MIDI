// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================


#include "stdafx.h"
#include <iostream>
#include <iomanip>

#include "MidiBSToUMPTransformTests.h"

#include "Midi2BS2UMPTransform.h"
#include "Midi2BS2UMPTransform_i.c"


_Use_decl_annotations_
void MidiBSToUMPTransformTests::InternalTestSysEx(
    uint8_t const groupIndex,
    uint8_t const sysexBytes[], 
    uint32_t const byteCount, 
    uint16_t const expectedMessageCount,
    std::vector<uint32_t> const expectedWords
)
{
    wil::com_ptr_nothrow<IMidiTransform> transformLib;
    wil::com_ptr_nothrow<IMidiDataTransform> transform;

    auto iid = __uuidof(Midi2BS2UMPTransform);

    VERIFY_SUCCEEDED(CoCreateInstance(iid, nullptr, CLSCTX_ALL, IID_PPV_ARGS(&transformLib)));

    VERIFY_SUCCEEDED(transformLib->Activate(__uuidof(IMidiDataTransform), (void**)(&transform)));

    std::wstring deviceId{ L"foobarbaz" };

    TRANSFORMCREATIONPARAMS creationParams{};
    creationParams.DataFormatIn = MidiDataFormats::MidiDataFormats_ByteStream;
    creationParams.DataFormatOut = MidiDataFormats::MidiDataFormats_UMP;
    creationParams.UmpGroupIndex = groupIndex;

    DWORD mmcssTaskId{};

    VERIFY_SUCCEEDED(transform->Initialize(deviceId.c_str(), &creationParams, &mmcssTaskId, this, 0, nullptr));

    uint16_t receivedMessageCount{ 0 };
    uint16_t expectedWordsIndex{ 0 };
    uint16_t countWordsCreated{ 0 };

    // set the callback 

    m_MidiInCallback = [&](PVOID payload, UINT payloadSize, LONGLONG /*payloadPosition*/, LONGLONG)
        {
            //std::cout << "callback" << std::endl;

            auto receivedWords = static_cast<uint32_t*>(payload);

            std::cout << "message received:" << std::endl;

            for (uint32_t i = 0; i < payloadSize / sizeof(uint32_t); i++)
            {
                countWordsCreated++;

                if (countWordsCreated > expectedWords.size())
                {
                    std::cout << "More words created than expected" << std::endl;
                    VERIFY_FAIL();
                }
                else
                {
                    std::cout
                        << std::dec << std::setw(3) << countWordsCreated
                        << ", Received: " << std::setfill('0') << std::setw(8) << std::hex << receivedWords[i]
                        << ", Expected: " << std::setfill('0') << std::setw(8) << std::hex << expectedWords[expectedWordsIndex]
                        << std::endl;

                    VERIFY_ARE_EQUAL(expectedWords[expectedWordsIndex], receivedWords[i]);

                    expectedWordsIndex++;
                }
            }


            std::cout << std::endl;

            receivedMessageCount++;
        };

    std::cout << "bytes sent: ";
    for (uint32_t i = 0; i < byteCount; i++)
    {
        std::cout << std::setfill('0') << std::setw(2) << std::hex << unsigned(sysexBytes[i]) << " ";
    }
    std::cout << std::endl;

    VERIFY_SUCCEEDED(transform->SendMidiMessage((void*)sysexBytes, byteCount, 0));

    // wait
    Sleep(1000);

    m_MidiInCallback = nullptr;

    transform->Shutdown();

    VERIFY_ARE_EQUAL(expectedMessageCount, receivedMessageCount);
}

//#include "Midi2"

void MidiBSToUMPTransformTests::TestBSToUMPWithSysEx7()
{
    uint8_t groupIndex{ 0 };

    uint8_t sysexBytes[] = {
        0xf0,
        0x0a, 0x0b, 0x0c, 0x0d, 0x0f, 0x1a,
        0x1b, 0x1c, 0x1d, 0x1e, 0x1f,
        0xf7 };

    std::vector<uint32_t> expectedWords
    {
        0x30160a0b, 0x0c0d0f1a,
        0x30351b1c, 0x1d1e1f00
    };

    InternalTestSysEx(groupIndex, sysexBytes, _countof(sysexBytes), 2, expectedWords);
}

void MidiBSToUMPTransformTests::TestTranslateFromBytesWithEmbeddedRealTimeAndSysEx7()
{
    uint8_t groupIndex{ 0 };
    const uint8_t sysexBytes[] =
    {
        0xf0,
        0x0a, 0x0b, 0x0c, 0x0d, 0x0f,           // 5-data-byte sysex message
        0xF8,                                   // real-time clock. because this arrives before previous message created, this ends up first
        0x2a, 0x2b, 0x2c, 0x2d, 0x2e, 0x2f,     // 6-data-byte sysex message
        0xf7,
    };

    std::vector<uint32_t> expectedWords
    {
        0x10f80000,                 // RT gets moved to first because full sysex message not yet generated
        0x30160a0b, 0x0c0d0f2a,
        0x30352b2c, 0x2d2e2f00
    };

    InternalTestSysEx(groupIndex, sysexBytes, _countof(sysexBytes), 3, expectedWords);
}


void MidiBSToUMPTransformTests::TestBSToUMPWithEmbeddedStartStopSysEx7()
{
    uint8_t groupIndex{ 0 };
    uint8_t sysexBytes[] =
    {
        0xf0, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x1a, 0x1b, 0x1c, 0x1d, 0x1e, 0x1f, 0xf7,       // 2 ump64 messages
        0xf0, 0x2a, 0x2b, 0x2c, 0x2d, 0x2e, 0x3a, 0x3b, 0x3c, 0x3d, 0x3e, 0x3f, 0xf7,       // 2 ump64 messages
        0xf0, 0x4a, 0x4b, 0x4c, 0x4d, 0x4e, 0xf7,                                           // 1 ump64 message
        0xf0, 0x5a, 0x5b, 0x5c, 0x5d, 0xf7,                                                 // 1 ump64 message
        0xf0, 0x6a, 0x6b, 0x6c, 0xf7,                                                       // 1 ump64 message
        0xf0, 0x7a, 0x7b, 0xf7                                                              // 1 ump64 message
    };


    std::vector<uint32_t> expectedWords
    {
        0x30160a0b, 0x0c0d0e1a,   0x30351b1c, 0x1d1e1f00,
        0x30162a2b, 0x2c2d2e3a,   0x30353b3c, 0x3d3e3f00,
        0x30054a4b, 0x4c4d4e00,
        0x30045a5b, 0x5c5d0000,
        0x30036a6b, 0x6c000000,
        0x30027a7b, 0x00000000
    };

    InternalTestSysEx(groupIndex, sysexBytes, _countof(sysexBytes), 8, expectedWords);

}


void MidiBSToUMPTransformTests::TestEmptySysEx7()
{
    uint8_t groupIndex{ 0 };
    uint8_t sysexBytes[] =
    {
        0xf0, 0xf7
    };

    std::vector<uint32_t> expectedWords{ 0x30000000, 0x00000000 };

    InternalTestSysEx(groupIndex, sysexBytes, _countof(sysexBytes), 1, expectedWords);

}

void MidiBSToUMPTransformTests::TestShortSysEx7()
{
    uint8_t groupIndex{ 0 };

    uint8_t sysexBytes[] =
    {
        0xf0, 0x0a, 0x0b, 0xf7
    };

    std::vector<uint32_t> expectedWords{ 0x30020a0b, 0x00000000 };

    InternalTestSysEx(groupIndex, sysexBytes, _countof(sysexBytes), 1, expectedWords);
}
