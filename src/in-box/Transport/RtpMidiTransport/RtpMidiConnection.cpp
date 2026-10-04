// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// One rtpMIDI connection.
// ============================================================================

#include "pch.h"

_Use_decl_annotations_
RtpMidiConnection::RtpMidiConnection(
    std::weak_ptr<RtpMidiNode> node,
    GUID const& entryId,
    bool const thisPcIsHost,
    uint32_t const participantId,
    std::wstring const& remoteName,
    RtpMidi::PeerAddress const& remoteControl) :
    m_node(std::move(node)),
    m_entryId(entryId),
    m_thisPcIsHost(thisPcIsHost),
    m_participantId(participantId),
    m_remoteName(remoteName),
    m_remoteControl(remoteControl)
{
    // The engine hands over complete messages with running status already expanded, so neither
    // translator has to track it across calls.
    m_bytestreamToUmp.defaultGroup = 0;
    m_bytestreamToUmp.enableRunningStatus = false;
    m_umpToBytestream.enableRunningStatus = false;
}


_Use_decl_annotations_
void
RtpMidiConnection::SetEndpointIds(std::wstring const& instanceId, std::wstring const& interfaceId)
{
    auto lock = std::scoped_lock{ m_endpointIdsLock };

    m_endpointDeviceInstanceId = instanceId;
    m_endpointDeviceInterfaceId = interfaceId;
}

void
RtpMidiConnection::ClearEndpointIds()
{
    auto lock = std::scoped_lock{ m_endpointIdsLock };

    m_endpointDeviceInstanceId.clear();
    m_endpointDeviceInterfaceId.clear();
}

std::wstring
RtpMidiConnection::EndpointDeviceInstanceId() const
{
    auto lock = std::scoped_lock{ m_endpointIdsLock };
    return m_endpointDeviceInstanceId;
}

std::wstring
RtpMidiConnection::EndpointDeviceInterfaceId() const
{
    auto lock = std::scoped_lock{ m_endpointIdsLock };
    return m_endpointDeviceInterfaceId;
}


_Use_decl_annotations_
HRESULT
RtpMidiConnection::ConnectMidiCallback(IMidiCallback* callback, LONGLONG const context)
{
    RETURN_HR_IF_NULL(E_INVALIDARG, callback);

    {
        auto lock = std::scoped_lock{ m_callbackLock };

        m_callback = callback;
        m_callbackContext = context;
    }

    m_reportedMissingCallback.store(false);

    return S_OK;
}

HRESULT
RtpMidiConnection::DisconnectMidiCallback()
{
    auto lock = std::scoped_lock{ m_callbackLock };

    m_callback = nullptr;
    m_callbackContext = 0;

    return S_OK;
}

_Use_decl_annotations_
HRESULT
RtpMidiConnection::DisconnectMidiCallbackIfCurrent(IMidiCallback* callback)
{
    auto lock = std::scoped_lock{ m_callbackLock };

    if (m_callback.get() != callback) return S_FALSE;

    m_callback = nullptr;
    m_callbackContext = 0;

    return S_OK;
}


_Use_decl_annotations_
void
RtpMidiConnection::DeliverFromNetwork(std::vector<uint8_t> const& bytes, uint64_t const midiTimestamp)
{
    if (bytes.empty()) return;

    try
    {
        // Held across the callback so pieces from the receive and timer threads stay in order.
        // Nothing the service does inside the callback comes back to this lock.
        auto lock = std::scoped_lock{ m_incomingLock };

        // A clock re-estimate can move the mapping back a little. Order matters more than that
        // fraction of a millisecond.
        auto const timestamp = (std::max)(midiTimestamp, m_lastIncomingTimestamp);
        m_lastIncomingTimestamp = timestamp;

        std::vector<uint32_t> words;
        words.reserve(bytes.size());

        for (size_t i = 0; i < bytes.size(); i++)
        {
            m_bytestreamToUmp.bytestreamParse(bytes[i]);

            if (i == bytes.size() - 1)
            {
                // a SysEx continued in the next packet is flushed now rather than held back
                m_bytestreamToUmp.dumpSysex7State(false);
            }

            while (m_bytestreamToUmp.availableUMP())
            {
                words.push_back(m_bytestreamToUmp.readUMP());
            }
        }

        if (!words.empty())
        {
            LOG_IF_FAILED(SendUmpWordsToCallback(words.data(), words.size(), timestamp));
        }
    }
    CATCH_LOG();
}


