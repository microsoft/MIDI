// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================


#include "stdafx.h"

#include "MidiCiResponder.h"

using namespace WindowsMidiServicesCapabilityInquiry;


namespace
{
    constexpr uint32_t OurMuid{ 0x0123456 };
    constexpr uint32_t TheirMuid{ 0x0000042 };

    Responder MakeResponder()
    {
        ResponderConfig config{};

        config.Muid = OurMuid;

        config.ManufacturerSysExId[0] = 0x00;
        config.ManufacturerSysExId[1] = 0x00;
        config.ManufacturerSysExId[2] = 0x41;

        config.DeviceFamily = 11;
        config.DeviceFamilyModelNumber = 1;
        config.SoftwareRevisionLevel[0] = 1;

        config.ReceivableMaximumSysExSize = 512;

        Responder responder;
        responder.Initialize(config);

        return responder;
    }

    ParsedMessage MakeDiscovery(_In_ uint32_t const destinationMuid, _In_ uint8_t const outputPathId)
    {
        ParsedMessage message{};

        message.Type = MessageType::Discovery;
        message.DeviceId = DeviceIdFunctionBlock;
        message.VersionFormat = 0x02;
        message.SourceMuid = TheirMuid;
        message.DestinationMuid = destinationMuid;
        message.OutputPathId = outputPathId;

        return message;
    }
}


void MidiCiResponderTests::TestRepliesToBroadcastDiscovery()
{
    auto responder = MakeResponder();

    uint8_t reply[64]{};
    size_t replyBytes{ 0 };

    const auto action = responder.ProcessMessage(
        MakeDiscovery(MuidBroadcast, 0), reply, sizeof(reply), &replyBytes);

    VERIFY_ARE_EQUAL((int)action, (int)ResponderAction::Replied);
    VERIFY_ARE_EQUAL(replyBytes, DiscoveryReplyByteCount);

    // The reply has to be addressed back to the initiator, not broadcast onward.
    ParsedMessage parsed{};

    VERIFY_ARE_EQUAL((int)Parse(reply, replyBytes, parsed), (int)ParseStatus::Ok);
    VERIFY_ARE_EQUAL((int)parsed.Type, (int)MessageType::DiscoveryReply);
    VERIFY_ARE_EQUAL(parsed.SourceMuid, OurMuid);
    VERIFY_ARE_EQUAL(parsed.DestinationMuid, TheirMuid);
}

void MidiCiResponderTests::TestReplyEchoesOutputPathId()
{
    auto responder = MakeResponder();

    uint8_t reply[64]{};
    size_t replyBytes{ 0 };

    responder.ProcessMessage(MakeDiscovery(MuidBroadcast, 0x05), reply, sizeof(reply), &replyBytes);

    VERIFY_ARE_EQUAL(replyBytes, DiscoveryReplyByteCount);

    // Second to last byte of the reply. An initiator uses this to match the reply to the path it
    // asked on, so returning zero instead of what arrived would misroute it.
    VERIFY_ARE_EQUAL(reply[DiscoveryReplyByteCount - 2], (uint8_t)0x05);
}

void MidiCiResponderTests::TestIgnoresMessagesAddressedElsewhere()
{
    auto responder = MakeResponder();

    uint8_t reply[64]{};
    size_t replyBytes{ 0 };

    // A conversation between two other devices on the same wire.
    const auto action = responder.ProcessMessage(
        MakeDiscovery(0x0000099, 0), reply, sizeof(reply), &replyBytes);

    VERIFY_ARE_EQUAL((int)action, (int)ResponderAction::Ignored);
    VERIFY_ARE_EQUAL(replyBytes, (size_t)0);

    // Addressed to us specifically is still answered.
    const auto direct = responder.ProcessMessage(
        MakeDiscovery(OurMuid, 0), reply, sizeof(reply), &replyBytes);

    VERIFY_ARE_EQUAL((int)direct, (int)ResponderAction::Replied);
}

void MidiCiResponderTests::TestInvalidateMuidOnlyWhenItIsOurs()
{
    auto responder = MakeResponder();

    uint8_t reply[64]{};
    size_t replyBytes{ 0 };

    ParsedMessage message{};

    message.Type = MessageType::InvalidateMuid;
    message.SourceMuid = TheirMuid;
    message.DestinationMuid = MuidBroadcast;
    message.TargetMuid = 0x0000777;

    // Another device withdrawing its own identifier is reported so a host can drop state it was
    // holding for it, but it must not disturb ours.
    VERIFY_ARE_EQUAL(
        (int)responder.ProcessMessage(message, reply, sizeof(reply), &replyBytes),
        (int)ResponderAction::InitiatorMuidInvalidated);

    VERIFY_ARE_EQUAL(replyBytes, (size_t)0);
    VERIFY_IS_FALSE(responder.MuidNeedsReplacement());
    VERIFY_ARE_EQUAL(responder.Muid(), OurMuid);

    message.TargetMuid = OurMuid;

    VERIFY_ARE_EQUAL(
        (int)responder.ProcessMessage(message, reply, sizeof(reply), &replyBytes),
        (int)ResponderAction::MuidInvalidated);

    VERIFY_IS_TRUE(responder.MuidNeedsReplacement());
    VERIFY_ARE_EQUAL(responder.Muid(), (uint32_t)0);

    // A fresh identifier clears the flag and puts the responder back in service.
    responder.SetMuid(0x0000ABC);

    VERIFY_IS_FALSE(responder.MuidNeedsReplacement());

    VERIFY_ARE_EQUAL(
        (int)responder.ProcessMessage(MakeDiscovery(MuidBroadcast, 0), reply, sizeof(reply), &replyBytes),
        (int)ResponderAction::Replied);
}

