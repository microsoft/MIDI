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
void MidiBSToUMPTransformTests::InternalTestBytes(
    uint8_t const groupIndex,
    uint8_t const bytes[], 
    uint32_t const byteCount, 
    /*uint16_t const expectedMessageCount, */
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

                std::cout
                    << std::dec << std::setw(3) << countWordsCreated
                    << ", Received: " << std::setfill('0') << std::setw(8) << std::hex << receivedWords[i];

                if (expectedWordsIndex < expectedWords.size())
                {

                    std::cout 
                        << ", Expected: " << std::setfill('0') << std::setw(8) << std::hex << expectedWords[expectedWordsIndex];
                }
                else
                {
                    std::cout << ", Expected: <none>";
                }

                std::cout << std::endl;

                VERIFY_ARE_EQUAL(expectedWords[expectedWordsIndex], receivedWords[i]);

                expectedWordsIndex++;
            }

            VERIFY_ARE_EQUAL(expectedWords.size(), countWordsCreated);

            std::cout << std::endl;

        };

    std::cout << "bytes sent: ";
    for (uint32_t i = 0; i < byteCount; i++)
    {
        std::cout << std::setfill('0') << std::setw(2) << std::hex << unsigned(bytes[i]) << " ";
    }
    std::cout << std::endl;

    // MessageOptionFlags_None means no running status
    VERIFY_SUCCEEDED(transform->SendMidiMessage(MessageOptionFlags_None, (void*)bytes, byteCount, 0));

    // wait
    Sleep(1000);

    m_MidiInCallback = nullptr;

    transform->Shutdown();

    //VERIFY_ARE_EQUAL(expectedMessageCount, receivedMessageCount);
}


_Use_decl_annotations_
void MidiBSToUMPTransformTests::InternalTestSysEx7InChunks(
    uint8_t const groupIndex,
    uint8_t const bytes[],
    uint32_t const byteCount,
    std::vector<uint32_t> const chunkSizePattern
)
{
    VERIFY_IS_GREATER_THAN(chunkSizePattern.size(), (size_t)0);
    VERIFY_IS_GREATER_THAN(byteCount, (uint32_t)2);
    VERIFY_ARE_EQUAL((uint8_t)0xF0, bytes[0]);
    VERIFY_ARE_EQUAL((uint8_t)0xF7, bytes[byteCount - 1]);

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

    std::vector<uint32_t> receivedWords{};

    m_MidiInCallback = [&](PVOID payload, UINT payloadSize, LONGLONG, LONGLONG)
        {
            auto words = static_cast<uint32_t*>(payload);

            for (uint32_t i = 0; i < payloadSize / sizeof(uint32_t); i++)
            {
                receivedWords.push_back(words[i]);
            }
        };

    uint32_t offset{ 0 };
    uint32_t patternIndex{ 0 };
    uint32_t chunkCount{ 0 };

    while (offset < byteCount)
    {
        auto remaining = byteCount - offset;
        auto chunkSize = chunkSizePattern[patternIndex % chunkSizePattern.size()];

        if (chunkSize == 0 || chunkSize > remaining)
        {
            chunkSize = remaining;
        }

        VERIFY_SUCCEEDED(transform->SendMidiMessage(MessageOptionFlags_None, (void*)(bytes + offset), chunkSize, 0));

        offset += chunkSize;
        patternIndex++;
        chunkCount++;
    }

    Sleep(1000);

    m_MidiInCallback = nullptr;
    transform->Shutdown();

    // walk the emitted SysEx7 packets and rebuild the payload the way any client must

    std::vector<uint8_t> reassembled{};
    uint32_t packetCount{ 0 };
    bool sawStart{ false };
    bool sawEnd{ false };

    VERIFY_ARE_EQUAL((size_t)0, receivedWords.size() % 2);

    for (size_t i = 0; i + 1 < receivedWords.size(); i += 2)
    {
        auto word0 = receivedWords[i];
        auto word1 = receivedWords[i + 1];

        VERIFY_ARE_EQUAL((uint32_t)0x3, (word0 >> 28) & 0xF);
        VERIFY_ARE_EQUAL((uint32_t)groupIndex, (word0 >> 24) & 0xF);

        auto status = (word0 >> 20) & 0xF;
        auto dataByteCount = (word0 >> 16) & 0xF;

        VERIFY_IS_LESS_THAN_OR_EQUAL(dataByteCount, (uint32_t)6);

        if (status == 0x0 || status == 0x1) { sawStart = true; }
        if (status == 0x0 || status == 0x3) { sawEnd = true; }

        uint8_t const packetBytes[]
        {
            (uint8_t)((word0 >> 8) & 0xFF),
            (uint8_t)(word0 & 0xFF),
            (uint8_t)((word1 >> 24) & 0xFF),
            (uint8_t)((word1 >> 16) & 0xFF),
            (uint8_t)((word1 >> 8) & 0xFF),
            (uint8_t)(word1 & 0xFF),
        };

        for (uint32_t b = 0; b < dataByteCount; b++)
        {
            reassembled.push_back(packetBytes[b]);
        }

        packetCount++;
    }

    std::vector<uint8_t> const expectedPayload(bytes + 1, bytes + (byteCount - 1));

    {
        std::wostringstream message{};
        message
            << L"Sent " << byteCount << L" bytes in " << chunkCount << L" chunks. Received "
            << packetCount << L" SysEx7 packets carrying " << reassembled.size()
            << L" of an expected " << expectedPayload.size() << L" payload bytes.";

        WEX::Logging::Log::Comment(message.str().c_str());
    }

    VERIFY_IS_TRUE(sawStart);
    VERIFY_IS_TRUE(sawEnd);

    // the payload must survive chunking exactly. how it was packed is a separate question
    VERIFY_ARE_EQUAL(expectedPayload.size(), reassembled.size());

    for (size_t i = 0; i < expectedPayload.size(); i++)
    {
        if (expectedPayload[i] != reassembled[i])
        {
            std::wostringstream message{};
            message << L"Payload mismatch at byte " << i << L".";
            WEX::Logging::Log::Error(message.str().c_str());
        }

        VERIFY_ARE_EQUAL(expectedPayload[i], reassembled[i]);
    }

    // fragmentation is legal, so this is reported rather than failed. the floor is
    // the number of packets needed if every packet but the last were filled
    auto minimumPacketCount = (uint32_t)((expectedPayload.size() + 5) / 6);

    if (packetCount > minimumPacketCount)
    {
        std::wostringstream message{};
        message
            << L"Packing is not optimal: " << packetCount << L" packets used where "
            << minimumPacketCount << L" would do (" << (packetCount - minimumPacketCount)
            << L" extra, " << ((packetCount * 6) - reassembled.size()) << L" unused payload bytes). "
            << L"Each chunk boundary flushes a partly filled packet.";

        WEX::Logging::Log::Warning(message.str().c_str());
    }
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

    InternalTestBytes(groupIndex, sysexBytes, _countof(sysexBytes), expectedWords);
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

    InternalTestBytes(groupIndex, sysexBytes, _countof(sysexBytes), expectedWords);
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

    InternalTestBytes(groupIndex, sysexBytes, _countof(sysexBytes), expectedWords);

}


