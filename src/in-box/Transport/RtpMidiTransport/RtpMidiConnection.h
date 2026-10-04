// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// One rtpMIDI connection: a participant in a host's or a client's session, and the
// MIDI endpoint that represents it.
// ============================================================================

#pragma once

class RtpMidiNode;

class RtpMidiConnection
{
public:
    RtpMidiConnection(
        _In_ std::weak_ptr<RtpMidiNode> node,
        _In_ GUID const& entryId,
        _In_ bool const thisPcIsHost,
        _In_ uint32_t const participantId,
        _In_ std::wstring const& remoteName,
        _In_ RtpMidi::PeerAddress const& remoteControl);

    GUID EntryId() const noexcept { return m_entryId; }
    bool ThisPcIsHost() const noexcept { return m_thisPcIsHost; }
    uint32_t ParticipantId() const noexcept { return m_participantId; }
    std::wstring const& RemoteName() const noexcept { return m_remoteName; }
    RtpMidi::PeerAddress const& RemoteControl() const noexcept { return m_remoteControl; }
    std::shared_ptr<RtpMidiNode> Node() const noexcept { return m_node.lock(); }

    void SetEndpointIds(_In_ std::wstring const& instanceId, _In_ std::wstring const& interfaceId);
    void ClearEndpointIds();
    std::wstring EndpointDeviceInstanceId() const;
    std::wstring EndpointDeviceInterfaceId() const;

    HRESULT ConnectMidiCallback(_In_ IMidiCallback* callback, _In_ LONGLONG const context);
    HRESULT DisconnectMidiCallback();

    // The service can connect a new endpoint before the one it replaces shuts down
    HRESULT DisconnectMidiCallbackIfCurrent(_In_ IMidiCallback* callback);

    // A MIDI 1.0 byte stream piece from the network, stamped in MIDI timestamp ticks
    void DeliverFromNetwork(_In_ std::vector<uint8_t> const& bytes, _In_ uint64_t const midiTimestamp);

    // UMP from the service, translated to a MIDI 1.0 byte stream for the network. With a send
    // speed limit, what the limit allows goes now and the rest waits in this connection's queue.
    HRESULT SendToNetwork(_In_reads_bytes_(length) PVOID const data, _In_ UINT const length);

    // Sends what the speed limit allows of the messages waiting for it. Called on the node's timer
    // thread. MIDI timestamp ticks until more may go, 0 when nothing is waiting.
    uint64_t SendPacedMidi();

    // The participant is gone. Drops what is waiting and releases senders waiting for room.
    void ClosePacedSend();

    // A speed for this connection alone, used instead of its host's or client's. Empty to go back
    // to that one. Takes effect at once and drops nothing.
    void SetOwnSendSpeedLimit(_In_ std::optional<uint32_t> const speedMultiple) noexcept;
    bool HasOwnSendSpeedLimit() const noexcept { return m_ownSendSpeedLimit.load() >= 0; }

    // What this connection sends at now: its own speed, or its node's
    uint32_t EffectiveSendSpeedLimit(_In_ RtpMidiNode const& node) const noexcept;

    uint64_t MessagesReceived() const noexcept { return m_messagesReceived.load(); }
    uint64_t MessagesSent() const noexcept { return m_messagesSent.load(); }

private:
    HRESULT SendUmpWordsToCallback(_In_reads_(wordCount) uint32_t const* const words, _In_ size_t const wordCount, _In_ uint64_t const timestamp);

    // Sends from the front of the queue as far as the speed limit allows now. Needs m_pacedLock.
    // False when the participant can no longer take data, which also empties the queue.
    bool SendAllowedLocked(
        _In_ std::shared_ptr<RtpMidiNode> const& node,
        _In_ uint64_t const now,
        _Out_ uint64_t& waitTicks,
        _Out_ size_t& messagesTaken);

    // Needs m_pacedLock
    bool HasRoomLocked(_In_ uint32_t const speedMultiple) const noexcept;

    void WakeSendersWaitingForRoom() noexcept;

    std::weak_ptr<RtpMidiNode> m_node;
    GUID m_entryId{};
    bool m_thisPcIsHost{ false };
    uint32_t m_participantId{ 0 };
    std::wstring m_remoteName;
    RtpMidi::PeerAddress m_remoteControl{};

    mutable std::mutex m_endpointIdsLock;
    std::wstring m_endpointDeviceInstanceId;
    std::wstring m_endpointDeviceInterfaceId;

    std::mutex m_callbackLock;
    wil::com_ptr_nothrow<IMidiCallback> m_callback;
    LONGLONG m_callbackContext{ 0 };
    std::atomic<bool> m_reportedMissingCallback{ false };

    // one byte stream in each direction, each with its own translation state
    std::mutex m_incomingLock;
    bytestreamToUMP m_bytestreamToUmp;
    uint64_t m_lastIncomingTimestamp{ 0 };

    std::mutex m_outgoingLock;
    umpToBytestream m_umpToBytestream;

    // Messages held back by the send speed limit, as MIDI 1.0 bytes, with the size of each
    // message so one is never split. Taken before the node's engine lock, never after it, and
    // held while sending so what the timer thread sends and what a sender sends never cross.
    std::mutex m_pacedLock;
    std::deque<uint8_t> m_pacedBytes;
    std::deque<uint32_t> m_pacedMessageSizes;
    WindowsMidiServicesInternal::MidiSendPacer m_sendPacer{};
    uint32_t m_sendPacerMultiple{ 0 };
    bool m_pacedSendClosed{ false };

    // -1 when the connection uses its node's speed
    std::atomic<int64_t> m_ownSendSpeedLimit{ -1 };

    // Changes whenever messages leave the queue. A sender waiting for room waits on its address.
    // Written under m_pacedLock.
    volatile ULONG m_pacedGeneration{ 0 };

    std::atomic<uint64_t> m_messagesReceived{ 0 };
    std::atomic<uint64_t> m_messagesSent{ 0 };
};
