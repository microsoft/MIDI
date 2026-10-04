// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================

#pragma once

#include "pch.h"


_Use_decl_annotations_
HRESULT 
MidiNetworkConnection::Initialize(
    MidiNetworkConnectionRole const role,
    winrt::guid const& configIdentifier,
    std::wstring const& parentDeviceInstanceId,
    winrt::Windows::Networking::Sockets::DatagramSocket const& socket,
    winrt::Windows::Networking::HostName const& hostName,
    winrt::hstring const& port,
    winrt::Windows::Networking::HostName const& localHostName,
    std::wstring const& thisEndpointName,
    std::wstring const& thisProductInstanceId,
    uint16_t const retransmitBufferMaxCommandPacketCount,
    uint8_t const maxForwardErrorCorrectionCommandPacketCount,
    bool createUmpEndpointsOnly,
    uint8_t const fallbackMidi1PortCount
)
{
    TraceLoggingWrite(
        MidiNetworkMidiTransportTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this"),
        TraceLoggingWideString(L"Enter", MIDI_TRACE_EVENT_MESSAGE_FIELD)
    );

    m_configIdentifier = configIdentifier;
    m_parentDeviceInstanceId = parentDeviceInstanceId;

    m_sessionActive = false;

    m_role = role;

    m_remoteHostName = hostName;
    m_remotePort = port;

    m_createUmpEndpointsOnly = createUmpEndpointsOnly;
    m_fallbackMidi1PortCount = fallbackMidi1PortCount;

    m_thisEndpointName = thisEndpointName;
    m_thisProductInstanceId = thisProductInstanceId;

    m_retransmitBufferMaxCommandPacketCount = retransmitBufferMaxCommandPacketCount;
    m_maxForwardErrorCorrectionCommandPacketCount = maxForwardErrorCorrectionCommandPacketCount;

    // gives the idle reclaim check a meaningful starting point
    m_lastIncomingValidUdpPacketTimestamp = internal::GetCurrentMidiTimestamp();

    m_outgoingUmpMessages.reserve(MIDI_NETWORK_STARTING_OUTBOUND_UMP_QUEUE_CAPACITY);

    // build out the retransmit buffer used for FEC and retransmit requests
    try
    {
        m_retransmitBuffer.set_capacity(max(m_retransmitBufferMaxCommandPacketCount, m_maxForwardErrorCorrectionCommandPacketCount));
    }
    catch (...)
    {
        RETURN_IF_FAILED(E_OUTOFMEMORY);
    }

    m_retransmitBufferCapacity = m_retransmitBuffer.capacity();
    m_retransmitBufferWordCount = 0;

    RETURN_IF_FAILED(m_sendWakeEvent.create(wil::EventOptions::None));

    // Half a millisecond of resolution without touching the global timer rate. A normal timer
    // still works if the flag is refused, only less precisely.
    m_sendPaceTimer.reset(CreateWaitableTimerExW(nullptr, nullptr, CREATE_WAITABLE_TIMER_HIGH_RESOLUTION, TIMER_ALL_ACCESS));

    if (!m_sendPaceTimer)
    {
        m_sendPaceTimer.reset(CreateWaitableTimerExW(nullptr, nullptr, 0, TIMER_ALL_ACCESS));
    }

    RETURN_LAST_ERROR_IF_NULL(m_sendPaceTimer.get());

    {
        auto lock = m_sendSpeedLock.lock();

        m_automaticSendSpeed.Configure(m_sendSpeedLimit, m_reduceSendSpeedAutomatically);
        m_currentSendSpeed = m_automaticSendSpeed.CurrentMultiple();
    }

    try
    {
        m_outgoingPingTracking.set_capacity(m_outgoingPingTrackingMaxEntries);
    }
    catch (...)
    {
        RETURN_IF_FAILED(E_OUTOFMEMORY);
    }


    RETURN_IF_FAILED(ResetSequenceNumbers());

    // create the data writer
    m_writer = std::make_shared<MidiNetworkDataWriter>();
    RETURN_IF_NULL_ALLOC(m_writer);

    try
    {
        // A client socket has already been ConnectAsync'd to the remote, so it has a single
        // output stream. Only a host socket, which serves many remotes from one bound port,
        // needs a per-remote stream.
        if (role == MidiNetworkConnectionRole::ConnectionWindowsIsClient)
        {
            RETURN_IF_FAILED(m_writer->Initialize(socket.OutputStream()));
        }
        else
        {
            RETURN_IF_FAILED(m_writer->Initialize(GetReplyOutputStream(socket, localHostName, hostName, port)));

            m_replySourceHostName = localHostName;
        }
    }
    catch (...)
    {
        auto hr = wil::ResultFromCaughtException();

        TraceLoggingWrite(
            MidiNetworkMidiTransportTelemetryProvider::Provider(),
            MIDI_TRACE_EVENT_ERROR,
            TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
            TraceLoggingLevel(WINEVENT_LEVEL_ERROR),
            TraceLoggingPointer(this, "this"),
            TraceLoggingWideString(L"Exception obtaining the output stream for the remote endpoint", MIDI_TRACE_EVENT_MESSAGE_FIELD),
            TraceLoggingWideString(hostName.ToString().c_str(), "remote hostname"),
            TraceLoggingWideString(port.c_str(), "remote port"),
            TraceLoggingHResult(hr, MIDI_TRACE_EVENT_HRESULT_FIELD)
        );

        m_writer.reset();

        RETURN_IF_FAILED(hr);
    }

    RETURN_IF_FAILED(StartConnectionWatchdogThread());

    TraceLoggingWrite(
        MidiNetworkMidiTransportTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this"),
        TraceLoggingWideString(L"Exit", MIDI_TRACE_EVENT_MESSAGE_FIELD)
    );

    return S_OK;
}

_Use_decl_annotations_
winrt::Windows::Storage::Streams::IOutputStream
MidiNetworkConnection::GetReplyOutputStream(
    winrt::Windows::Networking::Sockets::DatagramSocket const& socket,
    winrt::Windows::Networking::HostName const& localHostName,
    winrt::Windows::Networking::HostName const& remoteHostName,
    winrt::hstring const& remotePort)
{
    if (localHostName != nullptr)
    {
        try
        {
            return socket.GetOutputStreamAsync(
                winrt::Windows::Networking::EndpointPair(localHostName, L"", remoteHostName, remotePort)).get();
        }
        catch (...)
        {
            TraceLoggingWrite(
                MidiNetworkMidiTransportTelemetryProvider::Provider(),
                MIDI_TRACE_EVENT_WARNING,
                TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
                TraceLoggingLevel(WINEVENT_LEVEL_WARNING),
                TraceLoggingWideString(L"Unable to reply from the address the remote sent to. Windows will choose the source address.", MIDI_TRACE_EVENT_MESSAGE_FIELD),
                TraceLoggingWideString(localHostName.CanonicalName().c_str(), "local address"),
                TraceLoggingWideString(remoteHostName != nullptr ? remoteHostName.CanonicalName().c_str() : L"", "remote address"),
                TraceLoggingHResult(wil::ResultFromCaughtException(), MIDI_TRACE_EVENT_HRESULT_FIELD)
            );
        }
    }

    return socket.GetOutputStreamAsync(remoteHostName, remotePort).get();
}

_Use_decl_annotations_
HRESULT
MidiNetworkConnection::FollowReplySource(
    winrt::Windows::Networking::Sockets::DatagramSocket const& socket,
    winrt::Windows::Networking::HostName const& localHostName)
{
    // Declared HRESULT, and called on the socket receive callback, so it must not throw.
    try
    {
        RETURN_HR_IF(S_FALSE, m_role != MidiNetworkConnectionRole::ConnectionWindowsIsHost);
        RETURN_HR_IF_NULL(S_FALSE, socket);
        RETURN_HR_IF_NULL(S_FALSE, localHostName);

        winrt::Windows::Networking::HostName previous{ nullptr };

        {
            auto lock = m_socketWriterLock.lock();

            if (m_writer == nullptr ||
                (m_replySourceHostName != nullptr && m_replySourceHostName.IsEqual(localHostName)))
            {
                return S_FALSE;
            }

            previous = m_replySourceHostName;
        }

        // Outside the lock, because obtaining the stream waits on the socket
        auto stream = GetReplyOutputStream(socket, localHostName, m_remoteHostName, winrt::hstring{ m_remotePort });

        {
            auto lock = m_socketWriterLock.lock();

            // torn down meanwhile
            if (m_writer == nullptr)
            {
                return S_FALSE;
            }

            RETURN_IF_FAILED(m_writer->ReplaceStream(stream));

            m_replySourceHostName = localHostName;
        }

        TraceLoggingWrite(
            MidiNetworkMidiTransportTelemetryProvider::Provider(),
            MIDI_TRACE_EVENT_INFO,
            TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
            TraceLoggingLevel(WINEVENT_LEVEL_INFO),
            TraceLoggingPointer(this, "this"),
            TraceLoggingWideString(L"The remote invited this host at another of its addresses. Replying from that address now.", MIDI_TRACE_EVENT_MESSAGE_FIELD),
            TraceLoggingWideString(previous != nullptr ? previous.CanonicalName().c_str() : L"", "previous local address"),
            TraceLoggingWideString(localHostName.CanonicalName().c_str(), "local address"),
            TraceLoggingWideString(m_remoteHostName != nullptr ? m_remoteHostName.CanonicalName().c_str() : L"", "remote address"),
            TraceLoggingWideString(m_remotePort.c_str(), "remote port")
        );

        return S_OK;
    }
    CATCH_RETURN()
}


_Use_decl_annotations_
HRESULT
MidiNetworkConnection::ConnectionWatcherThreadWorker(std::stop_token stopToken)
{
    // if we haven't received any UDP messages in a certain amount of time,
    // send a ping to the remote

    while (!m_shuttingDown && !stopToken.stop_requested())
    {
        auto threadWaitStartTimestamp = internal::GetCurrentMidiTimestamp();

        // Read each time round rather than captured at construction, so changing the setting
        // reaches existing sessions within one interval instead of only new ones.
        m_connectionTimeoutEvent.wait(TransportState::Current().TransportSettings.OutboundPingInterval);

        if (m_shuttingDown || stopToken.stop_requested())
        {
            break;
        }

        // We only monitor the liveness of an established session. Pinging a remote which has
        // never established a session would let a single spoofed datagram generate traffic
        // toward a forged address.
        if (!m_sessionActive)
        {
            {
                auto lock = m_pingTrackingLock.lock();
                m_outgoingPingTracking.clear();
            }

            // an invitation we sent may still be unanswered
            LOG_IF_FAILED(OnWatchdogTick());

            continue;
        }

        if (m_lastIncomingValidUdpPacketTimestamp > threadWaitStartTimestamp)
        {
            // Traffic is flowing, so liveness is not in question. Still ping below: a Ping Reply
            // is the only round trip we can time, and a session which is carrying MIDI would
            // otherwise never produce a latency sample at all.
            m_connectionTimeoutEvent.ResetEvent();
        }
        else
        {
            uint16_t consecutiveFailures{ 0 };

            {
                auto lock = m_pingTrackingLock.lock();

                // check our ping entries. We want to check the last N entries and if all of them
                // have been ignored, we will take action.
                for (auto pingEntry = m_outgoingPingTracking.rbegin();
                    pingEntry != m_outgoingPingTracking.rend() && consecutiveFailures <= m_outgoingPingMaxIgnoredBeforeDisconnect; pingEntry++)
                {
                    if (!pingEntry->Received)
                    {
                        consecutiveFailures++;
                    }
                    else
                    {
                        // the first time we find one that has been received, we bail
                        break;
                    }
                }
            }

            if (m_shuttingDown || stopToken.stop_requested())
            {
                break;
            }

            if (consecutiveFailures >= m_outgoingPingMaxIgnoredBeforeDisconnect)
            {
                TraceLoggingWrite(
                    MidiNetworkMidiTransportTelemetryProvider::Provider(),
                    MIDI_TRACE_EVENT_WARNING,
                    TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
                    TraceLoggingLevel(WINEVENT_LEVEL_WARNING),
                    TraceLoggingPointer(this, "this"),
                    TraceLoggingWideString(L"Remote endpoint stopped responding to pings. Ending session.", MIDI_TRACE_EVENT_MESSAGE_FIELD),
                    TraceLoggingUInt16(consecutiveFailures, "consecutive missed pings")
                );

                LOG_IF_FAILED(EndActiveSessionDueToTimeout());

                continue;
            }
        }

        // Only a remote which has gone quiet can be disconnected for missed pings, so a remote
        // which streams to us but never answers a Ping keeps its session exactly as before.
        LOG_IF_FAILED(SendPing());
    }

    return S_OK;
}

HRESULT
MidiNetworkConnection::SignalHealthyConnectionAndUpdateArrivalTimestamp()
{
    m_lastIncomingValidUdpPacketTimestamp = internal::GetCurrentMidiTimestamp();

    m_connectionTimeoutEvent.ResetEvent();

    return S_OK;
}


