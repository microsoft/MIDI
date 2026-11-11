// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================


#include "stdafx.h"

#include <vector>

namespace ci = winrt::Windows::Devices::Midi2::CapabilityInquiry;
namespace json = winrt::Windows::Data::Json;

namespace
{
    // A capability inquiry message travels as a whole system exclusive transfer, and a chunked
    // property exchange reply is several of them one after another. Splitting on the packet status
    // is how a receiver tells where one ends and the next begins.
    std::vector<ci::MidiCapabilityInquiryMessage> DecodeAll(
        winrt::Windows::Foundation::Collections::IVector<MidiMessage64> const& messages)
    {
        std::vector<ci::MidiCapabilityInquiryMessage> decoded{};

        auto transfer = winrt::single_threaded_vector<MidiMessage64>();

        for (auto const& message : messages)
        {
            transfer.Append(message);

            auto const status = (message.Word0() >> 20) & 0x0F;

            // Complete in one packet, or the end of a multi packet transfer.
            if (status == 0x0 || status == 0x3)
            {
                decoded.push_back(ci::MidiCapabilityInquiryMessage::FromUmpMessages(transfer));

                transfer = winrt::single_threaded_vector<MidiMessage64>();
            }
        }

        return decoded;
    }

    ci::MidiCapabilityInquiryMessage DecodeOne(
        winrt::Windows::Foundation::Collections::IVector<MidiMessage64> const& messages)
    {
        auto const decoded = DecodeAll(messages);

        VERIFY_ARE_EQUAL(decoded.size(), (size_t)1);

        return decoded[0];
    }

    MidiDeclaredDeviceIdentity TestIdentity()
    {
        // Microsoft's own identifier, which is three bytes because the first is zero.
        return MidiDeclaredDeviceIdentity(
            0x00, 0x00, 0x41,
            0x0B, 0x00,
            0x01, 0x00,
            0x01, 0x02, 0x03, 0x04);
    }

    MidiGroup TestGroup()
    {
        return MidiGroup((uint8_t)0);
    }

    winrt::Windows::Foundation::Collections::IVector<uint8_t> ToByteVector(
        std::vector<uint8_t> const& bytes)
    {
        auto result = winrt::single_threaded_vector<uint8_t>();

        for (auto const value : bytes)
        {
            result.Append(value);
        }

        return result;
    }
}


void MidiCapabilityInquiryMessageTests::TestProfileIdNamesItsParts()
{
    auto const standard = ci::MidiProfileId::CreateStandardDefined(0x01, 0x02, 0x03, 0x04);

    VERIFY_IS_TRUE(standard.IsStandardDefined());
    VERIFY_ARE_EQUAL(standard.IdByte1(), (uint8_t)0x7E);
    VERIFY_ARE_EQUAL(standard.ProfileBank(), (uint8_t)0x01);
    VERIFY_ARE_EQUAL(standard.ProfileNumber(), (uint8_t)0x02);
    VERIFY_ARE_EQUAL(standard.ProfileVersion(), (uint8_t)0x03);
    VERIFY_ARE_EQUAL(standard.ProfileLevel(), (uint8_t)0x04);

    // A manufacturer's bytes mean something the profile specifications do not define, so the named
    // accessors must not report them as though they did.
    auto const manufacturer = ci::MidiProfileId::CreateManufacturerSpecific(0x41, 0x00, 0x00, 0x12, 0x34);

    VERIFY_IS_FALSE(manufacturer.IsStandardDefined());
    VERIFY_ARE_EQUAL(manufacturer.IdByte4(), (uint8_t)0x12);
    VERIFY_ARE_EQUAL(manufacturer.ProfileVersion(), (uint8_t)0x00);

    // Version and level are part of the identity, so two versions of one profile are two profiles.
    auto const otherVersion = ci::MidiProfileId::CreateStandardDefined(0x01, 0x02, 0x04, 0x04);

    VERIFY_IS_TRUE(standard.IsSameProfileAs(ci::MidiProfileId::CreateStandardDefined(0x01, 0x02, 0x03, 0x04)));
    VERIFY_IS_FALSE(standard.IsSameProfileAs(otherVersion));
    VERIFY_IS_FALSE(standard.IsSameProfileAs(nullptr));
}

