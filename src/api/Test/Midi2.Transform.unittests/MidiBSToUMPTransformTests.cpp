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
void MidiBSToUMPTransformTests::InternalTestSysEx(uint8_t sysexBytes[], uint32_t byteCount, uint16_t expectedMessageCount)
{
    wil::com_ptr_nothrow<IMidiTransform> transformLib;
    wil::com_ptr_nothrow<IMidiDataTransform> transform;

    auto iid = __uuidof(Midi2BS2UMPTransform);

    VERIFY_SUCCEEDED(CoCreateInstance(iid, nullptr, CLSCTX_ALL, IID_PPV_ARGS(&transformLib)));

    VERIFY_SUCCEEDED(transformLib->Activate(__uuidof(IMidiDataTransform), (void**)(&transform)));

    std::wstring deviceId{ L"foobarbaz" };

    TRANSFORMCREATIONPARAMS creationParams{};
    creationParams.DataFormatIn = MidiDataFormat::MidiDataFormat_ByteStream;
    creationParams.DataFormatOut = MidiDataFormat::MidiDataFormat_UMP;
    creationParams.InstanceConfigurationJsonData = nullptr;

    DWORD mmcssTaskId{};

    VERIFY_SUCCEEDED(transform->Initialize(deviceId.c_str(), &creationParams, &mmcssTaskId, this, 0, nullptr));

    uint16_t receivedMessageCount{ 0 };

    // set the callback 

    m_MidiInCallback = [&](PVOID payload, UINT32 payloadSize, LONGLONG /*payloadPosition*/, LONGLONG)
        {
            //std::cout << "callback" << std::endl;

            uint32_t* words = (uint32_t*)payload;

            std::cout << "words received: ";

            for (uint32_t i = 0; i < payloadSize / sizeof(uint32_t); i++)
            {
                std::cout << std::setfill('0') << std::setw(8) << std::hex << words[i] << " ";
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

    VERIFY_SUCCEEDED(transform->SendMidiMessage((void*)&sysexBytes, byteCount, 0));

    // wait
    Sleep(1000);

    m_MidiInCallback = nullptr;

    transform->Cleanup();

    VERIFY_ARE_EQUAL(expectedMessageCount, receivedMessageCount);
}

//#include "Midi2"

void MidiBSToUMPTransformTests::TestBSToUMPWithSysEx7()
{
    uint8_t sysexBytes[] = { 0xf0, 0x0a, 0x0b, 0x0c, 0x0d, 0x0f, 0x1a, 0x1b, 0x1c, 0x1d, 0x1e, 0x1f, 0xf7 };
    
    InternalTestSysEx(sysexBytes, _countof(sysexBytes), 2);
}


void MidiBSToUMPTransformTests::TestBSToUMPWithEmbeddedStartStopSysEx7()
{
    uint8_t sysexBytes[] =
    {
        0xf0, 0x0a, 0x0b, 0x0c, 0x0d, 0x0f, 0x1a, 0x1b, 0x1c, 0x1d, 0x1e, 0x1f, 0xf7,       // 2 messages
        0xf0, 0x2a, 0x2b, 0x2c, 0x2d, 0x2f, 0x3a, 0x3b, 0x3c, 0x3d, 0x3e, 0x3f, 0xf7,       // 2 messages
        0xf0, 0x4a, 0x4b, 0x4c, 0x4d, 0x4f, 0xf7,                                           // 1 message
        0xf0, 0x5a, 0x5b, 0x5c, 0x5d, 0xf7,                                                 // 1 message
        0xf0, 0x6a, 0x6b, 0x6c, 0xf7,                                                       // 1 message
        0xf0, 0x7a, 0x7b, 0xf7                                                              // 1 message
    };

    InternalTestSysEx(sysexBytes, _countof(sysexBytes), 8);

}

void MidiBSToUMPTransformTests::TestLongSysEx7()
{
    const uint8_t byteCount = 6 * 5;

    uint8_t sysexBytes[byteCount];

    for (int i = 1; i < byteCount - 1; i++)
    {
        sysexBytes[i] = i % 0x7F;
    }

    sysexBytes[0] = 0xf0;
    sysexBytes[byteCount - 1] = 0xf7;

    uint16_t expectedMessageCount = ((byteCount - 2) / 6) + ((byteCount - 2) % 6);

    InternalTestSysEx(sysexBytes, byteCount, expectedMessageCount);

}


void MidiBSToUMPTransformTests::TestEmptySysEx7()
{
    uint8_t sysexBytes[] =
    {
        0xf0, 0xf7
    };

    InternalTestSysEx(sysexBytes, _countof(sysexBytes), 1);

}

void MidiBSToUMPTransformTests::TestShortSysEx7()
{
    uint8_t sysexBytes[] =
    {
        0xf0, 0x0a, 0x0b, 0xf7
    };

    InternalTestSysEx(sysexBytes, _countof(sysexBytes), 1);

}