HRESULT
MidiNetworkConnection::StartOutboundMidiMessageProcessingThread()
{
    TraceLoggingWrite(
        MidiNetworkMidiTransportTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this"),
        TraceLoggingWideString(L"Enter", MIDI_TRACE_EVENT_MESSAGE_FIELD)
    );

    auto threadsLock = m_workerThreadsLock.lock();

    // Shutdown stops the threads once, so one started after it began would never be stopped
    RETURN_HR_IF(E_ABORT, m_shuttingDown);

    // A host connection can carry a new session after one times out. Its previous send thread is
    // woken so it stops now rather than sleeping out its interval.
    if (m_outboundProcessingThread.joinable())
    {
        m_outboundProcessingThread.request_stop();
        WakeSendThread();
        m_outboundProcessingThread.join();
    }

    // A new session starts with the whole allowance and nothing left to resend
    m_sendPacer.Reset();
    ClearResendRequests();

    // The stop token must come from jthread itself, not from reading the member back out of
    // the object we are in the middle of assigning to.
    m_outboundProcessingThread = std::jthread([this](std::stop_token stopToken)
        {
            try
            {
                LOG_IF_FAILED(OutboundProcessingThreadWorker(stopToken));
            }
            CATCH_LOG();
        });

    // Anything held while the endpoint was being created
    {
        auto queueLock = m_outgoingUmpMessageQueueLock.lock();

        if (m_outgoingReadIndex < m_outgoingUmpMessages.size())
        {
            WakeSendThread();
        }
    }

    TraceLoggingWrite(
        MidiNetworkMidiTransportTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this"),
        TraceLoggingWideString(L"Exit", MIDI_TRACE_EVENT_MESSAGE_FIELD)
    );

    return S_OK;
}


HRESULT
MidiNetworkConnection::StartConnectionWatchdogThread()
{
    TraceLoggingWrite(
        MidiNetworkMidiTransportTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this"),
        TraceLoggingWideString(L"Enter", MIDI_TRACE_EVENT_MESSAGE_FIELD)
    );

    m_connectionWatcherThread = std::jthread([this](std::stop_token stopToken)
        {
            try
            {
                LOG_IF_FAILED(ConnectionWatcherThreadWorker(stopToken));
            }
            CATCH_LOG();
        });

    return S_OK;
}

_Use_decl_annotations_
void
MidiNetworkConnection::LogSendFailure(HRESULT const hr)
{
    TraceLoggingWrite(
        MidiNetworkMidiTransportTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_ERROR,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_ERROR),
        TraceLoggingPointer(this, "this"),
        TraceLoggingWideString(L"Unable to send datagram to remote endpoint", MIDI_TRACE_EVENT_MESSAGE_FIELD),
        TraceLoggingWideString(m_remoteHostName != nullptr ? m_remoteHostName.ToString().c_str() : L"", "remote hostname"),
        TraceLoggingWideString(m_remotePort.c_str(), "remote port"),
        TraceLoggingHResult(hr, MIDI_TRACE_EVENT_HRESULT_FIELD)
    );
}

HRESULT
MidiNetworkConnection::StopAndJoinWorkerThreads()
{
    auto threadsLock = m_workerThreadsLock.lock();

    m_connectionWatcherThread.request_stop();
    m_outboundProcessingThread.request_stop();

    // wake both workers so they see the stop request instead of sleeping out their intervals
    m_connectionTimeoutEvent.SetEvent();
    WakeSendThread();

    // A worker can reach here indirectly (session teardown on the watchdog thread), and joining
    // ourselves would deadlock. The remaining teardown is safe against a still-running worker.
    if (m_connectionWatcherThread.joinable() && m_connectionWatcherThread.get_id() != std::this_thread::get_id())
    {
        m_connectionWatcherThread.join();
    }

    if (m_outboundProcessingThread.joinable() && m_outboundProcessingThread.get_id() != std::this_thread::get_id())
    {
        m_outboundProcessingThread.join();
    }

    return S_OK;
}

_Use_decl_annotations_
HRESULT 
MidiNetworkConnection::ConnectMidiCallback(
    wil::com_ptr_nothrow<IMidiCallback> callback
)
{
    // Declared HRESULT, so it must not throw: callers use RETURN_IF_FAILED and an
    // escaping WinRT exception would unwind past them into a worker thread.
    try
    {
        RETURN_HR_IF_NULL(E_INVALIDARG, callback);

        TraceLoggingWrite(
            MidiNetworkMidiTransportTelemetryProvider::Provider(),
            MIDI_TRACE_EVENT_INFO,
            TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
            TraceLoggingLevel(WINEVENT_LEVEL_INFO),
            TraceLoggingPointer(this, "this"),
            TraceLoggingWideString(L"Enter", MIDI_TRACE_EVENT_MESSAGE_FIELD),
            TraceLoggingPointer(callback.get(), "callback")
        );

        wil::com_ptr_nothrow<IMidiCallback> previous{ nullptr };

        {
            auto lock = m_callbackLock.lock();

            previous = std::move(m_callback);
            m_callback = callback;
        }

        // released outside the lock, because it can be the last reference to an older instance
        previous.reset();

        TraceLoggingWrite(
            MidiNetworkMidiTransportTelemetryProvider::Provider(),
            MIDI_TRACE_EVENT_INFO,
            TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
            TraceLoggingLevel(WINEVENT_LEVEL_INFO),
            TraceLoggingPointer(this, "this"),
            TraceLoggingWideString(L"Exit", MIDI_TRACE_EVENT_MESSAGE_FIELD)
        );

        return S_OK;
    }
    CATCH_RETURN()
}

_Use_decl_annotations_
HRESULT
MidiNetworkConnection::DisconnectMidiCallbackIfCurrent(IMidiCallback* callback)
{
    TraceLoggingWrite(
        MidiNetworkMidiTransportTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this"),
        TraceLoggingWideString(L"Enter", MIDI_TRACE_EVENT_MESSAGE_FIELD),
        TraceLoggingPointer(callback, "callback")
    );

    wil::com_ptr_nothrow<IMidiCallback> detached{ nullptr };

    {
        auto lock = m_callbackLock.lock();

        if (m_callback.get() != callback)
        {
            return S_FALSE;
        }

        detached = std::move(m_callback);
    }

    // Released outside the lock: this drops our reference on the Bidi, which can be the last one.
    detached.reset();

    return S_OK;
}

void
MidiNetworkConnection::BeginEndpointCreation() noexcept
{
    auto queueLock = m_outgoingUmpMessageQueueLock.lock();

    m_endpointBeingCreated = true;
}

void
MidiNetworkConnection::EndEndpointCreation() noexcept
{
    {
        auto queueLock = m_outgoingUmpMessageQueueLock.lock();

        m_endpointBeingCreated = false;

        if (!m_sessionActive)
        {
            ClearOutgoingQueue();
        }
    }

    WakeSendersWaitingForRoom();
}



_Use_decl_annotations_
HRESULT 
MidiNetworkConnection::HandleIncomingUmpData(
    uint64_t const timestamp,
    std::vector<uint32_t> const& words
)
{
    TraceLoggingWrite(
        MidiNetworkMidiTransportTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this"),
        TraceLoggingWideString(L"Enter", MIDI_TRACE_EVENT_MESSAGE_FIELD)
    );


    // A strong local reference keeps the Bidi alive for the duration of the callback even if
    // another thread tears the session down underneath us.
    auto callback = GetCallback();

    // empty UMP packets are a keep-alive approach
    // callback can be null if there are no open connections
    // from the client, but the remote device is sending messages
    if (m_sessionActive && words.size() > 0 && callback != nullptr)
    {
        // this may have more than one message, so we need to tease it apart here
        // and send the individual messages

        size_t index{ 0 };

        while (index < words.size())
        {
            uint8_t messageWordCount = internal::GetUmpLengthInMidiWordsFromFirstWord(words[index]);

            // The message type nibble is remote-supplied and can claim a length longer than what
            // was actually sent. Reading it would hand adjacent heap memory to every client.
            if (messageWordCount == 0 || index + messageWordCount > words.size())
            {
                TraceLoggingWrite(
                    MidiNetworkMidiTransportTelemetryProvider::Provider(),
                    MIDI_TRACE_EVENT_WARNING,
                    TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
                    TraceLoggingLevel(WINEVENT_LEVEL_WARNING),
                    TraceLoggingPointer(this, "this"),
                    TraceLoggingWideString(L"Truncated or malformed UMP message from remote endpoint. Discarding remainder of command.", MIDI_TRACE_EVENT_MESSAGE_FIELD),
                    TraceLoggingUInt8(messageWordCount, "declared word count"),
                    TraceLoggingUInt64(static_cast<uint64_t>(words.size() - index), "words remaining")
                );

                break;
            }

            LOG_IF_FAILED(callback->Callback(
                MessageOptionFlags::MessageOptionFlags_None,
                (PVOID)(&words[index]),
                (UINT)(messageWordCount * sizeof(uint32_t)),
                timestamp, 
                (LONGLONG)0));            // todo: may need to pass along the context

            index += messageWordCount;
        }
    }

    TraceLoggingWrite(
        MidiNetworkMidiTransportTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this"),
        TraceLoggingWideString(L"Exit", MIDI_TRACE_EVENT_MESSAGE_FIELD)
    );

    return S_OK;
}

HRESULT 
MidiNetworkConnection::ResetSequenceNumbers()
{
    // reset the last sent sequence number
    m_lastSentUmpCommandSequenceNumber = 0;
    m_lastSentUmpCommandSequenceNumber--;       // prepare for next

    // reset the last received sequence number.
    m_lastReceivedUmpCommandSequenceNumber = 0;
    m_lastReceivedUmpCommandSequenceNumber--;

    // clear out retransmit buffer
    m_retransmitBuffer.clear();
    m_retransmitBufferWordCount = 0;

    return S_OK;
}



HRESULT
MidiNetworkConnection::EndActiveSessionDueToTimeout()
{
    TraceLoggingWrite(
        MidiNetworkMidiTransportTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_WARNING,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_WARNING),
        TraceLoggingPointer(this, "this"),
        TraceLoggingWideString(L"Ending session due to timeout", MIDI_TRACE_EVENT_MESSAGE_FIELD)
    );

    // Tell the remote first. Ending the session tears down the state this needs.
    LOG_IF_FAILED(SendToNetwork([](MidiNetworkDataWriter& writer)
        {
            RETURN_IF_FAILED(writer.WriteCommandBye(MidiNetworkCommandByeReason::CommandByeReasonCommon_Timeout, internal::ResourceGetWString(IDS_MESSAGE_SESSION_TIMED_OUT)));

            return S_OK;
        }));

    LOG_IF_FAILED(EndActiveSession(false));

    OnSessionEndedByRemote();

    return S_OK;
}

_Use_decl_annotations_
HRESULT
MidiNetworkConnection::EndActiveSession(bool respondWithByeReply)
{
    TraceLoggingWrite(
        MidiNetworkMidiTransportTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this"),
        TraceLoggingWideString(L"Enter", MIDI_TRACE_EVENT_MESSAGE_FIELD)
    );

    std::wstring deviceInstanceId{ };

    {
        auto lock = m_sessionLock.lock();

        m_sessionActive = false;

        deviceInstanceId.swap(m_sessionDeviceInstanceId);

        // Under the lock the endpoint was claimed under, so a Bidi opened from here on can't
        // attach to a session which has ended.
        if (!m_sessionEndpointDeviceInterfaceId.empty())
        {
            LOG_IF_FAILED(TransportState::Current().DisassociateMidiEndpointFromConnection(m_sessionEndpointDeviceInterfaceId));

            m_sessionEndpointDeviceInterfaceId.clear();
        }
    }

    // Release our reference to the client callback before anything that can re-enter. Removing
    // the endpoint shuts down the Bidi, which calls back into this connection.
    auto callback = DetachCallback();
    callback.reset();

    // clear the outbound queue. Anything still waiting to be queued gives up.
    {
        auto queueLock = m_outgoingUmpMessageQueueLock.lock();
        ClearOutgoingQueue();
    }

    WakeSendersWaitingForRoom();
    ClearResendRequests();

    {
        auto lock = m_socketWriterLock.lock();
        LOG_IF_FAILED(ResetSequenceNumbers());
    }

    {
        auto lock = m_pingTrackingLock.lock();
        m_outgoingPingTracking.clear();
    }

    if (respondWithByeReply)
    {
        LOG_IF_FAILED(SendToNetwork([](MidiNetworkDataWriter& writer)
            {
                RETURN_IF_FAILED(writer.WriteCommandByeReply());

                return S_OK;
            }));
    }

    if (!deviceInstanceId.empty())
    {
        auto endpointManager = TransportState::Current().GetEndpointManager();

        if (endpointManager != nullptr)
        {
            // Removal blocks on the service, and this runs on the socket receive callback, the
            // watchdog and configuration calls, so the endpoint worker does it.
            LOG_IF_FAILED(endpointManager->RemoveEndpointForSession(deviceInstanceId));
        }
    }

    // The writer deliberately outlives the session. A session ending is not the connection
    // ending: the same remote address and port may send a fresh invitation, and destroying the
    // writer here is what previously made the connection permanently unusable.

    TraceLoggingWrite(
        MidiNetworkMidiTransportTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this"),
        TraceLoggingWideString(L"Exit", MIDI_TRACE_EVENT_MESSAGE_FIELD)
    );

    return S_OK;
}


