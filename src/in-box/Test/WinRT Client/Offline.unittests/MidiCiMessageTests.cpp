// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================


#include "stdafx.h"

#include <random>
#include <vector>

#include "MidiCiMessage.h"

using namespace WindowsMidiServicesCapabilityInquiry;


void MidiCiMessageTests::TestMuidEncodingIsSevenBitLeastSignificantFirst()
{
    uint8_t bytes[4]{};

    // One, to fix which end is which.
    WriteMuid(bytes, 1);

    VERIFY_ARE_EQUAL(bytes[0], (uint8_t)0x01);
    VERIFY_ARE_EQUAL(bytes[1], (uint8_t)0x00);
    VERIFY_ARE_EQUAL(bytes[2], (uint8_t)0x00);
    VERIFY_ARE_EQUAL(bytes[3], (uint8_t)0x00);

    // 128 is the first value that has to carry into the second byte. An eight bit packing would
    // leave this as 0x80 0x00 0x00 0x00 and set a high bit inside a system exclusive message.
    WriteMuid(bytes, 128);

    VERIFY_ARE_EQUAL(bytes[0], (uint8_t)0x00);
    VERIFY_ARE_EQUAL(bytes[1], (uint8_t)0x01);
    VERIFY_ARE_EQUAL(bytes[2], (uint8_t)0x00);
    VERIFY_ARE_EQUAL(bytes[3], (uint8_t)0x00);

    // The broadcast MUID is every bit of the twenty eight set.
    WriteMuid(bytes, MuidBroadcast);

    VERIFY_ARE_EQUAL(bytes[0], (uint8_t)0x7F);
    VERIFY_ARE_EQUAL(bytes[1], (uint8_t)0x7F);
    VERIFY_ARE_EQUAL(bytes[2], (uint8_t)0x7F);
    VERIFY_ARE_EQUAL(bytes[3], (uint8_t)0x7F);
}

void MidiCiMessageTests::TestMuidRoundTrip()
{
    for (uint32_t muid = 0; muid <= MuidMaxValue; muid += 7919)
    {
        uint8_t bytes[4]{};

        WriteMuid(bytes, muid);

        // No byte written into a system exclusive message may have its high bit set.
        VERIFY_IS_LESS_THAN(bytes[0], (uint8_t)0x80);
        VERIFY_IS_LESS_THAN(bytes[1], (uint8_t)0x80);
        VERIFY_IS_LESS_THAN(bytes[2], (uint8_t)0x80);
        VERIFY_IS_LESS_THAN(bytes[3], (uint8_t)0x80);

        VERIFY_ARE_EQUAL(ReadMuid(bytes), muid);
    }
}

void MidiCiMessageTests::TestParseDiscovery()
{
    const uint8_t message[]
    {
        0x7E, 0x7F, 0x0D, 0x70, 0x02,       // to the function block, discovery, version 2
        0x01, 0x00, 0x00, 0x00,             // source muid 1
        0x7F, 0x7F, 0x7F, 0x7F,             // destination muid, broadcast
        0x00, 0x00, 0x41,                   // manufacturer
        0x0B, 0x00,                         // family
        0x01, 0x00,                         // model
        0x01, 0x00, 0x00, 0x00,             // revision
        0x00,                               // capability bitmap
        0x00, 0x02, 0x00, 0x00,             // receivable maximum system exclusive size
        0x00                                // output path id
    };

    ParsedMessage parsed{};

    VERIFY_ARE_EQUAL((int)Parse(message, sizeof(message), parsed), (int)ParseStatus::Ok);

    VERIFY_ARE_EQUAL((int)parsed.Type, (int)MessageType::Discovery);
    VERIFY_ARE_EQUAL(parsed.DeviceId, (uint8_t)DeviceIdFunctionBlock);
    VERIFY_ARE_EQUAL(parsed.VersionFormat, (uint8_t)0x02);
    VERIFY_ARE_EQUAL(parsed.SourceMuid, (uint32_t)1);
    VERIFY_ARE_EQUAL(parsed.DestinationMuid, MuidBroadcast);
    VERIFY_IS_FALSE(parsed.HasPropertyExchangeFields);
}

void MidiCiMessageTests::TestParseInvalidateMuid()
{
    const uint8_t message[]
    {
        0x7E, 0x7F, 0x0D, 0x7E, 0x02,
        0x01, 0x00, 0x00, 0x00,             // source muid 1
        0x7F, 0x7F, 0x7F, 0x7F,             // destination muid, broadcast
        0x00, 0x01, 0x00, 0x00              // target muid 128
    };

    ParsedMessage parsed{};

    VERIFY_ARE_EQUAL((int)Parse(message, sizeof(message), parsed), (int)ParseStatus::Ok);

    VERIFY_ARE_EQUAL((int)parsed.Type, (int)MessageType::InvalidateMuid);
    VERIFY_ARE_EQUAL(parsed.TargetMuid, (uint32_t)128);
}

void MidiCiMessageTests::TestRejectsTruncatedMessage()
{
    const uint8_t full[]
    {
        0x7E, 0x7F, 0x0D, 0x70, 0x02,
        0x01, 0x00, 0x00, 0x00,
        0x7F, 0x7F, 0x7F, 0x7F
    };

    ParsedMessage parsed{};

    // Every prefix shorter than the common header must be refused rather than read past its end.
    for (size_t length = 0; length < sizeof(full); length++)
    {
        VERIFY_ARE_EQUAL((int)Parse(full, length, parsed), (int)ParseStatus::TooShort);
    }

    VERIFY_ARE_EQUAL((int)Parse(full, sizeof(full), parsed), (int)ParseStatus::Ok);
}

void MidiCiMessageTests::TestRejectsNonCapabilityInquiry()
{
    // A universal system exclusive identity request, which is not MIDI-CI.
    const uint8_t identityRequest[]
    {
        0x7E, 0x7F, 0x06, 0x01,
        0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00,
        0x00
    };

    ParsedMessage parsed{};

    VERIFY_ARE_EQUAL((int)Parse(identityRequest, sizeof(identityRequest), parsed), (int)ParseStatus::NotCapabilityInquiry);

    const uint8_t notUniversal[]
    {
        0x41, 0x7F, 0x0D, 0x70, 0x02,
        0x01, 0x00, 0x00, 0x00,
        0x7F, 0x7F, 0x7F, 0x7F
    };

    VERIFY_ARE_EQUAL((int)Parse(notUniversal, sizeof(notUniversal), parsed), (int)ParseStatus::NotUniversalSystemExclusive);
}

