// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================


#include "stdafx.h"


// SysEx7 messages are UMP Message Type 3 (64-bit Data Messages). The first word
// is laid out as:
//
//   nibble 1 : message type (always 0x3 for SysEx7)
//   nibble 2 : group
//   nibble 3 : status      (0 = complete, 1 = start, 2 = continue, 3 = end)
//   nibble 4 : number of valid data bytes in this message (0-6)
//   byte   3 : data byte 1
//   byte   4 : data byte 2
//
// The second word holds data bytes 3 through 6.

#define SYSEX7_STATUS_COMPLETE  0x0
#define SYSEX7_STATUS_START     0x1
#define SYSEX7_STATUS_CONTINUE  0x2
#define SYSEX7_STATUS_END       0x3


// Builds the first word of a SysEx7 message from its component fields
static uint32_t BuildSysEx7Word0(
    _In_ uint8_t const group,
    _In_ uint8_t const status,
    _In_ uint8_t const dataByteCount,
    _In_ uint8_t const dataByte1,
    _In_ uint8_t const dataByte2)
{
    return
        (0x3u << 28) |
        (static_cast<uint32_t>(group & 0x0F) << 24) |
        (static_cast<uint32_t>(status & 0x0F) << 20) |
        (static_cast<uint32_t>(dataByteCount & 0x0F) << 16) |
        (static_cast<uint32_t>(dataByte1) << 8) |
        (static_cast<uint32_t>(dataByte2));
}

// Builds the second word of a SysEx7 message from data bytes 3 through 6
static uint32_t BuildSysEx7Word1(
    _In_ uint8_t const dataByte3,
    _In_ uint8_t const dataByte4,
    _In_ uint8_t const dataByte5,
    _In_ uint8_t const dataByte6)
{
    return
        (static_cast<uint32_t>(dataByte3) << 24) |
        (static_cast<uint32_t>(dataByte4) << 16) |
        (static_cast<uint32_t>(dataByte5) << 8) |
        (static_cast<uint32_t>(dataByte6));
}

// Compares the contents of a returned byte vector against the expected bytes
static void VerifyDataBytes(
    _In_ IVector<uint8_t> const& actual,
    _In_ std::vector<uint8_t> const& expected)
{
    VERIFY_IS_NOT_NULL(actual);
    VERIFY_ARE_EQUAL(actual.Size(), static_cast<uint32_t>(expected.size()));

    for (uint32_t i = 0; i < expected.size(); i++)
    {
        std::cout
            << "byte " << i
            << ": expected 0x" << std::hex << (int)expected[i]
            << ", actual 0x" << (int)actual.GetAt(i) << std::dec << std::endl;

        VERIFY_ARE_EQUAL(actual.GetAt(i), expected[i]);
    }
}


