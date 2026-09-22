// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================


#include "stdafx.h"

using namespace winrt::Windows::Devices::Midi2;
using namespace winrt::Windows::Devices::Midi2::CapabilityInquiry;
using namespace winrt::Windows::Devices::Midi2::Diagnostics;

namespace json = winrt::Windows::Data::Json;

namespace
{
    // The two ends of a diagnostic loopback. What goes into one comes out of the other, which is
    // what lets an initiator and a device be tested together without any hardware.
    struct LoopbackPair
    {
        MidiSession Session{ nullptr };
        MidiEndpointConnection Initiator{ nullptr };
        MidiEndpointConnection Device{ nullptr };

        ~LoopbackPair()
        {
            if (Session != nullptr)
            {
                Session.Close();
            }
        }
    };

    std::unique_ptr<LoopbackPair> CreateLoopbackPair(std::wstring const& name)
    {
        VERIFY_IS_TRUE(MidiApi::EnsureServiceAvailable());

        auto pair = std::make_unique<LoopbackPair>();

        pair->Session = MidiSession::Create(winrt::hstring{ name });
        VERIFY_IS_NOT_NULL(pair->Session);

        pair->Initiator = pair->Session.CreateEndpointConnection(
            MidiDiagnostics::DiagnosticsLoopbackAEndpointDeviceId());
        VERIFY_IS_NOT_NULL(pair->Initiator);

        pair->Device = pair->Session.CreateEndpointConnection(
            MidiDiagnostics::DiagnosticsLoopbackBEndpointDeviceId());
        VERIFY_IS_NOT_NULL(pair->Device);

        return pair;
    }

    // Everything the tests do needs a discovered responder first, because a responder is how the
    // session learns the identifier to address and the size to chunk against.
    MidiCapabilityInquiryResponder DiscoverOne(
        MidiCapabilityInquirySession const& session,
        MidiCapabilityInquiryTestResponder& responder)
    {
        auto const found = session.DiscoverAsync().get();

        VERIFY_ARE_EQUAL(found.Size(), (uint32_t)1);
        VERIFY_ARE_EQUAL(
            found.GetAt(0).Muid().AsCombined28BitValue(),
            responder.Muid().AsCombined28BitValue());

        return found.GetAt(0);
    }

    std::string MakeLongJsonArray(size_t entryCount)
    {
        std::string body{ "[" };

        for (size_t i = 0; i < entryCount; i++)
        {
            if (i > 0)
            {
                body += ",";
            }

            body += "{\"title\":\"Entry ";
            body += std::to_string(i);
            body += "\",\"bankPC\":[0,0,";
            body += std::to_string(i % 128);
            body += "]}";
        }

        body += "]";

        return body;
    }
}


void MidiCapabilityInquirySessionTests::TestDiscoveryFindsAResponder()
{
    auto const pair = CreateLoopbackPair(L"TAEF CI Discovery");

    MidiCapabilityInquiryTestResponder responder{};
    responder.Start(pair->Device, 0x0123456);

    pair->Initiator.Open();
    pair->Device.Open();

    auto session = MidiCapabilityInquirySession::Create(pair->Initiator);
    VERIFY_IS_NOT_NULL(session);

    // The identifier is drawn fresh for the session, and must never be the broadcast value or one
    // of the reserved ones.
    VERIFY_IS_NOT_NULL(session.SourceMuid());
    VERIFY_IS_FALSE(session.SourceMuid().IsBroadcast());
    VERIFY_IS_FALSE(session.SourceMuid().IsReserved());

    auto const found = DiscoverOne(session, responder);

    VERIFY_IS_TRUE(found.SupportsPropertyExchange());
    VERIFY_IS_TRUE(found.SupportsProfiles());
    VERIFY_IS_FALSE(found.SupportsProcessInquiry());

    VERIFY_ARE_EQUAL(found.ReceivableMaximumSystemExclusiveSize(), (uint32_t)512);

    VERIFY_IS_NOT_NULL(found.Identity());
    VERIFY_ARE_EQUAL(found.Identity().SystemExclusiveId().at(2), (uint8_t)0x41);

    // Asking again must not produce a second copy of the same device.
    session.DiscoverAsync().get();
    VERIFY_ARE_EQUAL(session.GetResponders().Size(), (uint32_t)1);

    VERIFY_IS_NOT_NULL(session.GetResponder(found.Muid()));

    session.Close();
    responder.Stop();
}