void MidiCapabilityInquiryMessageTests::TestDiscoveryRoundTripsThroughPackets()
{
    auto const sourceMuid = ci::MidiUniqueId(0x0123456);

    auto const messages = ci::MidiCapabilityInquiryMessageBuilder::BuildDiscovery(
        0,
        TestGroup(),
        sourceMuid,
        TestIdentity(),
        ci::MidiCapabilityInquiryCategories::PropertyExchange | ci::MidiCapabilityInquiryCategories::ProfileConfiguration,
        512,
        0x03);

    VERIFY_IS_GREATER_THAN(messages.Size(), (uint32_t)0);

    auto const decoded = DecodeOne(messages);

    VERIFY_IS_TRUE(decoded.IsValid());
    VERIFY_ARE_EQUAL((int)decoded.MessageType(), (int)ci::MidiCapabilityInquiryMessageType::Discovery);
    VERIFY_ARE_EQUAL(decoded.SourceMuid().AsCombined28BitValue(), sourceMuid.AsCombined28BitValue());

    // Discovery is always broadcast: the initiator does not yet know who is listening.
    VERIFY_IS_TRUE(decoded.DestinationMuid().IsBroadcast());

    // A responder has to echo this so an initiator with several outputs knows which one arrived.
    VERIFY_ARE_EQUAL(decoded.OutputPathId(), (uint8_t)0x03);

    VERIFY_IS_FALSE(decoded.HasPropertyExchangeFields());
    VERIFY_IS_FALSE(decoded.HasProfileFields());
}

void MidiCapabilityInquiryMessageTests::TestDiscoveryReplyCarriesTheIdentity()
{
    auto const messages = ci::MidiCapabilityInquiryMessageBuilder::BuildDiscoveryReply(
        0,
        TestGroup(),
        ci::MidiUniqueId(1000),
        ci::MidiUniqueId(2000),
        TestIdentity(),
        ci::MidiCapabilityInquiryCategories::PropertyExchange,
        512,
        0x00,
        0x05);

    auto const decoded = DecodeOne(messages);

    VERIFY_IS_TRUE(decoded.IsValid());
    VERIFY_ARE_EQUAL((int)decoded.MessageType(), (int)ci::MidiCapabilityInquiryMessageType::DiscoveryReply);
    VERIFY_ARE_EQUAL(decoded.SourceMuid().AsCombined28BitValue(), (uint32_t)1000);
    VERIFY_ARE_EQUAL(decoded.DestinationMuid().AsCombined28BitValue(), (uint32_t)2000);

    // The identity is not decoded into fields, so read it out of the payload. Manufacturer,
    // family and model start immediately after the thirteen byte common header.
    auto const data = decoded.Data();

    VERIFY_ARE_EQUAL(data.GetAt(13), (uint8_t)0x00);
    VERIFY_ARE_EQUAL(data.GetAt(14), (uint8_t)0x00);
    VERIFY_ARE_EQUAL(data.GetAt(15), (uint8_t)0x41);
    VERIFY_ARE_EQUAL(data.GetAt(16), (uint8_t)0x0B);

    // The function block number is the last byte, and belongs only to a reply.
    VERIFY_ARE_EQUAL(data.GetAt(data.Size() - 1), (uint8_t)0x05);
}

void MidiCapabilityInquiryMessageTests::TestInvalidateMuidIsBroadcast()
{
    auto const messages = ci::MidiCapabilityInquiryMessageBuilder::BuildInvalidateMuid(
        0, TestGroup(), ci::MidiUniqueId(1000), ci::MidiUniqueId(2000));

    auto const decoded = DecodeOne(messages);

    VERIFY_ARE_EQUAL((int)decoded.MessageType(), (int)ci::MidiCapabilityInquiryMessageType::InvalidateMuid);
    VERIFY_IS_TRUE(decoded.DestinationMuid().IsBroadcast());
    VERIFY_ARE_EQUAL(decoded.TargetMuid().AsCombined28BitValue(), (uint32_t)2000);
}