void MidiSystemExclusive7MessageHelperTests::TestMessageIsSystemExclusiveMessage()
{
    // all four SysEx7 status values are valid SysEx7 messages
    VERIFY_IS_TRUE(MidiSystemExclusive7MessageHelper::MessageIsSystemExclusiveMessage(
        BuildSysEx7Word0(0, SYSEX7_STATUS_COMPLETE, 6, 0x00, 0x00)));
    VERIFY_IS_TRUE(MidiSystemExclusive7MessageHelper::MessageIsSystemExclusiveMessage(
        BuildSysEx7Word0(0, SYSEX7_STATUS_START, 6, 0x00, 0x00)));
    VERIFY_IS_TRUE(MidiSystemExclusive7MessageHelper::MessageIsSystemExclusiveMessage(
        BuildSysEx7Word0(0, SYSEX7_STATUS_CONTINUE, 6, 0x00, 0x00)));
    VERIFY_IS_TRUE(MidiSystemExclusive7MessageHelper::MessageIsSystemExclusiveMessage(
        BuildSysEx7Word0(0, SYSEX7_STATUS_END, 2, 0x00, 0x00)));

    // the group must not affect the result
    VERIFY_IS_TRUE(MidiSystemExclusive7MessageHelper::MessageIsSystemExclusiveMessage(
        BuildSysEx7Word0(0xF, SYSEX7_STATUS_COMPLETE, 6, 0x00, 0x00)));

    // message type 3 but with a status outside the SysEx7 range
    VERIFY_IS_FALSE(MidiSystemExclusive7MessageHelper::MessageIsSystemExclusiveMessage(
        BuildSysEx7Word0(0, 0x4, 0, 0x00, 0x00)));
    VERIFY_IS_FALSE(MidiSystemExclusive7MessageHelper::MessageIsSystemExclusiveMessage(
        BuildSysEx7Word0(0, 0xF, 0, 0x00, 0x00)));

    // other message types are not SysEx7
    VERIFY_IS_FALSE(MidiSystemExclusive7MessageHelper::MessageIsSystemExclusiveMessage(0x00000000)); // utility
    VERIFY_IS_FALSE(MidiSystemExclusive7MessageHelper::MessageIsSystemExclusiveMessage(0x10000000)); // system common
    VERIFY_IS_FALSE(MidiSystemExclusive7MessageHelper::MessageIsSystemExclusiveMessage(0x20000000)); // MIDI 1 channel voice
    VERIFY_IS_FALSE(MidiSystemExclusive7MessageHelper::MessageIsSystemExclusiveMessage(0x40000000)); // MIDI 2 channel voice
    VERIFY_IS_FALSE(MidiSystemExclusive7MessageHelper::MessageIsSystemExclusiveMessage(0x50000000)); // SysEx8 / mixed dataset
    VERIFY_IS_FALSE(MidiSystemExclusive7MessageHelper::MessageIsSystemExclusiveMessage(0xF0000000)); // stream
}


void MidiSystemExclusive7MessageHelperTests::TestGetDataByteCountFromFirstWord()
{
    for (uint8_t count = 0; count <= 6; count++)
    {
        auto word0 = BuildSysEx7Word0(0, SYSEX7_STATUS_COMPLETE, count, 0xFF, 0xFF);

        std::cout << "Checking data byte count " << (int)count << std::endl;

        VERIFY_ARE_EQUAL(
            MidiSystemExclusive7MessageHelper::GetDataByteCountFromSystemExclusiveMessageFirstWord(word0),
            count);
    }

    // the count field must be read independently of the group and status fields
    VERIFY_ARE_EQUAL(
        MidiSystemExclusive7MessageHelper::GetDataByteCountFromSystemExclusiveMessageFirstWord(
            BuildSysEx7Word0(0xF, SYSEX7_STATUS_END, 3, 0xFF, 0xFF)),
        (uint8_t)3);
}


void MidiSystemExclusive7MessageHelperTests::TestGetDataBytesFromSingleMessageWords()
{
    // a complete SysEx7 message carrying the full six data bytes
    auto word0 = BuildSysEx7Word0(0, SYSEX7_STATUS_COMPLETE, 6, 0x7E, 0x7F);
    auto word1 = BuildSysEx7Word1(0x0D, 0x70, 0x02, 0x03);

    auto dataBytes = MidiSystemExclusive7MessageHelper::GetDataBytesFromSingleSystemExclusiveMessage(word0, word1);

    VerifyDataBytes(dataBytes, { 0x7E, 0x7F, 0x0D, 0x70, 0x02, 0x03 });

    // a message which declares fewer bytes than the words can hold must return only
    // the declared bytes, ignoring the remaining (undefined) data
    auto partialWord0 = BuildSysEx7Word0(0, SYSEX7_STATUS_END, 3, 0x11, 0x22);
    auto partialWord1 = BuildSysEx7Word1(0x33, 0xAA, 0xBB, 0xCC);

    auto partialDataBytes = MidiSystemExclusive7MessageHelper::GetDataBytesFromSingleSystemExclusiveMessage(
        partialWord0, partialWord1);

    VerifyDataBytes(partialDataBytes, { 0x11, 0x22, 0x33 });
}