void MidiCapabilityInquirySessionTests::TestDiscoveryFindsNothingWhenNobodyAnswers()
{
    auto const pair = CreateLoopbackPair(L"TAEF CI Discovery Silent");

    MidiCapabilityInquiryTestResponder responder{};
    responder.AnswerDiscovery(false);
    responder.Start(pair->Device, 0x0123456);

    pair->Initiator.Open();
    pair->Device.Open();

    auto session = MidiCapabilityInquirySession::Create(pair->Initiator);
    session.ResponseTimeoutMilliseconds(500);

    auto const found = session.DiscoverAsync().get();

    // A cable with nothing on it is an ordinary outcome, not a failure.
    VERIFY_ARE_EQUAL(found.Size(), (uint32_t)0);

    session.Close();
    responder.Stop();
}

void MidiCapabilityInquirySessionTests::TestGetPropertyDataReturnsTheResource()
{
    auto const pair = CreateLoopbackPair(L"TAEF CI Get Property");

    MidiCapabilityInquiryTestResponder responder{};
    responder.SetResource("DeviceInfo", "{\"manufacturer\":\"Contoso\",\"model\":\"Test Keyboard\"}");
    responder.Start(pair->Device, 0x0123456);

    pair->Initiator.Open();
    pair->Device.Open();

    auto session = MidiCapabilityInquirySession::Create(pair->Initiator);
    auto const found = DiscoverOne(session, responder);

    json::JsonObject header{};
    header.SetNamedValue(L"resource", json::JsonValue::CreateStringValue(L"DeviceInfo"));

    auto const response = session.GetPropertyDataAsync(found.Muid(), header).get();

    VERIFY_ARE_EQUAL((int)response.Status(), (int)MidiCapabilityInquiryStatus::Success);
    VERIFY_ARE_EQUAL(response.ResourceStatus(), 200);
    VERIFY_ARE_EQUAL(response.ChunkCount(), (uint16_t)1);

    VERIFY_IS_NOT_NULL(response.BodyAsJson());
    VERIFY_ARE_EQUAL(
        response.BodyAsJson().GetObject().GetNamedString(L"manufacturer"),
        winrt::hstring{ L"Contoso" });

    session.Close();
    responder.Stop();
}