void MidiCiMessageTests::TestPropertyExchangeHeaderLengthCannotExceedBuffer()
{
    // Property Exchange carries three lengths that arrive from whatever is on the other end of the
    // cable. This one claims the largest header a fourteen bit field can express, inside a message
    // that holds none of it.
    const uint8_t message[]
    {
        0x7E, 0x7F, 0x0D, 0x34, 0x02,       // inquiry: get property data
        0x01, 0x00, 0x00, 0x00,
        0x02, 0x00, 0x00, 0x00,
        0x01,                               // request id
        0x7F, 0x7F                          // header length 16383, and nothing follows
    };

    ParsedMessage parsed{};

    VERIFY_ARE_EQUAL((int)Parse(message, sizeof(message), parsed), (int)ParseStatus::LengthFieldExceedsBuffer);

    // One byte over is just as bad as sixteen kilobytes over.
    const uint8_t justOver[]
    {
        0x7E, 0x7F, 0x0D, 0x34, 0x02,
        0x01, 0x00, 0x00, 0x00,
        0x02, 0x00, 0x00, 0x00,
        0x01,
        0x03, 0x00,                         // header length 3
        0x7B, 0x7D                          // but only two bytes of it
    };

    VERIFY_ARE_EQUAL((int)Parse(justOver, sizeof(justOver), parsed), (int)ParseStatus::LengthFieldExceedsBuffer);
}

void MidiCiMessageTests::TestPropertyExchangeDataLengthCannotExceedBuffer()
{
    const uint8_t message[]
    {
        0x7E, 0x7F, 0x0D, 0x34, 0x02,
        0x01, 0x00, 0x00, 0x00,
        0x02, 0x00, 0x00, 0x00,
        0x01,                               // request id
        0x02, 0x00,                         // header length 2
        0x7B, 0x7D,                         // header
        0x01, 0x00,                         // chunk count 1
        0x01, 0x00,                         // chunk number 1
        0x7F, 0x7F                          // data length 16383, and nothing follows
    };

    ParsedMessage parsed{};

    VERIFY_ARE_EQUAL((int)Parse(message, sizeof(message), parsed), (int)ParseStatus::LengthFieldExceedsBuffer);
}

void MidiCiMessageTests::TestParseValidPropertyExchangeRequest()
{
    const uint8_t message[]
    {
        0x7E, 0x7F, 0x0D, 0x34, 0x02,
        0x01, 0x00, 0x00, 0x00,             // source muid 1
        0x02, 0x00, 0x00, 0x00,             // destination muid 2
        0x05,                               // request id
        0x03, 0x00,                         // header length 3
        0x7B, 0x22, 0x7D,                   // header, three bytes
        0x01, 0x00,                         // chunk count 1
        0x01, 0x00,                         // chunk number 1
        0x02, 0x00,                         // data length 2
        0x5B, 0x5D                          // data, two bytes
    };

    ParsedMessage parsed{};

    VERIFY_ARE_EQUAL((int)Parse(message, sizeof(message), parsed), (int)ParseStatus::Ok);

    VERIFY_ARE_EQUAL((int)parsed.Type, (int)MessageType::PropertyGetDataInquiry);
    VERIFY_IS_TRUE(parsed.HasPropertyExchangeFields);

    VERIFY_ARE_EQUAL(parsed.PropertyExchange.RequestId, (uint8_t)5);
    VERIFY_ARE_EQUAL(parsed.PropertyExchange.HeaderByteCount, (uint16_t)3);
    VERIFY_ARE_EQUAL(parsed.PropertyExchange.ChunkCount, (uint16_t)1);
    VERIFY_ARE_EQUAL(parsed.PropertyExchange.ChunkNumber, (uint16_t)1);
    VERIFY_ARE_EQUAL(parsed.PropertyExchange.DataByteCount, (uint16_t)2);

    // The offsets must point at the real bytes, not past them.
    VERIFY_ARE_EQUAL(message[parsed.PropertyExchange.HeaderOffset], (uint8_t)0x7B);
    VERIFY_ARE_EQUAL(message[parsed.PropertyExchange.DataOffset], (uint8_t)0x5B);

    VERIFY_IS_LESS_THAN_OR_EQUAL(
        (size_t)parsed.PropertyExchange.DataOffset + parsed.PropertyExchange.DataByteCount,
        sizeof(message));
}

void MidiCiMessageTests::TestFuzzedMessagesNeverReportOffsetsPastTheBuffer()
{
    // The cases above are the ones worth naming. This is for the ones nobody thought of. Anything
    // the parser accepts must describe bytes that are really there, because every caller is going
    // to index with these offsets.
    std::mt19937 generator{ 20260915 };
    std::uniform_int_distribution<int> byteValue{ 0, 255 };
    std::uniform_int_distribution<size_t> totalLength{ 13, 64 };
    std::uniform_int_distribution<int> shape{ 0, 9 };

    // Lengths that straddle what is really left in the buffer, so the boundary itself gets hit
    // constantly rather than by luck.
    std::uniform_int_distribution<int> claimedLength{ 0, 24 };

    uint8_t buffer[64]{};

    size_t accepted{ 0 };
    size_t rejected{ 0 };

    for (int iteration = 0; iteration < 200000; iteration++)
    {
        const size_t length = totalLength(generator);

        for (size_t i = 0; i < length; i++)
        {
            buffer[i] = static_cast<uint8_t>(byteValue(generator));
        }

        // Three quarters of the cases are shaped like a real message, because pure noise is
        // rejected on the first byte and exercises none of the length handling. Every branch that
        // reads a count off the wire gets a share of them.
        const int thisShape = shape(generator);

        if (thisShape < 8)
        {
            buffer[0] = 0x7E;
            buffer[2] = 0x0D;

            if (thisShape < 4)
            {
                buffer[3] = 0x34;

                if (length >= 16)
                {
                    const auto header = static_cast<uint16_t>(claimedLength(generator));

                    WriteFourteenBitValue(buffer + 14, header);

                    const size_t dataLengthOffset = 16 + (size_t)header + 4;

                    if (dataLengthOffset + 2 <= length)
                    {
                        WriteFourteenBitValue(
                            buffer + dataLengthOffset,
                            static_cast<uint16_t>(claimedLength(generator)));
                    }
                }
            }
            else if (thisShape < 6)
            {
                // Reply to Profile Inquiry: two counts, each multiplied by five before use.
                buffer[3] = 0x21;

                if (length >= 15)
                {
                    const auto enabled = static_cast<uint16_t>(claimedLength(generator) % 6);

                    WriteFourteenBitValue(buffer + 13, enabled);

                    const size_t disabledCountOffset = 15 + (size_t)enabled * 5;

                    if (disabledCountOffset + 2 <= length)
                    {
                        WriteFourteenBitValue(
                            buffer + disabledCountOffset,
                            static_cast<uint16_t>(claimedLength(generator) % 6));
                    }
                }
            }
            else if (thisShape < 7)
            {
                // Profile Specific Data, whose length is four seven bit bytes rather than two.
                buffer[3] = 0x2F;

                if (length >= 22)
                {
                    WriteTwentyEightBitValue(
                        buffer + 18, static_cast<uint32_t>(claimedLength(generator)));
                }
            }
            else
            {
                buffer[3] = 0x7F;
            }
        }

        ParsedMessage parsed{};

        if (Parse(buffer, length, parsed) == ParseStatus::Ok)
        {
            accepted++;

            if (parsed.HasPropertyExchangeFields)
            {
                VERIFY_IS_LESS_THAN_OR_EQUAL(
                    (size_t)parsed.PropertyExchange.HeaderOffset + parsed.PropertyExchange.HeaderByteCount,
                    length);

                VERIFY_IS_LESS_THAN_OR_EQUAL(
                    (size_t)parsed.PropertyExchange.DataOffset + parsed.PropertyExchange.DataByteCount,
                    length);
            }

            if (parsed.HasProfileFields)
            {
                VERIFY_IS_LESS_THAN_OR_EQUAL(
                    (size_t)parsed.Profile.EnabledProfileOffset +
                        (size_t)parsed.Profile.EnabledProfileCount * ProfileIdByteCount,
                    length);

                VERIFY_IS_LESS_THAN_OR_EQUAL(
                    (size_t)parsed.Profile.DisabledProfileOffset +
                        (size_t)parsed.Profile.DisabledProfileCount * ProfileIdByteCount,
                    length);

                VERIFY_IS_LESS_THAN_OR_EQUAL(
                    (size_t)parsed.Profile.TargetDataOffset + parsed.Profile.TargetDataByteCount,
                    length);
            }

            if (parsed.HasAcknowledgmentFields)
            {
                VERIFY_IS_LESS_THAN_OR_EQUAL(
                    (size_t)parsed.Acknowledgment.MessageTextOffset + parsed.Acknowledgment.MessageTextByteCount,
                    length);
            }
        }
        else
        {
            rejected++;
        }
    }

    LOG_OUTPUT(L"Fuzz: %llu accepted, %llu rejected", (unsigned long long)accepted, (unsigned long long)rejected);

    // A run that accepts almost nothing passes this test while exercising almost none of it.
    VERIFY_IS_GREATER_THAN(accepted, (size_t)20000);
    VERIFY_IS_GREATER_THAN(rejected, (size_t)20000);
}

