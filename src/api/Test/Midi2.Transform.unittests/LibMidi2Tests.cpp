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

#include "LibMidi2Tests.h"
#include "libmidi2\bytestreamToUMP.h"

_Use_decl_annotations_
void LibMidi2Tests::InternalTestSysEx(
    uint8_t groupIndex, 
    uint8_t const sysexBytes[], 
    uint32_t const byteCount, 
    std::vector<uint32_t> expectedWords)
{
    bytestreamToUMP bs2ump{};

    bs2ump.defaultGroup = groupIndex;

//    std::cout << "bytes parsed: " << std::endl;

    std::cout << "words created: " << std::endl;

    uint16_t wordsIndex{ 0 };
    uint16_t countWordsCreated{ 0 };

    for (uint32_t i = 0; i < byteCount; i++)
    {
        uint8_t b = sysexBytes[i];
        bs2ump.bytestreamParse(b);

//        std::cout << std::setfill('0') << std::setw(2) << std::hex << unsigned(b) << " ";

        while (bs2ump.availableUMP())
        {
            auto word = bs2ump.readUMP();
            countWordsCreated++;

            if (countWordsCreated <= expectedWords.size())
            {
                std::cout
                    << std::dec << std::setw(3) << countWordsCreated
                    << ", Generated: " << std::setfill('0') << std::setw(8) << std::hex << word
                    << ", Expected: " << std::setfill('0') << std::setw(8) << std::hex << expectedWords[wordsIndex]
                    << std::endl;

                VERIFY_ARE_EQUAL(expectedWords[wordsIndex], word);

                wordsIndex++;
            }
            else
            {
                VERIFY_FAIL();
            }
        }
    }
    //std::cout << std::endl << std::endl;

    std::cout << std::endl;

    VERIFY_ARE_EQUAL(expectedWords.size(), countWordsCreated);
}

void LibMidi2Tests::TestTranslateFromBytesWithSysEx7()
{
    const uint8_t sysexBytes[] = { 
        0xf0, 
        0x0a, 0x0b, 0x0c, 0x0d, 0x0f, 0x1a, 
        0x1b, 0x1c, 0x1d, 0x1e, 0x1f, 
        0xf7 };

    std::vector<uint32_t> expectedWords
    { 
        0x30160a0b, 0x0c0d0f1a, 
        0x30351b1c, 0x1d1e1f00 
    };

    InternalTestSysEx(0, sysexBytes, _countof(sysexBytes), expectedWords);
}

void LibMidi2Tests::TestTranslateFromBytesWithSysEx7AndNonZeroGroup()
{
    const uint8_t sysexBytes[] = {
        0xf0,
        0x0a, 0x0b, 0x0c, 0x0d, 0x0f, 0x1a,
        0x1b, 0x1c, 0x1d, 0x1e, 0x1f,
        0xf7 };

    std::vector<uint32_t> expectedWords
    { 
        0x39160a0b, 0x0c0d0f1a, 
        0x39351b1c, 0x1d1e1f00 
    };

    InternalTestSysEx(9, sysexBytes, _countof(sysexBytes), expectedWords);
}



void LibMidi2Tests::TestTranslateFromBytesWithEmbeddedRealTimeAndSysEx7()
{
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

    InternalTestSysEx(0, sysexBytes, _countof(sysexBytes), expectedWords);
}


void LibMidi2Tests::TestTranslateFromBytesWithEmbeddedStartStopSysEx7()
{
    const uint8_t sysexBytes[] =
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

    InternalTestSysEx(0, sysexBytes, _countof(sysexBytes), expectedWords);
}

void LibMidi2Tests::TestTranslateFromBytesWithLongSysEx7()
{
    //const uint8_t numUmpsToCreate = 5;
    //const uint8_t byteCount = 6 * numUmpsToCreate + 2;

    //uint8_t sysexBytes[byteCount];

    //for (int i = 1; i < byteCount - 1; i++)
    //{
    //    sysexBytes[i] = i % 0x7F;
    //}

    //sysexBytes[0] = 0xf0;
    //sysexBytes[byteCount - 1] = 0xf7;

    //InternalTestSysEx(0, sysexBytes, byteCount, expectedWords);
}


void LibMidi2Tests::TestTranslateFromBytesWithEmptySysEx7()
{
    const uint8_t sysexBytes[] =
    {
        0xf0, 0xf7
    };

    std::vector<uint32_t> expectedWords{ 0x30000000, 0x00000000 };

    InternalTestSysEx(0, sysexBytes, _countof(sysexBytes), expectedWords);
}

void LibMidi2Tests::TestTranslateFromBytesWithShortSysEx7()
{
    const uint8_t sysexBytes[] =
    {
        0xf0, 0x0a, 0x0b, 0xf7
    };

    std::vector<uint32_t> expectedWords{ 0x30020a0b, 0x00000000 };

    InternalTestSysEx(0, sysexBytes, _countof(sysexBytes), expectedWords);
}