void MidiCapabilityInquiryMessageTests::TestProfileInquiryReplyReturnsProfiles()
{
    auto enabled = winrt::single_threaded_vector<ci::MidiProfileId>();
    enabled.Append(ci::MidiProfileId::CreateStandardDefined(0x01, 0x01, 0x01, 0x01));
    enabled.Append(ci::MidiProfileId::CreateStandardDefined(0x01, 0x02, 0x01, 0x01));

    auto disabled = winrt::single_threaded_vector<ci::MidiProfileId>();
    disabled.Append(ci::MidiProfileId::CreateManufacturerSpecific(0x41, 0x00, 0x00, 0x12, 0x34));

    auto const messages = ci::MidiCapabilityInquiryMessageBuilder::BuildProfileInquiryReply(
        0, TestGroup(), 0x00, ci::MidiUniqueId(1000), ci::MidiUniqueId(2000), enabled, disabled);

    auto const decoded = DecodeOne(messages);

    VERIFY_IS_TRUE(decoded.IsValid());
    VERIFY_IS_TRUE(decoded.HasProfileFields());

    // The reply describes whole lists, so it names no single profile.
    VERIFY_IS_NULL(decoded.ProfileId());

    VERIFY_ARE_EQUAL(decoded.EnabledProfiles().Size(), (uint32_t)2);
    VERIFY_ARE_EQUAL(decoded.DisabledProfiles().Size(), (uint32_t)1);

    VERIFY_IS_TRUE(decoded.EnabledProfiles().GetAt(1).IsSameProfileAs(
        ci::MidiProfileId::CreateStandardDefined(0x01, 0x02, 0x01, 0x01)));

    VERIFY_IS_FALSE(decoded.DisabledProfiles().GetAt(0).IsStandardDefined());
    VERIFY_ARE_EQUAL(decoded.DisabledProfiles().GetAt(0).IdByte5(), (uint8_t)0x34);

    // A device with nothing to declare still replies, and both lists come back empty rather than
    // as a message that failed to decode.
    auto const emptyReply = ci::MidiCapabilityInquiryMessageBuilder::BuildProfileInquiryReply(
        0, TestGroup(), 0x7F, ci::MidiUniqueId(1000), ci::MidiUniqueId(2000), nullptr, nullptr);

    auto const emptyDecoded = DecodeOne(emptyReply);

    VERIFY_IS_TRUE(emptyDecoded.IsValid());
    VERIFY_ARE_EQUAL(emptyDecoded.EnabledProfiles().Size(), (uint32_t)0);
    VERIFY_ARE_EQUAL(emptyDecoded.DisabledProfiles().Size(), (uint32_t)0);
}

void MidiCapabilityInquiryMessageTests::TestSetProfileOnCarriesTheChannelCount()
{
    auto const profileId = ci::MidiProfileId::CreateStandardDefined(0x01, 0x01, 0x01, 0x01);

    auto const messages = ci::MidiCapabilityInquiryMessageBuilder::BuildSetProfileOn(
        0, TestGroup(), 0x03, ci::MidiUniqueId(1000), ci::MidiUniqueId(2000), profileId, 4);

    auto const decoded = DecodeOne(messages);

    VERIFY_ARE_EQUAL((int)decoded.MessageType(), (int)ci::MidiCapabilityInquiryMessageType::SetProfileOn);

    // Profile messages may address a single channel, which is what makes them different from
    // property exchange.
    VERIFY_ARE_EQUAL(decoded.DeviceId(), (uint8_t)0x03);

    VERIFY_IS_TRUE(decoded.ProfileId().IsSameProfileAs(profileId));
    VERIFY_ARE_EQUAL(decoded.ProfileChannelCount(), (uint16_t)4);

    // Set Profile Off has the same shape with those two bytes reserved.
    auto const offMessages = ci::MidiCapabilityInquiryMessageBuilder::BuildSetProfileOff(
        0, TestGroup(), 0x03, ci::MidiUniqueId(1000), ci::MidiUniqueId(2000), profileId);

    auto const offDecoded = DecodeOne(offMessages);

    VERIFY_ARE_EQUAL((int)offDecoded.MessageType(), (int)ci::MidiCapabilityInquiryMessageType::SetProfileOff);
    VERIFY_ARE_EQUAL(offDecoded.ProfileChannelCount(), (uint16_t)0);
}