namespace
{
    DiscoveryReplyFields MakeSynthDiscoveryReplyFields()
    {
        DiscoveryReplyFields fields{};

        fields.SourceMuid = 128;            // exercises the carry into the second seven bit byte
        fields.DestinationMuid = 1;

        fields.ManufacturerSysExId[0] = 0x00;
        fields.ManufacturerSysExId[1] = 0x00;
        fields.ManufacturerSysExId[2] = 0x41;

        fields.DeviceFamily = 11;
        fields.DeviceFamilyModelNumber = 1;

        fields.SoftwareRevisionLevel[0] = 1;

        fields.CapabilityCategories = 0x00;
        fields.ReceivableMaximumSysExSize = 512;

        fields.OutputPathId = 0;
        fields.FunctionBlockNumber = 0;

        return fields;
    }
}

void MidiCiMessageTests::TestBuildDiscoveryReplyBytes()
{
    uint8_t buffer[64]{};

    const auto written = BuildDiscoveryReply(MakeSynthDiscoveryReplyFields(), buffer, sizeof(buffer));

    VERIFY_ARE_EQUAL(written, DiscoveryReplyByteCount);

    // Worked out from the specification rather than from the builder, so that a builder and parser
    // sharing one wrong idea of the layout cannot agree their way past this.
    const uint8_t expected[]
    {
        0x7E, 0x7F, 0x0D, 0x71, 0x02,
        0x00, 0x01, 0x00, 0x00,             // source muid 128
        0x01, 0x00, 0x00, 0x00,             // destination muid 1
        0x00, 0x00, 0x41,                   // manufacturer
        0x0B, 0x00,                         // family 11
        0x01, 0x00,                         // model 1
        0x01, 0x00, 0x00, 0x00,             // revision
        0x00,                               // no categories supported yet
        0x00, 0x04, 0x00, 0x00,             // receivable maximum system exclusive size 512
        0x00,                               // output path id
        0x00                                // function block number
    };

    VERIFY_ARE_EQUAL(sizeof(expected), DiscoveryReplyByteCount);

    for (size_t i = 0; i < sizeof(expected); i++)
    {
        if (buffer[i] != expected[i])
        {
            LOG_OUTPUT(L"Mismatch at byte %llu: got %02x, expected %02x",
                (unsigned long long)i, buffer[i], expected[i]);
        }

        VERIFY_ARE_EQUAL(buffer[i], expected[i]);
    }

    // Nothing written into a system exclusive message may have its high bit set.
    for (size_t i = 0; i < written; i++)
    {
        VERIFY_IS_LESS_THAN(buffer[i], (uint8_t)0x80);
    }
}

void MidiCiMessageTests::TestBuildDiscoveryReplyRefusesShortBuffer()
{
    uint8_t buffer[DiscoveryReplyByteCount]{};

    const auto fields = MakeSynthDiscoveryReplyFields();

    // One byte short must write nothing at all rather than a truncated reply.
    VERIFY_ARE_EQUAL(BuildDiscoveryReply(fields, buffer, DiscoveryReplyByteCount - 1), (size_t)0);
    VERIFY_ARE_EQUAL(BuildDiscoveryReply(fields, buffer, 0), (size_t)0);
    VERIFY_ARE_EQUAL(BuildDiscoveryReply(fields, nullptr, sizeof(buffer)), (size_t)0);

    VERIFY_ARE_EQUAL(BuildDiscoveryReply(fields, buffer, sizeof(buffer)), DiscoveryReplyByteCount);
}

void MidiCiMessageTests::TestDiscoveryReplyParsesBack()
{
    uint8_t buffer[64]{};

    const auto fields = MakeSynthDiscoveryReplyFields();
    const auto written = BuildDiscoveryReply(fields, buffer, sizeof(buffer));

    ParsedMessage parsed{};

    VERIFY_ARE_EQUAL((int)Parse(buffer, written, parsed), (int)ParseStatus::Ok);

    VERIFY_ARE_EQUAL((int)parsed.Type, (int)MessageType::DiscoveryReply);
    VERIFY_ARE_EQUAL(parsed.SourceMuid, fields.SourceMuid);
    VERIFY_ARE_EQUAL(parsed.DestinationMuid, fields.DestinationMuid);
}

void MidiCiMessageTests::TestBuildPropertyExchangeReplyParsesBack()
{
    const uint8_t header[]{ '{', '"', 's', '"', ':', '2', '0', '0', '}' };
    const uint8_t data[]{ '[', ']' };

    PropertyExchangeMessageFields fields{};

    fields.Type = MessageType::PropertyGetDataReply;
    fields.SourceMuid = 0x0000042;
    fields.DestinationMuid = 0x0123456;
    fields.RequestId = 9;
    fields.Header = header;
    fields.HeaderByteCount = (uint16_t)sizeof(header);
    fields.ChunkCount = 1;
    fields.ChunkNumber = 1;
    fields.Data = data;
    fields.DataByteCount = (uint16_t)sizeof(data);

    uint8_t buffer[128]{};

    const auto written = BuildPropertyExchangeMessage(fields, buffer, sizeof(buffer));

    VERIFY_ARE_EQUAL(written, PropertyExchangeFixedByteCount + sizeof(header) + sizeof(data));

    ParsedMessage parsed{};

    VERIFY_ARE_EQUAL((int)Parse(buffer, written, parsed), (int)ParseStatus::Ok);
    VERIFY_IS_TRUE(parsed.HasPropertyExchangeFields);

    // A reply that loses the request id cannot be matched to its inquiry.
    VERIFY_ARE_EQUAL(parsed.PropertyExchange.RequestId, (uint8_t)9);
    VERIFY_ARE_EQUAL(parsed.PropertyExchange.HeaderByteCount, (uint16_t)sizeof(header));
    VERIFY_ARE_EQUAL(parsed.PropertyExchange.DataByteCount, (uint16_t)sizeof(data));

    for (size_t i = 0; i < sizeof(header); i++)
    {
        VERIFY_ARE_EQUAL(buffer[parsed.PropertyExchange.HeaderOffset + i], header[i]);
    }

    for (size_t i = 0; i < sizeof(data); i++)
    {
        VERIFY_ARE_EQUAL(buffer[parsed.PropertyExchange.DataOffset + i], data[i]);
    }
}