void MidiBSToUMPTransformTests::TestEmptySysEx7()
{
    uint8_t groupIndex{ 0 };
    uint8_t sysexBytes[] =
    {
        0xf0, 0xf7
    };

    std::vector<uint32_t> expectedWords{ 0x30000000, 0x00000000 };

    InternalTestBytes(groupIndex, sysexBytes, _countof(sysexBytes), expectedWords);

}

void MidiBSToUMPTransformTests::TestShortSysEx7()
{
    uint8_t groupIndex{ 0 };

    uint8_t sysexBytes[] =
    {
        0xf0, 0x0a, 0x0b, 0xf7
    };

    std::vector<uint32_t> expectedWords{ 0x30020a0b, 0x00000000 };

    InternalTestBytes(groupIndex, sysexBytes, _countof(sysexBytes), expectedWords);
}

void MidiBSToUMPTransformTests::TestTimingClock()
{
    uint8_t groupIndex{ 0 };

    uint8_t bytes[] =
    {
        0xf8
    };

    std::vector<uint32_t> expectedWords{ 0x10F80000 };

    InternalTestBytes(groupIndex, bytes, _countof(bytes), expectedWords);
}


void MidiBSToUMPTransformTests::TestTimingClockPadded()
{
    uint8_t groupIndex{ 0 };

    uint8_t bytes[] =
    {
        // 12 bytes total. This reflects what happens with inMusic drivers
        0xf8, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
    };

    std::vector<uint32_t> expectedWords{ 0x10F80000 };

    InternalTestBytes(groupIndex, bytes, _countof(bytes), expectedWords);
}