void MidiCapabilityInquiryMessageTests::TestProfileReportsAreBroadcast()
{
    auto const profileId = ci::MidiProfileId::CreateStandardDefined(0x01, 0x01, 0x01, 0x01);

    auto const enabledReport = DecodeOne(
        ci::MidiCapabilityInquiryMessageBuilder::BuildProfileEnabledReport(
            0, TestGroup(), 0x00, ci::MidiUniqueId(1000), profileId, 2));

    VERIFY_ARE_EQUAL((int)enabledReport.MessageType(), (int)ci::MidiCapabilityInquiryMessageType::ProfileEnabledReport);
    VERIFY_IS_TRUE(enabledReport.DestinationMuid().IsBroadcast());
    VERIFY_ARE_EQUAL(enabledReport.ProfileChannelCount(), (uint16_t)2);

    auto const addedReport = DecodeOne(
        ci::MidiCapabilityInquiryMessageBuilder::BuildProfileAddedReport(
            0, TestGroup(), 0x00, ci::MidiUniqueId(1000), profileId));

    VERIFY_ARE_EQUAL((int)addedReport.MessageType(), (int)ci::MidiCapabilityInquiryMessageType::ProfileAddedReport);
    VERIFY_IS_TRUE(addedReport.DestinationMuid().IsBroadcast());
    VERIFY_IS_TRUE(addedReport.ProfileId().IsSameProfileAs(profileId));

    auto const removedReport = DecodeOne(
        ci::MidiCapabilityInquiryMessageBuilder::BuildProfileRemovedReport(
            0, TestGroup(), 0x00, ci::MidiUniqueId(1000), profileId));

    VERIFY_ARE_EQUAL((int)removedReport.MessageType(), (int)ci::MidiCapabilityInquiryMessageType::ProfileRemovedReport);
    VERIFY_IS_TRUE(removedReport.DestinationMuid().IsBroadcast());
}

void MidiCapabilityInquiryMessageTests::TestProfileSpecificDataRoundTrips()
{
    auto const profileId = ci::MidiProfileId::CreateManufacturerSpecific(0x41, 0x00, 0x00, 0x01, 0x02);

    std::vector<uint8_t> payload{};

    for (uint8_t i = 0; i < 100; i++)
    {
        payload.push_back(i);
    }

    auto const messages = ci::MidiCapabilityInquiryMessageBuilder::BuildProfileSpecificData(
        0, TestGroup(), 0x7F, ci::MidiUniqueId(1000), ci::MidiUniqueId(2000),
        profileId, ToByteVector(payload));

    auto const decoded = DecodeOne(messages);

    VERIFY_ARE_EQUAL((int)decoded.MessageType(), (int)ci::MidiCapabilityInquiryMessageType::ProfileSpecificData);
    VERIFY_IS_TRUE(decoded.ProfileId().IsSameProfileAs(profileId));
    VERIFY_ARE_EQUAL(decoded.ProfileData().Size(), (uint32_t)payload.size());
    VERIFY_ARE_EQUAL(decoded.ProfileData().GetAt(99), (uint8_t)99);

    // A details reply is the same idea behind a shorter length field, and carries the target back.
    auto const detailsMessages = ci::MidiCapabilityInquiryMessageBuilder::BuildProfileDetailsReply(
        0, TestGroup(), 0x00, ci::MidiUniqueId(1000), ci::MidiUniqueId(2000),
        profileId, 0x01, ToByteVector({ 0x0A, 0x0B }));

    auto const detailsDecoded = DecodeOne(detailsMessages);

    VERIFY_IS_TRUE(detailsDecoded.HasProfileInquiryTarget());
    VERIFY_ARE_EQUAL(detailsDecoded.ProfileInquiryTarget(), (uint8_t)0x01);
    VERIFY_ARE_EQUAL(detailsDecoded.ProfileData().Size(), (uint32_t)2);
    VERIFY_ARE_EQUAL(detailsDecoded.ProfileData().GetAt(1), (uint8_t)0x0B);
}

