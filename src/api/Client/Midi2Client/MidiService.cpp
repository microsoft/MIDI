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
    midi2::MidiServicePingResponseSummary MidiService::PingService(uint8_t const pingCount, uint32_t timeoutMilliseconds) noexcept
    {
        auto responseSummary = winrt::make_self<implementation::MidiServicePingResponseSummary>();

        if (responseSummary == nullptr)
        {
            // just need to fail
            return nullptr;
        }

        if (pingCount == 0)
        {
            responseSummary->InternalSetFailed(L"Ping count is zero.");
            return *responseSummary;
        }

        std::vector<winrt::com_ptr<implementation::MidiServicePingResponse>> pings{};
        pings.resize(pingCount);

        if (timeoutMilliseconds == 0)
        {
            responseSummary->InternalSetFailed(L"Timeout milliseconds is zero.");
            return *responseSummary;
        }

        // we use the full session API from here to get accurate timing

        auto session = midi2::MidiSession::CreateSession(L"Ping Test");

        if (session == nullptr)
        {
            responseSummary->InternalSetFailed(L"Unable to create session.");
            return *responseSummary;
        }

        MidiEndpointConnectionOptions options;
        
        // This ID must be consistent with what the service is set up to use.

        auto endpoint = session.CreateEndpointConnection(MIDI_DIAGNOSTICS_PING_BIDI_ID, options);

        if (endpoint == nullptr)
        {
            responseSummary->InternalSetFailed(L"Unable to create ping endpoint.");
            return *responseSummary;
        }


        // originally I was going to use a random number for this, but using the low bits of
        // the timestamp makes more sense, and will be unique enough for this.

        uint32_t pingSourceId = (uint32_t)(MidiClock::Now() & 0x00000000FFFFFFFF);

        wil::unique_event_nothrow allMessagesReceived;
        allMessagesReceived.create();

        uint8_t receivedCount{ 0 };

        // we have the session and the endpoint, so set up the ping handler
        // changing any of this internal implementation detail requires a coordinated change with the server code
        // this is not an official UMP ping message. This is just an internal representation that is
        // recognized only by this endpoint, and should never be used elsewhere.


        auto MessageReceivedHandler = [&](foundation::IInspectable const& /*sender*/, midi2::MidiMessageReceivedEventArgs const& args)
            {
                internal::MidiTimestamp actualReceiveEventTimestamp = MidiClock::Now();

                uint32_t word0;
                uint32_t word1;
                uint32_t word2;
                uint32_t word3;

                args.FillWords(word0, word1, word2, word3);

                // ensure this is a ping message, just in case

                if (word0 == INTERNAL_PING_RESPONSE_UMP_WORD0 && word1 == pingSourceId)
                { 
                    if (word2 < pings.size())
                    {
                        // word2 is our ping index
                        pings[word2]->InternalSetReceiveInfo(args.Timestamp(), actualReceiveEventTimestamp);

                        receivedCount++;

                        if (receivedCount == pingCount)
                        {
                            allMessagesReceived.SetEvent();
                        }
                    }
                    else
                    {
                        // something really wrong happened. Our index has been messed up.
                    }
                }
                else
                {
                    // someone else is sending stuff to this ping service. Naughty if not another ping.
                }

            };

        // any failures after this need to revoke the event handler as well
        auto eventRevokeToken = endpoint.MessageReceived(MessageReceivedHandler);

        // open the endpoint. We've already set options for it not to send out discovery messages
        if (!endpoint.Open())
        {
            responseSummary->InternalSetFailed(L"Endpoint open failed. The service may be unavailable.");
            endpoint.MessageReceived(eventRevokeToken);

            session.DisconnectEndpointConnection(endpoint.ConnectionId());

            return *responseSummary;
        }

        // send out the ping messages

        for (uint32_t pingIndex = 0; pingIndex < pingCount; pingIndex++)
        {
            internal::PackedPingRequestUmp request;

            auto response = winrt::make_self<implementation::MidiServicePingResponse>();

            internal::MidiTimestamp timestamp = MidiClock::Now();

            // Add this info to the tracking before we send, so no race condition
            // granted that this adds a few ticks to add this to the collection and build the object

            response->InternalSetSendInfo(pingSourceId, pingIndex, timestamp);
            
            //
            // TODO: Should this use copy_from?
            pings[pingIndex] = response;

            // send the ping
            endpoint.SendMessageWords(timestamp, request.Word0, pingSourceId, pingIndex, request.Padding);

            Sleep(0);
        }

        // Wait for all responses to come in (receivedCount == pingCount). If not all responses come back, report the failure.
        if (!allMessagesReceived.wait(timeoutMilliseconds))
        {
            responseSummary->InternalSetFailed(L"Not all ping responses received within appropriate time window.");
        }
        else
        {
            // all received
            responseSummary->InternalSetSucceeded();

            // copy over the holding array into the response and also calculate the totals

            uint64_t totalPing{ 0 };

            for (const auto& response : pings)
            {
                totalPing += response->ClientDeltaTimestamp();

                responseSummary->InternalAddResponse(*response);

                // does I need to remove the com_ptr ref or will going out of scope be sufficient?
            }

            uint64_t averagePing = totalPing / responseSummary->Responses().Size();

            responseSummary->InternalSetTotals(totalPing, averagePing);
        }

        // unwire the event and close the session.
        endpoint.MessageReceived(eventRevokeToken);

        // not strictly necessary
        session.DisconnectEndpointConnection(endpoint.ConnectionId());

        session.Close();

        return *responseSummary;
    }

    _Use_decl_annotations_
    midi2::MidiServicePingResponseSummary MidiService::PingService(uint8_t const pingCount) noexcept
    {
        return PingService(pingCount, pingCount * 20 + 1000);
    }

    foundation::Collections::IVectorView<midi2::MidiTransportInformation> MidiService::GetInstalledTransports()
    {
        // TODO: Need to implement GetInstalledTransports. For now, return an empty collection instead of throwing

        return winrt::single_threaded_vector<midi2::MidiTransportInformation>().GetView();
    }
}