void MidiCiMessageTests::TestPropertyExchangeRefusesHighBitPayload()
{
    // Masking the high bit off would silently change the caller's data. Refusing is the only safe
    // answer, because a high bit inside a system exclusive message ends it early.
    const uint8_t header[]{ '{', '}' };
    const uint8_t badData[]{ 'a', 0x80, 'b' };

    PropertyExchangeMessageFields fields{};

    fields.Type = MessageType::PropertyGetDataReply;
    fields.Header = header;
    fields.HeaderByteCount = (uint16_t)sizeof(header);
    fields.ChunkCount = 1;
    fields.ChunkNumber = 1;
    fields.Data = badData;
    fields.DataByteCount = (uint16_t)sizeof(badData);

    uint8_t buffer[128]{};

    VERIFY_ARE_EQUAL(BuildPropertyExchangeMessage(fields, buffer, sizeof(buffer)), (size_t)0);

    const uint8_t badHeader[]{ '{', 0xFF };

    fields.Header = badHeader;
    fields.HeaderByteCount = (uint16_t)sizeof(badHeader);
    fields.Data = nullptr;
    fields.DataByteCount = 0;

    VERIFY_ARE_EQUAL(BuildPropertyExchangeMessage(fields, buffer, sizeof(buffer)), (size_t)0);

    // Not a Property Exchange type at all.
    fields.Header = header;
    fields.HeaderByteCount = (uint16_t)sizeof(header);
    fields.Type = MessageType::Discovery;

    VERIFY_ARE_EQUAL(BuildPropertyExchangeMessage(fields, buffer, sizeof(buffer)), (size_t)0);
}

void MidiCiMessageTests::TestChunkArithmetic()
{
    // A device that declared 512 bytes, answering with a 40 byte header.
    const auto perChunk = MaximumPropertyDataBytesPerChunk(512, 40);

    VERIFY_ARE_EQUAL(perChunk, (uint16_t)(512 - PropertyExchangeFixedByteCount - 40 - 2));

    // Whatever the arithmetic says must actually fit, including the F0 and F7.
    VERIFY_IS_LESS_THAN_OR_EQUAL(
        PropertyExchangeFixedByteCount + (size_t)40 + perChunk + 2,
        (size_t)512);

    // A budget too small for the overhead yields nothing rather than underflowing.
    VERIFY_ARE_EQUAL(MaximumPropertyDataBytesPerChunk(16, 0), (uint16_t)0);
    VERIFY_ARE_EQUAL(MaximumPropertyDataBytesPerChunk(PropertyExchangeFixedByteCount + 2, 0), (uint16_t)0);

    VERIFY_ARE_EQUAL(ChunkCountForDataSize(0, 100), (uint16_t)1);        // empty is still one chunk
    VERIFY_ARE_EQUAL(ChunkCountForDataSize(100, 100), (uint16_t)1);      // exact fit is not two
    VERIFY_ARE_EQUAL(ChunkCountForDataSize(101, 100), (uint16_t)2);
    VERIFY_ARE_EQUAL(ChunkCountForDataSize(199, 100), (uint16_t)2);
    VERIFY_ARE_EQUAL(ChunkCountForDataSize(200, 100), (uint16_t)2);
    VERIFY_ARE_EQUAL(ChunkCountForDataSize(201, 100), (uint16_t)3);
    VERIFY_ARE_EQUAL(ChunkCountForDataSize(100, 0), (uint16_t)0);
}

void MidiCiMessageTests::TestChunkedResourceReassembles()
{
    // A resource far larger than one message, which is the real ProgramList case.
    uint8_t resource[1000]{};

    for (size_t i = 0; i < sizeof(resource); i++)
    {
        resource[i] = static_cast<uint8_t>(i % 0x80);
    }

    const uint8_t header[]{ '{', '}' };

    const auto perChunk = MaximumPropertyDataBytesPerChunk(512, (uint16_t)sizeof(header));
    const auto chunkCount = ChunkCountForDataSize(sizeof(resource), perChunk);

    VERIFY_IS_GREATER_THAN(chunkCount, (uint16_t)1);

    uint8_t reassembled[sizeof(resource)]{};
    size_t reassembledLength{ 0 };

    for (uint16_t chunk = 1; chunk <= chunkCount; chunk++)
    {
        const size_t offset = (size_t)(chunk - 1) * perChunk;
        const size_t remaining = sizeof(resource) - offset;
        const auto thisChunk = (uint16_t)(remaining < perChunk ? remaining : perChunk);

        PropertyExchangeMessageFields fields{};

        fields.Type = MessageType::PropertyGetDataReply;
        fields.SourceMuid = 1;
        fields.DestinationMuid = 2;
        fields.RequestId = 3;
        fields.Header = header;
        fields.HeaderByteCount = (uint16_t)sizeof(header);
        fields.ChunkCount = chunkCount;
        fields.ChunkNumber = chunk;
        fields.Data = resource + offset;
        fields.DataByteCount = thisChunk;

        uint8_t buffer[512]{};

        const auto written = BuildPropertyExchangeMessage(fields, buffer, sizeof(buffer));

        VERIFY_IS_GREATER_THAN(written, (size_t)0);

        // Every chunk, including the last, must fit what the far end said it can receive.
        VERIFY_IS_LESS_THAN_OR_EQUAL(written + 2, (size_t)512);

        ParsedMessage parsed{};

        VERIFY_ARE_EQUAL((int)Parse(buffer, written, parsed), (int)ParseStatus::Ok);
        VERIFY_ARE_EQUAL(parsed.PropertyExchange.ChunkNumber, chunk);
        VERIFY_ARE_EQUAL(parsed.PropertyExchange.ChunkCount, chunkCount);
        VERIFY_ARE_EQUAL(parsed.PropertyExchange.RequestId, (uint8_t)3);

        for (uint16_t i = 0; i < parsed.PropertyExchange.DataByteCount; i++)
        {
            reassembled[reassembledLength++] = buffer[parsed.PropertyExchange.DataOffset + i];
        }
    }

    VERIFY_ARE_EQUAL(reassembledLength, sizeof(resource));

    for (size_t i = 0; i < sizeof(resource); i++)
    {
        VERIFY_ARE_EQUAL(reassembled[i], resource[i]);
    }
}

namespace
{
    // Roughly what a ProgramList for this sound set comes to.
    constexpr size_t LargeResourceBytes{ 9000 };
}