void MidiCapabilityInquiryMessageTests::TestNakCarriesStatusAndText()
{
    auto const messages = ci::MidiCapabilityInquiryMessageBuilder::BuildNak(
        0,
        TestGroup(),
        0x7F,
        ci::MidiUniqueId(1000),
        ci::MidiUniqueId(2000),
        ci::MidiCapabilityInquiryMessageType::PropertyGetDataInquiry,
        0x01,
        0x00,
        ToByteVector({ 0x05, 0x01, 0x00, 0x00, 0x00 }),
        L"Resource not available");

    auto const decoded = DecodeOne(messages);

    VERIFY_ARE_EQUAL((int)decoded.MessageType(), (int)ci::MidiCapabilityInquiryMessageType::Nak);
    VERIFY_IS_TRUE(decoded.HasAcknowledgmentFields());

    // Matching a failure back to the transaction that caused it is the whole point of these fields.
    VERIFY_ARE_EQUAL(
        (int)decoded.OriginalMessageType(),
        (int)ci::MidiCapabilityInquiryMessageType::PropertyGetDataInquiry);

    VERIFY_ARE_EQUAL(decoded.StatusCode(), (uint8_t)0x01);
    VERIFY_ARE_EQUAL(decoded.StatusDetails().Size(), (uint32_t)5);
    VERIFY_ARE_EQUAL(decoded.StatusDetails().GetAt(0), (uint8_t)0x05);
    VERIFY_ARE_EQUAL(decoded.StatusMessage(), winrt::hstring{ L"Resource not available" });

    // An ACK is the same shape under a different sub id.
    auto const ackDecoded = DecodeOne(
        ci::MidiCapabilityInquiryMessageBuilder::BuildAck(
            0, TestGroup(), 0x7F, ci::MidiUniqueId(1000), ci::MidiUniqueId(2000),
            ci::MidiCapabilityInquiryMessageType::PropertySetDataInquiry, 0x00, 0x00, nullptr, L""));

    VERIFY_ARE_EQUAL((int)ackDecoded.MessageType(), (int)ci::MidiCapabilityInquiryMessageType::Ack);
    VERIFY_IS_TRUE(ackDecoded.HasAcknowledgmentFields());
    VERIFY_ARE_EQUAL(ackDecoded.StatusMessage(), winrt::hstring{ L"" });
}

void MidiCapabilityInquiryMessageTests::TestBuilderRefusesTextThatCannotTravel()
{
    // Status text travels as seven bit bytes. Masking the high bit would put a different word on
    // the wire under the caller's name, so the message is refused instead.
    auto const messages = ci::MidiCapabilityInquiryMessageBuilder::BuildNak(
        0,
        TestGroup(),
        0x7F,
        ci::MidiUniqueId(1000),
        ci::MidiUniqueId(2000),
        ci::MidiCapabilityInquiryMessageType::PropertyGetDataInquiry,
        0x01,
        0x00,
        nullptr,
        L"Ressource indisponible\u00E9");

    VERIFY_ARE_EQUAL(messages.Size(), (uint32_t)0);
}

void MidiCapabilityInquiryMessageTests::TestPropertyGetDataInquiryHeaderIsReadable()
{
    json::JsonObject header{};

    header.SetNamedValue(L"resource", json::JsonValue::CreateStringValue(L"ProgramList"));
    header.SetNamedValue(L"offset", json::JsonValue::CreateNumberValue(100));
    header.SetNamedValue(L"limit", json::JsonValue::CreateNumberValue(25));

    auto const messages = ci::MidiCapabilityInquiryMessageBuilder::BuildPropertyGetDataInquiry(
        0, TestGroup(), ci::MidiUniqueId(1000), ci::MidiUniqueId(2000), 7, header);

    auto const decoded = DecodeOne(messages);

    VERIFY_ARE_EQUAL(
        (int)decoded.MessageType(),
        (int)ci::MidiCapabilityInquiryMessageType::PropertyGetDataInquiry);

    // Property exchange is only ever addressed to the whole function block.
    VERIFY_ARE_EQUAL(decoded.DeviceId(), (uint8_t)0x7F);

    VERIFY_IS_TRUE(decoded.HasPropertyExchangeFields());
    VERIFY_ARE_EQUAL(decoded.RequestId(), (uint8_t)7);

    // A message with a header and no body is one chunk, numbered one.
    VERIFY_ARE_EQUAL(decoded.ChunkCount(), (uint16_t)1);
    VERIFY_ARE_EQUAL(decoded.ChunkNumber(), (uint16_t)1);
    VERIFY_ARE_EQUAL(decoded.Body().Size(), (uint32_t)0);

    VERIFY_IS_NOT_NULL(decoded.Header());
    VERIFY_ARE_EQUAL(decoded.Header().GetNamedString(L"resource"), winrt::hstring{ L"ProgramList" });
    VERIFY_ARE_EQUAL((int)decoded.Header().GetNamedNumber(L"offset"), 100);
    VERIFY_ARE_EQUAL((int)decoded.Header().GetNamedNumber(L"limit"), 25);
}