HRESULT
MidiNetworkConnection::SendShutdownBye()
{
    // Nothing to say if the session already ended - the remote either sent us the Bye or timed
    // out, and an unsolicited Bye would just earn a "session not established" refusal.
    if (!m_sessionActive)
    {
        return S_FALSE;
    }

    // teardown is two-phase, so this can arrive twice for the same connection
    if (m_shutdownByeSent.exchange(true))
    {
        return S_FALSE;
    }

    // Fire and forget. Waiting for a Bye Reply here only delays a shutdown that is already
    // under way, and with many sessions those waits add up against the service stop timeout.
    // If the datagram is lost the remote falls back to its own ping timeout.
    LOG_IF_FAILED(SendToNetwork([](MidiNetworkDataWriter& writer)
        {
            // "Power Down" rather than "User terminated", because the latter tends to make the
            // remote discard what it knows about us. This path covers both service shutdown and
            // an explicit disconnect, so the message says neither.
            RETURN_IF_FAILED(writer.WriteCommandBye(
                MidiNetworkCommandByeReason::CommandByeReasonCommon_PowerDown,
                internal::ResourceGetWString(IDS_MESSAGE_CONNECTION_ENDED).c_str()));

            return S_OK;
        }));

    return S_OK;
}

HRESULT
MidiNetworkConnection::SendUserTerminatedByeAndAwaitReply()
{
    if (!m_sessionActive || m_shuttingDown)
    {
        return S_FALSE;
    }

    m_byeReplyEvent.ResetEvent();

    bool replyReceived{ false };
    uint16_t attempts{ 0 };

    while (attempts < MIDI_NETWORK_BYE_MAX_ATTEMPTS && !m_shuttingDown)
    {
        attempts++;

        LOG_IF_FAILED(SendToNetwork([](MidiNetworkDataWriter& writer)
            {
                RETURN_IF_FAILED(writer.WriteCommandBye(
                    MidiNetworkCommandByeReason::CommandByeReasonCommon_UserTerminated,
                    internal::ResourceGetWString(IDS_MESSAGE_USER_DISCONNECTED).c_str()));

                return S_OK;
            }));

        if (m_byeReplyEvent.wait(MIDI_NETWORK_BYE_REPLY_TIMEOUT_MILLISECONDS))
        {
            // Shutdown sets this too, so a wake is only a reply if we are not shutting down
            replyReceived = !m_shuttingDown;
            break;
        }
    }

    TraceLoggingWrite(
        MidiNetworkMidiTransportTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this"),
        TraceLoggingWideString(L"User-initiated disconnect", MIDI_TRACE_EVENT_MESSAGE_FIELD),
        TraceLoggingBoolean(replyReceived, "bye reply received"),
        TraceLoggingUInt16(attempts, "attempts"),
        TraceLoggingBoolean(m_shuttingDown, "shutting down")
    );

    // The session is over whether or not the remote acknowledged, and marking it ended here is
    // what stops Shutdown() from sending a second Bye with a different reason.
    LOG_IF_FAILED(EndActiveSession(false));

    return replyReceived ? S_OK : S_FALSE;
}

HRESULT
MidiNetworkConnection::HandleIncomingByeReply()
{
    // Only a user-initiated disconnect waits on this. The shutdown Bye is fire-and-forget.
    m_byeReplyEvent.SetEvent();

    return S_OK;
}

_Use_decl_annotations_
HRESULT
MidiNetworkConnection::HandleIncomingBye(MidiNetworkCommandByeReason const reason)
{
    TraceLoggingWrite(
        MidiNetworkMidiTransportTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this"),
        TraceLoggingWideString(L"Enter", MIDI_TRACE_EVENT_MESSAGE_FIELD),
        TraceLoggingUInt8(reason, "bye reason")
    );

    // whatever the outcome, the remote has answered us
    OnByeReceived(reason);

    bool sessionWasActive{ false };

    {
        auto lock = m_sessionLock.lock();

        sessionWasActive = m_sessionActive;

        // Decided under the lock a session is established under. An endpoint finishing on the
        // worker then either sees this and is removed, or has made the session active first and
        // is ended below.
        if (!sessionWasActive)
        {
            // No session means any endpoint we were told to build for this remote is now pointless.
            OnSessionEndedBeforeEndpointCreated();
        }
    }

    if (sessionWasActive)
    {
        LOG_IF_FAILED(EndActiveSession(true));

        // the remote said goodbye on its own, so this is one to re-establish
        OnSessionEndedByRemote();
    }
    else
    {
        // Spec 6.16: "Because the Bye Command might be repeated, the Bye Reply shall also be
        // sent if there is no Pending or Established Session." Staying silent here leaves the
        // sender repeating until its own timeout.
        LOG_IF_FAILED(SendToNetwork([](MidiNetworkDataWriter& writer)
            {
                RETURN_IF_FAILED(writer.WriteCommandByeReply());

                return S_OK;
            }));
    }

    TraceLoggingWrite(
        MidiNetworkMidiTransportTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this"),
        TraceLoggingWideString(L"Exit", MIDI_TRACE_EVENT_MESSAGE_FIELD)
    );

    return S_OK;
}


_Use_decl_annotations_
HRESULT
MidiNetworkConnection::HandleIncomingInvitationReplyAccepted(
    MidiNetworkCommandPacketHeader const& header,
    std::wstring const& remoteHostUmpEndpointName,
    std::wstring const& remoteHostProductInstanceId
)
{
    UNREFERENCED_PARAMETER(remoteHostUmpEndpointName);
    UNREFERENCED_PARAMETER(remoteHostProductInstanceId);

    TraceLoggingWrite(
        MidiNetworkMidiTransportTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_ERROR,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_ERROR),
        TraceLoggingPointer(this, "this"),
        TraceLoggingWideString(L"We are not in the client role, but received an invitation accept. Not normal.", MIDI_TRACE_EVENT_MESSAGE_FIELD)
    );

    // we are a host, not a client, so NAK this per spec 6.4
    LOG_IF_FAILED(SendToNetwork([&header](MidiNetworkDataWriter& writer)
        {
            RETURN_IF_FAILED(writer.WriteCommandNAK(header.HeaderWord, MidiNetworkCommandNAKReason::CommandNAKReason_CommandNotExpected, internal::ResourceGetWString(IDS_MESSAGE_UNEXPECTED_INVITATION_ACCEPT)));

            return S_OK;
        }));

    return S_OK;
}

_Use_decl_annotations_
HRESULT
MidiNetworkConnection::HandleIncomingInvitation(
    MidiNetworkCommandPacketHeader const& header,
    MidiNetworkCommandInvitationCapabilities const& capabilities,
    std::wstring const& clientUmpEndpointName,
    std::wstring const& clientProductInstanceId
)
{
    UNREFERENCED_PARAMETER(capabilities);
    UNREFERENCED_PARAMETER(clientUmpEndpointName);
    UNREFERENCED_PARAMETER(clientProductInstanceId);

    // we are a client, not a host, so NAK this per spec 6.4
    LOG_IF_FAILED(SendToNetwork([&header](MidiNetworkDataWriter& writer)
        {
            RETURN_IF_FAILED(writer.WriteCommandNAK(header.HeaderWord, MidiNetworkCommandNAKReason::CommandNAKReason_CommandNotExpected, internal::ResourceGetWString(IDS_MESSAGE_UNEXPECTED_INVITATION)));

            return S_OK;
        }));

    return S_OK;
}

_Use_decl_annotations_
HRESULT
MidiNetworkConnection::HandleIncomingInvitationWithAuthentication(
    MidiNetworkCommandPacketHeader const& header)
{
    UNREFERENCED_PARAMETER(header);

    // Only a host is ever answered with this. A client receiving one has no challenge in
    // flight, so it withdraws rather than leaving the remote waiting.
    return RefuseInvitationForAuthentication(MidiNetworkCommandByeReason::CommandByeReasonClientToHost_InvitationCanceled);
}

void
MidiNetworkConnection::AbandonCurrentRetransmitRequest()
{
    // Forcing the attempt count to the limit makes the next gapped UMP Data command
    // resynchronize instead of asking again.
    m_retransmitRequestAttempts = MIDI_NETWORK_MAX_RETRANSMIT_REQUEST_ATTEMPTS;
}

void
MidiNetworkConnection::ResetRetransmitRequestState()
{
    m_retransmitRequestOutstanding = false;
    m_retransmitRequestAttempts = 0;
    m_retransmitRequestSequenceNumber = 0;
}

_Use_decl_annotations_
HRESULT
MidiNetworkConnection::HandleIncomingNAK(
    MidiNetworkCommandNAKReason const reason,
    MidiNetworkCommandPacketHeader const& originalCommandHeader,
    std::wstring const& text)
{
    TraceLoggingWrite(
        MidiNetworkMidiTransportTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_WARNING,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_WARNING),
        TraceLoggingPointer(this, "this"),
        TraceLoggingWideString(L"Remote endpoint sent a NAK", MIDI_TRACE_EVENT_MESSAGE_FIELD),
        TraceLoggingUInt8(reason, "reason"),
        TraceLoggingUInt8(originalCommandHeader.HeaderData.CommandCode, "NAKed command code"),
        TraceLoggingWideString(text.c_str(), "text")
    );

    if (originalCommandHeader.HeaderData.CommandCode == MidiNetworkCommandCode::CommandCommon_RetransmitRequest)
    {
        if (reason == MidiNetworkCommandNAKReason::CommandNAKReason_CommandNotSupported)
        {
            // Spec 7.2.3: the remote does not implement retransmit, so we must stop asking
            // for the rest of the session and just live with the gaps.
            m_remoteSupportsRetransmit = false;

            TraceLoggingWrite(
                MidiNetworkMidiTransportTelemetryProvider::Provider(),
                MIDI_TRACE_EVENT_INFO,
                TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
                TraceLoggingLevel(WINEVENT_LEVEL_INFO),
                TraceLoggingPointer(this, "this"),
                TraceLoggingWideString(L"Remote endpoint does not support retransmit. No further retransmit requests will be sent for this session.", MIDI_TRACE_EVENT_MESSAGE_FIELD)
            );
        }

        AbandonCurrentRetransmitRequest();
    }

    return S_OK;
}

_Use_decl_annotations_
HRESULT
MidiNetworkConnection::HandleIncomingRetransmitError(
    MidiNetworkCommandRetransmitErrorReason const reason,
    uint16_t const sequenceNumber)
{
    TraceLoggingWrite(
        MidiNetworkMidiTransportTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_WARNING,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_WARNING),
        TraceLoggingPointer(this, "this"),
        TraceLoggingWideString(L"Remote endpoint cannot fulfill the retransmit request. Accepting the loss.", MIDI_TRACE_EVENT_MESSAGE_FIELD),
        TraceLoggingUInt8(reason, "reason"),
        TraceLoggingUInt16(sequenceNumber, "earliest available sequence number")
    );

    // The data is gone. Waiting for it would stall the session permanently.
    AbandonCurrentRetransmitRequest();

    return S_OK;
}

HRESULT
MidiNetworkConnection::HandleIncomingSessionReset()
{
    TraceLoggingWrite(
        MidiNetworkMidiTransportTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this"),
        TraceLoggingWideString(L"Remote endpoint requested a session reset", MIDI_TRACE_EVENT_MESSAGE_FIELD)
    );

    {
        auto lock = m_socketWriterLock.lock();
        LOG_IF_FAILED(ResetSequenceNumbers());
    }

    // what was asked for refers to sequence numbers which no longer exist
    ClearResendRequests();
    ResetRetransmitRequestState();

    // spec 6.13: the reset is only complete once we have acknowledged it
    RETURN_IF_FAILED(SendToNetwork([](MidiNetworkDataWriter& writer)
        {
            RETURN_IF_FAILED(writer.WriteCommandSessionResetReply());

            return S_OK;
        }));

    return S_OK;
}

HRESULT
MidiNetworkConnection::HandleIncomingSessionResetReply()
{
    TraceLoggingWrite(
        MidiNetworkMidiTransportTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this"),
        TraceLoggingWideString(L"Remote endpoint acknowledged our session reset", MIDI_TRACE_EVENT_MESSAGE_FIELD)
    );

    {
        auto lock = m_socketWriterLock.lock();
        LOG_IF_FAILED(ResetSequenceNumbers());
    }

    ClearResendRequests();
    ResetRetransmitRequestState();

    return S_OK;
}


_Use_decl_annotations_
HRESULT
MidiNetworkConnection::RefuseSessionForEndpointCreationFailure(HRESULT const creationResult)
{
    bool alreadyAttached = (creationResult == HRESULT_FROM_WIN32(ERROR_DEVICE_ALREADY_ATTACHED));

    MidiNetworkCommandByeReason reason{ MidiNetworkCommandByeReason::CommandByeReasonCommon_Undefined };
    std::wstring message{ internal::ResourceGetWString(IDS_MESSAGE_ENDPOINT_CREATION_FAILED) };

    if (alreadyAttached)
    {
        reason = ByeReasonForDeviceAlreadyAttached();

        message = internal::ResourceGetWString(IDS_MESSAGE_DEVICE_ALREADY_CONNECTED);
    }

    TraceLoggingWrite(
        MidiNetworkMidiTransportTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_WARNING,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_WARNING),
        TraceLoggingPointer(this, "this"),
        TraceLoggingWideString(L"Declining session because the MIDI endpoint could not be created", MIDI_TRACE_EVENT_MESSAGE_FIELD),
        TraceLoggingWideString(message.c_str(), "reason text"),
        TraceLoggingHResult(creationResult, MIDI_TRACE_EVENT_HRESULT_FIELD)
    );

    LOG_IF_FAILED(SendToNetwork([&reason, &message](MidiNetworkDataWriter& writer)
        {
            RETURN_IF_FAILED(writer.WriteCommandBye(reason, message));

            return S_OK;
        }));

    return S_OK;
}