void MidiCapabilityInquirySessionTests::TestPropertyExchangeCapabilitiesComeFirst()
{
    auto const pair = CreateLoopbackPair(L"TAEF CI Capabilities First");

    MidiCapabilityInquiryTestResponder responder{};
    responder.SetResource("DeviceInfo", "{\"manufacturer\":\"Contoso\"}");
    responder.SetResource("ChannelList", "[]");
    responder.Start(pair->Device, 0x0123456);

    pair->Initiator.Open();
    pair->Device.Open();

    auto session = MidiCapabilityInquirySession::Create(pair->Initiator);
    auto const found = DiscoverOne(session, responder);

    // Nothing has asked for a property yet, so the responder has said nothing about its limits.
    VERIFY_ARE_EQUAL(found.MaximumSimultaneousPropertyRequests(), (uint8_t)0);

    VERIFY_IS_NOT_NULL(session.GetDeviceInfoAsync(found.Muid()).get());

    // The test responder replies with four.
    VERIFY_ARE_EQUAL(
        session.GetResponder(found.Muid()).MaximumSimultaneousPropertyRequests(), (uint8_t)4);

    auto const log = responder.MessageLog();

    size_t capabilitiesInquiries{ 0 };
    size_t firstCapabilitiesInquiry{ log.size() };
    size_t firstPropertyGet{ log.size() };

    for (size_t i = 0; i < log.size(); i++)
    {
        if (log[i] == MidiCapabilityInquiryMessageType::PropertyExchangeCapabilitiesInquiry)
        {
            capabilitiesInquiries++;

            if (firstCapabilitiesInquiry == log.size())
            {
                firstCapabilitiesInquiry = i;
            }
        }
        else if (log[i] == MidiCapabilityInquiryMessageType::PropertyGetDataInquiry &&
                 firstPropertyGet == log.size())
        {
            firstPropertyGet = i;
        }
    }

    VERIFY_ARE_EQUAL(capabilitiesInquiries, (size_t)1);
    VERIFY_IS_LESS_THAN(firstCapabilitiesInquiry, firstPropertyGet);

    // Asking for a second resource must not repeat the capabilities transaction.
    VERIFY_IS_NOT_NULL(session.GetChannelListAsync(found.Muid()).get());

    auto const secondLog = responder.MessageLog();

    capabilitiesInquiries = 0;

    for (auto const messageType : secondLog)
    {
        if (messageType == MidiCapabilityInquiryMessageType::PropertyExchangeCapabilitiesInquiry)
        {
            capabilitiesInquiries++;
        }
    }

    VERIFY_ARE_EQUAL(capabilitiesInquiries, (size_t)1);

    session.Close();
    responder.Stop();
}

void MidiCapabilityInquirySessionTests::TestLargeResourceIsReassembledFromChunks()
{
    auto const pair = CreateLoopbackPair(L"TAEF CI Chunked");

    // Long enough that a device with the smallest legal buffer must break it up.
    auto const body = MakeLongJsonArray(200);

    MidiCapabilityInquiryTestResponder responder{};
    responder.SetResource("ProgramList", body);
    responder.Start(pair->Device, 0x0123456);

    pair->Initiator.Open();
    pair->Device.Open();

    auto session = MidiCapabilityInquirySession::Create(pair->Initiator);
    session.ResponseTimeoutMilliseconds(4000);

    auto const found = DiscoverOne(session, responder);

    json::JsonObject header{};
    header.SetNamedValue(L"resource", json::JsonValue::CreateStringValue(L"ProgramList"));

    auto const response = session.GetPropertyDataAsync(found.Muid(), header).get();

    VERIFY_ARE_EQUAL((int)response.Status(), (int)MidiCapabilityInquiryStatus::Success);

    // This is the whole point of the test: it has to have taken several chunks, or it proves
    // nothing about reassembly.
    VERIFY_IS_GREATER_THAN(response.ChunkCount(), (uint16_t)1);

    VERIFY_ARE_EQUAL(response.Body().Size(), (uint32_t)body.size());

    VERIFY_IS_NOT_NULL(response.BodyAsJson());
    VERIFY_ARE_EQUAL(response.BodyAsJson().GetArray().Size(), (uint32_t)200);
    VERIFY_ARE_EQUAL(
        response.BodyAsJson().GetArray().GetObjectAt(199).GetNamedString(L"title"),
        winrt::hstring{ L"Entry 199" });

    session.Close();
    responder.Stop();
}

void MidiCapabilityInquirySessionTests::TestSilenceIsReportedAsNoResponse()
{
    auto const pair = CreateLoopbackPair(L"TAEF CI No Response");

    MidiCapabilityInquiryTestResponder responder{};
    responder.Start(pair->Device, 0x0123456);

    pair->Initiator.Open();
    pair->Device.Open();

    auto session = MidiCapabilityInquirySession::Create(pair->Initiator);
    auto const found = DiscoverOne(session, responder);

    // A device may simply not implement a resource, and is allowed to say nothing at all.
    responder.AnswerNothing(true);
    session.ResponseTimeoutMilliseconds(400);

    json::JsonObject header{};
    header.SetNamedValue(L"resource", json::JsonValue::CreateStringValue(L"SomethingElse"));

    auto const start = std::chrono::steady_clock::now();
    auto const response = session.GetPropertyDataAsync(found.Muid(), header).get();
    auto const elapsed = std::chrono::steady_clock::now() - start;

    VERIFY_ARE_EQUAL((int)response.Status(), (int)MidiCapabilityInquiryStatus::NoResponse);

    // It has to actually give up, not hang, and not return before the device had a chance.
    VERIFY_IS_GREATER_THAN_OR_EQUAL(
        std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count(), (int64_t)350);

    VERIFY_IS_LESS_THAN(
        std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count(), (int64_t)5000);

    session.Close();
    responder.Stop();
}