// if this fails, then it means running status is enabled.
void MidiBSToUMPTransformTests::TestCCPadded()
{
    uint8_t groupIndex{ 0 };

    uint8_t bytes[] =
    {
        // 12 bytes total. This reflects what happens with inMusic drivers
        0xb0, 0x10, 0x07, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
    };

    std::vector<uint32_t> expectedWords{ 0x20b01007 };

    InternalTestBytes(groupIndex, bytes, _countof(bytes), expectedWords);
}

void MidiBSToUMPTransformTests::TestIssueGithub1040CorruptedIncomingSysExIdeal()
{

    uint8_t groupIndex{ 0 };

    uint8_t bytes[] =
    {
        // F0 00 02 17 0F 02 02 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 09 09 09 09 09 09 09 09 09 09 09 09 09 09 09 09 F7

        0xF0, 0x00, 0x02, 0x17, 0x0F, 0x02, 
        0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
        0x00, 0x00, 0x00, 0x00, 0x00, 0x09, 
        0x09, 0x09, 0x09, 0x09, 0x09, 0x09, 
        0x09, 0x09, 0x09, 0x09, 0x09, 0x09,
        0x09, 0x09, 0x09, 0xF7
    };

    std::vector<uint32_t> expectedWords
    {  
        0x30160002, 0x170F0202,
        0x30260000, 0x00000000,
        0x30260000, 0x00000000,
        0x30260000, 0x00000000,
        0x30260000, 0x00000000,
        0x30260000, 0x00000000,
        0x30260000, 0x00000000,
        0x30260000, 0x00000000,
        0x30260000, 0x00000000,
        0x30260000, 0x00000000,
        0x30260000, 0x00000000,
        0x30260000, 0x00000000,
        0x30260000, 0x00000000,
        0x30260000, 0x00000000,
        0x30260000, 0x00000000,
        0x30260000, 0x00000000,
        0x30260000, 0x00000000,
        0x30260000, 0x00000000,
        0x30260000, 0x00000000,
        0x30260000, 0x00000909,
        0x30260909, 0x09090909,
        0x30260909, 0x09090909,
        0x30320909, 0x00000000
    };

    InternalTestBytes(groupIndex, bytes, _countof(bytes), expectedWords);

}

// Same SysEx as the ideal case, but delivered the way it actually arrives from the
// device: usbaudio.sys hands up USB-MIDI 1.0 packets that carry three data bytes each,
// so every inbound buffer is a multiple of three and rarely lines up with the six data
// bytes a SysEx7 packet holds. The chunk sizes here were inferred from the short-packet
// boundaries in the capture attached to the issue.
void MidiBSToUMPTransformTests::TestIssueGithub1040SysEx7SurvivesUsbSizedChunkedDelivery()
{
    uint8_t groupIndex{ 0 };

    uint8_t bytes[] =
    {
        0xF0, 0x00, 0x02, 0x17, 0x0F, 0x02,
        0x02, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x09,
        0x09, 0x09, 0x09, 0x09, 0x09, 0x09,
        0x09, 0x09, 0x09, 0x09, 0x09, 0x09,
        0x09, 0x09, 0x09, 0xF7
    };

    std::vector<uint32_t> chunkSizePattern{ 9, 3, 9, 21, 9, 15, 9, 27, 9, 3 };

    InternalTestSysEx7InChunks(groupIndex, bytes, _countof(bytes), chunkSizePattern);
}

// Issue 1040: a device that abandons a dump part way through and immediately starts a new
// one. Once more than six bytes have accumulated the message is in the Continue state, so
// terminating it has to emit an End. The Start-state case is covered by
// TestSysEx7StartWithPendingDataStartsNewMessage, which takes the other branch and emits
// a Complete instead.
void MidiBSToUMPTransformTests::TestSysEx7AbandonedMidMessageIsTerminated()
{
    uint8_t groupIndex{ 0 };

    uint8_t bytes[] =
    {
        0xF0, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17,
        0xF0, 0x31, 0x32, 0x33, 0x34, 0xF7
    };

    std::vector<uint32_t> expectedWords
    {
        0x30161112, 0x13141516,
        0x30311700, 0x00000000,
        0x30043132, 0x33340000
    };

    InternalTestBytes(groupIndex, bytes, _countof(bytes), expectedWords);
}