void MidiCapabilityInquiryMessageTests::TestPropertyHeaderEscapesTextOutsideSevenBits()
{
    // A resource identifier or a title can hold anything a device chose to call it. The header
    // still has to travel as seven bit bytes, so anything above that is escaped rather than
    // dropped, and it has to survive the round trip intact.
    json::JsonObject header{};

    header.SetNamedValue(L"resource", json::JsonValue::CreateStringValue(L"ProgramList"));
    header.SetNamedValue(L"resId", json::JsonValue::CreateStringValue(L"Klavier \u00FCber alles \u4E2D\u6587"));

    auto const messages = ci::MidiCapabilityInquiryMessageBuilder::BuildPropertyGetDataInquiry(
        0, TestGroup(), ci::MidiUniqueId(1000), ci::MidiUniqueId(2000), 1, header);

    auto const decoded = DecodeOne(messages);

    VERIFY_IS_NOT_NULL(decoded.Header());
    VERIFY_ARE_EQUAL(
        decoded.Header().GetNamedString(L"resId"),
        winrt::hstring{ L"Klavier \u00FCber alles \u4E2D\u6587" });

    // Nothing in the payload may have its high bit set, whatever the header said.
    for (auto const value : decoded.Data())
    {
        VERIFY_IS_LESS_THAN(value, (uint8_t)0x80);
    }
}

void MidiCapabilityInquiryMessageTests::TestLargePropertyBodyIsChunked()
{
    json::JsonObject header{};

    header.SetNamedValue(L"status", json::JsonValue::CreateNumberValue(200));

    std::vector<uint8_t> body{};

    for (size_t i = 0; i < 4000; i++)
    {
        body.push_back(static_cast<uint8_t>(i % 0x80));
    }

    // A device that declared the smallest size the specification allows.
    const uint32_t destinationMaximum = 512;

    auto const expectedChunks = ci::MidiCapabilityInquiryMessageBuilder::GetPropertyChunkCount(
        header, (uint32_t)body.size(), destinationMaximum);

    VERIFY_IS_GREATER_THAN(expectedChunks, (uint16_t)5);

    auto const messages = ci::MidiCapabilityInquiryMessageBuilder::BuildPropertyMessage(
        0,
        TestGroup(),
        ci::MidiCapabilityInquiryMessageType::PropertyGetDataInquiryReply,
        ci::MidiUniqueId(1000),
        ci::MidiUniqueId(2000),
        9,
        header,
        ToByteVector(body),
        destinationMaximum);

    auto const decoded = DecodeAll(messages);

    // The count reported without building anything has to be the count actually built, or a
    // caller showing progress would show the wrong thing.
    VERIFY_ARE_EQUAL(decoded.size(), (size_t)expectedChunks);

    std::vector<uint8_t> reassembled{};

    for (size_t i = 0; i < decoded.size(); i++)
    {
        auto const& chunk = decoded[i];

        VERIFY_IS_TRUE(chunk.IsValid());
        VERIFY_IS_TRUE(chunk.HasPropertyExchangeFields());
        VERIFY_ARE_EQUAL(chunk.RequestId(), (uint8_t)9);
        VERIFY_ARE_EQUAL(chunk.ChunkCount(), expectedChunks);
        VERIFY_ARE_EQUAL(chunk.ChunkNumber(), (uint16_t)(i + 1));

        // The header goes on the first chunk and no other.
        if (i == 0)
        {
            VERIFY_IS_NOT_NULL(chunk.Header());
            VERIFY_ARE_EQUAL((int)chunk.Header().GetNamedNumber(L"status"), 200);
        }
        else
        {
            VERIFY_IS_NULL(chunk.Header());
            VERIFY_ARE_EQUAL(chunk.HeaderText(), winrt::hstring{ L"" });
        }

        for (auto const value : chunk.Body())
        {
            reassembled.push_back(value);
        }
    }

    // No byte dropped at a boundary and none sent twice.
    VERIFY_ARE_EQUAL(reassembled.size(), body.size());
    VERIFY_IS_TRUE(reassembled == body);
}