_Use_decl_annotations_
HRESULT
MidiNetworkConnection::RefuseInvitationForAuthentication(MidiNetworkCommandByeReason const reason)
{
    TraceLoggingWrite(
        MidiNetworkMidiTransportTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_WARNING,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_WARNING),
        TraceLoggingPointer(this, "this"),
        TraceLoggingWideString(L"Refusing invitation because authentication is not supported", MIDI_TRACE_EVENT_MESSAGE_FIELD),
        TraceLoggingUInt8(reason, "bye reason")
    );

    RETURN_IF_FAILED(SendToNetwork([&reason](MidiNetworkDataWriter& writer)
        {
            RETURN_IF_FAILED(writer.WriteCommandBye(reason, internal::ResourceGetWString(IDS_MESSAGE_AUTHENTICATION_NOT_SUPPORTED)));

            return S_OK;
        }));

    return S_OK;
}

_Use_decl_annotations_
HRESULT
MidiNetworkConnection::HandleIncomingInvitationReplyAuthenticationRequired(
    MidiNetworkCommandPacketHeader const& header)
{
    UNREFERENCED_PARAMETER(header);

    // Only a client has an invitation in flight to be challenged over.
    return RefuseInvitationForAuthentication(MidiNetworkCommandByeReason::CommandByeReasonClientToHost_InvitationCanceled);
}

HRESULT
MidiNetworkConnection::HandleIncomingInvitationReplyPending()
{
    // Only a client is answered with this, and it has nothing to wait for.
    return S_OK;
}


_Use_decl_annotations_
HRESULT
MidiNetworkConnection::SendByeSessionNotEstablished(uint8_t const commandCode)
{
    // One remote per connection, so a constant key is all that is needed here. Without this a
    // peer can make us emit a refusal for every command in every datagram it sends.
    if (!m_replyRateLimiter.ShouldSend(0))
    {
        return S_FALSE;
    }

    TraceLoggingWrite(
        MidiNetworkMidiTransportTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_WARNING,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_WARNING),
        TraceLoggingPointer(this, "this"),
        TraceLoggingWideString(L"Command received outside an established session", MIDI_TRACE_EVENT_MESSAGE_FIELD),
        TraceLoggingUInt8(commandCode, "Command Code")
    );

    RETURN_IF_FAILED(SendToNetwork([](MidiNetworkDataWriter& writer)
        {
            RETURN_IF_FAILED(writer.WriteCommandBye(MidiNetworkCommandByeReason::CommandByeReasonCommon_SessionNotEstablished, internal::ResourceGetWString(IDS_MESSAGE_NO_SESSION_ESTABLISHED)));

            return S_OK;
        }));

    return S_OK;
}




_Use_decl_annotations_
HRESULT
MidiNetworkConnection::HandleIncomingPing(uint32_t const pingId)
{
    TraceLoggingWrite(
        MidiNetworkMidiTransportTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this"),
        TraceLoggingWideString(L"Enter", MIDI_TRACE_EVENT_MESSAGE_FIELD)
    );

    RETURN_IF_FAILED(SendToNetwork([&pingId](MidiNetworkDataWriter& writer)
        {
            RETURN_IF_FAILED(writer.WriteCommandPingReply(pingId));

            return S_OK;
        }));

    TraceLoggingWrite(
        MidiNetworkMidiTransportTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this"),
        TraceLoggingWideString(L"Exit", MIDI_TRACE_EVENT_MESSAGE_FIELD)
    );

    return S_OK;
}

_Use_decl_annotations_
HRESULT
MidiNetworkConnection::HandleIncomingPingReply(uint32_t const pingId)
{
    TraceLoggingWrite(
        MidiNetworkMidiTransportTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this"),
        TraceLoggingWideString(L"Enter", MIDI_TRACE_EVENT_MESSAGE_FIELD)
    );

    auto timestamp = internal::GetCurrentMidiTimestamp();

    {
        auto lock = m_pingTrackingLock.lock();

        for (auto& pingEntry : m_outgoingPingTracking)
        {
            if (pingEntry.PingId == pingId)
            {
                pingEntry.PingReceiveTimestamp = timestamp;
                pingEntry.Received = true;

                // calculate latency
                AddLatencyToAverageLatencyTicks(pingEntry.PingReceiveTimestamp - pingEntry.PingSendTimestamp);

                // we may want to do a running average here instead of just the last.

                // todo: update the latency properties used in the scheduler

                break;
            }
        }
    }

    TraceLoggingWrite(
        MidiNetworkMidiTransportTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this"),
        TraceLoggingWideString(L"Exit", MIDI_TRACE_EVENT_MESSAGE_FIELD)
    );

    return S_OK;

}


_Use_decl_annotations_
HRESULT
MidiNetworkConnection::ReadUtf8String(
    winrt::Windows::Storage::Streams::DataReader const& reader, 
    size_t const byteCount,
    std::wstring& value)
{
    value.clear();

    if (byteCount == 0)
    {
        return S_OK;
    }

    // the length is remote-supplied, so it is never trusted against the actual datagram
    RETURN_HR_IF(E_INVALIDARG, byteCount > reader.UnconsumedBufferLength());

    auto bytes = std::vector<byte>(byteCount);

    try
    {
        reader.ReadBytes(bytes);
    }
    catch (...)
    {
        RETURN_IF_FAILED(wil::ResultFromCaughtException());
    }

    // strings on the wire are zero-padded out to a 32-bit word boundary. Without trimming, the
    // padding becomes embedded nulls and every later comparison against the string fails.
    auto stringEnd = std::find(bytes.begin(), bytes.end(), (byte)0);

    std::string s(bytes.begin(), stringEnd);

    try
    {
#pragma warning (push)
#pragma warning (disable: 4996)
        std::wstring_convert<std::codecvt_utf8<wchar_t>> convert;
        value = convert.from_bytes(s);
#pragma warning (pop)
    }
    catch (...)
    {
        // malformed UTF-8 from the remote. Recoverable, but we can't use the string.
        value.clear();

        TraceLoggingWrite(
            MidiNetworkMidiTransportTelemetryProvider::Provider(),
            MIDI_TRACE_EVENT_WARNING,
            TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
            TraceLoggingLevel(WINEVENT_LEVEL_WARNING),
            TraceLoggingPointer(this, "this"),
            TraceLoggingWideString(L"Remote endpoint sent a string which is not valid UTF-8", MIDI_TRACE_EVENT_MESSAGE_FIELD)
        );

        RETURN_IF_FAILED(HRESULT_FROM_WIN32(ERROR_NO_UNICODE_TRANSLATION));
    }

    return S_OK;
}

// Attempt counting and give-up live in the caller, which is the only place that knows the
// sequence numbers involved.
HRESULT
MidiNetworkConnection::RequestMissingPackets()
{
    TraceLoggingWrite(
        MidiNetworkMidiTransportTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this"),
        TraceLoggingWideString(L"Enter", MIDI_TRACE_EVENT_MESSAGE_FIELD)
    );

    // this requests all packets after the last one we received
    auto startingSequenceNumber = m_lastReceivedUmpCommandSequenceNumber + 1;

    RETURN_IF_FAILED(SendToNetwork([&startingSequenceNumber](MidiNetworkDataWriter& writer)
        {
            RETURN_IF_FAILED(writer.WriteCommandRetransmitRequest(startingSequenceNumber, 0));

            return S_OK;
        }));

    TraceLoggingWrite(
        MidiNetworkMidiTransportTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this"),
        TraceLoggingWideString(L"Exit", MIDI_TRACE_EVENT_MESSAGE_FIELD)
    );

    return S_OK;
}

_Use_decl_annotations_
HRESULT
MidiNetworkConnection::ProcessIncomingMessage(
    winrt::Windows::Storage::Streams::DataReader const& reader,
    uint32_t const firstCommandHeaderWord
)
{
    TraceLoggingWrite(
        MidiNetworkMidiTransportTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this"),
        TraceLoggingWideString(L"Enter", MIDI_TRACE_EVENT_MESSAGE_FIELD)
    );

    m_totalNetworkPacketsReceived++;

    // Datagrams for the same remote can be dispatched on more than one thread pool thread, so
    // parsing is serialized here. Sequence tracking and the retransmit buffer are not reentrant.
    auto incomingLock = m_incomingMessageLock.lock();

    // we've received a new message, so reset our disconnect event
    // this also sets the timestamp of the incoming
    LOG_IF_FAILED(SignalHealthyConnectionAndUpdateArrivalTimestamp());

    DatagramReplyState replyState{ };

    try
    {
        uint32_t commandHeaderWord = firstCommandHeaderWord;
        bool haveCommand = true;

        while (haveCommand)
        {
            MidiNetworkCommandPacketHeader commandHeader;
            commandHeader.HeaderWord = commandHeaderWord;

            // Everything below this point is remote-supplied. Validating the declared payload
            // length against what actually arrived is what keeps every handler in bounds.
            uint32_t const payloadLengthInBytes = static_cast<uint32_t>(commandHeader.HeaderData.CommandPayloadLength) * sizeof(uint32_t);

            if (payloadLengthInBytes > reader.UnconsumedBufferLength())
            {
                TraceLoggingWrite(
                    MidiNetworkMidiTransportTelemetryProvider::Provider(),
                    MIDI_TRACE_EVENT_WARNING,
                    TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
                    TraceLoggingLevel(WINEVENT_LEVEL_WARNING),
                    TraceLoggingPointer(this, "this"),
                    TraceLoggingWideString(L"Command declares a payload longer than the datagram. Discarding remainder.", MIDI_TRACE_EVENT_MESSAGE_FIELD),
                    TraceLoggingUInt8(commandHeader.HeaderData.CommandCode, "Command Code"),
                    TraceLoggingUInt32(payloadLengthInBytes, "declared payload bytes"),
                    TraceLoggingUInt32(reader.UnconsumedBufferLength(), "bytes remaining")
                );

                break;
            }

            uint32_t const unconsumedLengthAfterCommand = reader.UnconsumedBufferLength() - payloadLengthInBytes;

            DispatchIncomingCommand(reader, commandHeader, payloadLengthInBytes, replyState);

            if (reader.UnconsumedBufferLength() < unconsumedLengthAfterCommand)
            {
                // a handler read past its own payload. We can no longer locate the next command.
                TraceLoggingWrite(
                    MidiNetworkMidiTransportTelemetryProvider::Provider(),
                    MIDI_TRACE_EVENT_ERROR,
                    TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
                    TraceLoggingLevel(WINEVENT_LEVEL_ERROR),
                    TraceLoggingPointer(this, "this"),
                    TraceLoggingWideString(L"Command handler over-consumed its payload. Discarding remainder of datagram.", MIDI_TRACE_EVENT_MESSAGE_FIELD),
                    TraceLoggingUInt8(commandHeader.HeaderData.CommandCode, "Command Code")
                );

                break;
            }

            // Skip whatever this command's payload holds that the handler didn't consume. Without
            // this, payload bytes of an unhandled command get parsed as the next command header.
            while (reader.UnconsumedBufferLength() > unconsumedLengthAfterCommand)
            {
                auto excess = reader.UnconsumedBufferLength() - unconsumedLengthAfterCommand;

                if (excess >= sizeof(uint32_t))
                {
                    reader.ReadUInt32();
                }
                else if (excess >= sizeof(uint16_t))
                {
                    reader.ReadUInt16();
                }
                else
                {
                    reader.ReadByte();
                }
            }

            if (reader.UnconsumedBufferLength() >= sizeof(uint32_t))
            {
                commandHeaderWord = reader.ReadUInt32();
            }
            else
            {
                haveCommand = false;
            }
        }
    }
    catch (...)
    {
        // a malformed datagram must never take the service down
        auto hr = wil::ResultFromCaughtException();

        TraceLoggingWrite(
            MidiNetworkMidiTransportTelemetryProvider::Provider(),
            MIDI_TRACE_EVENT_ERROR,
            TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
            TraceLoggingLevel(WINEVENT_LEVEL_ERROR),
            TraceLoggingPointer(this, "this"),
            TraceLoggingWideString(L"Exception processing inbound datagram. Datagram discarded.", MIDI_TRACE_EVENT_MESSAGE_FIELD),
            TraceLoggingHResult(hr, MIDI_TRACE_EVENT_HRESULT_FIELD)
        );
    }

    TraceLoggingWrite(
        MidiNetworkMidiTransportTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this"),
        TraceLoggingWideString(L"Exit", MIDI_TRACE_EVENT_MESSAGE_FIELD)
    );

    return S_OK;
}

