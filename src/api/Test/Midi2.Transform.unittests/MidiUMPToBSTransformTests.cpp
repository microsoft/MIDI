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

#include "MidiUMPToBSTransformTests.h"

#include "Midi2UMP2BSTransform.h"
#include "Midi2UMP2BSTransform_i.c"


_Use_decl_annotations_
void MidiUMPToBSTransformTests::InternalTestMessages(
    std::vector<uint32_t> const words,
    std::vector<uint8_t> const expectedBytes
)
{
    wil::com_ptr_nothrow<IMidiTransform> transformLib;
    wil::com_ptr_nothrow<IMidiDataTransform> transform;

    auto iid = __uuidof(Midi2UMP2BSTransform);

    VERIFY_SUCCEEDED(CoCreateInstance(iid, nullptr, CLSCTX_ALL, IID_PPV_ARGS(&transformLib)));

    VERIFY_SUCCEEDED(transformLib->Activate(__uuidof(IMidiDataTransform), (void**)(&transform)));

    std::wstring deviceId{ L"foobarbaz" };

    TRANSFORMCREATIONPARAMS creationParams{};
    creationParams.DataFormatIn = MidiDataFormats::MidiDataFormats_UMP;
    creationParams.DataFormatOut = MidiDataFormats::MidiDataFormats_ByteStream;

    DWORD mmcssTaskId{};

    VERIFY_SUCCEEDED(transform->Initialize(deviceId.c_str(), &creationParams, &mmcssTaskId, this, 0, nullptr));

    uint16_t receivedMessageCount{ 0 };
    uint16_t expectedBytesIndex{ 0 };
    uint16_t countBytesCreated{ 0 };

    // set the callback 

    m_MidiInCallback = [&](PVOID payload, UINT payloadSize, LONGLONG /*payloadPosition*/, LONGLONG)
        {
            //std::cout << "callback" << std::endl;

            auto receivedBytes = static_cast<uint8_t*>(payload);

            std::cout << "message received:" << std::endl;

            for (uint32_t i = 0; i < payloadSize; i++)
            {
                countBytesCreated++;

                if (countBytesCreated > expectedBytes.size())
                {
                    std::cout << "More bytes created than expected" << std::endl;
                    VERIFY_FAIL();
                }
                else
                {
                    std::cout
                        << std::setfill('0') << std::setw(2) << std::hex << (uint16_t)receivedBytes[i]
                        << "(" << std::setfill('0') << std::setw(2) << std::hex << (uint16_t)(expectedBytes[expectedBytesIndex]) << ") ";


                    VERIFY_ARE_EQUAL(expectedBytes[expectedBytesIndex], receivedBytes[i]);

                    expectedBytesIndex++;
                }
            }


            std::cout << std::endl;

            receivedMessageCount++;
        };

    //std::cout << "bytes sent: ";
    //for (uint32_t i = 0; i < byteCount; i++)
    //{
    //    std::cout << std::setfill('0') << std::setw(2) << std::hex << unsigned(sysexBytes[i]) << " ";
    //}
    //std::cout << std::endl;

    VERIFY_SUCCEEDED(transform->SendMidiMessage(MessageOptionFlags_None, (PVOID)words.data(), (UINT)(words.size() * sizeof(uint32_t)), 0));

    // wait (TODO: This should be an event, not a sleep)
    Sleep(1000);

    m_MidiInCallback = nullptr;

    transform->Shutdown();
}


void MidiUMPToBSTransformTests::TestChannelVoiceMessages()
{
    std::vector<uint32_t> input =
    {
        0x20E01230,
        0x20E11231,
        0x20E21232,
        0x20E31233,
        0x20E41234,
        0x20E51235,
        0x20E61236,
        0x20E71237,
        0x20E81238,
        0x20E91239,
        0x20EA123A,
        0x20EB123B,
        0x20EC123C,
        0x20ED123D,
        0x20EE123E,
        0x20EF123F,

        0x20C01000,
        0x20C11100,
        0x20C21200,
        0x20C31300,
        0x20C41400,
        0x20C51500,
        0x20C61600,
        0x20C71700,
        0x20C81800,
        0x20C91900,
        0x20CA1A00,
        0x20CB1B00,
        0x20CC1C00,
        0x20CD1D00,
        0x20CE1E00,
        0x20CF1F00,
    };

    std::vector<uint8_t> expectedOutput =
    {
        0xE0, 0x12, 0x30,
        0xE1, 0x12, 0x31,
        0xE2, 0x12, 0x32,
        0xE3, 0x12, 0x33,
        0xE4, 0x12, 0x34,
        0xE5, 0x12, 0x35,
        0xE6, 0x12, 0x36,
        0xE7, 0x12, 0x37,
        0xE8, 0x12, 0x38,
        0xE9, 0x12, 0x39,
        0xEA, 0x12, 0x3A,
        0xEB, 0x12, 0x3B,
        0xEC, 0x12, 0x3C,
        0xED, 0x12, 0x3D,
        0xEE, 0x12, 0x3E,
        0xEF, 0x12, 0x3F,

        0xC0, 0x10,
        0xC1, 0x11,
        0xC2, 0x12,
        0xC3, 0x13,
        0xC4, 0x14,
        0xC5, 0x15,
        0xC6, 0x16,
        0xC7, 0x17,
        0xC8, 0x18,
        0xC9, 0x19,
        0xCA, 0x1A,
        0xCB, 0x1B,
        0xCC, 0x1C,
        0xCD, 0x1D,
        0xCE, 0x1E,
        0xCF, 0x1F,
    };

    InternalTestMessages(input, expectedOutput);
}