void MidiCiMessageTests::TestChunkerEmitsEveryChunkExactlyOnce()
{
    std::vector<uint8_t> resource(LargeResourceBytes);

    for (size_t i = 0; i < resource.size(); i++)
    {
        resource[i] = static_cast<uint8_t>(i % 0x80);
    }

    const uint8_t header[]{ '{', '"', 's', '"', ':', '2', '0', '0', '}' };

    PropertyReplyChunker chunker{};

    chunker.Resource = resource.data();
    chunker.ResourceByteCount = resource.size();
    chunker.Header = header;
    chunker.HeaderByteCount = (uint16_t)sizeof(header);

    // A device that declared the smallest size seen in practice.
    VERIFY_IS_TRUE(chunker.Plan(512));
    VERIFY_IS_GREATER_THAN(chunker.ChunkCount, (uint16_t)10);

    std::vector<uint8_t> reassembled;

    for (uint16_t chunk = 1; chunk <= chunker.ChunkCount; chunk++)
    {
        uint8_t buffer[512]{};

        const auto written = chunker.BuildChunk(chunk, 1, 2, 7, buffer, sizeof(buffer));

        VERIFY_IS_GREATER_THAN(written, (size_t)0);
        VERIFY_IS_LESS_THAN_OR_EQUAL(written + 2, (size_t)512);

        ParsedMessage parsed{};

        VERIFY_ARE_EQUAL((int)Parse(buffer, written, parsed), (int)ParseStatus::Ok);

        // Every chunk of one message carries the same request id and the same total.
        VERIFY_ARE_EQUAL(parsed.PropertyExchange.RequestId, (uint8_t)7);
        VERIFY_ARE_EQUAL(parsed.PropertyExchange.ChunkCount, chunker.ChunkCount);
        VERIFY_ARE_EQUAL(parsed.PropertyExchange.ChunkNumber, chunk);
        VERIFY_ARE_EQUAL(parsed.PropertyExchange.HeaderByteCount, (uint16_t)sizeof(header));

        for (uint16_t i = 0; i < parsed.PropertyExchange.DataByteCount; i++)
        {
            reassembled.push_back(buffer[parsed.PropertyExchange.DataOffset + i]);
        }
    }

    // No byte dropped at a boundary and none sent twice.
    VERIFY_ARE_EQUAL(reassembled.size(), resource.size());
    VERIFY_IS_TRUE(reassembled == resource);
}

void MidiCiMessageTests::TestChunkerRejectsOutOfRangeChunkNumbers()
{
    uint8_t resource[100]{};
    const uint8_t header[]{ '{', '}' };

    PropertyReplyChunker chunker{};

    chunker.Resource = resource;
    chunker.ResourceByteCount = sizeof(resource);
    chunker.Header = header;
    chunker.HeaderByteCount = (uint16_t)sizeof(header);

    VERIFY_IS_TRUE(chunker.Plan(512));

    uint8_t buffer[512]{};

    // Chunk numbering starts at one, so zero is not the first chunk and must not be treated as one.
    VERIFY_ARE_EQUAL(chunker.BuildChunk(0, 1, 2, 0, buffer, sizeof(buffer)), (size_t)0);
    VERIFY_ARE_EQUAL(chunker.BuildChunk((uint16_t)(chunker.ChunkCount + 1), 1, 2, 0, buffer, sizeof(buffer)), (size_t)0);
    VERIFY_IS_GREATER_THAN(chunker.BuildChunk(1, 1, 2, 0, buffer, sizeof(buffer)), (size_t)0);

    // A budget that cannot hold even the overhead must fail planning rather than produce chunks
    // that cannot be sent.
    PropertyReplyChunker tiny{};

    tiny.Resource = resource;
    tiny.ResourceByteCount = sizeof(resource);
    tiny.Header = header;
    tiny.HeaderByteCount = (uint16_t)sizeof(header);

    VERIFY_IS_FALSE(tiny.Plan(16));
    VERIFY_ARE_EQUAL(tiny.BuildChunk(1, 1, 2, 0, buffer, sizeof(buffer)), (size_t)0);
}


void MidiCiMessageTests::TestParseProfileInquiryReply()
{
    // Two enabled profiles and one disabled one, worked out from the message table by hand rather
    // than from our own builder, so builder and parser cannot be wrong together and still agree.
    const uint8_t message[]
    {
        0x7E, 0x00, 0x0D, 0x21, 0x02,       // to channel 1, reply to profile inquiry
        0x01, 0x00, 0x00, 0x00,             // source muid 1
        0x02, 0x00, 0x00, 0x00,             // destination muid 2
        0x02, 0x00,                         // two currently enabled
        0x7E, 0x40, 0x01, 0x01, 0x01,
        0x7E, 0x40, 0x02, 0x01, 0x01,
        0x01, 0x00,                         // one currently disabled
        0x41, 0x00, 0x00, 0x12, 0x34        // a manufacturer specific profile
    };

    ParsedMessage parsed{};

    VERIFY_ARE_EQUAL((int)Parse(message, sizeof(message), parsed), (int)ParseStatus::Ok);

    VERIFY_ARE_EQUAL((int)parsed.Type, (int)MessageType::ProfileInquiryReply);
    VERIFY_ARE_EQUAL(parsed.DeviceId, (uint8_t)0x00);
    VERIFY_IS_TRUE(parsed.HasProfileFields);
    VERIFY_IS_FALSE(parsed.Profile.HasProfileId);

    VERIFY_ARE_EQUAL(parsed.Profile.EnabledProfileCount, (uint16_t)2);
    VERIFY_ARE_EQUAL(parsed.Profile.EnabledProfileOffset, (uint16_t)15);
    VERIFY_ARE_EQUAL(parsed.Profile.DisabledProfileCount, (uint16_t)1);
    VERIFY_ARE_EQUAL(parsed.Profile.DisabledProfileOffset, (uint16_t)27);

    // The offsets have to land on the identifiers themselves, not merely inside the buffer.
    VERIFY_ARE_EQUAL(message[parsed.Profile.EnabledProfileOffset + 2], (uint8_t)0x01);
    VERIFY_ARE_EQUAL(message[parsed.Profile.EnabledProfileOffset + 7], (uint8_t)0x02);
    VERIFY_ARE_EQUAL(message[parsed.Profile.DisabledProfileOffset], (uint8_t)0x41);
}