_Use_decl_annotations_
void
MidiNetworkConnection::DispatchIncomingCommand(
    winrt::Windows::Storage::Streams::DataReader const& reader,
    MidiNetworkCommandPacketHeader const& commandHeader,
    uint32_t const payloadLengthInBytes,
    DatagramReplyState& replyState)
{
    switch (commandHeader.HeaderData.CommandCode)
    {
    case CommandCommon_NAK:
        ReadIncomingNAK(reader, commandHeader, payloadLengthInBytes);
        break;

    case CommandCommon_Ping:
        if (payloadLengthInBytes >= sizeof(uint32_t))
        {
            LOG_IF_FAILED(HandleIncomingPing(reader.ReadUInt32()));
        }
        break;

    case CommandCommon_PingReply:
        if (payloadLengthInBytes >= sizeof(uint32_t))
        {
            LOG_IF_FAILED(HandleIncomingPingReply(reader.ReadUInt32()));
        }
        break;

    case CommandCommon_Bye:
        LOG_IF_FAILED(HandleIncomingBye(static_cast<MidiNetworkCommandByeReason>(commandHeader.HeaderData.CommandSpecificData.AsBytes.Byte1)));
        break;

    case CommandCommon_ByeReply:
        LOG_IF_FAILED(HandleIncomingByeReply());
        break;

    case CommandClientToHost_Invitation:
    {
        std::wstring clientEndpointName{ };
        std::wstring clientProductInstanceId{ };

        if (TryReadIdentityPayload(reader, commandHeader, payloadLengthInBytes, clientEndpointName, clientProductInstanceId))
        {
            auto capabilities = static_cast<MidiNetworkCommandInvitationCapabilities>(commandHeader.HeaderData.CommandSpecificData.AsBytes.Byte2);

            LOG_IF_FAILED(HandleIncomingInvitation(commandHeader, capabilities, clientEndpointName, clientProductInstanceId));
        }
    }
        break;

    case CommandClientToHost_InvitationWithAuthentication:
    case CommandClientToHost_InvitationWithUserAuthentication:
        LOG_IF_FAILED(HandleIncomingInvitationWithAuthentication(commandHeader));
        break;

    case CommandCommon_UmpData:
        ReadIncomingUmpData(reader, commandHeader, replyState);
        break;

    case CommandCommon_RetransmitRequest:
        if (!RefuseCommandOutsideSession(commandHeader, replyState) && payloadLengthInBytes >= sizeof(uint32_t))
        {
            uint16_t sequenceNumber = commandHeader.HeaderData.CommandSpecificData.AsUInt16;
            uint16_t numberOfUmpCommands = reader.ReadUInt16();

            reader.ReadUInt16();    // reserved

            LOG_IF_FAILED(HandleIncomingRetransmitRequest(commandHeader, sequenceNumber, numberOfUmpCommands));
        }
        break;

    case CommandCommon_RetransmitError:
        if (!RefuseCommandOutsideSession(commandHeader, replyState) && payloadLengthInBytes >= sizeof(uint32_t))
        {
            auto reason = static_cast<MidiNetworkCommandRetransmitErrorReason>(commandHeader.HeaderData.CommandSpecificData.AsBytes.Byte1);
            uint16_t earliestAvailableSequenceNumber = reader.ReadUInt16();

            reader.ReadUInt16();    // reserved

            LOG_IF_FAILED(HandleIncomingRetransmitError(reason, earliestAvailableSequenceNumber));
        }
        break;

    case CommandCommon_SessionReset:
        if (!RefuseCommandOutsideSession(commandHeader, replyState))
        {
            LOG_IF_FAILED(HandleIncomingSessionReset());
        }
        break;

    case CommandCommon_SessionResetReply:
        if (!RefuseCommandOutsideSession(commandHeader, replyState))
        {
            LOG_IF_FAILED(HandleIncomingSessionResetReply());
        }
        break;

    case CommandHostToClient_InvitationReplyAccepted:
    {
        std::wstring hostEndpointName{ };
        std::wstring hostProductInstanceId{ };

        if (TryReadIdentityPayload(reader, commandHeader, payloadLengthInBytes, hostEndpointName, hostProductInstanceId))
        {
            LOG_IF_FAILED(HandleIncomingInvitationReplyAccepted(commandHeader, hostEndpointName, hostProductInstanceId));
        }
    }
        break;

    case CommandHostToClient_InvitationReplyPending:
        LOG_IF_FAILED(HandleIncomingInvitationReplyPending());
        break;

    case CommandHostToClient_InvitationReplyAuthenticationRequired:
    case CommandHostToClient_InvitationReplyUserAuthenticationRequired:
        LOG_IF_FAILED(HandleIncomingInvitationReplyAuthenticationRequired(commandHeader));
        break;

    default:
        ReplyCommandNotSupported(commandHeader);
        break;
    }
}

_Use_decl_annotations_
bool
MidiNetworkConnection::TryReadIdentityPayload(
    winrt::Windows::Storage::Streams::DataReader const& reader,
    MidiNetworkCommandPacketHeader const& commandHeader,
    uint32_t const payloadLengthInBytes,
    std::wstring& umpEndpointName,
    std::wstring& productInstanceId)
{
    umpEndpointName.clear();
    productInstanceId.clear();

    uint32_t const endpointNameLengthInBytes = static_cast<uint32_t>(commandHeader.HeaderData.CommandSpecificData.AsBytes.Byte1) * sizeof(uint32_t);

    // The name length is a portion of the payload. If it claims more, the product instance id
    // length would underflow.
    if (endpointNameLengthInBytes > payloadLengthInBytes)
    {
        TraceLoggingWrite(
            MidiNetworkMidiTransportTelemetryProvider::Provider(),
            MIDI_TRACE_EVENT_WARNING,
            TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
            TraceLoggingLevel(WINEVENT_LEVEL_WARNING),
            TraceLoggingPointer(this, "this"),
            TraceLoggingWideString(L"Command declares an endpoint name longer than its payload. Ignoring.", MIDI_TRACE_EVENT_MESSAGE_FIELD),
            TraceLoggingUInt8(commandHeader.HeaderData.CommandCode, "Command Code")
        );

        return false;
    }

    return SUCCEEDED(ReadUtf8String(reader, endpointNameLengthInBytes, umpEndpointName)) &&
        SUCCEEDED(ReadUtf8String(reader, payloadLengthInBytes - endpointNameLengthInBytes, productInstanceId));
}

_Use_decl_annotations_
void
MidiNetworkConnection::ReadIncomingNAK(
    winrt::Windows::Storage::Streams::DataReader const& reader,
    MidiNetworkCommandPacketHeader const& commandHeader,
    uint32_t const payloadLengthInBytes)
{
    // payload is the original command header word, optionally followed by text
    if (payloadLengthInBytes < sizeof(uint32_t))
    {
        return;
    }

    auto reason = static_cast<MidiNetworkCommandNAKReason>(commandHeader.HeaderData.CommandSpecificData.AsBytes.Byte1);

    MidiNetworkCommandPacketHeader originalCommandHeader;
    originalCommandHeader.HeaderWord = reader.ReadUInt32();

    std::wstring text{ };
    LOG_IF_FAILED(ReadUtf8String(reader, payloadLengthInBytes - sizeof(uint32_t), text));

    LOG_IF_FAILED(HandleIncomingNAK(reason, originalCommandHeader, text));
}

_Use_decl_annotations_
bool
MidiNetworkConnection::RefuseCommandOutsideSession(
    MidiNetworkCommandPacketHeader const& commandHeader,
    DatagramReplyState& replyState)
{
    if (m_sessionActive)
    {
        return false;
    }

    // one Bye per datagram, so a peer talking to a dead session can't make us flood it
    if (!replyState.SessionNotEstablishedSent)
    {
        replyState.SessionNotEstablishedSent = true;

        LOG_IF_FAILED(SendByeSessionNotEstablished(commandHeader.HeaderData.CommandCode));
    }

    return true;
}

_Use_decl_annotations_
void
MidiNetworkConnection::ReadIncomingUmpData(
    winrt::Windows::Storage::Streams::DataReader const& reader,
    MidiNetworkCommandPacketHeader const& commandHeader,
    DatagramReplyState& replyState)
{
    // the payload is skipped by the caller's resynchronization
    if (RefuseCommandOutsideSession(commandHeader, replyState))
    {
        return;
    }

    uint8_t numberOfWords = commandHeader.HeaderData.CommandPayloadLength;
    MidiSequenceNumber sequenceNumber(commandHeader.HeaderData.CommandSpecificData.AsUInt16);

    std::vector<uint32_t> words{ };

    auto const readWords = [&]()
        {
            words.reserve(numberOfWords);

            for (uint8_t i = 0; i < numberOfWords; i++)
            {
                words.push_back(reader.ReadUInt32());
            }
        };

    if (sequenceNumber <= m_lastReceivedUmpCommandSequenceNumber)
    {
        // already seen. This is FEC or a retransmit, so the payload is skipped by the caller.
    }
    else if (sequenceNumber == m_lastReceivedUmpCommandSequenceNumber + 1)
    {
        // Process UMP data because this is the next expected sequence number
        // a command with zero words is a valid keep-alive and still advances the sequence

        m_lastReceivedUmpCommandSequenceNumber = sequenceNumber;

        // we're back in sequence, so any gap we were chasing is resolved
        ResetRetransmitRequestState();

        readWords();
    }
    else
    {
        // A gap, which means we lost more datagrams than the forward error correction
        // window covers. We ask for a retransmit a bounded number of times, then accept
        // the loss and carry on. A remote that cannot or will not retransmit must never
        // be able to wedge the session by leaving us stuck on a sequence number.

        auto expectedSequenceNumber = m_lastReceivedUmpCommandSequenceNumber + 1;

        if (!m_retransmitRequestOutstanding || !(m_retransmitRequestSequenceNumber == expectedSequenceNumber))
        {
            // a different gap than the one we were chasing
            m_retransmitRequestOutstanding = true;
            m_retransmitRequestSequenceNumber = expectedSequenceNumber;
            m_retransmitRequestAttempts = 0;
        }

        bool waitForRetransmit{ false };

        if (m_remoteSupportsRetransmit && m_retransmitRequestAttempts < MIDI_NETWORK_MAX_RETRANSMIT_REQUEST_ATTEMPTS)
        {
            if (replyState.RetransmitRequested)
            {
                // already asked once for this datagram. Wait for the answer.
                waitForRetransmit = true;
            }
            else
            {
                m_retransmitRequestAttempts++;
                replyState.RetransmitRequested = true;

                waitForRetransmit = SUCCEEDED(RequestMissingPackets());
            }
        }

        if (!waitForRetransmit)
        {
            TraceLoggingWrite(
                MidiNetworkMidiTransportTelemetryProvider::Provider(),
                MIDI_TRACE_EVENT_WARNING,
                TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
                TraceLoggingLevel(WINEVENT_LEVEL_WARNING),
                TraceLoggingPointer(this, "this"),
                TraceLoggingWideString(L"Giving up on missing UMP data and resynchronizing to the current sequence number", MIDI_TRACE_EVENT_MESSAGE_FIELD),
                TraceLoggingUInt16(expectedSequenceNumber.Value(), "expected sequence number"),
                TraceLoggingUInt16(sequenceNumber.Value(), "received sequence number"),
                TraceLoggingBoolean(m_remoteSupportsRetransmit, "remote supports retransmit")
            );

            m_lastReceivedUmpCommandSequenceNumber = sequenceNumber;

            ResetRetransmitRequestState();

            readWords();
        }
    }

    if (words.size() > 0)
    {
        LOG_IF_FAILED(HandleIncomingUmpData(m_lastIncomingValidUdpPacketTimestamp, words));
    }
}

_Use_decl_annotations_
void
MidiNetworkConnection::ReplyCommandNotSupported(
    MidiNetworkCommandPacketHeader const& commandHeader)
{
    TraceLoggingWrite(
        MidiNetworkMidiTransportTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_WARNING,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_WARNING),
        TraceLoggingPointer(this, "this"),
        TraceLoggingWideString(L"Unexpected network MIDI 2.0 command code", MIDI_TRACE_EVENT_MESSAGE_FIELD),
        TraceLoggingUInt8(commandHeader.HeaderData.CommandCode, "Command Code")
    );

    // Only answered inside an established session, so unsolicited junk from an arbitrary source
    // doesn't get a reply.
    if (!m_sessionActive)
    {
        return;
    }

    LOG_IF_FAILED(SendToNetwork([&commandHeader](MidiNetworkDataWriter& writer)
        {
            RETURN_IF_FAILED(writer.WriteCommandNAK(
                commandHeader.HeaderWord,
                MidiNetworkCommandNAKReason::CommandNAKReason_CommandNotSupported,
                internal::ResourceGetWString(IDS_MESSAGE_COMMAND_NOT_SUPPORTED)));

            return S_OK;
        }));
}