void MidiCiResponderTests::TestShortReplyBufferIsReportedNotTruncated()
{
    auto responder = MakeResponder();

    uint8_t reply[DiscoveryReplyByteCount]{};
    size_t replyBytes{ 0 };

    const auto action = responder.ProcessMessage(
        MakeDiscovery(MuidBroadcast, 0), reply, DiscoveryReplyByteCount - 1, &replyBytes);

    VERIFY_ARE_EQUAL((int)action, (int)ResponderAction::ReplyBufferTooSmall);
    VERIFY_ARE_EQUAL(replyBytes, (size_t)0);

    // Nothing may have been written.
    for (size_t i = 0; i < sizeof(reply); i++)
    {
        VERIFY_ARE_EQUAL(reply[i], (uint8_t)0);
    }
}

void MidiCiResponderTests::TestDoesNotReplyWithoutAMuid()
{
    ResponderConfig config{};
    config.ManufacturerSysExId[2] = 0x41;

    Responder responder;
    responder.Initialize(config);

    uint8_t reply[64]{};
    size_t replyBytes{ 0 };

    // Replying with a MUID of zero would claim an identifier we were never given.
    const auto action = responder.ProcessMessage(
        MakeDiscovery(MuidBroadcast, 0), reply, sizeof(reply), &replyBytes);

    VERIFY_ARE_EQUAL((int)action, (int)ResponderAction::Ignored);
    VERIFY_ARE_EQUAL(replyBytes, (size_t)0);
}

void MidiCiResponderTests::TestPropertyExchangeCapabilityBit()
{
    uint8_t reply[64]{};
    size_t replyBytes{ 0 };

    // Off by default. Byte 24: five bytes of preamble, two MUIDs, three manufacturer, two family,
    // two model, four revision.
    constexpr size_t capabilityOffset{ 24 };

    auto plain = MakeResponder();
    plain.ProcessMessage(MakeDiscovery(MuidBroadcast, 0), reply, sizeof(reply), &replyBytes);

    VERIFY_ARE_EQUAL(replyBytes, DiscoveryReplyByteCount);
    VERIFY_ARE_EQUAL(reply[capabilityOffset], (uint8_t)0x00);

    // With Property Exchange declared, bit D3 must be set or no initiator will ever ask us for a
    // property.
    ResponderConfig config{};

    config.Muid = OurMuid;
    config.ManufacturerSysExId[2] = 0x41;
    config.SupportsPropertyExchange = true;

    Responder supporting;
    supporting.Initialize(config);

    supporting.ProcessMessage(MakeDiscovery(MuidBroadcast, 0), reply, sizeof(reply), &replyBytes);

    VERIFY_ARE_EQUAL(replyBytes, DiscoveryReplyByteCount);
    VERIFY_ARE_EQUAL(reply[capabilityOffset] & CapabilityBitPropertyExchange, (int)CapabilityBitPropertyExchange);
}

void MidiCiResponderTests::TestGetPropertyDataIsHandedToTheCaller()
{
    ResponderConfig config{};

    config.Muid = OurMuid;
    config.ManufacturerSysExId[2] = 0x41;
    config.SupportsPropertyExchange = true;

    Responder responder;
    responder.Initialize(config);

    ParsedMessage message{};

    message.Type = MessageType::PropertyGetDataInquiry;
    message.SourceMuid = TheirMuid;
    message.DestinationMuid = OurMuid;
    message.HasPropertyExchangeFields = true;
    message.PropertyExchange.RequestId = 4;

    uint8_t reply[64]{};
    size_t replyBytes{ 0 };

    // The responder must not answer this itself. Reading the header means parsing JSON, which at
    // this layer is not allowed, so the request is handed up instead.
    VERIFY_ARE_EQUAL(
        (int)responder.ProcessMessage(message, reply, sizeof(reply), &replyBytes),
        (int)ResponderAction::PropertyDataRequested);

    VERIFY_ARE_EQUAL(replyBytes, (size_t)0);

    // A device that never declared Property Exchange must not accept a property request.
    auto notSupporting = MakeResponder();

    VERIFY_ARE_EQUAL(
        (int)notSupporting.ProcessMessage(message, reply, sizeof(reply), &replyBytes),
        (int)ResponderAction::Ignored);

    // Framing that never parsed cannot be trusted to describe a request.
    message.HasPropertyExchangeFields = false;

    VERIFY_ARE_EQUAL(
        (int)responder.ProcessMessage(message, reply, sizeof(reply), &replyBytes),
        (int)ResponderAction::Ignored);
}

