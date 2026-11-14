// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================

#include "pch.h"
#include "BlePeripheralStreamTests.h"

using namespace WEX::Common;
using namespace WEX::Logging;
using namespace WEX::TestExecution;

namespace
{
    namespace sr = MidiBleStreamResponder;

    constexpr uint32_t NoteOn = 0x20903C7F;

    // UMP 1.1 Endpoint Discovery with the given filter
    std::vector<uint32_t> EndpointDiscovery(_In_ uint8_t const filter)
    {
        return { 0xF0000101, filter, 0, 0 };
    }

    std::vector<uint32_t> StreamConfigurationRequest(_In_ uint8_t const protocol, _In_ bool const receiveJr, _In_ bool const sendJr)
    {
        return { 0xF0050000 | (static_cast<uint32_t>(protocol) << 8) | (receiveJr ? 0x2u : 0u) | (sendJr ? 0x1u : 0u), 0, 0, 0 };
    }

    sr::Responder MakeResponder(_In_ std::string const& name, _In_ bool const canSendJr)
    {
        sr::Identity identity{};
        identity.EndpointName = name;
        identity.ManufacturerSysExId[2] = 0x41;
        identity.DeviceFamily = 11;
        identity.DeviceFamilyModelNumber = 5;
        identity.SoftwareRevision[0] = 1;

        sr::Responder responder{};
        responder.SetIdentity(identity);
        responder.SetJrCapabilities(true, canSendJr);

        return responder;
    }

    // Status of each reply message, in order
    std::vector<uint16_t> ReplyStatuses(_In_ std::vector<uint32_t> const& replies)
    {
        std::vector<uint16_t> statuses;

        for (size_t i = 0; i + 3 < replies.size(); i += 4)
        {
            statuses.push_back(sr::GetStatus(replies[i]));
        }

        return statuses;
    }

    std::string DecodeName(_In_ std::vector<uint32_t> const& replies)
    {
        std::string name;

        for (size_t i = 0; i + 3 < replies.size(); i += 4)
        {
            if (sr::GetStatus(replies[i]) != sr::StatusEndpointNameNotification)
            {
                continue;
            }

            uint8_t bytes[14]{
                static_cast<uint8_t>(replies[i] >> 8), static_cast<uint8_t>(replies[i]) };

            for (size_t w = 0; w < 3; w++)
            {
                for (size_t b = 0; b < 4; b++)
                {
                    bytes[2 + w * 4 + b] = static_cast<uint8_t>(replies[i + 1 + w] >> (24 - 8 * b));
                }
            }

            for (auto const byte : bytes)
            {
                if (byte != 0)
                {
                    name.push_back(static_cast<char>(byte));
                }
            }
        }

        return name;
    }
}


void BlePeripheralStreamTests::TestEndpointDiscoveryIsAnsweredAndNotForwarded()
{
    auto responder = MakeResponder("PETE-PC", true);

    std::vector<uint32_t> payload{ NoteOn };
    auto const discovery = EndpointDiscovery(0x1F);
    payload.insert(payload.end(), discovery.begin(), discovery.end());
    payload.push_back(NoteOn);

    std::vector<uint32_t> forward;
    std::vector<uint32_t> replies;

    VERIFY_IS_FALSE(responder.ProcessPayload(payload.data(), payload.size(), forward, replies));

    // the music goes on, the question doesn't
    VERIFY_ARE_EQUAL(static_cast<size_t>(2), forward.size());
    VERIFY_ARE_EQUAL(NoteOn, forward[0]);
    VERIFY_ARE_EQUAL(NoteOn, forward[1]);

    auto const statuses = ReplyStatuses(replies);

    // Endpoint Info, Device Identity, one Endpoint Name message, Stream Configuration
    VERIFY_ARE_EQUAL(static_cast<size_t>(4), statuses.size());
    VERIFY_ARE_EQUAL(sr::StatusEndpointInfoNotification, statuses[0]);
    VERIFY_ARE_EQUAL(sr::StatusDeviceIdentityNotification, statuses[1]);
    VERIFY_ARE_EQUAL(sr::StatusEndpointNameNotification, statuses[2]);
    VERIFY_ARE_EQUAL(sr::StatusStreamConfigurationNotification, statuses[3]);

    // UMP 1.1, no Function Blocks, both protocols, JR both ways
    VERIFY_ARE_EQUAL(0xF0010101u, replies[0]);
    VERIFY_ARE_EQUAL(0x00000303u, replies[1]);

    // Microsoft, Windows 11 family, model 5, revision 1.0.0.0
    VERIFY_ARE_EQUAL(0x00000041u, replies[5]);
    VERIFY_ARE_EQUAL(0x0B000500u, replies[6]);
    VERIFY_ARE_EQUAL(0x01000000u, replies[7]);

    VERIFY_ARE_EQUAL(std::string{ "PETE-PC" }, DecodeName(replies));

    // MIDI 2.0, no JR until it's asked for
    VERIFY_ARE_EQUAL(0xF0060200u, replies[12]);
}