void MidiCiMessageTests::TestProfileListCountCannotExceedBuffer()
{
    // A count of profiles is as much an attacker controlled length as a property exchange header
    // length is, and it is multiplied by five before it is used.
    const uint8_t tooManyEnabled[]
    {
        0x7E, 0x7F, 0x0D, 0x21, 0x02,
        0x01, 0x00, 0x00, 0x00,
        0x02, 0x00, 0x00, 0x00,
        0x7F, 0x7F,                         // 16383 profiles, in a message carrying none
    };

    ParsedMessage parsed{};

    VERIFY_ARE_EQUAL((int)Parse(tooManyEnabled, sizeof(tooManyEnabled), parsed), (int)ParseStatus::LengthFieldExceedsBuffer);

    // One profile short is just as bad as sixteen thousand short.
    const uint8_t oneShort[]
    {
        0x7E, 0x7F, 0x0D, 0x21, 0x02,
        0x01, 0x00, 0x00, 0x00,
        0x02, 0x00, 0x00, 0x00,
        0x02, 0x00,                         // claims two
        0x7E, 0x40, 0x01, 0x01, 0x01        // and carries one
    };

    VERIFY_ARE_EQUAL((int)Parse(oneShort, sizeof(oneShort), parsed), (int)ParseStatus::LengthFieldExceedsBuffer);

    // The disabled list is a second length field, read after the first has already been honored.
    const uint8_t disabledTooMany[]
    {
        0x7E, 0x7F, 0x0D, 0x21, 0x02,
        0x01, 0x00, 0x00, 0x00,
        0x02, 0x00, 0x00, 0x00,
        0x00, 0x00,                         // no enabled profiles
        0x03, 0x00                          // three disabled, none present
    };

    VERIFY_ARE_EQUAL((int)Parse(disabledTooMany, sizeof(disabledTooMany), parsed), (int)ParseStatus::LengthFieldExceedsBuffer);

    // Both lists empty is a legal reply, and the one a device with no profiles has to send.
    const uint8_t empty[]
    {
        0x7E, 0x7F, 0x0D, 0x21, 0x02,
        0x01, 0x00, 0x00, 0x00,
        0x02, 0x00, 0x00, 0x00,
        0x00, 0x00,
        0x00, 0x00
    };

    VERIFY_ARE_EQUAL((int)Parse(empty, sizeof(empty), parsed), (int)ParseStatus::Ok);
    VERIFY_ARE_EQUAL(parsed.Profile.EnabledProfileCount, (uint16_t)0);
    VERIFY_ARE_EQUAL(parsed.Profile.DisabledProfileCount, (uint16_t)0);
}

void MidiCiMessageTests::TestParseSetProfileOnWithAndWithoutChannelCount()
{
    const uint8_t withCount[]
    {
        0x7E, 0x03, 0x0D, 0x22, 0x02,       // to channel 4, set profile on
        0x01, 0x00, 0x00, 0x00,
        0x02, 0x00, 0x00, 0x00,
        0x7E, 0x40, 0x01, 0x01, 0x01,       // profile id
        0x04, 0x00                          // four channels requested
    };

    ParsedMessage parsed{};

    VERIFY_ARE_EQUAL((int)Parse(withCount, sizeof(withCount), parsed), (int)ParseStatus::Ok);

    VERIFY_IS_TRUE(parsed.HasProfileFields);
    VERIFY_IS_TRUE(parsed.Profile.HasProfileId);
    VERIFY_ARE_EQUAL(parsed.Profile.ProfileId[0], (uint8_t)0x7E);
    VERIFY_ARE_EQUAL(parsed.Profile.ProfileId[4], (uint8_t)0x01);
    VERIFY_IS_TRUE(parsed.Profile.HasChannelCount);
    VERIFY_ARE_EQUAL(parsed.Profile.ChannelCount, (uint16_t)4);

    // A device speaking message version 1 stops after the identifier. That is a shorter message,
    // not a broken one, and the profile it names still has to come through.
    const uint8_t version1[]
    {
        0x7E, 0x03, 0x0D, 0x22, 0x01,
        0x01, 0x00, 0x00, 0x00,
        0x02, 0x00, 0x00, 0x00,
        0x7E, 0x40, 0x01, 0x01, 0x01
    };

    VERIFY_ARE_EQUAL((int)Parse(version1, sizeof(version1), parsed), (int)ParseStatus::Ok);

    VERIFY_IS_TRUE(parsed.Profile.HasProfileId);
    VERIFY_IS_FALSE(parsed.Profile.HasChannelCount);
    VERIFY_ARE_EQUAL(parsed.Profile.ChannelCount, (uint16_t)0);

    // One byte short of a whole identifier must be refused, not read past.
    const uint8_t truncated[]
    {
        0x7E, 0x03, 0x0D, 0x22, 0x02,
        0x01, 0x00, 0x00, 0x00,
        0x02, 0x00, 0x00, 0x00,
        0x7E, 0x40, 0x01, 0x01
    };

    VERIFY_ARE_EQUAL((int)Parse(truncated, sizeof(truncated), parsed), (int)ParseStatus::TooShort);
}

void MidiCiMessageTests::TestParseProfileSpecificData()
{
    const uint8_t message[]
    {
        0x7E, 0x7F, 0x0D, 0x2F, 0x02,       // profile specific data
        0x01, 0x00, 0x00, 0x00,
        0x02, 0x00, 0x00, 0x00,
        0x7E, 0x40, 0x01, 0x01, 0x01,       // profile id
        0x03, 0x00, 0x00, 0x00,             // three bytes follow, in a four byte length field
        0x11, 0x22, 0x33
    };

    ParsedMessage parsed{};

    VERIFY_ARE_EQUAL((int)Parse(message, sizeof(message), parsed), (int)ParseStatus::Ok);

    VERIFY_IS_TRUE(parsed.HasProfileFields);
    VERIFY_ARE_EQUAL(parsed.Profile.TargetDataByteCount, (uint32_t)3);
    VERIFY_ARE_EQUAL(message[parsed.Profile.TargetDataOffset], (uint8_t)0x11);
    VERIFY_ARE_EQUAL(message[parsed.Profile.TargetDataOffset + 2], (uint8_t)0x33);
}

void MidiCiMessageTests::TestProfileSpecificDataLengthCannotExceedBuffer()
{
    // Four seven bit bytes can express 268435455, which is far more than any offset type here can
    // hold. Adding it to the offset in a narrow type would wrap and let the check pass.
    const uint8_t message[]
    {
        0x7E, 0x7F, 0x0D, 0x2F, 0x02,
        0x01, 0x00, 0x00, 0x00,
        0x02, 0x00, 0x00, 0x00,
        0x7E, 0x40, 0x01, 0x01, 0x01,
        0x7F, 0x7F, 0x7F, 0x7F,             // 268435455 bytes claimed
        0x11
    };

    ParsedMessage parsed{};

    VERIFY_ARE_EQUAL((int)Parse(message, sizeof(message), parsed), (int)ParseStatus::LengthFieldExceedsBuffer);

    // A details reply carries the same payload behind a two byte length, checked the same way.
    const uint8_t detailsReply[]
    {
        0x7E, 0x7F, 0x0D, 0x29, 0x02,
        0x01, 0x00, 0x00, 0x00,
        0x02, 0x00, 0x00, 0x00,
        0x7E, 0x40, 0x01, 0x01, 0x01,
        0x00,                               // inquiry target
        0x7F, 0x7F,                         // 16383 bytes claimed
        0x11
    };

    VERIFY_ARE_EQUAL((int)Parse(detailsReply, sizeof(detailsReply), parsed), (int)ParseStatus::LengthFieldExceedsBuffer);
}

