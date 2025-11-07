// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

#include "pch.h"


#include "UmpIteratorTests.h"
using namespace WindowsMidiServicesInternal;

void UmpIteratorTests::TestBasicIteration()
{
    uint32_t words[] =
    {
        0x20000001,
        0x20000002,
        0x20000003,
        0x20000004,
        0x20000005,
        0x20000006,
        0x20000007,
        0x20000008,
        0x20000009,
    };

    uint32_t wordCount = ARRAYSIZE(words);

    UmpBufferIterator bufferIterator(words, wordCount);

    uint32_t i = 0;
    for (auto it = bufferIterator.begin(); it < bufferIterator.end(); ++it)
    {
        VERIFY_ARE_EQUAL(*it, words[i]);

        VERIFY_IS_TRUE(it.CurrentMessageSeemsComplete());
        VERIFY_IS_TRUE(it.CurrentMessageHasGroupField());

        i++;
    }

    VERIFY_ARE_EQUAL(i, ARRAYSIZE(words));
}

void UmpIteratorTests::TestMixedIteration()
{
    // test a mix of message sizes
    uint32_t words[] =
    {
        0x20000001,
        0x20000002,
        0x20000003,
        0x20000004,
        0x20000005,
        0x20000006,
        0x40000001, 0x01234567,
        0x40000002, 0x02345678,
        0x40000003, 0x03456789,
        0x20000007,
        0x20000008,
        0xF0000001, 0x18675309, 0x01010101, 0x02020202,
        0xF0000002, 0x28675309, 0x21010101, 0x22020202,
        0x20000009,
        0xF0000003, 0x38675309, 0x31010101, 0x32020202,
    };

    uint32_t expectedFirstWords[] =
    {
        0x20000001,
        0x20000002,
        0x20000003,
        0x20000004,
        0x20000005,
        0x20000006,
        0x40000001, 
        0x40000002, 
        0x40000003, 
        0x20000007,
        0x20000008,
        0xF0000001, 
        0xF0000002,
        0x20000009,
        0xF0000003,
    };


    uint32_t wordCount = ARRAYSIZE(words);
    UmpBufferIterator bufferIterator(words, wordCount);

    uint32_t i = 0;
    for (auto it = bufferIterator.begin(); it < bufferIterator.end(); ++it)
    {
        std::cout << "Iterator Value: 0x" << std::hex << std::setw(8) << *it << std::endl;

        VERIFY_ARE_EQUAL(*it, expectedFirstWords[i]);

        VERIFY_IS_TRUE(it.CurrentMessageSeemsComplete());

        i++;
    }

    VERIFY_ARE_EQUAL(i, ARRAYSIZE(expectedFirstWords));

}

void UmpIteratorTests::TestGetMessageType()
{
    // test a mix of message sizes
    uint32_t words[] =
    {
        0x20000001,
        0x20000002,
        0x20000003,
        0x20000004,
        0x20000005,
        0x20000006,
        0x40000001, 0x01234567,
        0x40000002, 0x02345678,
        0x40000003, 0x03456789,
        0x20000007,
        0x20000008,
        0xF0000001, 0x18675309, 0x01010101, 0x02020202,
        0xF0000002, 0x28675309, 0x21010101, 0x22020202,
        0x20000009,
        0xF0000003, 0x38675309, 0x31010101, 0x32020202,
    };

    uint32_t expectedMessageTypes[] =
    {
        0x2,
        0x2,
        0x2,
        0x2,
        0x2,
        0x2,
        0x4,
        0x4,
        0x4,
        0x2,
        0x2,
        0xF,
        0xF,
        0x2,
        0xF,
    };


    uint32_t wordCount = ARRAYSIZE(words);
    UmpBufferIterator bufferIterator(words, wordCount);

    uint32_t i = 0;
    for (auto it = bufferIterator.begin(); it < bufferIterator.end(); ++it)
    {
        std::cout << "Iterator Value: 0x" << std::hex << std::setw(8) << *it << std::endl;

        VERIFY_ARE_EQUAL(it.CurrentMessageType(), expectedMessageTypes[i]);

        i++;
    }

    VERIFY_ARE_EQUAL(i, ARRAYSIZE(expectedMessageTypes));

}