void BlePeripheralStreamTests::TestEndpointDiscoveryAnswersOnlyWhatWasAskedFor()
{
    auto responder = MakeResponder("PETE-PC", false);

    auto const discovery = EndpointDiscovery(sr::FilterEndpointInfo);

    std::vector<uint32_t> forward;
    std::vector<uint32_t> replies;

    responder.ProcessPayload(discovery.data(), discovery.size(), forward, replies);

    VERIFY_IS_TRUE(forward.empty());
    VERIFY_ARE_EQUAL(static_cast<size_t>(4), replies.size());

    // can't send JR without the clock schedule, so it isn't offered
    VERIFY_ARE_EQUAL(0x00000302u, replies[1]);
}

void BlePeripheralStreamTests::TestEndpointNameIsSplitAcrossMessages()
{
    std::string const name{ "A Rather Long Bluetooth Name For This PC" };

    auto responder = MakeResponder(name, true);

    auto const discovery = EndpointDiscovery(sr::FilterEndpointName);

    std::vector<uint32_t> forward;
    std::vector<uint32_t> replies;

    responder.ProcessPayload(discovery.data(), discovery.size(), forward, replies);

    // 40 bytes is three messages: start, continue, end
    VERIFY_ARE_EQUAL(static_cast<size_t>(12), replies.size());
    VERIFY_ARE_EQUAL(1, static_cast<int>(sr::GetForm(replies[0])));
    VERIFY_ARE_EQUAL(2, static_cast<int>(sr::GetForm(replies[4])));
    VERIFY_ARE_EQUAL(3, static_cast<int>(sr::GetForm(replies[8])));

    VERIFY_ARE_EQUAL(name, DecodeName(replies));
}

void BlePeripheralStreamTests::TestEndpointNameIsNotCutInsideACharacter()
{
    // 97 ASCII bytes, then a two-byte character which would straddle the 98-byte limit
    std::string name(97, 'x');
    name += "\xC3\xA9";

    VERIFY_ARE_EQUAL(static_cast<size_t>(97), sr::Utf8PrefixLength(name, sr::EndpointNameMaxBytes));

    std::string const shortName{ "Caf\xC3\xA9" };
    VERIFY_ARE_EQUAL(shortName.size(), sr::Utf8PrefixLength(shortName, sr::EndpointNameMaxBytes));
}

void BlePeripheralStreamTests::TestStreamConfigurationRequestIsAgreed()
{
    auto responder = MakeResponder("PETE-PC", true);

    auto const request = StreamConfigurationRequest(sr::ProtocolMidi2, true, true);

    std::vector<uint32_t> forward;
    std::vector<uint32_t> replies;

    VERIFY_IS_TRUE(responder.ProcessPayload(request.data(), request.size(), forward, replies));

    VERIFY_IS_TRUE(forward.empty());
    VERIFY_ARE_EQUAL(static_cast<size_t>(4), replies.size());
    VERIFY_ARE_EQUAL(0xF0060203u, replies[0]);

    VERIFY_IS_TRUE(responder.ReceivesJr());
    VERIFY_IS_TRUE(responder.SendsJr());

    // the same request again changes nothing, but is still answered
    VERIFY_IS_FALSE(responder.ProcessPayload(request.data(), request.size(), forward, replies));
    VERIFY_ARE_EQUAL(static_cast<size_t>(4), replies.size());
}

void BlePeripheralStreamTests::TestStreamConfigurationRefusesWhatCannotBeDone()
{
    auto responder = MakeResponder("PETE-PC", false);

    // a reserved protocol, and JR this PC can't send
    auto const request = StreamConfigurationRequest(0x07, true, true);

    std::vector<uint32_t> forward;
    std::vector<uint32_t> replies;

    responder.ProcessPayload(request.data(), request.size(), forward, replies);

    // the reply is final, and carries what was actually agreed
    VERIFY_ARE_EQUAL(0xF0060202u, replies[0]);
    VERIFY_ARE_EQUAL(sr::ProtocolMidi2, responder.Protocol());
    VERIFY_IS_TRUE(responder.ReceivesJr());
    VERIFY_IS_FALSE(responder.SendsJr());

    auto const midi1 = StreamConfigurationRequest(sr::ProtocolMidi1, false, false);
    responder.ProcessPayload(midi1.data(), midi1.size(), forward, replies);

    VERIFY_ARE_EQUAL(0xF0060100u, replies[0]);
}

void BlePeripheralStreamTests::TestFunctionBlockDiscoveryIsConsumedWithoutAnswer()
{
    auto responder = MakeResponder("PETE-PC", true);

    std::vector<uint32_t> const discovery{ 0xF010FF03, 0, 0, 0 };

    std::vector<uint32_t> forward;
    std::vector<uint32_t> replies;

    responder.ProcessPayload(discovery.data(), discovery.size(), forward, replies);

    VERIFY_IS_TRUE(forward.empty());
    VERIFY_IS_TRUE(replies.empty());
}

void BlePeripheralStreamTests::TestResetForgetsTheConfiguration()
{
    auto responder = MakeResponder("PETE-PC", true);

    auto const request = StreamConfigurationRequest(sr::ProtocolMidi1, true, true);

    std::vector<uint32_t> forward;
    std::vector<uint32_t> replies;

    responder.ProcessPayload(request.data(), request.size(), forward, replies);

    responder.Reset();

    VERIFY_ARE_EQUAL(sr::ProtocolMidi2, responder.Protocol());
    VERIFY_IS_FALSE(responder.ReceivesJr());
    VERIFY_IS_FALSE(responder.SendsJr());
}