void MidiCapabilityInquiryMessageTests::TestBuilderRefusesAMessageTypeItCannotBuild()
{
    json::JsonObject header{};

    header.SetNamedValue(L"resource", json::JsonValue::CreateStringValue(L"DeviceInfo"));

    // Discovery is not a property exchange message, and encoding it through this path would put a
    // property exchange body behind a management sub id.
    auto const messages = ci::MidiCapabilityInquiryMessageBuilder::BuildPropertyMessage(
        0,
        TestGroup(),
        ci::MidiCapabilityInquiryMessageType::Discovery,
        ci::MidiUniqueId(1000),
        ci::MidiUniqueId(2000),
        1,
        header,
        nullptr,
        512);

    VERIFY_ARE_EQUAL(messages.Size(), (uint32_t)0);
}

void MidiCapabilityInquiryMessageTests::TestEndpointInquiryAndReplyRoundTrip()
{
    auto const inquiry = DecodeOne(
        ci::MidiCapabilityInquiryMessageBuilder::BuildEndpointInquiry(
            0, TestGroup(), ci::MidiUniqueId(1000), ci::MidiUniqueId(2000), 0x00));

    VERIFY_IS_TRUE(inquiry.IsValid());
    VERIFY_ARE_EQUAL((int)inquiry.MessageType(), (int)ci::MidiCapabilityInquiryMessageType::EndpointInquiry);
    VERIFY_ARE_EQUAL(inquiry.SourceMuid().AsCombined28BitValue(), (uint32_t)1000);
    VERIFY_ARE_EQUAL(inquiry.DestinationMuid().AsCombined28BitValue(), (uint32_t)2000);
    VERIFY_IS_TRUE(inquiry.HasEndpointFields());
    VERIFY_ARE_EQUAL(inquiry.EndpointStatus(), (uint8_t)0x00);
    VERIFY_ARE_EQUAL(inquiry.EndpointInformation().Size(), (uint32_t)0);

    auto const reply = DecodeOne(
        ci::MidiCapabilityInquiryMessageBuilder::BuildEndpointReply(
            0, TestGroup(), ci::MidiUniqueId(2000), ci::MidiUniqueId(1000), 0x00,
            ToByteVector({ 'G', 'M', '1' })));

    VERIFY_IS_TRUE(reply.IsValid());
    VERIFY_ARE_EQUAL((int)reply.MessageType(), (int)ci::MidiCapabilityInquiryMessageType::EndpointInquiryReply);
    VERIFY_ARE_EQUAL(reply.SourceMuid().AsCombined28BitValue(), (uint32_t)2000);
    VERIFY_ARE_EQUAL(reply.DestinationMuid().AsCombined28BitValue(), (uint32_t)1000);
    VERIFY_IS_TRUE(reply.HasEndpointFields());
    VERIFY_ARE_EQUAL(reply.EndpointStatus(), (uint8_t)0x00);
    VERIFY_ARE_EQUAL(reply.EndpointInformation().Size(), (uint32_t)3);
    VERIFY_ARE_EQUAL(reply.EndpointInformation().GetAt(2), (uint8_t)'1');

    // Only status 0x00 has rules for its information. Anything else is carried as given.
    auto const otherStatus = DecodeOne(
        ci::MidiCapabilityInquiryMessageBuilder::BuildEndpointReply(
            0, TestGroup(), ci::MidiUniqueId(2000), ci::MidiUniqueId(1000), 0x05,
            ToByteVector({ 0x00, 0x7F })));

    VERIFY_ARE_EQUAL(otherStatus.EndpointStatus(), (uint8_t)0x05);
    VERIFY_ARE_EQUAL(otherStatus.EndpointInformation().Size(), (uint32_t)2);
    VERIFY_ARE_EQUAL(otherStatus.EndpointInformation().GetAt(1), (uint8_t)0x7F);
}