_Use_decl_annotations_
HRESULT
RtpMidiConnection::SendUmpWordsToCallback(uint32_t const* const words, size_t const wordCount, uint64_t const timestamp)
{
    RETURN_HR_IF_NULL(E_INVALIDARG, words);
    RETURN_HR_IF(E_INVALIDARG, wordCount == 0);

    for (size_t i = 0; i < wordCount; )
    {
        auto const messageWordCount = internal::GetUmpLengthInMidiWordsFromFirstWord(words[i]);
        if (messageWordCount == 0 || i + messageWordCount > wordCount) break;

        m_messagesReceived++;
        i += messageWordCount;
    }

    wil::com_ptr_nothrow<IMidiCallback> callback{ nullptr };
    LONGLONG callbackContext{ 0 };

    {
        auto lock = std::scoped_lock{ m_callbackLock };

        if (m_callback == nullptr)
        {
            if (!m_reportedMissingCallback.exchange(true))
            {
                TraceLoggingWrite(
                    MidiRtpMidiTransportTelemetryProvider::Provider(),
                    MIDI_TRACE_EVENT_WARNING,
                    TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
                    TraceLoggingLevel(WINEVENT_LEVEL_WARNING),
                    TraceLoggingPointer(this, "this"),
                    TraceLoggingWideString(L"Discarding incoming messages because no client has this endpoint open", MIDI_TRACE_EVENT_MESSAGE_FIELD),
                    TraceLoggingWideString(m_remoteName.c_str(), "remote name")
                );
            }

            return S_FALSE;
        }

        // Kept alive by this reference so the lock can be dropped before calling out. Removing an
        // endpoint re-enters synchronously through the bidi's Shutdown, which takes this lock.
        callback = m_callback;
        callbackContext = m_callbackContext;
    }

    RETURN_IF_FAILED(callback->Callback(
        MessageOptionFlags_None,
        const_cast<uint32_t*>(words),
        static_cast<UINT>(wordCount * sizeof(uint32_t)),
        static_cast<LONGLONG>(timestamp),
        callbackContext));

    return S_OK;
}