_Use_decl_annotations_
HRESULT
MidiNetworkConnection::AddUmpPacketToRetransmitBuffer(
    MidiSequenceNumber const sequenceNumber,
    uint32_t const* words,
    size_t const wordCount)
{
    if (m_retransmitBuffer.capacity() == 0)
    {
        return S_OK;
    }

    try
    {
        MidiRetransmitBufferEntry entry;
        entry.SequenceNumber = sequenceNumber;

        if (wordCount > 0 && words != nullptr)
        {
            entry.Words.assign(words, words + wordCount);

            // what a resend of it costs against the speed limit
            for (size_t index = 0; index < wordCount; )
            {
                size_t const messageWordCount = internal::GetUmpLengthInMidiWordsFromFirstWord(words[index]);

                if (messageWordCount == 0 || index + messageWordCount > wordCount)
                {
                    break;
                }

                entry.WireByteCount += ::WindowsMidiServicesInternal::EstimateMidi1WireByteCount(words[index], static_cast<uint32_t>(messageWordCount));
                index += messageWordCount;
            }
        }

        // The oldest go first, whether the buffer is out of entries or out of bytes
        while (!m_retransmitBuffer.empty() &&
            (m_retransmitBuffer.full() ||
             (m_retransmitBufferWordCount + entry.Words.size()) * sizeof(uint32_t) > MIDI_NETWORK_RETRANSMIT_BUFFER_MAX_BYTES))
        {
            m_retransmitBufferWordCount -= m_retransmitBuffer.front().Words.size();
            m_retransmitBuffer.pop_front();
        }

        m_retransmitBufferWordCount += entry.Words.size();
        m_retransmitBuffer.push_back(std::move(entry));
    }
    CATCH_RETURN();

    return S_OK;
}

_Use_decl_annotations_
size_t
MidiNetworkConnection::CalculateWholeUmpMessageWordCount(
    std::vector<uint32_t> const& words,
    size_t const position,
    size_t const maxWords)
{
    size_t count{ 0 };

    while (position + count < words.size())
    {
        auto messageWordCount = internal::GetUmpLengthInMidiWordsFromFirstWord(words[position + count]);

        if (messageWordCount == 0)
        {
            break;
        }

        // never split a UMP message across two commands
        if (position + count + messageWordCount > words.size())
        {
            break;
        }

        if (count + messageWordCount > maxWords)
        {
            break;
        }

        count += messageWordCount;
    }

    return count;
}

_Use_decl_annotations_
HRESULT
MidiNetworkConnection::HandleIncomingRetransmitRequest(
    MidiNetworkCommandPacketHeader const& header,
    uint16_t const startingSequenceNumber, 
    uint16_t const retransmitPacketCount)
{
    TraceLoggingWrite(
        MidiNetworkMidiTransportTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this"),
        TraceLoggingWideString(L"Enter", MIDI_TRACE_EVENT_MESSAGE_FIELD)
    );

    // Counted before any of the ways this can be refused, so the measure says how often the
    // remote had to ask, not how often we were able to answer.
    m_retransmitRequestCount++;

    // Spec 7.2.3: if we don't implement retransmit at all, the answer is a NAK rather than a
    // retransmit error, and the remote is expected to stop asking.
    if (m_retransmitBufferCapacity == 0)
    {
        RETURN_IF_FAILED(SendToNetwork([&header](MidiNetworkDataWriter& writer)
            {
                RETURN_IF_FAILED(writer.WriteCommandNAK(
                    header.HeaderWord,
                    MidiNetworkCommandNAKReason::CommandNAKReason_CommandNotSupported,
                    internal::ResourceGetWString(IDS_MESSAGE_RETRANSMIT_DISABLED)));

                return S_OK;
            }));

        return S_OK;
    }

    bool queued{ false };

    {
        auto lock = m_resendRequestsLock.lock();

        // Spec 7.2.3: a request repeated before it has been served may be ignored
        auto const alreadyWaiting = std::any_of(m_resendRequests.begin(), m_resendRequests.end(),
            [startingSequenceNumber](ResendRequest const& request) { return request.StartingSequenceNumber == MidiSequenceNumber(startingSequenceNumber); });

        if (!alreadyWaiting && m_resendRequests.size() < MIDI_NETWORK_MAX_PENDING_RETRANSMIT_REQUESTS)
        {
            try
            {
                m_resendRequests.push_back({ MidiSequenceNumber(startingSequenceNumber), retransmitPacketCount });
                queued = true;
            }
            CATCH_LOG();
        }
    }

    if (queued)
    {
        // A repeat of a request which is still waiting says nothing new about loss
        NoteRemoteLoss();

        // Served by the send thread, ahead of new messages. Answering here would wait on whatever
        // it is sending, and every remote of a host shares this receive thread.
        WakeSendThread();
    }

    TraceLoggingWrite(
        MidiNetworkMidiTransportTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this"),
        TraceLoggingWideString(L"Exit", MIDI_TRACE_EVENT_MESSAGE_FIELD)
    );

    return S_OK;
}


HRESULT
MidiNetworkConnection::SendPing()
{
    TraceLoggingWrite(
        MidiNetworkMidiTransportTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this"),
        TraceLoggingWideString(L"Enter", MIDI_TRACE_EVENT_MESSAGE_FIELD)
    );

    MidiOutgoingPingTrackingEntry pingInfo;

    pingInfo.PingSendTimestamp = internal::GetCurrentMidiTimestamp();
    pingInfo.PingId = (uint32_t)(pingInfo.PingSendTimestamp & 0xFFFFFFFF);

    {
        auto lock = m_pingTrackingLock.lock();
        m_outgoingPingTracking.push_back(pingInfo);
    }

    RETURN_IF_FAILED(SendToNetwork([&pingInfo](MidiNetworkDataWriter& writer)
        {
            RETURN_IF_FAILED(writer.WriteCommandPing(pingInfo.PingId));

            return S_OK;
        }));

    TraceLoggingWrite(
        MidiNetworkMidiTransportTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this"),
        TraceLoggingWideString(L"Exit", MIDI_TRACE_EVENT_MESSAGE_FIELD)
    );

    return S_OK;
}







_Use_decl_annotations_
HRESULT
MidiNetworkConnection::OutboundProcessingThreadWorker(std::stop_token stopToken)
{
    TraceLoggingWrite(
        MidiNetworkMidiTransportTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this"),
        TraceLoggingWideString(L"Enter", MIDI_TRACE_EVENT_MESSAGE_FIELD),
        TraceLoggingBoolean(m_sessionActive, "Session active")
    );

    // Spec 7.2.1: with nothing to send, a zero length UMP Data command goes out at growing
    // intervals, so the remote can tell the last of a burst was not lost
    uint64_t lastUmpDataTime{ GetTickCount64() };
    DWORD keepAliveIntervalMilliseconds{ m_outgoingUmpEmptyPacketStartingIntervalMilliseconds };

    HANDLE const waitHandles[]{ m_sendWakeEvent.get(), m_sendPaceTimer.get() };

    while (!m_shuttingDown && !stopToken.stop_requested())
    {
        auto const sinceLastUmpData = GetTickCount64() - lastUmpDataTime;
        DWORD const timeout = (sinceLastUmpData >= keepAliveIntervalMilliseconds) ? 0 : static_cast<DWORD>(keepAliveIntervalMilliseconds - sinceLastUmpData);

        auto const waitResult = WaitForMultipleObjects(ARRAYSIZE(waitHandles), waitHandles, FALSE, timeout);

        if (waitResult == WAIT_FAILED)
        {
            // never spin on a broken handle
            LOG_LAST_ERROR();
            break;
        }

        if (m_shuttingDown || stopToken.stop_requested())
        {
            break;
        }

        // we only send messages if there's an active session
        if (!m_sessionActive)
        {
            lastUmpDataTime = GetTickCount64();
            keepAliveIntervalMilliseconds = m_outgoingUmpEmptyPacketStartingIntervalMilliseconds;

            continue;
        }

        uint64_t waitTicks{ 0 };
        bool sentData{ false };

        LOG_IF_FAILED(SendWhatIsAllowed(waitTicks, sentData));

        // the speed limit lets more go later
        if (waitTicks > 0)
        {
            ArmSendPaceTimer(waitTicks);
        }

        if (sentData)
        {
            lastUmpDataTime = GetTickCount64();
            keepAliveIntervalMilliseconds = m_outgoingUmpEmptyPacketStartingIntervalMilliseconds;
        }
        else if (waitTicks == 0 && GetTickCount64() - lastUmpDataTime >= keepAliveIntervalMilliseconds)
        {
            LOG_IF_FAILED(SendKeepAlive());

            lastUmpDataTime = GetTickCount64();
            keepAliveIntervalMilliseconds = (std::min)(keepAliveIntervalMilliseconds + 200, static_cast<DWORD>(m_outgoingUmpEmptyPacketMaxIntervalMilliseconds));
        }
    }

    TraceLoggingWrite(
        MidiNetworkMidiTransportTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this"),
        TraceLoggingWideString(L"Exit", MIDI_TRACE_EVENT_MESSAGE_FIELD),
        TraceLoggingBoolean(m_sessionActive, "Session active")
    );

    return S_OK;
}




_Use_decl_annotations_
std::vector<size_t>
MidiNetworkConnection::ChooseForwardErrorCorrectionPackets(size_t& budgetBytes)
{
    constexpr size_t commandHeaderBytes{ sizeof(uint32_t) };

    // Forward error correction repeats the most recent packets, so when the budget is tight
    // we keep the newest and drop the oldest.
    std::vector<size_t> indexes;

    size_t maxCount = min(m_retransmitBuffer.size(), static_cast<size_t>(m_maxForwardErrorCorrectionCommandPacketCount));

    for (size_t i = 0; i < maxCount; i++)
    {
        size_t index = m_retransmitBuffer.size() - 1 - i;
        size_t cost = commandHeaderBytes + (m_retransmitBuffer.at(index).Words.size() * sizeof(uint32_t));

        if (cost > budgetBytes)
        {
            break;
        }

        budgetBytes -= cost;
        indexes.push_back(index);
    }

    // the receiver processes in sequence order, so write oldest first
    std::reverse(indexes.begin(), indexes.end());

    return indexes;
}

void
MidiNetworkConnection::WakeSendThread() noexcept
{
    if (m_sendWakeEvent.is_valid())
    {
        m_sendWakeEvent.SetEvent();
    }
}

_Use_decl_annotations_
void
MidiNetworkConnection::ArmSendPaceTimer(uint64_t const ticks) noexcept
{
    auto const frequency = internal::GetMidiTimestampFrequency();

    if (!m_sendPaceTimer || frequency == 0)
    {
        return;
    }

    // relative, in 100 nanosecond units, and never zero
    LARGE_INTEGER dueTime{};
    dueTime.QuadPart = -static_cast<LONGLONG>((std::max)((ticks * 10'000'000ull) / frequency, 1ull));

    // If this fails, the keep-alive interval still wakes the thread, only later
    LOG_IF_WIN32_BOOL_FALSE(SetWaitableTimer(m_sendPaceTimer.get(), &dueTime, 0, nullptr, nullptr, FALSE));
}

_Use_decl_annotations_
void
MidiNetworkConnection::SetSendSpeedLimit(uint32_t const speedMultiple, bool const reduceAutomatically) noexcept
{
    auto const multiple = ::WindowsMidiServicesInternal::ClampMidiSendSpeedMultiple(speedMultiple);

    {
        auto lock = m_sendSpeedLock.lock();

        // The same settings again would throw away what automatic reduction has learned
        if (multiple == m_sendSpeedLimit && reduceAutomatically == m_reduceSendSpeedAutomatically)
        {
            return;
        }

        m_sendSpeedLimit = multiple;
        m_reduceSendSpeedAutomatically = reduceAutomatically;

        m_automaticSendSpeed.Configure(multiple, reduceAutomatically);
        m_currentSendSpeed = m_automaticSendSpeed.CurrentMultiple();
    }

    TraceLoggingWrite(
        MidiNetworkMidiTransportTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this"),
        TraceLoggingWideString(L"Send speed limit changed", MIDI_TRACE_EVENT_MESSAGE_FIELD),
        TraceLoggingUInt32(multiple, "multiple of MIDI 1.0 wire speed, 0 for no limit"),
        TraceLoggingBoolean(reduceAutomatically, "reduce automatically")
    );

    // Senders waiting for room may have more of it now
    {
        auto queueLock = m_outgoingUmpMessageQueueLock.lock();
        m_outgoingQueueGeneration = m_outgoingQueueGeneration + 1;
    }

    WakeSendersWaitingForRoom();
    WakeSendThread();
}

void
MidiNetworkConnection::NoteRemoteLoss() noexcept
{
    bool changed{ false };
    uint32_t current{ 0 };

    {
        auto lock = m_sendSpeedLock.lock();

        changed = m_automaticSendSpeed.OnLoss(internal::GetCurrentMidiTimestamp(), internal::GetMidiTimestampFrequency());
        current = m_automaticSendSpeed.CurrentMultiple();

        if (changed)
        {
            m_currentSendSpeed = current;
        }
    }

    if (changed)
    {
        TraceLoggingWrite(
            MidiNetworkMidiTransportTelemetryProvider::Provider(),
            MIDI_TRACE_EVENT_INFO,
            TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
            TraceLoggingLevel(WINEVENT_LEVEL_INFO),
            TraceLoggingPointer(this, "this"),
            TraceLoggingWideString(L"The remote is losing data. Sending more slowly.", MIDI_TRACE_EVENT_MESSAGE_FIELD),
            TraceLoggingUInt32(current, "multiple of MIDI 1.0 wire speed")
        );

        WakeSendThread();
    }
}

