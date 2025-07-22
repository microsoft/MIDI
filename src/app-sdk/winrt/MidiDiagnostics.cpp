// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App SDK and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================


#include "pch.h"
#include "MidiDiagnostics.h"
#include "Diagnostics.MidiDiagnostics.g.cpp"

namespace winrt::Microsoft::Windows::Devices::Midi2::Diagnostics::implementation
{
    _Use_decl_annotations_
    diag::MidiServicePingResponseSummary MidiDiagnostics::PingService(
        uint8_t pingCount) noexcept
    {
        return PingService(pingCount, pingCount * 20 + 1000);
    }

    _Use_decl_annotations_
    diag::MidiServicePingResponseSummary MidiDiagnostics::PingService(
        uint8_t pingCount, 
        uint32_t timeoutMilliseconds) noexcept
    {
        auto responseSummary = winrt::make_self<implementation::MidiServicePingResponseSummary>();

        try
        {
            if (responseSummary == nullptr)
            {
                LOG_IF_FAILED(E_FAIL);

                TraceLoggingWrite(
                    Midi2SdkTelemetryProvider::Provider(),
                    MIDI_SDK_TRACE_EVENT_ERROR,
                    TraceLoggingString(__FUNCTION__, MIDI_SDK_TRACE_LOCATION_FIELD),
                    TraceLoggingLevel(WINEVENT_LEVEL_ERROR),
                    TraceLoggingWideString(MIDI_SDK_STATIC_THIS_PLACEHOLDER_FIELD_VALUE, MIDI_SDK_TRACE_THIS_FIELD),
                    TraceLoggingWideString(L"Could not create response summary", MIDI_SDK_TRACE_MESSAGE_FIELD)
                );

                // just need to fail
                return nullptr;
            }

            if (pingCount == 0)
            {
                LOG_IF_FAILED(E_INVALIDARG);

                TraceLoggingWrite(
                    Midi2SdkTelemetryProvider::Provider(),
                    MIDI_SDK_TRACE_EVENT_ERROR,
                    TraceLoggingString(__FUNCTION__, MIDI_SDK_TRACE_LOCATION_FIELD),
                    TraceLoggingLevel(WINEVENT_LEVEL_ERROR),
                    TraceLoggingWideString(MIDI_SDK_STATIC_THIS_PLACEHOLDER_FIELD_VALUE, MIDI_SDK_TRACE_THIS_FIELD),
                    TraceLoggingWideString(L"Ping count is zero", MIDI_SDK_TRACE_MESSAGE_FIELD)
                );

                responseSummary->InternalSetFailed(L"Ping count is zero.");
                return *responseSummary;
            }

            std::vector<diag::MidiServicePingResponse> pings{};
            pings.resize(pingCount);

            if (timeoutMilliseconds == 0)
            {
                LOG_IF_FAILED(E_INVALIDARG);

                TraceLoggingWrite(
                    Midi2SdkTelemetryProvider::Provider(),
                    MIDI_SDK_TRACE_EVENT_ERROR,
                    TraceLoggingString(__FUNCTION__, MIDI_SDK_TRACE_LOCATION_FIELD),
                    TraceLoggingLevel(WINEVENT_LEVEL_ERROR),
                    TraceLoggingWideString(MIDI_SDK_STATIC_THIS_PLACEHOLDER_FIELD_VALUE, MIDI_SDK_TRACE_THIS_FIELD),
                    TraceLoggingWideString(L"Timeout milliseconds is zero", MIDI_SDK_TRACE_MESSAGE_FIELD)
                );

                responseSummary->InternalSetFailed(L"Timeout milliseconds is zero.");
                return *responseSummary;
            }

            // we use the full session API from here to get accurate timing

            auto session = midi2::MidiSession::Create(L"Ping Test");

            if (session == nullptr)
            {
                LOG_IF_FAILED(E_POINTER);

                TraceLoggingWrite(
                    Midi2SdkTelemetryProvider::Provider(),
                    MIDI_SDK_TRACE_EVENT_ERROR,
                    TraceLoggingString(__FUNCTION__, MIDI_SDK_TRACE_LOCATION_FIELD),
                    TraceLoggingLevel(WINEVENT_LEVEL_ERROR),
                    TraceLoggingWideString(MIDI_SDK_STATIC_THIS_PLACEHOLDER_FIELD_VALUE, MIDI_SDK_TRACE_THIS_FIELD),
                    TraceLoggingWideString(L"Unable to create session", MIDI_SDK_TRACE_MESSAGE_FIELD)
                );

                responseSummary->InternalSetFailed(L"Unable to create session.");
                return *responseSummary;
            }

            // This ID must be consistent with what the service is set up to use.

            auto endpoint = session.CreateEndpointConnection(MIDI_DIAGNOSTICS_PING_BIDI_ID, nullptr);

            if (endpoint == nullptr)
            {
                LOG_IF_FAILED(E_POINTER);

                TraceLoggingWrite(
                    Midi2SdkTelemetryProvider::Provider(),
                    MIDI_SDK_TRACE_EVENT_ERROR,
                    TraceLoggingString(__FUNCTION__, MIDI_SDK_TRACE_LOCATION_FIELD),
                    TraceLoggingLevel(WINEVENT_LEVEL_ERROR),
                    TraceLoggingWideString(MIDI_SDK_STATIC_THIS_PLACEHOLDER_FIELD_VALUE, MIDI_SDK_TRACE_THIS_FIELD),
                    TraceLoggingWideString(L"Unable to create ping endpoint", MIDI_SDK_TRACE_MESSAGE_FIELD)
                );

                responseSummary->InternalSetFailed(L"Unable to create ping endpoint.");
                return *responseSummary;
            }

            // originally I was going to use a random number for this, but using the low bits of
            // the timestamp makes more sense, and will be unique enough for this.

            uint32_t pingSourceId = (uint32_t)(MidiClock::Now() & 0x00000000FFFFFFFF);

            wil::unique_event_nothrow allMessagesReceived;
            allMessagesReceived.create(wil::EventOptions::ManualReset);

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

                            auto ping = pings[word2];

                            ping.ClientReceiveMidiTimestamp = actualReceiveEventTimestamp;
                            ping.ServiceReportedMidiTimestamp = args.Timestamp();
                            ping.ClientDeltaTimestamp = ping.ClientReceiveMidiTimestamp - ping.ClientSendMidiTimestamp;

                            pings[word2] = std::move(ping);

                            receivedCount++;

                            if (receivedCount == pingCount)
                            {
                                allMessagesReceived.SetEvent();
                            }
                        }
                        else
                        {
                            // something really wrong happened. Our index has been messed up.

                            LOG_IF_FAILED(E_UNEXPECTED);

                            TraceLoggingWrite(
                                Midi2SdkTelemetryProvider::Provider(),
                                MIDI_SDK_TRACE_EVENT_ERROR,
                                TraceLoggingString(__FUNCTION__, MIDI_SDK_TRACE_LOCATION_FIELD),
                                TraceLoggingLevel(WINEVENT_LEVEL_ERROR),
                                TraceLoggingWideString(MIDI_SDK_STATIC_THIS_PLACEHOLDER_FIELD_VALUE, MIDI_SDK_TRACE_THIS_FIELD),
                                TraceLoggingWideString(L"Index of ping response is out of bounds", MIDI_SDK_TRACE_MESSAGE_FIELD)
                            );
                        }
                    }
                    else
                    {
                        // someone else is sending stuff to this ping service. Naughty if not another ping.
                        LOG_IF_FAILED(E_UNEXPECTED);

                        TraceLoggingWrite(
                            Midi2SdkTelemetryProvider::Provider(),
                            MIDI_SDK_TRACE_EVENT_ERROR,
                            TraceLoggingString(__FUNCTION__, MIDI_SDK_TRACE_LOCATION_FIELD),
                            TraceLoggingLevel(WINEVENT_LEVEL_ERROR),
                            TraceLoggingWideString(MIDI_SDK_STATIC_THIS_PLACEHOLDER_FIELD_VALUE, MIDI_SDK_TRACE_THIS_FIELD),
                            TraceLoggingWideString(L"Another process may be sending messages to this endpoint", MIDI_SDK_TRACE_MESSAGE_FIELD)
                        );
                    }
                };

            // any failures after this need to revoke the event handler as well
            auto eventRevokeToken = endpoint.MessageReceived(winrt::auto_revoke, MessageReceivedHandler);

            // open the endpoint. We've already set options for it not to send out discovery messages
            if (!endpoint.Open())
            {
                LOG_IF_FAILED(E_FAIL);

                TraceLoggingWrite(
                    Midi2SdkTelemetryProvider::Provider(),
                    MIDI_SDK_TRACE_EVENT_ERROR,
                    TraceLoggingString(__FUNCTION__, MIDI_SDK_TRACE_LOCATION_FIELD),
                    TraceLoggingLevel(WINEVENT_LEVEL_ERROR),
                    TraceLoggingWideString(MIDI_SDK_STATIC_THIS_PLACEHOLDER_FIELD_VALUE, MIDI_SDK_TRACE_THIS_FIELD),
                    TraceLoggingWideString(L"Could not open ping endpoint", MIDI_SDK_TRACE_MESSAGE_FIELD)
                );

                responseSummary->InternalSetFailed(L"Endpoint open failed. The service may be unavailable.");

                session.DisconnectEndpointConnection(endpoint.ConnectionId());

                return *responseSummary;
            }

            // send out the ping messages

            for (uint32_t pingIndex = 0; pingIndex < pingCount; pingIndex++)
            {
                internal::PackedPingRequestUmp request;

                diag::MidiServicePingResponse response;

                internal::MidiTimestamp timestamp = MidiClock::Now();

                // Add this info to the tracking before we send, so no race condition
                // granted that this adds a few ticks to add this to the collection and build the object

                response.SourceId = pingSourceId;
                response.Index = pingIndex;
                response.ClientSendMidiTimestamp = timestamp;

                pings[pingIndex] = std::move(response);

                // send the ping

                auto sendMessageResult = endpoint.SendSingleMessageWords(timestamp, request.Word0, pingSourceId, pingIndex, request.Padding);

                if (midi2::MidiEndpointConnection::SendMessageFailed(sendMessageResult))
                {
                    LOG_IF_FAILED(E_FAIL);

                    TraceLoggingWrite(
                        Midi2SdkTelemetryProvider::Provider(),
                        MIDI_SDK_TRACE_EVENT_ERROR,
                        TraceLoggingString(__FUNCTION__, MIDI_SDK_TRACE_LOCATION_FIELD),
                        TraceLoggingLevel(WINEVENT_LEVEL_ERROR),
                        TraceLoggingWideString(MIDI_SDK_STATIC_THIS_PLACEHOLDER_FIELD_VALUE, MIDI_SDK_TRACE_THIS_FIELD),
                        TraceLoggingWideString(L"Ping send message failed", MIDI_SDK_TRACE_MESSAGE_FIELD)
                    );

                    responseSummary->InternalSetFailed(L"Sending message failed");

                    session.DisconnectEndpointConnection(endpoint.ConnectionId());

                    return *responseSummary;
                }

                //Sleep(0);
            }

            bool allReceivedFlag = allMessagesReceived.is_signaled();

            if (!allReceivedFlag)
            {
                // Wait for all responses to come in (receivedCount == pingCount). If not all responses come back, report the failure.
                allReceivedFlag = allMessagesReceived.wait(timeoutMilliseconds);

                if (!allReceivedFlag)
                {
                    responseSummary->InternalSetFailed(L"Not all ping responses received within appropriate time window.");

                    LOG_IF_FAILED(E_FAIL);

                    TraceLoggingWrite(
                        Midi2SdkTelemetryProvider::Provider(),
                        MIDI_SDK_TRACE_EVENT_ERROR,
                        TraceLoggingString(__FUNCTION__, MIDI_SDK_TRACE_LOCATION_FIELD),
                        TraceLoggingLevel(WINEVENT_LEVEL_ERROR),
                        TraceLoggingWideString(MIDI_SDK_STATIC_THIS_PLACEHOLDER_FIELD_VALUE, MIDI_SDK_TRACE_THIS_FIELD),
                        TraceLoggingWideString(L"Not all ping responses received within appropriate time window.", MIDI_SDK_TRACE_MESSAGE_FIELD)
                    );
                }
            }

            allMessagesReceived.reset();

            if (allReceivedFlag)
            {
                // all received
                responseSummary->InternalSetSucceeded();

                // copy over the holding array into the response and also calculate the totals

                uint64_t totalPing{ 0 };

                for (auto& response : pings)
                {
                    // internal::LogInfo(__FUNCTION__, L"Calculating total ping");

                    totalPing += response.ClientDeltaTimestamp;

                    responseSummary->InternalAddResponse(response);

                    // does I need to remove the com_ptr ref or will going out of scope be sufficient?
                }

                uint64_t averagePing = totalPing / responseSummary->Responses().Size();

                responseSummary->InternalSetTotals(totalPing, averagePing);
            }

            session.DisconnectEndpointConnection(endpoint.ConnectionId());
            session.Close();

            return *responseSummary;
        }
        catch (std::exception ex)
        {
            LOG_IF_FAILED(E_FAIL);

            TraceLoggingWrite(
                Midi2SdkTelemetryProvider::Provider(),
                MIDI_SDK_TRACE_EVENT_ERROR,
                TraceLoggingString(__FUNCTION__, MIDI_SDK_TRACE_LOCATION_FIELD),
                TraceLoggingLevel(WINEVENT_LEVEL_ERROR),
                TraceLoggingWideString(MIDI_SDK_STATIC_THIS_PLACEHOLDER_FIELD_VALUE, MIDI_SDK_TRACE_THIS_FIELD),
                TraceLoggingWideString(L"Exception pinging service", MIDI_SDK_TRACE_MESSAGE_FIELD),
                TraceLoggingString(ex.what(), MIDI_SDK_TRACE_ERROR_FIELD)
            );

            if (responseSummary != nullptr)
            {
                responseSummary->InternalSetFailed(winrt::to_hstring(ex.what()));

                return *responseSummary;
            }
            else
            {
                return nullptr;
            }
        }
        catch (...)
        {
            LOG_IF_FAILED(E_FAIL);

            TraceLoggingWrite(
                Midi2SdkTelemetryProvider::Provider(),
                MIDI_SDK_TRACE_EVENT_ERROR,
                TraceLoggingString(__FUNCTION__, MIDI_SDK_TRACE_LOCATION_FIELD),
                TraceLoggingLevel(WINEVENT_LEVEL_ERROR),
                TraceLoggingWideString(MIDI_SDK_STATIC_THIS_PLACEHOLDER_FIELD_VALUE, MIDI_SDK_TRACE_THIS_FIELD),
                TraceLoggingWideString(L"Exception pinging service", MIDI_SDK_TRACE_MESSAGE_FIELD)
            );

            if (responseSummary != nullptr)
            {
                responseSummary->InternalSetFailed(L"Exception pinging service");

                return *responseSummary;
            }
            else
            {
                return nullptr;
            }
        }
    }
}