_Use_decl_annotations_
HRESULT
RtpMidiConnection::SendToNetwork(PVOID const data, UINT const length)
try
{
    RETURN_HR_IF_NULL(E_INVALIDARG, data);
    RETURN_HR_IF(E_INVALIDARG, length < sizeof(uint32_t));

    auto node = m_node.lock();
    RETURN_HR_IF_NULL(HRESULT_FROM_WIN32(ERROR_DEVICE_NOT_CONNECTED), node);

    auto const words = static_cast<uint32_t const*>(data);
    auto const wordCount = length / sizeof(uint32_t);

    std::vector<uint8_t> bytes;
    bytes.reserve(wordCount * 4);

    // how many of the bytes each message became, so the speed limit never splits one
    std::vector<uint32_t> messageSizes;

    {
        auto lock = std::scoped_lock{ m_outgoingLock };

        size_t index{ 0 };

        while (index < wordCount)
        {
            auto const messageWordCount = internal::GetUmpLengthInMidiWordsFromFirstWord(words[index]);

            if (messageWordCount == 0 || index + messageWordCount > wordCount)
            {
                // an incomplete message leaves the translator no way to find the next boundary
                m_umpToBytestream.resetBuffer();
                break;
            }

            for (uint8_t i = 0; i < messageWordCount; i++)
            {
                m_umpToBytestream.UMPStreamParse(words[index + i]);
            }

            auto const sizeBefore = bytes.size();

            while (m_umpToBytestream.availableBS())
            {
                bytes.push_back(m_umpToBytestream.readBS());
            }

            if (bytes.size() > sizeBefore)
            {
                m_messagesSent++;
                messageSizes.push_back(static_cast<uint32_t>(bytes.size() - sizeBefore));
            }

            index += messageWordCount;
        }
    }

    if (bytes.empty()) return S_OK;

    // While the queue is over its limit, the sender waits for room, so an app sending faster than
    // the limit is slowed down rather than having its messages dropped. Never for longer than the
    // app's side of the service pipe waits, though: past that, the messages are queued anyway.
    auto const waitDeadline = GetTickCount64() + MIDI_RTP_SEND_QUEUE_WAIT_LIMIT_MILLISECONDS;

    uint64_t waitTicks{ 0 };
    size_t messagesTaken{ 0 };
    bool participantReady{ true };

    while (true)
    {
        ULONG observedGeneration{ 0 };

        {
            auto lock = std::scoped_lock{ m_pacedLock };

            RETURN_HR_IF(HRESULT_FROM_WIN32(ERROR_DEVICE_NOT_CONNECTED), m_pacedSendClosed || !node->IsRunning());

            if (HasRoomLocked(node->SendSpeedLimit()) || GetTickCount64() >= waitDeadline)
            {
                if (m_pacedMessageSizes.empty() && node->SendSpeedLimit() == 0)
                {
                    // no limit and nothing ahead of it, so straight out as before
                    participantReady = node->SendMidi(m_participantId, bytes.data(), bytes.size());

                    break;
                }

                if (m_pacedBytes.size() + bytes.size() > MIDI_RTP_SEND_QUEUE_HARD_MAX_BYTES)
                {
                    TraceLoggingWrite(
                        MidiRtpMidiTransportTelemetryProvider::Provider(),
                        MIDI_TRACE_EVENT_WARNING,
                        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
                        TraceLoggingLevel(WINEVENT_LEVEL_WARNING),
                        TraceLoggingPointer(this, "this"),
                        TraceLoggingWideString(L"Outbound queue is full and not draining. Dropping messages.", MIDI_TRACE_EVENT_MESSAGE_FIELD),
                        TraceLoggingWideString(m_remoteName.c_str(), "remote name"),
                        TraceLoggingUInt64(static_cast<uint64_t>(bytes.size()), "byte count")
                    );

                    return S_OK;
                }

                m_pacedBytes.insert(m_pacedBytes.end(), bytes.begin(), bytes.end());
                m_pacedMessageSizes.insert(m_pacedMessageSizes.end(), messageSizes.begin(), messageSizes.end());

                // What the limit allows goes now, on this thread, as everything did before there
                // was a limit. Without one, that is all of it.
                participantReady = SendAllowedLocked(node, internal::GetCurrentMidiTimestamp(), waitTicks, messagesTaken);

                break;
            }

            observedGeneration = m_pacedGeneration;
        }

        // Woken when messages leave the queue, and checks again at least this often
        auto const nowTicks = GetTickCount64();
        auto const remaining = (waitDeadline > nowTicks) ? waitDeadline - nowTicks : 0;

        WaitOnAddress(&m_pacedGeneration, &observedGeneration, sizeof(observedGeneration),
            static_cast<DWORD>((std::min)(remaining, static_cast<uint64_t>(MIDI_RTP_SEND_QUEUE_WAIT_SLICE_MILLISECONDS))));
    }

    if (messagesTaken > 0) WakeSendersWaitingForRoom();

    // the rest goes from the node's timer thread
    if (waitTicks > 0) node->WakeForPacedSend();

    RETURN_HR_IF(HRESULT_FROM_WIN32(ERROR_DEVICE_NOT_CONNECTED), !participantReady);

    return S_OK;
}
CATCH_RETURN();