void MidiSystemExclusive7MessageHelperTests::TestGetDataBytesFromSingleMessage64()
{
    auto word0 = BuildSysEx7Word0(5, SYSEX7_STATUS_COMPLETE, 5, 0x41, 0x42);
    auto word1 = BuildSysEx7Word1(0x43, 0x44, 0x45, 0x00);

    MidiMessage64 message(0, word0, word1);

    auto dataBytes = MidiSystemExclusive7MessageHelper::GetDataBytesFromSingleSystemExclusiveMessage(message);

    VerifyDataBytes(dataBytes, { 0x41, 0x42, 0x43, 0x44, 0x45 });

    // the MidiMessage64 overload must agree with the raw words overload
    auto dataBytesFromWords = MidiSystemExclusive7MessageHelper::GetDataBytesFromSingleSystemExclusiveMessage(word0, word1);

    VERIFY_ARE_EQUAL(dataBytes.Size(), dataBytesFromWords.Size());

    for (uint32_t i = 0; i < dataBytes.Size(); i++)
    {
        VERIFY_ARE_EQUAL(dataBytes.GetAt(i), dataBytesFromWords.GetAt(i));
    }
}


void MidiSystemExclusive7MessageHelperTests::TestGetDataBytesFromSingleMessageWithZeroDataBytes()
{
    // a SysEx7 message may legitimately carry zero data bytes
    auto word0 = BuildSysEx7Word0(0, SYSEX7_STATUS_END, 0, 0xAA, 0xBB);
    auto word1 = BuildSysEx7Word1(0xCC, 0xDD, 0xEE, 0xFF);

    auto dataBytes = MidiSystemExclusive7MessageHelper::GetDataBytesFromSingleSystemExclusiveMessage(word0, word1);

    VERIFY_IS_NOT_NULL(dataBytes);
    VERIFY_ARE_EQUAL(dataBytes.Size(), (uint32_t)0);
}


void MidiSystemExclusive7MessageHelperTests::TestGetDataBytesDoesNotMaskHighBit()
{
    // SysEx7 data bytes should only ever have the high bit clear, but the helper
    // deliberately does not mask the value so that malformed data remains visible
    // to the caller rather than being silently corrected.
    auto word0 = BuildSysEx7Word0(0, SYSEX7_STATUS_COMPLETE, 4, 0xFF, 0x80);
    auto word1 = BuildSysEx7Word1(0x81, 0x7F, 0x00, 0x00);

    auto dataBytes = MidiSystemExclusive7MessageHelper::GetDataBytesFromSingleSystemExclusiveMessage(word0, word1);

    VerifyDataBytes(dataBytes, { 0xFF, 0x80, 0x81, 0x7F });
}


void MidiSystemExclusive7MessageHelperTests::TestAppendDataBytesFromSingleMessage()
{
    // appending must preserve any bytes already in the destination vector
    auto dataBytes = winrt::single_threaded_vector<uint8_t>();
    dataBytes.Append(0xAA);
    dataBytes.Append(0xBB);

    auto word0 = BuildSysEx7Word0(0, SYSEX7_STATUS_CONTINUE, 2, 0x01, 0x02);
    auto word1 = BuildSysEx7Word1(0x00, 0x00, 0x00, 0x00);

    auto appendedCount = MidiSystemExclusive7MessageHelper::AppendDataBytesFromSingleSystemExclusiveMessage(
        word0, word1, dataBytes);

    VERIFY_ARE_EQUAL(appendedCount, (uint8_t)2);

    VerifyDataBytes(dataBytes, { 0xAA, 0xBB, 0x01, 0x02 });

    // appending a second message must continue to accumulate
    auto secondWord0 = BuildSysEx7Word0(0, SYSEX7_STATUS_END, 3, 0x03, 0x04);
    auto secondWord1 = BuildSysEx7Word1(0x05, 0x00, 0x00, 0x00);

    auto secondAppendedCount = MidiSystemExclusive7MessageHelper::AppendDataBytesFromSingleSystemExclusiveMessage(
        secondWord0, secondWord1, dataBytes);

    VERIFY_ARE_EQUAL(secondAppendedCount, (uint8_t)3);

    VerifyDataBytes(dataBytes, { 0xAA, 0xBB, 0x01, 0x02, 0x03, 0x04, 0x05 });

    // appending a message with no data bytes must not change the destination
    auto emptyWord0 = BuildSysEx7Word0(0, SYSEX7_STATUS_END, 0, 0x06, 0x07);

    auto emptyAppendedCount = MidiSystemExclusive7MessageHelper::AppendDataBytesFromSingleSystemExclusiveMessage(
        emptyWord0, 0x00000000, dataBytes);

    VERIFY_ARE_EQUAL(emptyAppendedCount, (uint8_t)0);
    VERIFY_ARE_EQUAL(dataBytes.Size(), (uint32_t)7);
}