_Use_decl_annotations_
void
MidiNetworkConnection::UpdateSendPacer(uint64_t const now)
{
    bool raised{ false };
    uint32_t current{ 0 };

    {
        auto lock = m_sendSpeedLock.lock();

        raised = m_automaticSendSpeed.OnTick(now, internal::GetMidiTimestampFrequency());
        current = m_automaticSendSpeed.CurrentMultiple();

        if (raised)
        {
            m_currentSendSpeed = current;
        }
    }

    if (raised)
    {
        TraceLoggingWrite(
            MidiNetworkMidiTransportTelemetryProvider::Provider(),
            MIDI_TRACE_EVENT_INFO,
            TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
            TraceLoggingLevel(WINEVENT_LEVEL_INFO),
            TraceLoggingPointer(this, "this"),
            TraceLoggingWideString(L"No lost data for a while. Sending faster again.", MIDI_TRACE_EVENT_MESSAGE_FIELD),
            TraceLoggingUInt32(current, "multiple of MIDI 1.0 wire speed, 0 for no limit")
        );

        // senders waiting for room may have more of it now
        WakeSendersWaitingForRoom();
    }

    if (current != m_sendPacerMultiple)
    {
        m_sendPacer.Configure(current, internal::GetMidiTimestampFrequency());
        m_sendPacerMultiple = current;
    }
}

void
MidiNetworkConnection::ClearResendRequests() noexcept
{
    auto lock = m_resendRequestsLock.lock();

    m_resendRequests.clear();
}

bool
MidiNetworkConnection::OutgoingQueueHasRoom() const noexcept
{
    auto const queuedWords = m_outgoingUmpMessages.size() - m_outgoingReadIndex;

    if (queuedWords >= MIDI_NETWORK_SEND_QUEUE_UNLIMITED_MAX_WORDS)
    {
        return false;
    }

    auto const multiple = m_currentSendSpeed.load();

    if (multiple == 0)
    {
        return true;
    }

    // About the same time at any speed
    auto const pacedLimit = (std::max)(
        (static_cast<uint64_t>(multiple) * ::WindowsMidiServicesInternal::MidiWireSpeedBytesPerSecond * MIDI_NETWORK_SEND_QUEUE_PACED_MILLISECONDS) / 1000,
        static_cast<uint64_t>(MIDI_NETWORK_SEND_QUEUE_PACED_MINIMUM_WIRE_BYTES));

    return m_outgoingQueuedWireBytes < pacedLimit;
}

void
MidiNetworkConnection::ClearOutgoingQueue() noexcept
{
    m_outgoingUmpMessages.clear();
    m_outgoingReadIndex = 0;
    m_outgoingQueuedWireBytes = 0;
    m_outgoingQueueGeneration = m_outgoingQueueGeneration + 1;
}

void
MidiNetworkConnection::WakeSendersWaitingForRoom() noexcept
{
    WakeByAddressAll(const_cast<ULONG*>(&m_outgoingQueueGeneration));
}

_Use_decl_annotations_
HRESULT
MidiNetworkConnection::SendRetransmitErrorIfAllowed(MidiSequenceNumber const requestedSequenceNumber, uint64_t const now)
{
    auto const frequency = internal::GetMidiTimestampFrequency();

    // A remote asking again and again for data which is gone does not get an answer every time
    if (m_lastRetransmitErrorTimestamp != 0 &&
        m_lastRetransmitErrorSequenceNumber == requestedSequenceNumber &&
        now - m_lastRetransmitErrorTimestamp < (frequency * MIDI_NETWORK_RETRANSMIT_ERROR_REPEAT_MILLISECONDS) / 1000)
    {
        return S_FALSE;
    }

    if (now - m_retransmitErrorWindowStart >= frequency)
    {
        m_retransmitErrorWindowStart = now;
        m_retransmitErrorsInWindow = 0;
    }

    if (m_retransmitErrorsInWindow >= MIDI_NETWORK_RETRANSMIT_ERROR_MAX_PER_SECOND)
    {
        return S_FALSE;
    }

    m_retransmitErrorsInWindow++;
    m_lastRetransmitErrorSequenceNumber = requestedSequenceNumber;
    m_lastRetransmitErrorTimestamp = now;

    // Spec 7.2.4: the error carries the first sequence number still held
    MidiSequenceNumber earliestAvailable{ 0 };

    {
        auto lock = m_socketWriterLock.lock();

        if (!m_retransmitBuffer.empty())
        {
            earliestAvailable = m_retransmitBuffer.front().SequenceNumber;
        }
    }

    RETURN_IF_FAILED(SendToNetwork([&earliestAvailable](MidiNetworkDataWriter& writer)
        {
            RETURN_IF_FAILED(writer.WriteCommandRetransmitError(earliestAvailable, MidiNetworkCommandRetransmitErrorReason::RetransmitErrorReason_DataNotAvailable));

            return S_OK;
        }));

    return S_OK;
}

_Use_decl_annotations_
HRESULT
MidiNetworkConnection::SendWhatIsAllowed(uint64_t& waitTicks, bool& sentData)
{
    waitTicks = 0;
    sentData = false;

    try
    {
        auto now = internal::GetCurrentMidiTimestamp();

        UpdateSendPacer(now);

        while (!m_shuttingDown && m_sessionActive)
        {
            // The remote cannot pass on anything after a gap until the gap is filled, so resends
            // go ahead of new messages
            LOG_IF_FAILED(SendRequestedRetransmits(now, waitTicks, sentData));

            if (waitTicks > 0)
            {
                break;
            }

            // S_FALSE when it stopped to let a resend request go first
            auto const hr = SendQueuedMessages(now, waitTicks, sentData);
            LOG_IF_FAILED(hr);

            if (hr != S_FALSE)
            {
                break;
            }
        }
    }
    CATCH_RETURN();

    return S_OK;
}

_Use_decl_annotations_
HRESULT
MidiNetworkConnection::SendRequestedRetransmits(uint64_t& now, uint64_t& waitTicks, bool& sentData)
{
    waitTicks = 0;

    while (!m_shuttingDown && m_sessionActive)
    {
        ResendRequest request{};

        {
            auto lock = m_resendRequestsLock.lock();

            if (m_resendRequests.empty())
            {
                break;
            }

            request = m_resendRequests.front();
        }

        bool unavailable{ false };
        bool finished{ false };
        size_t sentCount{ 0 };
        MidiSequenceNumber nextStart{ request.StartingSequenceNumber };

        {
            // the retransmit buffer is guarded by the socket writer lock
            auto lock = m_socketWriterLock.lock();

            auto first = std::find_if(m_retransmitBuffer.begin(), m_retransmitBuffer.end(),
                [&request](MidiRetransmitBufferEntry const& entry) { return entry.SequenceNumber == request.StartingSequenceNumber; });

            if (first == m_retransmitBuffer.end())
            {
                unavailable = true;
            }
            else
            {
                // A count larger than what we hold, or the "send everything" value of zero, is
                // clamped to what is actually in the buffer
                size_t const available = static_cast<size_t>(std::distance(first, m_retransmitBuffer.end()));
                size_t const wanted = (request.CommandCount == 0) ? available : (std::min)(static_cast<size_t>(request.CommandCount), available);

                // One datagram at a time, and only as much as the speed limit allows. DontFragment
                // is set, so an oversized datagram would simply be dropped.
                size_t budgetBytes{ MIDI_NETWORK_MAX_UDP_PAYLOAD_BYTES - sizeof(uint32_t) };
                size_t count{ 0 };

                for (auto probe = first; count < wanted && probe != m_retransmitBuffer.end(); probe++)
                {
                    size_t const cost = sizeof(uint32_t) + (probe->Words.size() * sizeof(uint32_t));

                    if (cost > budgetBytes)
                    {
                        break;
                    }

                    // one command at a time, charged as it is taken
                    auto const allowedIn = m_sendPacer.TicksUntilAllowed(probe->WireByteCount, now);

                    if (allowedIn > 0)
                    {
                        if (count == 0)
                        {
                            waitTicks = allowedIn;
                        }

                        break;
                    }

                    m_sendPacer.Charge(probe->WireByteCount, now);

                    budgetBytes -= cost;
                    count++;
                }

                if (count > 0)
                {
                    LOG_IF_FAILED(SendToNetwork([&first, count](MidiNetworkDataWriter& writer)
                        {
                            auto writeIterator = first;

                            for (size_t i = 0; i < count; i++, writeIterator++)
                            {
                                RETURN_IF_FAILED(writer.WriteCommandUmpMessages(writeIterator->SequenceNumber, writeIterator->Words.data(), static_cast<uint8_t>(writeIterator->Words.size())));
                            }

                            return S_OK;
                        }));

                    m_retransmitCount += static_cast<uint32_t>(count);
                    sentData = true;

                    auto last = first;
                    std::advance(last, count - 1);

                    sentCount = count;
                    nextStart = last->SequenceNumber + 1;
                    finished = (count >= wanted);
                }
                else if (waitTicks == 0)
                {
                    // Nothing could go and nothing to wait for. Cannot happen, but the request
                    // must never sit at the front for good.
                    finished = true;
                }
            }
        }

        if (unavailable)
        {
            LOG_IF_FAILED(SendRetransmitErrorIfAllowed(request.StartingSequenceNumber, now));
            finished = true;
        }

        if (!finished && sentCount == 0)
        {
            // the speed limit holds it, still ahead of new messages
            break;
        }

        {
            auto lock = m_resendRequestsLock.lock();

            // a session reset may have emptied the list meanwhile
            if (!m_resendRequests.empty() && m_resendRequests.front().StartingSequenceNumber == request.StartingSequenceNumber)
            {
                if (finished)
                {
                    m_resendRequests.pop_front();
                }
                else
                {
                    auto& front = m_resendRequests.front();

                    front.StartingSequenceNumber = nextStart;

                    if (request.CommandCount != 0)
                    {
                        front.CommandCount = static_cast<uint16_t>(request.CommandCount - sentCount);
                    }
                }
            }
        }

        now = internal::GetCurrentMidiTimestamp();
    }

    return S_OK;
}

_Use_decl_annotations_
HRESULT
MidiNetworkConnection::SendQueuedMessages(uint64_t& now, uint64_t& waitTicks, bool& sentData)
{
    waitTicks = 0;

    constexpr size_t commandHeaderBytes{ sizeof(uint32_t) };
    constexpr size_t datagramBudgetBytes{ MIDI_NETWORK_MAX_UDP_PAYLOAD_BYTES - sizeof(uint32_t) };   // less the UDP packet header

    // Forward error correction copies never take the room the largest UMP message needs, or a
    // high correction setting could leave no room for new messages at all
    constexpr size_t correctionBudgetBytes{ datagramBudgetBytes - (commandHeaderBytes + (4 * sizeof(uint32_t))) };

    // Taken words are removed from the front of the queue in bulk, not a datagram at a time
    constexpr size_t compactAfterWords{ 16 * 1024 };

    while (!m_shuttingDown && m_sessionActive)
    {
        // Room the forward error correction copies take. Only this thread adds to the
        // retransmit buffer, so they can only take less when the datagram is written.
        size_t forwardErrorCorrectionBytes{ 0 };

        {
            auto lock = m_socketWriterLock.lock();

            size_t remaining{ correctionBudgetBytes };
            ChooseForwardErrorCorrectionPackets(remaining);
            forwardErrorCorrectionBytes = correctionBudgetBytes - remaining;
        }

        size_t const budgetBytes{ datagramBudgetBytes - forwardErrorCorrectionBytes };

        std::vector<uint32_t> words{};
        uint64_t wireBytes{ 0 };
        bool tookFromQueue{ false };

        {
            auto queueLock = m_outgoingUmpMessageQueueLock.lock();

            size_t const start{ m_outgoingReadIndex };
            size_t position{ start };
            size_t datagramBytes{ 0 };
            size_t wordsInCommand{ 0 };
            bool discardRest{ false };

            while (position < m_outgoingUmpMessages.size())
            {
                size_t const messageWordCount = internal::GetUmpLengthInMidiWordsFromFirstWord(m_outgoingUmpMessages[position]);

                if (messageWordCount == 0 || position + messageWordCount > m_outgoingUmpMessages.size())
                {
                    // a partial message can never be sent, and would hold up everything behind it
                    TraceLoggingWrite(
                        MidiNetworkMidiTransportTelemetryProvider::Provider(),
                        MIDI_TRACE_EVENT_WARNING,
                        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
                        TraceLoggingLevel(WINEVENT_LEVEL_WARNING),
                        TraceLoggingPointer(this, "this"),
                        TraceLoggingWideString(L"Incomplete UMP message at the end of the outbound queue. Discarding it.", MIDI_TRACE_EVENT_MESSAGE_FIELD),
                        TraceLoggingUInt64(static_cast<uint64_t>(m_outgoingUmpMessages.size() - position), "words discarded")
                    );

                    discardRest = true;
                    break;
                }

                // Whole messages only, up to 64 words to a command, each command with its own header
                bool const startsCommand = (wordsInCommand == 0 || wordsInCommand + messageWordCount > MIDI_MAX_UMP_WORDS_PER_PACKET);
                size_t const addedBytes = (startsCommand ? commandHeaderBytes : 0) + (messageWordCount * sizeof(uint32_t));

                if (datagramBytes + addedBytes > budgetBytes)
                {
                    break;
                }

                auto const cost = ::WindowsMidiServicesInternal::EstimateMidi1WireByteCount(m_outgoingUmpMessages[position], static_cast<uint32_t>(messageWordCount));

                // one message at a time, charged as it is taken
                auto const allowedIn = m_sendPacer.TicksUntilAllowed(cost, now);

                if (allowedIn > 0)
                {
                    if (position == start)
                    {
                        waitTicks = allowedIn;
                    }

                    break;
                }

                m_sendPacer.Charge(cost, now);

                datagramBytes += addedBytes;
                wordsInCommand = startsCommand ? messageWordCount : wordsInCommand + messageWordCount;
                wireBytes += cost;
                position += messageWordCount;
            }

            if (position > start)
            {
                words.assign(m_outgoingUmpMessages.begin() + start, m_outgoingUmpMessages.begin() + position);
            }

            m_outgoingReadIndex = discardRest ? m_outgoingUmpMessages.size() : position;
            m_outgoingQueuedWireBytes = (m_outgoingQueuedWireBytes > wireBytes) ? m_outgoingQueuedWireBytes - wireBytes : 0;

            if (m_outgoingReadIndex >= m_outgoingUmpMessages.size())
            {
                m_outgoingUmpMessages.clear();
                m_outgoingReadIndex = 0;
                m_outgoingQueuedWireBytes = 0;
            }
            else if (m_outgoingReadIndex >= compactAfterWords && m_outgoingReadIndex * 2 >= m_outgoingUmpMessages.size())
            {
                m_outgoingUmpMessages.erase(m_outgoingUmpMessages.begin(), m_outgoingUmpMessages.begin() + m_outgoingReadIndex);
                m_outgoingReadIndex = 0;
            }

            tookFromQueue = (position > start) || discardRest;

            if (tookFromQueue)
            {
                m_outgoingQueueGeneration = m_outgoingQueueGeneration + 1;
            }
        }

        if (tookFromQueue)
        {
            WakeSendersWaitingForRoom();
        }

        if (words.empty())
        {
            // empty, or the speed limit holds the rest
            break;
        }

        // Once taken, the messages are gone whether or not this works. Holding on to them would
        // let an unreachable remote grow the queue without bound.
        LOG_IF_FAILED(SendUmpDataCommands(words));

        sentData = true;

        now = internal::GetCurrentMidiTimestamp();

        // a resend request which came in meanwhile goes first
        {
            auto lock = m_resendRequestsLock.lock();

            if (!m_resendRequests.empty())
            {
                return S_FALSE;
            }
        }
    }

    return S_OK;
}