void MidiCiMessageTests::TestBuildProfileMessagesParseBack()
{
    const uint8_t profileId[ProfileIdByteCount]{ 0x7E, 0x40, 0x01, 0x01, 0x01 };
    const uint8_t payload[]{ 0x01, 0x02, 0x03, 0x04 };

    uint8_t buffer[256]{};

    // Set Profile On, which carries the channel count.
    ProfileMessageFields setOn{};

    setOn.Type = MessageType::SetProfileOn;
    setOn.DeviceId = 0x02;
    setOn.SourceMuid = 11;
    setOn.DestinationMuid = 22;
    memcpy(setOn.ProfileId, profileId, sizeof(profileId));
    setOn.ChannelCount = 9;

    auto written = BuildProfileMessage(setOn, buffer, sizeof(buffer));

    VERIFY_IS_GREATER_THAN(written, (size_t)0);

    ParsedMessage parsed{};

    VERIFY_ARE_EQUAL((int)Parse(buffer, written, parsed), (int)ParseStatus::Ok);
    VERIFY_ARE_EQUAL((int)parsed.Type, (int)MessageType::SetProfileOn);
    VERIFY_ARE_EQUAL(parsed.DeviceId, (uint8_t)0x02);
    VERIFY_ARE_EQUAL(parsed.SourceMuid, (uint32_t)11);
    VERIFY_ARE_EQUAL(parsed.DestinationMuid, (uint32_t)22);
    VERIFY_ARE_EQUAL(parsed.Profile.ChannelCount, (uint16_t)9);
    VERIFY_ARE_EQUAL(memcmp(parsed.Profile.ProfileId, profileId, sizeof(profileId)), 0);

    // Profile Specific Data, which carries a payload behind the four byte length.
    ProfileMessageFields specific{};

    specific.Type = MessageType::ProfileSpecificData;
    specific.SourceMuid = 11;
    specific.DestinationMuid = 22;
    memcpy(specific.ProfileId, profileId, sizeof(profileId));
    specific.Data = payload;
    specific.DataByteCount = (uint32_t)sizeof(payload);

    written = BuildProfileMessage(specific, buffer, sizeof(buffer));

    VERIFY_IS_GREATER_THAN(written, (size_t)0);
    VERIFY_ARE_EQUAL((int)Parse(buffer, written, parsed), (int)ParseStatus::Ok);
    VERIFY_ARE_EQUAL(parsed.Profile.TargetDataByteCount, (uint32_t)sizeof(payload));
    VERIFY_ARE_EQUAL(memcmp(buffer + parsed.Profile.TargetDataOffset, payload, sizeof(payload)), 0);

    // Profile Inquiry names no profile at all, so the identifier must not be written.
    ProfileMessageFields inquiry{};

    inquiry.Type = MessageType::ProfileInquiry;
    inquiry.SourceMuid = 11;
    inquiry.DestinationMuid = 22;

    written = BuildProfileMessage(inquiry, buffer, sizeof(buffer));

    VERIFY_ARE_EQUAL(written, CommonHeaderByteCount);
    VERIFY_ARE_EQUAL((int)Parse(buffer, written, parsed), (int)ParseStatus::Ok);
    VERIFY_IS_FALSE(parsed.Profile.HasProfileId);

    // A payload with a high bit set cannot travel inside a system exclusive message, and masking it
    // would change the caller's data without saying so.
    const uint8_t highBit[]{ 0x01, 0x80 };

    specific.Data = highBit;
    specific.DataByteCount = (uint32_t)sizeof(highBit);

    VERIFY_ARE_EQUAL(BuildProfileMessage(specific, buffer, sizeof(buffer)), (size_t)0);

    // A buffer one byte short must be refused rather than half filled.
    specific.Data = payload;
    specific.DataByteCount = (uint32_t)sizeof(payload);

    const size_t required = CommonHeaderByteCount + ProfileIdByteCount + 4 + sizeof(payload);

    VERIFY_ARE_EQUAL(BuildProfileMessage(specific, buffer, required - 1), (size_t)0);
    VERIFY_ARE_EQUAL(BuildProfileMessage(specific, buffer, required), required);
}

void MidiCiMessageTests::TestBuildProfileInquiryReplyParsesBack()
{
    const uint8_t enabled[]
    {
        0x7E, 0x40, 0x01, 0x01, 0x01,
        0x7E, 0x40, 0x02, 0x01, 0x01
    };

    const uint8_t disabled[]{ 0x41, 0x00, 0x00, 0x12, 0x34 };

    uint8_t buffer[256]{};

    const auto written = BuildProfileInquiryReply(
        0x7F, 11, 22, enabled, 2, disabled, 1, buffer, sizeof(buffer));

    VERIFY_IS_GREATER_THAN(written, (size_t)0);

    ParsedMessage parsed{};

    VERIFY_ARE_EQUAL((int)Parse(buffer, written, parsed), (int)ParseStatus::Ok);
    VERIFY_ARE_EQUAL(parsed.Profile.EnabledProfileCount, (uint16_t)2);
    VERIFY_ARE_EQUAL(parsed.Profile.DisabledProfileCount, (uint16_t)1);
    VERIFY_ARE_EQUAL(memcmp(buffer + parsed.Profile.EnabledProfileOffset, enabled, sizeof(enabled)), 0);
    VERIFY_ARE_EQUAL(memcmp(buffer + parsed.Profile.DisabledProfileOffset, disabled, sizeof(disabled)), 0);

    // A device with nothing to declare still replies.
    const auto writtenEmpty = BuildProfileInquiryReply(
        0x7F, 11, 22, nullptr, 0, nullptr, 0, buffer, sizeof(buffer));

    VERIFY_ARE_EQUAL(writtenEmpty, CommonHeaderByteCount + 4);
    VERIFY_ARE_EQUAL((int)Parse(buffer, writtenEmpty, parsed), (int)ParseStatus::Ok);
    VERIFY_ARE_EQUAL(parsed.Profile.EnabledProfileCount, (uint16_t)0);

    // A count with no array behind it is a caller error, not something to read from a null pointer.
    VERIFY_ARE_EQUAL(
        BuildProfileInquiryReply(0x7F, 11, 22, nullptr, 3, nullptr, 0, buffer, sizeof(buffer)),
        (size_t)0);
}