void MidiCapabilityInquiryMessageTests::TestEndpointReplyRefusesAnInvalidProductInstanceId()
{
    auto const build = [](uint8_t const status, std::vector<uint8_t> const& information)
        {
            return ci::MidiCapabilityInquiryMessageBuilder::BuildEndpointReply(
                0, TestGroup(), ci::MidiUniqueId(2000), ci::MidiUniqueId(1000), status,
                ToByteVector(information));
        };

    // The specification allows printable ASCII only, up to 42 bytes, and requires the id itself.
    VERIFY_ARE_EQUAL(build(0x00, { 'G', 0x09, '1' }).Size(), (uint32_t)0);
    VERIFY_ARE_EQUAL(build(0x00, std::vector<uint8_t>(43, 'A')).Size(), (uint32_t)0);
    VERIFY_ARE_EQUAL(build(0x00, {}).Size(), (uint32_t)0);

    VERIFY_IS_GREATER_THAN(build(0x00, std::vector<uint8_t>(42, 'A')).Size(), (uint32_t)0);

    // Every status still has to fit in seven bit system exclusive.
    VERIFY_ARE_EQUAL(build(0x05, { 0x80 }).Size(), (uint32_t)0);
}

void MidiCapabilityInquiryMessageTests::TestResourceListEntryFollowsTheResourcesOwnDefaults()
{
    auto const parse = [](wchar_t const* text)
        {
            return ci::MidiResourceList::FromJson(json::JsonArray::Parse(text));
        };

    // The minimal entries the specifications themselves show. M2-107-UM section 2.4 makes a
    // ProgramList paged and keyed by resource id without either being written, and LocalOn can be
    // written unless the device says it cannot.
    auto const minimal = parse(
        L"[{\"resource\":\"ProgramList\"},{\"resource\":\"ChannelList\"},{\"resource\":\"LocalOn\"}]");

    VERIFY_ARE_EQUAL(minimal.Entries().Size(), (uint32_t)3);

    auto const programList = minimal.GetEntry(L"ProgramList");

    VERIFY_IS_NOT_NULL(programList);
    VERIFY_IS_TRUE(programList.CanPaginate());
    VERIFY_IS_TRUE(programList.RequireResourceId());
    VERIFY_ARE_EQUAL(programList.CanSet(), ci::MidiResourceListEntry::CanSetNone());

    auto const channelList = minimal.GetEntry(L"ChannelList");

    VERIFY_IS_FALSE(channelList.CanPaginate());
    VERIFY_IS_FALSE(channelList.RequireResourceId());

    VERIFY_ARE_EQUAL(minimal.GetEntry(L"LocalOn").CanSet(), ci::MidiResourceListEntry::CanSetFull());

    // What a device says outright still wins, even where it contradicts the specification.
    auto const stated = parse(L"[{\"resource\":\"ProgramList\",\"canPaginate\":false,\"requireResId\":false}]");

    VERIFY_IS_FALSE(stated.GetEntry(L"ProgramList").CanPaginate());
    VERIFY_IS_FALSE(stated.GetEntry(L"ProgramList").RequireResourceId());

    // Writing leaves out anything the reader will assume, and keeps what it would not.
    ci::MidiResourceListEntry written{ L"ProgramList" };

    VERIFY_IS_TRUE(written.CanPaginate());
    VERIFY_IS_TRUE(written.RequireResourceId());
    VERIFY_ARE_EQUAL(written.GetJson().Stringify(), winrt::hstring{ L"{\"resource\":\"ProgramList\"}" });

    written.CanPaginate(false);

    VERIFY_IS_TRUE(written.GetJson().HasKey(L"canPaginate"));
    VERIFY_IS_FALSE(written.GetJson().GetNamedBoolean(L"canPaginate"));
    VERIFY_IS_FALSE(written.GetJson().HasKey(L"requireResId"));

    // A manufacturer's own resource has only the general defaults, so the same values are written.
    ci::MidiResourceListEntry custom{ L"X-Patches" };

    custom.CanPaginate(true);
    custom.RequireResourceId(true);

    VERIFY_IS_TRUE(custom.GetJson().GetNamedBoolean(L"canPaginate"));
    VERIFY_IS_TRUE(custom.GetJson().GetNamedBoolean(L"requireResId"));
}
