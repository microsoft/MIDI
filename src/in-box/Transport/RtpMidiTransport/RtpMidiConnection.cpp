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
{
    RETURN_HR_IF_NULL(E_INVALIDARG, data);
    RETURN_HR_IF(E_INVALIDARG, length < sizeof(uint32_t));

    auto node = m_node.lock();
    RETURN_HR_IF_NULL(HRESULT_FROM_WIN32(ERROR_DEVICE_NOT_CONNECTED), node);

    auto const words = static_cast<uint32_t const*>(data);
    auto const wordCount = length / sizeof(uint32_t);

    std::vector<uint8_t> bytes;
    bytes.reserve(wordCount * 4);

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

            bool producedBytes = false;

            while (m_umpToBytestream.availableBS())
            {
                bytes.push_back(m_umpToBytestream.readBS());
                producedBytes = true;
            }

            if (producedBytes) m_messagesSent++;

            index += messageWordCount;
        }
    }

    if (bytes.empty()) return S_OK;

    RETURN_HR_IF(HRESULT_FROM_WIN32(ERROR_DEVICE_NOT_CONNECTED), !node->SendMidi(m_participantId, bytes.data(), bytes.size()));

    return S_OK;
}