void MidiCiMessageTests::TestParseNak()
{
    const uint8_t message[]
    {
        0x7E, 0x7F, 0x0D, 0x7F, 0x02,       // NAK
        0x01, 0x00, 0x00, 0x00,
        0x02, 0x00, 0x00, 0x00,
        0x34,                               // answering a get property data inquiry
        0x01,                               // status code: message not supported
        0x00,                               // status data
        0x05, 0x01, 0x00, 0x00, 0x00,       // details: request id 5, chunk 1
        0x02, 0x00,                         // two bytes of text
        0x4E, 0x6F                          // "No"
    };

    ParsedMessage parsed{};

    VERIFY_ARE_EQUAL((int)Parse(message, sizeof(message), parsed), (int)ParseStatus::Ok);

    VERIFY_ARE_EQUAL((int)parsed.Type, (int)MessageType::Nak);
    VERIFY_IS_TRUE(parsed.HasAcknowledgmentFields);
    VERIFY_ARE_EQUAL(parsed.Acknowledgment.OriginalMessageType, (uint8_t)0x34);
    VERIFY_ARE_EQUAL(parsed.Acknowledgment.StatusCode, (uint8_t)0x01);
    VERIFY_ARE_EQUAL(parsed.Acknowledgment.Details[0], (uint8_t)0x05);
    VERIFY_ARE_EQUAL(parsed.Acknowledgment.MessageTextByteCount, (uint16_t)2);
    VERIFY_ARE_EQUAL(message[parsed.Acknowledgment.MessageTextOffset], (uint8_t)0x4E);

    // A device speaking message version 1 sends the header and nothing else. It is still a NAK and
    // still tells the initiator its transaction failed.
    const uint8_t bare[]
    {
        0x7E, 0x7F, 0x0D, 0x7F, 0x01,
        0x01, 0x00, 0x00, 0x00,
        0x02, 0x00, 0x00, 0x00
    };

    VERIFY_ARE_EQUAL((int)Parse(bare, sizeof(bare), parsed), (int)ParseStatus::Ok);
    VERIFY_ARE_EQUAL((int)parsed.Type, (int)MessageType::Nak);
    VERIFY_IS_FALSE(parsed.HasAcknowledgmentFields);

    // The text length is the last attacker controlled field in the message.
    const uint8_t overlongText[]
    {
        0x7E, 0x7F, 0x0D, 0x7F, 0x02,
        0x01, 0x00, 0x00, 0x00,
        0x02, 0x00, 0x00, 0x00,
        0x34, 0x01, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00,
        0x7F, 0x7F,
        0x4E
    };

    VERIFY_ARE_EQUAL((int)Parse(overlongText, sizeof(overlongText), parsed), (int)ParseStatus::LengthFieldExceedsBuffer);
}

void MidiCiMessageTests::TestBuildAcknowledgmentParsesBack()
{
    const uint8_t text[]{ 'B', 'u', 's', 'y' };

    AcknowledgmentFields fields{};

    fields.OriginalMessageType = 0x36;
    fields.StatusCode = 0x20;
    fields.StatusData = 0x01;
    fields.Details[0] = 0x07;

    uint8_t buffer[128]{};

    auto written = BuildAcknowledgment(
        MessageType::Nak, DeviceIdFunctionBlock, 11, 22, fields,
        text, (uint16_t)sizeof(text), buffer, sizeof(buffer));

    VERIFY_ARE_EQUAL(written, AcknowledgmentFixedByteCount + sizeof(text));

    ParsedMessage parsed{};

    VERIFY_ARE_EQUAL((int)Parse(buffer, written, parsed), (int)ParseStatus::Ok);
    VERIFY_ARE_EQUAL((int)parsed.Type, (int)MessageType::Nak);
    VERIFY_ARE_EQUAL(parsed.Acknowledgment.OriginalMessageType, (uint8_t)0x36);
    VERIFY_ARE_EQUAL(parsed.Acknowledgment.StatusCode, (uint8_t)0x20);
    VERIFY_ARE_EQUAL(parsed.Acknowledgment.StatusData, (uint8_t)0x01);
    VERIFY_ARE_EQUAL(parsed.Acknowledgment.MessageTextByteCount, (uint16_t)sizeof(text));
    VERIFY_ARE_EQUAL(memcmp(buffer + parsed.Acknowledgment.MessageTextOffset, text, sizeof(text)), 0);

    // An ACK is the same shape under a different sub id. Anything else is not an acknowledgment
    // and must not be encoded as one.
    written = BuildAcknowledgment(
        MessageType::Ack, DeviceIdFunctionBlock, 11, 22, fields,
        nullptr, 0, buffer, sizeof(buffer));

    VERIFY_ARE_EQUAL(written, AcknowledgmentFixedByteCount);
    VERIFY_ARE_EQUAL((int)Parse(buffer, written, parsed), (int)ParseStatus::Ok);
    VERIFY_ARE_EQUAL((int)parsed.Type, (int)MessageType::Ack);

    VERIFY_ARE_EQUAL(
        BuildAcknowledgment(MessageType::Discovery, DeviceIdFunctionBlock, 11, 22, fields,
            nullptr, 0, buffer, sizeof(buffer)),
        (size_t)0);
}

void MidiCiMessageTests::TestBuildDiscoveryBytes()
{
    DiscoveryReplyFields fields{};

    fields.SourceMuid = 1;
    fields.ManufacturerSysExId[0] = 0x00;
    fields.ManufacturerSysExId[1] = 0x00;
    fields.ManufacturerSysExId[2] = 0x41;
    fields.DeviceFamily = 0x0B;
    fields.DeviceFamilyModelNumber = 0x01;
    fields.SoftwareRevisionLevel[0] = 0x01;
    fields.CapabilityCategories = CategoryPropertyExchange;
    fields.ReceivableMaximumSysExSize = 512;
    fields.OutputPathId = 0x03;

    // The function block number belongs only to a reply, and must not appear here.
    fields.FunctionBlockNumber = 0x05;

    uint8_t buffer[64]{};

    const auto written = BuildDiscovery(fields, buffer, sizeof(buffer));

    VERIFY_ARE_EQUAL(written, DiscoveryByteCount);

    // Worked out from the message table, not from the builder.
    const uint8_t expected[]
    {
        0x7E, 0x7F, 0x0D, 0x70, 0x02,
        0x01, 0x00, 0x00, 0x00,
        0x7F, 0x7F, 0x7F, 0x7F,
        0x00, 0x00, 0x41,
        0x0B, 0x00,
        0x01, 0x00,
        0x01, 0x00, 0x00, 0x00,
        0x08,
        0x00, 0x04, 0x00, 0x00,
        0x03
    };

    VERIFY_ARE_EQUAL(written, sizeof(expected));
    VERIFY_ARE_EQUAL(memcmp(buffer, expected, sizeof(expected)), 0);

    ParsedMessage parsed{};

    VERIFY_ARE_EQUAL((int)Parse(buffer, written, parsed), (int)ParseStatus::Ok);
    VERIFY_ARE_EQUAL((int)parsed.Type, (int)MessageType::Discovery);
    VERIFY_ARE_EQUAL(parsed.DestinationMuid, MuidBroadcast);
    VERIFY_ARE_EQUAL(parsed.OutputPathId, (uint8_t)0x03);

    VERIFY_ARE_EQUAL(BuildDiscovery(fields, buffer, DiscoveryByteCount - 1), (size_t)0);
}

void MidiCiMessageTests::TestBuildInvalidateMuidParsesBack()
{
    uint8_t buffer[64]{};

    const auto written = BuildInvalidateMuid(11, 22, buffer, sizeof(buffer));

    VERIFY_ARE_EQUAL(written, InvalidateMuidByteCount);

    ParsedMessage parsed{};

    VERIFY_ARE_EQUAL((int)Parse(buffer, written, parsed), (int)ParseStatus::Ok);
    VERIFY_ARE_EQUAL((int)parsed.Type, (int)MessageType::InvalidateMuid);
    VERIFY_ARE_EQUAL(parsed.SourceMuid, (uint32_t)11);
    VERIFY_ARE_EQUAL(parsed.TargetMuid, (uint32_t)22);

    // Invalidate MUID is always broadcast, whoever is being told about it.
    VERIFY_ARE_EQUAL(parsed.DestinationMuid, MuidBroadcast);

    VERIFY_ARE_EQUAL(BuildInvalidateMuid(11, 22, buffer, InvalidateMuidByteCount - 1), (size_t)0);
}
