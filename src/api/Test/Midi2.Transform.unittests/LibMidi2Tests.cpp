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
#include "libmidi2\umpToBytestream.h"

_Use_decl_annotations_
void LibMidi2Tests::InternalTranslateMidi1BytesToUmpWords(
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

_Use_decl_annotations_
void LibMidi2Tests::InternalTranslateUmpWordsToMidi1Bytes(
    std::vector<uint32_t> messageWords,
    std::vector<uint8_t> const expectedBytes)
{
    umpToBytestream ump2bs{};

    std::vector<uint8_t> generatedBytes;
    generatedBytes.reserve(expectedBytes.size());

    std::cout << "bytes created: " << std::endl;

    uint16_t generatedBytesIndex{ 0 };
    uint32_t countBytesCreated{ 0 };

    std::cout << std::endl;

    for (uint32_t i = 0; i < messageWords.size(); i++)
    {
        std::cout << std::setfill('0') << std::setw(8) << std::hex << messageWords[i] << " ";

        ump2bs.UMPStreamParse(messageWords[i]);

        if (ump2bs.availableBS())
        {
            std::cout << std::endl;

            while (ump2bs.availableBS())
            {
                auto b = ump2bs.readBS();
                countBytesCreated++;

                if (countBytesCreated <= expectedBytes.size())
                {
                    std::cout
                        << std::setfill('0') << std::setw(2) << std::hex << (uint16_t)b
                        << "(" << std::setfill('0') << std::setw(2) << std::hex << (uint16_t)(expectedBytes[generatedBytesIndex]) << ") ";

                    VERIFY_ARE_EQUAL(expectedBytes[generatedBytesIndex], b);

                    generatedBytesIndex++;
                }
                else
                {
                    std::cout << std::endl;
                    VERIFY_FAIL();
                }
            }

            std::cout << std::endl;
        }
    }
    //std::cout << std::endl << std::endl;

    std::cout << std::endl;

    VERIFY_ARE_EQUAL(expectedBytes.size(), countBytesCreated);
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

    InternalTranslateMidi1BytesToUmpWords(0, sysexBytes, _countof(sysexBytes), expectedWords);
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

    InternalTranslateMidi1BytesToUmpWords(9, sysexBytes, _countof(sysexBytes), expectedWords);
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

    InternalTranslateMidi1BytesToUmpWords(0, sysexBytes, _countof(sysexBytes), expectedWords);
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

    InternalTranslateMidi1BytesToUmpWords(0, sysexBytes, _countof(sysexBytes), expectedWords);
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

    //InternalTranslateBytesToUmpWords(0, sysexBytes, byteCount, expectedWords);
}


void LibMidi2Tests::TestProgramChangeFromBytes()
{
    const uint8_t bytes[] =
    {
        0xC0, 0x12,
        0xB0, 0x00, 0x40,
        0xC0, 0x34,
        0xB0, 0x20, 0x30,
        0xC0, 0x56,
        0xB0, 0x20, 0x20,
        0xC0, 0x78,
    };

    std::vector<uint32_t> expectedWords
    { 
        0x20C01200,
        0x20B00040,
        0x20C03400,
        0x20B02030,
        0x20C05600,
        0x20B02020,
        0x20C07800,
    };

    InternalTranslateMidi1BytesToUmpWords(0, bytes, _countof(bytes), expectedWords);
}

void LibMidi2Tests::TestProgramChangeToBytes()
{

    std::vector<uint32_t> words
    {
        0x20C01200,
        0x20B00040,
        0x20C03400,
        0x20B02030,
        0x20C05600,
        0x20B02020,
        0x20C07800,
    };

    std::vector<uint8_t> bytes =
    {
        0xC0, 0x12,
        0xB0, 0x00, 0x40,
        0xC0, 0x34,
        0xB0, 0x20, 0x30,
        0xC0, 0x56,
        0xB0, 0x20, 0x20,
        0xC0, 0x78,
    };

    InternalTranslateUmpWordsToMidi1Bytes(words, bytes);
}







void LibMidi2Tests::TestTranslateFromBytesWithEmptySysEx7()
{
    const uint8_t sysexBytes[] =
    {
        0xf0, 0xf7
    };

    std::vector<uint32_t> expectedWords{ 0x30000000, 0x00000000 };

    InternalTranslateMidi1BytesToUmpWords(0, sysexBytes, _countof(sysexBytes), expectedWords);
}

void LibMidi2Tests::TestTranslateFromBytesWithShortSysEx7()
{
    const uint8_t sysexBytes[] =
    {
        0xf0, 0x0a, 0x0b, 0xf7
    };

    std::vector<uint32_t> expectedWords{ 0x30020a0b, 0x00000000 };

    InternalTranslateMidi1BytesToUmpWords(0, sysexBytes, _countof(sysexBytes), expectedWords);
}



void LibMidi2Tests::TestTranslateToBytesWithSysEx7()
{
    std::vector<uint32_t> input =
    {
        0x30010100, 0x00000000,
        0x30160801, 0x02030405,
        0x30320607, 0x00000000
    };

    std::vector<uint8_t> expectedOutput =
    {
        0xF0, 0x01, 0xF7,
        0xF0, 0x08, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0xF7
    };

    InternalTranslateUmpWordsToMidi1Bytes(input, expectedOutput);

}

void LibMidi2Tests::TestTranslateToBytesWithInterruptedSysEx7()
{
    std::vector<uint32_t> input =
    {
        0x30010100, 0x00000000,
        0x30160801, 0x02030405,
        0x10FE0000,
        0x30320607, 0x00000000
    };

    std::vector<uint8_t> expectedOutput =
    {
        0xF0, 0x01, 0xF7,
        0xF0, 0x08, 0x01, 0x02, 0x03, 0x04, 0x05, 
        0xFE, 
        0x06, 0x07, 0xF7
    };

    InternalTranslateUmpWordsToMidi1Bytes(input, expectedOutput);
}

void LibMidi2Tests::TestTranslateToBytesWithCanceledSysEx7()
{
    std::vector<uint32_t> input =
    {
        0x30010100, 0x00000000,
        0x30160801, 0x02030405,
        0x20905050,
        0x20806060,
        0x30320607, 0x00000000
    };

    // verifying this behavior stays consistent. LibMidi2 will pick up the rest
    // of the sysex, and will just embed the note on/off messages inside it
    std::vector<uint8_t> expectedOutput =
    {
        0xF0, 0x01, 0xF7,
        0xF0, 0x08, 0x01, 0x02, 0x03, 0x04, 0x05,
        0x90, 0x50, 0x50,   // note off
        0x80, 0x60, 0x60,   // note on
        0x06, 0x07, 0xF7    // rest of sysex, including F7
    };

    InternalTranslateUmpWordsToMidi1Bytes(input, expectedOutput);

}