void UmpIteratorTests::TestIncompleteBuffer()
{
    // test a mix of message sizes
    uint32_t words[] =
    {
        0x20000001,
        0x20000002,
        0x20000003,
        0x20000004,
        0x20000005,
        0x20000006,
        0x40000001, 0x01234567,
        0x40000002, 0x02345678,
        0x40000003, 0x03456789,
        0x20000007,
        0x20000008,
        0xF0000001, 0x18675309, 0x01010101, 0x02020202,
        0xF0000002, 0x28675309, 0x21010101, 0x22020202,
        0x20000009,
        0xF0000003, 0x38675309,     // incomplete UMP
    };

    uint32_t expectedFirstWords[] =
    {
        0x20000001,
        0x20000002,
        0x20000003,
        0x20000004,
        0x20000005,
        0x20000006,
        0x40000001,
        0x40000002,
        0x40000003,
        0x20000007,
        0x20000008,
        0xF0000001,
        0xF0000002,
        0x20000009,
        0xF0000003, // this is the incomplete UMP
    };


    uint32_t wordCount = ARRAYSIZE(words);
    UmpBufferIterator bufferIterator(words, wordCount);

    uint32_t i = 0;
    for (auto it = bufferIterator.begin(); it < bufferIterator.end(); ++it)
    {
        std::cout << "Iterator Value: 0x" << std::hex << std::setw(8) << *it << std::endl;

        VERIFY_ARE_EQUAL(*it, expectedFirstWords[i]);

        if (*it == 0xF0000003)
        {
            // verify that the last message is indicated as incomplete
            VERIFY_IS_FALSE(it.CurrentMessageSeemsComplete());
        }
        else
        {
            VERIFY_IS_TRUE(it.CurrentMessageSeemsComplete());
        }

        i++;
    }

    VERIFY_ARE_EQUAL(i, ARRAYSIZE(expectedFirstWords));

}



void UmpIteratorTests::TestValidateIncompleteBufferHasCompleteUmps()
{
    // test a mix of message sizes
    uint32_t words[] =
    {
        0x20000001,
        0x20000002,
        0x20000003,
        0x20000004,
        0x20000005,
        0x20000006,
        0x40000001, 0x01234567,
        0x40000002, 0x02345678,
        0x40000003, 0x03456789,
        0x20000007,
        0x20000008,
        0xF0000001, 0x18675309, 0x01010101, 0x02020202,
        0xF0000002, 0x28675309, 0x21010101, 0x22020202,
        0x20000009,
        0xF0000003, 0x38675309,     // incomplete UMP
    };

    VERIFY_IS_FALSE(internal::ValidateBufferHasCompleteUmps(words, ARRAYSIZE(words)));
}

void UmpIteratorTests::TestValidateCompleteBufferHasCompleteUmps()
{
    uint32_t words[] =
    {
        0x20000001,
        0x20000002,
        0x20000003,
        0x20000004,
        0x20000005,
        0x20000006,
        0x40000001, 0x01234567,
        0x40000002, 0x02345678,
        0x40000003, 0x03456789,
        0x20000007,
        0x20000008,
        0xF0000001, 0x18675309, 0x01010101, 0x02020202,
        0xF0000002, 0x28675309, 0x21010101, 0x22020202,
        0x20000009,
    };

    VERIFY_IS_TRUE(internal::ValidateBufferHasCompleteUmps(words, ARRAYSIZE(words)));

}


void UmpIteratorTests::TestCopyWordsToVector()
{
    std::vector<uint32_t> destination{};

    uint32_t words[] =
    {
        0x20000001,
        0x20000006,
        0x40000001, 0x01234567,
        0x20000007,
        0x20000008,
        0xF0000001, 0x18675309, 0x01010101, 0x02020202,
        0x20000009,
    };

    UmpBufferIterator bufferIterator(words, ARRAYSIZE(words));
    for (auto it = bufferIterator.begin(); it < bufferIterator.end(); ++it)
    {
        it.CopyCurrentMessageToVector(destination);
    }

    // now, check
    for (uint32_t i = 0; i < ARRAYSIZE(words); i++)
    {
        VERIFY_ARE_EQUAL(words[i], destination[i]);
    }

}