void MidiBSToUMPTransformTests::TestBasicMalformedSysex()
{
    // SysEx 7 UMP status: 0x0 - complete message in one UMP. 0x1: start, 0x2: continue, 0x3: end


    uint8_t groupIndex{ 0 };

    uint8_t bytes[] =
    {
        0xF0, 0x01, 0x02, 0x03, 0x04, 0x05,                     // Scenario 1: 5 data bytes no f7
        0xF0, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18,   // Scenario 2: 8 data bytes no f7
        0xF0, 0x21, 0x22, 0x23, 0x24, 0x25, 0xF7, 0xF7,         // Scenario 3: 3 data bytes, two f7, expectation is extra F7 is ignored
        0xF0, 0xF0, 0x31, 0x32, 0x33, 0x34, 0xF7,               // Scenario 4: 4 data bytes, two f0, expectation is extra F0 is ignored.
    };

    std::vector<uint32_t> expectedWords
    {
        0x30050102, 0x03040500,                             // Scenario 1: SysEx Complete with 5 data bytes. The F0 that opens scenario 2
                                                            //             terminates this one, so it is complete rather than a dangling Start.

        0x30161112, 0x13141516, 0x30321718, 0x00000000,     // Scenario 2: SysEx Start + End with 8 data bytes. Terminated by the F0 that
                                                            //             opens scenario 3, so the second packet is End rather than Continue.

        0x30052122, 0x23242500,                             // Scenario 3: SysEx Complete in one message with 5 data bytes.
                                                            //             The extra F7 is ignored.

        0x30043132, 0x33340000,                             // Scenario 4:  SysEx Complete in one message with 4 data bytes. Nothing had
                                                            //              accumulated when the second F0 arrived, so it is ignored.
    };

    InternalTestBytes(groupIndex, bytes, _countof(bytes), expectedWords);

}


// A Channel Voice status byte arriving inside an open SysEx must terminate that SysEx.
// M2-104-UM 7.7.1: only System Real-Time may be embedded in a SysEx, everything else ends it.
// Reported against libMIDI2 in https://github.com/midi2-dev/AM_MIDI2.0Lib/issues/39
void MidiBSToUMPTransformTests::TestStatusByteTerminatesOpenSysEx7()
{
    uint8_t groupIndex{ 0 };

    uint8_t bytes[] =
    {
        0xF0, 0x11, 0x22,           // SysEx start, two data bytes, never ended
        0x90, 0x3C, 0x7F,           // note on. Must close the SysEx above, not be swallowed by it
        0x90, 0x3E, 0x7F            // note on
    };

    std::vector<uint32_t> expectedWords
    {
        0x30021122, 0x00000000,     // SysEx Complete, 2 data bytes, terminated by the 0x90
        0x20903C7F,                 // both notes survive
        0x20903E7F
    };

    InternalTestBytes(groupIndex, bytes, _countof(bytes), expectedWords);
}


// Same as above, but a SysEx end byte turns up later. Once the status byte has closed the
// SysEx the stray 0xF7 has nothing to end and must be ignored, and the note bytes must not
// have leaked into the SysEx payload.
void MidiBSToUMPTransformTests::TestStatusByteTerminatesOpenSysEx7WithLateEndByte()
{
    uint8_t groupIndex{ 0 };

    uint8_t bytes[] =
    {
        0xF0, 0x11, 0x22,
        0x90, 0x3C, 0x7F,
        0xF7,                       // stray, the SysEx already ended at the 0x90
        0x90, 0x3E, 0x7F
    };

    std::vector<uint32_t> expectedWords
    {
        0x30021122, 0x00000000,
        0x20903C7F,
        0x20903E7F
    };

    InternalTestBytes(groupIndex, bytes, _countof(bytes), expectedWords);
}


// A second SysEx start with data already accumulated must close the first fragment as its own
// message rather than folding both payloads into one. TestBasicMalformedSysex scenario 4 covers
// the adjacent F0 F0 case, where there is nothing accumulated and the second F0 is ignored.
void MidiBSToUMPTransformTests::TestSysEx7StartWithPendingDataStartsNewMessage()
{
    uint8_t groupIndex{ 0 };

    uint8_t bytes[] =
    {
        0xF0, 0x11, 0x12,           // abandoned fragment
        0xF0, 0x31, 0x32, 0x33, 0x34, 0xF7
    };

    std::vector<uint32_t> expectedWords
    {
        0x30021112, 0x00000000,     // fragment closed as SysEx Complete, 2 data bytes
        0x30043132, 0x33340000      // second message, complete, 4 data bytes
    };

    InternalTestBytes(groupIndex, bytes, _countof(bytes), expectedWords);
}



bool MidiBSToUMPTransformTests::ClassSetup()
{
    PrintStagingStates();

    WEX::TestExecution::SetVerifyOutput verifySettings(WEX::TestExecution::VerifyOutputSettings::LogOnlyFailures);

    return true;
}

