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

    VERIFY_ARE_EQUAL(
        (int)responder.ProcessMessage(message, reply, sizeof(reply), &replyBytes),
        (int)ResponderAction::Ignored);

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