void MidiCapabilityInquirySessionTests::TestNegativeAcknowledgmentIsReported()
{
    auto const pair = CreateLoopbackPair(L"TAEF CI Nak");

    MidiCapabilityInquiryTestResponder responder{};
    responder.Start(pair->Device, 0x0123456);

    pair->Initiator.Open();
    pair->Device.Open();

    auto session = MidiCapabilityInquirySession::Create(pair->Initiator);
    auto const found = DiscoverOne(session, responder);

    responder.AnswerWithNak(true);

    json::JsonObject header{};
    header.SetNamedValue(L"resource", json::JsonValue::CreateStringValue(L"DeviceInfo"));

    auto const response = session.GetPropertyDataAsync(found.Muid(), header).get();

    VERIFY_ARE_EQUAL((int)response.Status(), (int)MidiCapabilityInquiryStatus::NegativeAcknowledgment);
    VERIFY_ARE_EQUAL(response.NakStatusCode(), (uint8_t)0x01);

    // The specification asks that the device's own words be shown to the person using the app, so
    // they have to survive as far as the caller.
    VERIFY_ARE_EQUAL(response.NakStatusMessage(), winrt::hstring{ L"Not available" });

    session.Close();
    responder.Stop();
}

void MidiCapabilityInquirySessionTests::TestDecliningStatusIsReported()
{
    auto const pair = CreateLoopbackPair(L"TAEF CI Declined");

    MidiCapabilityInquiryTestResponder responder{};
    responder.SetResource("DeviceInfo", "{}");
    responder.SetResourceStatus(404);
    responder.Start(pair->Device, 0x0123456);

    pair->Initiator.Open();
    pair->Device.Open();

    auto session = MidiCapabilityInquirySession::Create(pair->Initiator);
    auto const found = DiscoverOne(session, responder);

    json::JsonObject header{};
    header.SetNamedValue(L"resource", json::JsonValue::CreateStringValue(L"DeviceInfo"));

    auto const response = session.GetPropertyDataAsync(found.Muid(), header).get();

    // A reply which arrived and said no is not the same as no reply, and the header status is how
    // a device says no without sending a negative acknowledgment.
    VERIFY_ARE_EQUAL((int)response.Status(), (int)MidiCapabilityInquiryStatus::NegativeAcknowledgment);
    VERIFY_ARE_EQUAL(response.ResourceStatus(), 404);

    session.Close();
    responder.Stop();
}