_Use_decl_annotations_
HRESULT
MidiNetworkConnection::SendUmpDataCommands(std::vector<uint32_t> const& words)
{
    auto lock = m_socketWriterLock.lock();

    std::vector<OutboundUmpChunk> chunks{};
    auto sequenceNumber = m_lastSentUmpCommandSequenceNumber;
    size_t chunkBytes{ 0 };

    for (size_t position = 0; position < words.size(); )
    {
        auto const wordCount = CalculateWholeUmpMessageWordCount(words, position, MIDI_MAX_UMP_WORDS_PER_PACKET);

        if (wordCount == 0)
        {
            // only whole messages are taken from the queue
            break;
        }

        sequenceNumber = sequenceNumber + 1;
        chunks.push_back({ position, wordCount, sequenceNumber });

        chunkBytes += sizeof(uint32_t) + (wordCount * sizeof(uint32_t));
        position += wordCount;
    }

    if (chunks.empty())
    {
        return S_FALSE;
    }

    size_t budgetBytes{ MIDI_NETWORK_MAX_UDP_PAYLOAD_BYTES - sizeof(uint32_t) };   // less the UDP packet header
    budgetBytes = (budgetBytes > chunkBytes) ? budgetBytes - chunkBytes : 0;

    auto const forwardErrorCorrectionIndexes = ChooseForwardErrorCorrectionPackets(budgetBytes);

    auto const hr = SendToNetwork([&](MidiNetworkDataWriter& writer)
        {
            for (auto const& index : forwardErrorCorrectionIndexes)
            {
                auto const& entry = m_retransmitBuffer.at(index);

                RETURN_IF_FAILED(writer.WriteCommandUmpMessages(entry.SequenceNumber, entry.Words.data(), static_cast<uint8_t>(entry.Words.size())));
            }

            for (auto const& chunk : chunks)
            {
                RETURN_IF_FAILED(writer.WriteCommandUmpMessages(chunk.SequenceNumber, words.data() + chunk.Offset, static_cast<uint8_t>(chunk.WordCount)));
            }

            return S_OK;
        });

    if (hr == S_OK)
    {
        // only committed once the datagram is actually on the wire
        for (auto const& chunk : chunks)
        {
            m_lastSentUmpCommandSequenceNumber = chunk.SequenceNumber;

            LOG_IF_FAILED(AddUmpPacketToRetransmitBuffer(chunk.SequenceNumber, words.data() + chunk.Offset, chunk.WordCount));
        }
    }

    return hr;
}

HRESULT
MidiNetworkConnection::SendKeepAlive()
{
    auto lock = m_socketWriterLock.lock();

    // called straight from the send thread, so nothing may escape
    std::vector<size_t> forwardErrorCorrectionIndexes{};

    try
    {
        size_t budgetBytes{ MIDI_NETWORK_MAX_UDP_PAYLOAD_BYTES - sizeof(uint32_t) - sizeof(uint32_t) };   // less the UDP packet header and this command's header
        forwardErrorCorrectionIndexes = ChooseForwardErrorCorrectionPackets(budgetBytes);
    }
    CATCH_RETURN();

    // a UMP Data command with no words, which still advances the sequence
    auto const sequenceNumber = m_lastSentUmpCommandSequenceNumber + 1;

    auto const hr = SendToNetwork([&](MidiNetworkDataWriter& writer)
        {
            for (auto const& index : forwardErrorCorrectionIndexes)
            {
                auto const& entry = m_retransmitBuffer.at(index);

                RETURN_IF_FAILED(writer.WriteCommandUmpMessages(entry.SequenceNumber, entry.Words.data(), static_cast<uint8_t>(entry.Words.size())));
            }

            RETURN_IF_FAILED(writer.WriteCommandUmpMessages(sequenceNumber, nullptr, 0));

            return S_OK;
        });

    if (hr == S_OK)
    {
        m_lastSentUmpCommandSequenceNumber = sequenceNumber;

        LOG_IF_FAILED(AddUmpPacketToRetransmitBuffer(sequenceNumber, nullptr, 0));
    }

    return hr;
}



_Use_decl_annotations_
HRESULT
MidiNetworkConnection::QueueMidiMessagesToSendToNetwork(
    std::vector<uint32_t> const& words)
{
    TraceLoggingWrite(
        MidiNetworkMidiTransportTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this"),
        TraceLoggingWideString(L"Enter", MIDI_TRACE_EVENT_MESSAGE_FIELD),
        TraceLoggingUInt32(static_cast<uint32_t>(words.size()), "Word count")
    );

    // what these take on a MIDI 1.0 cable, for the speed limit
    uint64_t wireBytes{ 0 };

    for (size_t index = 0; index < words.size(); )
    {
        size_t const messageWordCount = internal::GetUmpLengthInMidiWordsFromFirstWord(words[index]);

        if (messageWordCount == 0 || index + messageWordCount > words.size())
        {
            // the send thread discards it
            break;
        }

        wireBytes += ::WindowsMidiServicesInternal::EstimateMidi1WireByteCount(words[index], static_cast<uint32_t>(messageWordCount));
        index += messageWordCount;
    }

    // While the queue is over its limit, the sender waits for room, so an app sending faster than
    // the connection carries is slowed down rather than having its messages dropped. Never for
    // longer than the app's side of the service pipe waits, though: past that, the messages are
    // queued anyway.
    auto const waitDeadline = GetTickCount64() + MIDI_NETWORK_SEND_QUEUE_WAIT_LIMIT_MILLISECONDS;

    while (true)
    {
        ULONG observedGeneration{ 0 };

        {
            auto lock = m_outgoingUmpMessageQueueLock.lock();

            // Under the queue lock, so nothing is added after a session's queue has been cleared
            if (m_shuttingDown || (!m_sessionActive && !m_endpointBeingCreated))
            {
                return S_OK;
            }

            // Before the session starts, nothing drains the queue, so there is nothing to wait for
            if (!m_sessionActive || OutgoingQueueHasRoom() || GetTickCount64() >= waitDeadline)
            {
                if ((m_outgoingUmpMessages.size() - m_outgoingReadIndex) + words.size() > MIDI_NETWORK_SEND_QUEUE_HARD_MAX_WORDS)
                {
                    TraceLoggingWrite(
                        MidiNetworkMidiTransportTelemetryProvider::Provider(),
                        MIDI_TRACE_EVENT_WARNING,
                        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
                        TraceLoggingLevel(WINEVENT_LEVEL_WARNING),
                        TraceLoggingPointer(this, "this"),
                        TraceLoggingWideString(L"Outbound queue is full and not draining. Dropping messages.", MIDI_TRACE_EVENT_MESSAGE_FIELD),
                        TraceLoggingUInt32(static_cast<uint32_t>(words.size()), "Word count")
                    );

                    return S_OK;
                }

                // can throw, and nothing may escape into the service
                try
                {
                    m_outgoingUmpMessages.insert(m_outgoingUmpMessages.end(), words.begin(), words.end());
                }
                CATCH_RETURN();

                m_outgoingQueuedWireBytes += wireBytes;

                break;
            }

            observedGeneration = m_outgoingQueueGeneration;
        }

        // Woken when the send thread takes from the queue, and checks again at least this often
        auto const nowTicks = GetTickCount64();
        auto const remaining = (waitDeadline > nowTicks) ? waitDeadline - nowTicks : 0;

        WaitOnAddress(&m_outgoingQueueGeneration, &observedGeneration, sizeof(observedGeneration),
            static_cast<DWORD>((std::min)(remaining, static_cast<uint64_t>(MIDI_NETWORK_SEND_QUEUE_WAIT_SLICE_MILLISECONDS))));
    }

    WakeSendThread();

    TraceLoggingWrite(
        MidiNetworkMidiTransportTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this"),
        TraceLoggingWideString(L"Exit", MIDI_TRACE_EVENT_MESSAGE_FIELD)
    );

    return S_OK;
}



_Use_decl_annotations_
HRESULT
MidiNetworkConnection::QueueMidiMessagesToSendToNetwork(
    PVOID const bytes,
    UINT const byteCount)
{
    TraceLoggingWrite(
        MidiNetworkMidiTransportTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this"),
        TraceLoggingWideString(L"Enter", MIDI_TRACE_EVENT_MESSAGE_FIELD),
        TraceLoggingUInt32(byteCount, "Byte count")
    );

    RETURN_HR_IF_NULL(E_INVALIDARG, bytes);
    RETURN_HR_IF(E_INVALIDARG, byteCount < sizeof(uint32_t));

    std::vector<uint32_t> words{ };
    uint32_t* wordPointer{ static_cast<uint32_t*>(bytes) };
    size_t wordCount{ byteCount / sizeof(uint32_t) };

    // can throw, and nothing may escape into the service
    try
    {
        words.insert(words.end(), wordPointer, wordPointer + wordCount);
    }
    CATCH_RETURN();

    // TODO: Can optimize this to not create the temporary vector and instead
    // insert directly into m_outgoingUmpMessages. Duplicates some code.

    return QueueMidiMessagesToSendToNetwork(words);
}



HRESULT
MidiNetworkConnection::Shutdown()
{
    TraceLoggingWrite(
        MidiNetworkMidiTransportTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this"),
        TraceLoggingWideString(L"Enter", MIDI_TRACE_EVENT_MESSAGE_FIELD)
    );


    if (m_shuttingDown.exchange(true))
    {
        // already shut down. This is called from both the owning host/client and from
        // TransportState cleanup, so it has to be safe to repeat.
        return S_OK;
    }

    // release a user-initiated disconnect which is mid-retry
    m_byeReplyEvent.SetEvent();

    // don't process any more MIDI UMP messages
    auto callback = DetachCallback();
    callback.reset();

    // say bye while the writer is still up
    LOG_IF_FAILED(SendShutdownBye());

    // cleanup. Does not tear down the writer.
    LOG_IF_FAILED(EndActiveSession(false));

    // Both workers must be stopped before anything they touch is released.
    LOG_IF_FAILED(StopAndJoinWorkerThreads());

    {
        auto lock = m_socketWriterLock.lock();

        m_retransmitBuffer.clear();
        m_retransmitBufferWordCount = 0;

        if (m_writer != nullptr)
        {
            LOG_IF_FAILED(m_writer->Shutdown());
            m_writer.reset();
        }
    }

    {
        auto queueLock = m_outgoingUmpMessageQueueLock.lock();
        ClearOutgoingQueue();
    }

    WakeSendersWaitingForRoom();
    ClearResendRequests();

    TraceLoggingWrite(
        MidiNetworkMidiTransportTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this"),
        TraceLoggingWideString(L"Exit", MIDI_TRACE_EVENT_MESSAGE_FIELD)
    );

    return S_OK;
}