uint64_t
RtpMidiConnection::SendPacedMidi()
{
    auto node = m_node.lock();
    if (node == nullptr) return 0;

    uint64_t waitTicks{ 0 };
    size_t messagesTaken{ 0 };

    {
        auto lock = std::scoped_lock{ m_pacedLock };

        if (m_pacedSendClosed || m_pacedMessageSizes.empty()) return 0;

        if (!SendAllowedLocked(node, internal::GetCurrentMidiTimestamp(), waitTicks, messagesTaken))
        {
            TraceLoggingWrite(
                MidiRtpMidiTransportTelemetryProvider::Provider(),
                MIDI_TRACE_EVENT_WARNING,
                TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
                TraceLoggingLevel(WINEVENT_LEVEL_WARNING),
                TraceLoggingPointer(this, "this"),
                TraceLoggingWideString(L"The remote can no longer take data. Dropping messages held back by the speed limit.", MIDI_TRACE_EVENT_MESSAGE_FIELD),
                TraceLoggingWideString(m_remoteName.c_str(), "remote name")
            );
        }
    }

    if (messagesTaken > 0) WakeSendersWaitingForRoom();

    return waitTicks;
}


_Use_decl_annotations_
bool
RtpMidiConnection::SendAllowedLocked(
    std::shared_ptr<RtpMidiNode> const& node,
    uint64_t const now,
    uint64_t& waitTicks,
    size_t& messagesTaken)
{
    waitTicks = 0;
    messagesTaken = 0;

    auto const multiple = node->SendSpeedLimit();

    if (multiple != m_sendPacerMultiple)
    {
        m_sendPacer.Configure(multiple, internal::GetMidiTimestampFrequency());
        m_sendPacerMultiple = multiple;
    }

    size_t byteCount{ 0 };

    for (auto const size : m_pacedMessageSizes)
    {
        // one message at a time, charged as it is taken
        auto const allowedIn = m_sendPacer.TicksUntilAllowed(size, now);

        if (allowedIn > 0)
        {
            if (messagesTaken == 0) waitTicks = allowedIn;
            break;
        }

        m_sendPacer.Charge(size, now);

        byteCount += size;
        messagesTaken++;
    }

    if (messagesTaken == 0) return true;

    auto const byteEnd = m_pacedBytes.begin() + static_cast<std::ptrdiff_t>(byteCount);

    std::vector<uint8_t> const bytes(m_pacedBytes.begin(), byteEnd);

    m_pacedBytes.erase(m_pacedBytes.begin(), byteEnd);
    m_pacedMessageSizes.erase(m_pacedMessageSizes.begin(), m_pacedMessageSizes.begin() + static_cast<std::ptrdiff_t>(messagesTaken));
    m_pacedGeneration = m_pacedGeneration + 1;

    if (!node->SendMidi(m_participantId, bytes.data(), bytes.size()))
    {
        // nothing behind these can go either
        m_pacedBytes.clear();
        m_pacedMessageSizes.clear();

        return false;
    }

    return true;
}


_Use_decl_annotations_
bool
RtpMidiConnection::HasRoomLocked(uint32_t const speedMultiple) const noexcept
{
    if (speedMultiple == 0)
    {
        return m_pacedBytes.size() < MIDI_RTP_SEND_QUEUE_UNLIMITED_MAX_BYTES;
    }

    // about the same time at any speed
    auto const limit = (std::max)(
        (static_cast<uint64_t>(speedMultiple) * WindowsMidiServicesInternal::MidiWireSpeedBytesPerSecond * MIDI_RTP_SEND_QUEUE_PACED_MILLISECONDS) / 1000,
        static_cast<uint64_t>(MIDI_RTP_SEND_QUEUE_PACED_MINIMUM_BYTES));

    return m_pacedBytes.size() < limit;
}


void
RtpMidiConnection::ClosePacedSend()
{
    {
        auto lock = std::scoped_lock{ m_pacedLock };

        m_pacedSendClosed = true;
        m_pacedBytes.clear();
        m_pacedMessageSizes.clear();
        m_pacedGeneration = m_pacedGeneration + 1;
    }

    WakeSendersWaitingForRoom();
}


void
RtpMidiConnection::WakeSendersWaitingForRoom() noexcept
{
    WakeByAddressAll(const_cast<ULONG*>(&m_pacedGeneration));
}