void MidiCiResponderTests::TestSubscriptionIsHandedToTheCaller()
{
    ResponderConfig config{};

    config.Muid = OurMuid;
    config.ManufacturerSysExId[2] = 0x41;
    config.SupportsPropertyExchange = true;

    Responder responder;
    responder.Initialize(config);

    ParsedMessage message{};

    message.Type = MessageType::PropertySubscriptionInquiry;
    message.SourceMuid = TheirMuid;
    message.DestinationMuid = OurMuid;
    message.HasPropertyExchangeFields = true;
    message.PropertyExchange.RequestId = 9;

    uint8_t reply[64]{};
    size_t replyBytes{ 0 };

    // Same reason as a get: the command and the subscription identifier are both in the header,
    // and reading JSON is not something this layer may do.
    VERIFY_ARE_EQUAL(
        (int)responder.ProcessMessage(message, reply, sizeof(reply), &replyBytes),
        (int)ResponderAction::PropertySubscriptionRequested);

    VERIFY_ARE_EQUAL(replyBytes, (size_t)0);

    auto notSupporting = MakeResponder();

    VERIFY_ARE_EQUAL(
        (int)notSupporting.ProcessMessage(message, reply, sizeof(reply), &replyBytes),
        (int)ResponderAction::Ignored);

    message.HasPropertyExchangeFields = false;

    VERIFY_ARE_EQUAL(
        (int)responder.ProcessMessage(message, reply, sizeof(reply), &replyBytes),
        (int)ResponderAction::Ignored);
}

void MidiCiResponderTests::TestPropertyExchangeCapabilitiesReply()
{
    ResponderConfig config{};

    config.Muid = OurMuid;
    config.ManufacturerSysExId[2] = 0x41;
    config.SupportsPropertyExchange = true;

    Responder responder;
    responder.Initialize(config);

    ParsedMessage message{};

    message.Type = MessageType::PropertyExchangeCapabilitiesInquiry;
    message.SourceMuid = TheirMuid;
    message.DestinationMuid = OurMuid;

    uint8_t reply[64]{};
    size_t replyBytes{ 0 };

    VERIFY_ARE_EQUAL(
        (int)responder.ProcessMessage(message, reply, sizeof(reply), &replyBytes),
        (int)ResponderAction::Replied);

    VERIFY_ARE_EQUAL(replyBytes, PropertyExchangeCapabilitiesByteCount);

    // Worked out by hand from the message format table, not from the builder.
    const uint8_t expected[]
    {
        0x7E, 0x7F, 0x0D, 0x31, 0x02,
        0x56, 0x68, 0x48, 0x00,             // source muid 0x0123456
        0x42, 0x00, 0x00, 0x00,             // destination muid 0x42
        0x01,                               // one simultaneous request
        0x00, 0x00                          // property exchange version 0.0
    };

    VERIFY_ARE_EQUAL(sizeof(expected), PropertyExchangeCapabilitiesByteCount);

    for (size_t i = 0; i < sizeof(expected); i++)
    {
        if (reply[i] != expected[i])
        {
            LOG_OUTPUT(L"Mismatch at %llu: got %02x expected %02x",
                (unsigned long long)i, reply[i], expected[i]);
        }

        VERIFY_ARE_EQUAL(reply[i], expected[i]);
    }

    // The same inquiry from a MIDI-CI 1.1 device. It reads a fixed length, so the reply has to
    // stop after the request count and be stamped 1.1 rather than carry the two bytes 1.2 added.
    message.VersionFormat = MessageVersion11;

    VERIFY_ARE_EQUAL(
        (int)responder.ProcessMessage(message, reply, sizeof(reply), &replyBytes),
        (int)ResponderAction::Replied);

    VERIFY_ARE_EQUAL(replyBytes, PropertyExchangeCapabilitiesByteCountVersion11);
    VERIFY_ARE_EQUAL(reply[4], (uint8_t)MessageVersion11);
    VERIFY_ARE_EQUAL(reply[13], (uint8_t)0x01);

    message.VersionFormat = 0;

    // A device that never declared Property Exchange has no capabilities to report.
    auto plain = MakeResponder();

    VERIFY_ARE_EQUAL(
        (int)plain.ProcessMessage(message, reply, sizeof(reply), &replyBytes),
        (int)ResponderAction::Ignored);
}