void MidiCapabilityInquirySessionTests::TestNamedResourcesAreParsed()
{
    auto const pair = CreateLoopbackPair(L"TAEF CI Named Resources");

    MidiCapabilityInquiryTestResponder responder{};

    responder.SetResource("ResourceList",
        "[{\"resource\":\"DeviceInfo\"},"
        "{\"resource\":\"ChannelList\"},"
        "{\"resource\":\"ProgramList\",\"canPaginate\":true}]");

    responder.SetResource("DeviceInfo",
        "{\"manufacturerId\":[0,0,65],\"manufacturer\":\"Contoso\",\"model\":\"Test Keyboard\"}");

    responder.SetResource("ChannelList",
        "[{\"title\":\"Part 1\",\"channel\":1,"
        "\"programTitle\":\"Piano\","
        "\"links\":[{\"resource\":\"ProgramList\",\"resId\":\"main\"}]}]");

    responder.Start(pair->Device, 0x0123456);

    pair->Initiator.Open();
    pair->Device.Open();

    auto session = MidiCapabilityInquirySession::Create(pair->Initiator);
    auto const found = DiscoverOne(session, responder);

    auto const resources = session.GetResourceListAsync(found.Muid()).get();

    VERIFY_IS_NOT_NULL(resources);
    VERIFY_ARE_EQUAL(resources.Entries().Size(), (uint32_t)3);
    VERIFY_IS_TRUE(resources.SupportsResource(L"ProgramList"));
    VERIFY_IS_FALSE(resources.SupportsResource(L"StateList"));

    auto const deviceInfo = session.GetDeviceInfoAsync(found.Muid()).get();

    VERIFY_IS_NOT_NULL(deviceInfo);
    VERIFY_ARE_EQUAL(deviceInfo.Manufacturer(), winrt::hstring{ L"Contoso" });
    VERIFY_ARE_EQUAL(deviceInfo.Model(), winrt::hstring{ L"Test Keyboard" });

    auto const channels = session.GetChannelListAsync(found.Muid()).get();

    VERIFY_IS_NOT_NULL(channels);
    VERIFY_ARE_EQUAL(channels.Entries().Size(), (uint32_t)1);

    // The link from a channel to its program list is how an application knows what to ask for next.
    VERIFY_ARE_EQUAL(channels.GetProgramListLinks().Size(), (uint32_t)1);
    VERIFY_ARE_EQUAL(
        channels.GetProgramListLinks().GetAt(0).ResourceId(),
        winrt::hstring{ L"main" });

    session.Close();
    responder.Stop();
}

void MidiCapabilityInquirySessionTests::TestProgramListPagesUntilItIsComplete()
{
    auto const pair = CreateLoopbackPair(L"TAEF CI Paging");

    // More entries than one page holds, so the session has to ask more than once.
    std::vector<std::string> entries{};

    for (size_t i = 0; i < 250; i++)
    {
        entries.push_back(
            "{\"title\":\"Program " + std::to_string(i) + "\",\"bankPC\":[0,0," + std::to_string(i % 128) + "]}");
    }

    MidiCapabilityInquiryTestResponder responder{};
    responder.SetPagedEntries(entries);
    responder.Start(pair->Device, 0x0123456);

    pair->Initiator.Open();
    pair->Device.Open();

    auto session = MidiCapabilityInquirySession::Create(pair->Initiator);
    session.ResponseTimeoutMilliseconds(4000);

    auto const found = DiscoverOne(session, responder);

    auto const programs = session.GetProgramListAsync(found.Muid(), L"main").get();

    VERIFY_IS_NOT_NULL(programs);

    // The whole list, not the first page of it.
    VERIFY_ARE_EQUAL(programs.Entries().Size(), (uint32_t)250);
    VERIFY_ARE_EQUAL(programs.TotalCount(), (uint32_t)250);
    VERIFY_IS_FALSE(programs.HasMoreEntries());

    VERIFY_ARE_EQUAL(programs.Entries().GetAt(0).Title(), winrt::hstring{ L"Program 0" });
    VERIFY_ARE_EQUAL(programs.Entries().GetAt(249).Title(), winrt::hstring{ L"Program 249" });

    // It also has to have actually paged, or this is testing nothing.
    VERIFY_IS_GREATER_THAN(responder.RequestCount(), (uint32_t)1);

    // One page on its own still works, for an application which would rather page itself.
    auto const page = session.GetProgramListPageAsync(found.Muid(), L"main", 100, 10).get();

    VERIFY_IS_NOT_NULL(page);
    VERIFY_ARE_EQUAL(page.Entries().Size(), (uint32_t)10);
    VERIFY_ARE_EQUAL(page.Offset(), (uint32_t)100);
    VERIFY_ARE_EQUAL(page.TotalCount(), (uint32_t)250);
    VERIFY_IS_TRUE(page.HasMoreEntries());
    VERIFY_ARE_EQUAL(page.NextOffset(), (uint32_t)110);
    VERIFY_ARE_EQUAL(page.Entries().GetAt(0).Title(), winrt::hstring{ L"Program 100" });

    session.Close();
    responder.Stop();
}

