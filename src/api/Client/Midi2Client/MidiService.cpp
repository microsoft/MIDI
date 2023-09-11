// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================

#include "pch.h"
#include "MidiService.h"
#include "MidiService.g.cpp"

#include "ping_ump_types.h"

#include <Windows.h>
#include <wil\resource.h>

namespace winrt::Windows::Devices::Midi2::implementation
{
    _Use_decl_annotations_
    midi2::MidiServicePingResponseSummary MidiService::PingService(uint8_t const pingCount) noexcept
    {
        auto responseSummary = winrt::make_self<implementation::MidiServicePingResponseSummary>();

        if (pingCount == 0)
        {
            responseSummary->InternalSetFailed(L"Ping count is zero.");
            return *responseSummary;
        }

        // we use the API from here to get accurate timing

        auto session = midi2::MidiSession::CreateSession(L"Ping Test");

        if (session == nullptr)
        {
            responseSummary->InternalSetFailed(L"Unable to create session.");
            return *responseSummary;
        }

        // we don't want any automatic plugins or messages
        MidiBidirectionalEndpointOpenOptions options;
        options.DisableAutomaticEndpointDiscoveryMessages(true);
        options.DisableAutomaticFunctionBlockInfoMessages(true);

        // This ID must be consistent with what the service is set up to use.
        const winrt::hstring pingEndpointId = L"";

        auto endpoint = session.ConnectBidirectionalEndpoint(pingEndpointId, options);

        if (endpoint == nullptr)
        {
            responseSummary->InternalSetFailed(L"Unable to create ping endpoint.");
            return *responseSummary;
        }

        // we have the session and the endpoint, so set up the ping
        // changing any of this internal implementation detail requires a coordinated change with the server code
        // this is not an official UMP ping message. This is just an internal representation that is
        // recognized only by this endpoint, and should never be used elsewhere.

        // set up message received handler

        wil::unique_event_nothrow allMessagesReceived;
        allMessagesReceived.create();

        uint8_t receivedCount{ 0 };

        auto MessageReceivedHandler = [&](foundation::IInspectable const& /*sender*/, midi2::MidiMessageReceivedEventArgs const& args)
            {
                internal::MidiTimestamp actualReceiveEventTimestamp = MidiClock::GetMidiTimestamp();

                uint32_t word0;
                uint32_t word1;
                uint32_t word2;
                uint32_t word3;

                args.FillWords(word0, word1, word2, word3);

                // ensure this is a ping message, just in case

                if (word0 == INTERNAL_PING_RESPONSE_UMP_WORD0)
                { 
                    // get the pingId from the words
                    uint64_t pingId = (uint64_t)word1 << 4 | word2;

                    // find the sending message in the list. This would be slightly faster with a map, 
                    // but not bothering with that here as most people are only expected to ping < 10 times
                    auto iterator = std::find_if(
                        responseSummary->Responses().begin(),
                        responseSummary->Responses().end(),
                        [&](const midi2::MidiServicePingResponse res) { return res.Id() == pingId; });

                    // one of ours, so handle it
                    if (iterator != responseSummary->Responses().end())
                    {
                        // get the internal representation so we can call internal functions
                        auto res = winrt::get_self<implementation::MidiServicePingResponse>(*iterator);

                        if (res != nullptr)
                        {
                            res->InternalSetReceiveInfo(args.Timestamp(), actualReceiveEventTimestamp);
                        }

                        receivedCount++;

                        if (receivedCount == pingCount)
                        {
                            allMessagesReceived.SetEvent();
                        }
                    }
                }
                else
                {
                    // someone else is sending stuff to this ping service. Naughty.
                }

            };

        // any failures after this need to revoke the event handler as well
        auto eventRevokeToken = endpoint.MessageReceived(MessageReceivedHandler);

        endpoint.Open();

        // set up our ping Id

        std::srand((uint32_t)(MidiClock::GetMidiTimestamp() & 0x00000000FFFFFFFF));

        uint32_t word1RandomNumber = (uint32_t)std::rand();
        uint32_t word2RandomNumber = (uint32_t)std::rand() << 1;    // unnecessary, but why not. We shift over 1 to give room for the index

        // send out the ping messages
        // there's no yield here, so this is going to just flood the outgoing message queue

        for (int i = 0; i < pingCount; i++)
        {
            internal::PackedPingRequestUmp request;

            word2RandomNumber += i;

            auto response = winrt::make_self<implementation::MidiServicePingResponse>();

            internal::MidiTimestamp timestamp = MidiClock::GetMidiTimestamp();
            uint64_t pingId = (uint64_t)word1RandomNumber << 4 | (uint64_t)word2RandomNumber;

            // Add this info to the tracking before we send, so no race condition
            // granted that this adds a few ticks to add this to the collection and build the object

            response->InternalSetSendInfo(pingId, timestamp);
            
            // send the ping
            endpoint.SendUmpWords(timestamp, request.Word0, word1RandomNumber, word2RandomNumber, request.Padding);
        }

        // Wait for all responses to come in (receivedCount == pingCount). If not all responses come back, report the failure.
        if (!allMessagesReceived.wait(1000 + 20 * pingCount))
        {
            responseSummary->InternalSetFailed(L"Not all ping responses received within appropriate time window.");
        }

        endpoint.MessageReceived(eventRevokeToken);
        session.Close();

        return *responseSummary;
    }

    _Use_decl_annotations_
    foundation::Collections::IVectorView<winrt::Windows::Devices::Midi2::MidiTransportInformation> MidiService::GetInstalledTransports()
    {
        throw hresult_not_implemented();
    }
}