void MidiSystemExclusive7MessageHelperTests::TestAppendDataBytesFromSingleMessage64()
{
    auto dataBytes = winrt::single_threaded_vector<uint8_t>();

    auto word0 = BuildSysEx7Word0(3, SYSEX7_STATUS_START, 6, 0xF0, 0x7E);
    auto word1 = BuildSysEx7Word1(0x7F, 0x09, 0x01, 0x00);

    MidiMessage64 message(0, word0, word1);

    auto appendedCount = MidiSystemExclusive7MessageHelper::AppendDataBytesFromSingleSystemExclusiveMessage(
        message, dataBytes);

    VERIFY_ARE_EQUAL(appendedCount, (uint8_t)6);

    VerifyDataBytes(dataBytes, { 0xF0, 0x7E, 0x7F, 0x09, 0x01, 0x00 });
}


void MidiSystemExclusive7MessageHelperTests::TestGetDataBytesFromMultipleMessages()
{
    // A multi-message SysEx7 transfer: start, continue, end. The helper must
    // concatenate the data bytes across all three messages in order.

    MidiMessage64 startMessage(
        0,
        BuildSysEx7Word0(2, SYSEX7_STATUS_START, 6, 0x7E, 0x00),
        BuildSysEx7Word1(0x01, 0x02, 0x03, 0x04));

    MidiMessage64 continueMessage(
        0,
        BuildSysEx7Word0(2, SYSEX7_STATUS_CONTINUE, 6, 0x05, 0x06),
        BuildSysEx7Word1(0x07, 0x08, 0x09, 0x0A));

    // the final message carries only three of the six possible data bytes
    MidiMessage64 endMessage(
        0,
        BuildSysEx7Word0(2, SYSEX7_STATUS_END, 3, 0x0B, 0x0C),
        BuildSysEx7Word1(0x0D, 0x00, 0x00, 0x00));

    auto messages = winrt::single_threaded_vector<MidiMessage64>(
        std::vector<MidiMessage64>{ startMessage, continueMessage, endMessage });

    auto dataBytes = MidiSystemExclusive7MessageHelper::GetDataBytesFromMultipleSystemExclusiveMessages(messages);

    VerifyDataBytes(dataBytes,
        {
            0x7E, 0x00, 0x01, 0x02, 0x03, 0x04,
            0x05, 0x06, 0x07, 0x08, 0x09, 0x0A,
            0x0B, 0x0C, 0x0D
        });
}


void MidiSystemExclusive7MessageHelperTests::TestGetDataBytesFromEmptyMessageList()
{
    auto messages = winrt::single_threaded_vector<MidiMessage64>();

    auto dataBytes = MidiSystemExclusive7MessageHelper::GetDataBytesFromMultipleSystemExclusiveMessages(messages);

    VERIFY_IS_NOT_NULL(dataBytes);
    VERIFY_ARE_EQUAL(dataBytes.Size(), (uint32_t)0);
}