void MidiCapabilityInquirySessionTests::TestProfileInquiryReturnsBothLists()
{
    auto const pair = CreateLoopbackPair(L"TAEF CI Profiles");

    auto const enabledProfile = MidiProfileId::CreateStandardDefined(0x01, 0x01, 0x01, 0x01);
    auto const disabledProfile = MidiProfileId::CreateManufacturerSpecific(0x41, 0x00, 0x00, 0x12, 0x34);

    MidiCapabilityInquiryTestResponder responder{};
    responder.SetProfiles({ enabledProfile }, { disabledProfile });
    responder.Start(pair->Device, 0x0123456);

    pair->Initiator.Open();
    pair->Device.Open();

    auto session = MidiCapabilityInquirySession::Create(pair->Initiator);
    auto const found = DiscoverOne(session, responder);

    auto const response = session.GetProfilesAsync(found.Muid(), 0x7F).get();

    VERIFY_ARE_EQUAL((int)response.Status(), (int)MidiCapabilityInquiryStatus::Success);
    VERIFY_ARE_EQUAL(response.DeviceId(), (uint8_t)0x7F);

    VERIFY_ARE_EQUAL(response.EnabledProfiles().Size(), (uint32_t)1);
    VERIFY_IS_TRUE(response.EnabledProfiles().GetAt(0).IsSameProfileAs(enabledProfile));

    VERIFY_ARE_EQUAL(response.DisabledProfiles().Size(), (uint32_t)1);
    VERIFY_IS_TRUE(response.DisabledProfiles().GetAt(0).IsSameProfileAs(disabledProfile));

    // Asking a device to turn a profile on is a send, not a request: the device answers with a
    // broadcast report, if it answers at all.
    VERIFY_IS_TRUE(session.SendSetProfileOn(found.Muid(), 0x7F, disabledProfile, 0));
    VERIFY_IS_TRUE(session.SendSetProfileOff(found.Muid(), 0x7F, enabledProfile));

    session.Close();
    responder.Stop();
}

void MidiCapabilityInquirySessionTests::TestUnsolicitedProfileReportRaisesAnEvent()
{
    auto const pair = CreateLoopbackPair(L"TAEF CI Profile Report");

    MidiCapabilityInquiryTestResponder responder{};
    responder.Start(pair->Device, 0x0123456);

    pair->Initiator.Open();
    pair->Device.Open();

    auto session = MidiCapabilityInquirySession::Create(pair->Initiator);

    wil::unique_event_nothrow reportReceived;
    reportReceived.create();

    MidiProfileId reportedProfile{ nullptr };
    uint16_t reportedChannels{ 0 };

    session.ProfileStateChanged([&](auto&&, MidiCapabilityInquiryMessageReceivedEventArgs const& args)
        {
            if (args.Message().MessageType() == MidiCapabilityInquiryMessageType::ProfileEnabledReport)
            {
                reportedProfile = args.Message().ProfileId();
                reportedChannels = args.Message().ProfileChannelCount();

                reportReceived.SetEvent();
            }
        });

    auto const profileId = MidiProfileId::CreateStandardDefined(0x02, 0x03, 0x01, 0x01);

    responder.SendProfileEnabledReport(profileId, 4);

    VERIFY_IS_TRUE(reportReceived.wait(3000));

    // A report is broadcast and arrives without anything having been asked for, which is the whole
    // reason this is an event rather than a reply.
    VERIFY_IS_NOT_NULL(reportedProfile);
    VERIFY_IS_TRUE(reportedProfile.IsSameProfileAs(profileId));
    VERIFY_ARE_EQUAL(reportedChannels, (uint16_t)4);

    session.Close();
    responder.Stop();
}
