// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// One local rtpMIDI port pair: its sockets, its protocol engine and the thread that
// runs the engine's timers.
//
// A host node is advertised and accepts invitations. A client node invites one remote and
// accepts nothing. Each participant that reaches the connected state becomes a connection, and
// each connection gets its own endpoint.
//
// The engine is not thread safe, so every call into it is made under m_engineLock. Anything the
// engine reports through ISessionHost is queued under that lock and delivered after it is
// released, so no call out of the transport is ever made while holding it.
// ============================================================================

#pragma once

class RtpMidiNode :
    public RtpMidi::ISessionHost,
    public std::enable_shared_from_this<RtpMidiNode>
{
public:
    enum class Role { Host, Client };

    // Called on the node's own threads without the engine lock held. Implementations queue work
    // and return, because receive and timer threads are the data path.
    class IListener
    {
    public:
        virtual ~IListener() = default;
        virtual void OnConnectionUp(_In_ std::shared_ptr<RtpMidiConnection> const& connection) = 0;
        virtual void OnConnectionDown(_In_ std::shared_ptr<RtpMidiConnection> const& connection, _In_ RtpMidi::EndReason const reason) = 0;
        virtual void OnInvitationEnded(_In_ RtpMidiNode const* node, _In_ RtpMidi::EndReason const reason) = 0;
    };

    RtpMidiNode(
        _In_ Role const role,
        _In_ GUID const& entryId,
        _In_ std::wstring const& localName,
        _In_ bool const sendRecoveryJournal,
        _In_ IListener* const listener);

    ~RtpMidiNode();

    RtpMidiNode(_In_ RtpMidiNode const&) = delete;
    RtpMidiNode& operator=(_In_ RtpMidiNode const&) = delete;

    // Host only, and before Start. Called on a receive thread with the engine lock held, so it
    // must not call back into the node.
    using AdmissionCheck = std::function<RtpMidi::Admission(std::wstring const& remoteName, std::wstring const& remoteAddress)>;
    void SetAdmissionCheck(_In_ AdmissionCheck check) { m_admissionCheck = std::move(check); }

    // A host limited to one network adapter passes that adapter's interfaces, and hears nothing
    // which arrives on any other. Empty is every interface.
    HRESULT Start(
        _In_ uint16_t const preferredControlPort,
        _In_ std::vector<std::pair<uint16_t, uint16_t>> const& fallbackRanges,
        _In_ std::vector<uint32_t> const& interfaces = {});

    // Host only. Blocks while the DNS client probes the name, which takes most of a second, and
    // gives up early when stopToken is signaled. A non-zero interface index advertises on that
    // adapter only.
    HRESULT Advertise(_In_ std::wstring const& instanceLabel, _In_ std::stop_token const& stopToken, _In_ uint32_t const interfaceIndex = 0);

    // Says goodbye to every participant, then stops the threads and withdraws the advertisement.
    // Nothing that arrives once it has begun can start a connection, and every connection reported
    // up is reported down. Safe to call more than once. Never call from a listener callback.
    void Stop();

    HRESULT Invite(_In_ RtpMidi::PeerAddress const& remoteControl);
    HRESULT EndConnection(_In_ uint32_t const participantId);

    bool SendMidi(_In_ uint32_t const participantId, _In_reads_(count) uint8_t const* bytes, _In_ size_t const count);

    // Everything this node sends, as a multiple of MIDI 1.0 wire speed, 0 for no limit. Takes
    // effect from the next message, without dropping a connection.
    void SetSendSpeedLimit(_In_ uint32_t const speedMultiple) noexcept;
    uint32_t SendSpeedLimit() const noexcept { return m_sendSpeedLimit.load(); }

    // For every connection without its own choice. Takes effect from the next message.
    void SetSkipInBetweenValuesToCatchUp(_In_ bool const skip) noexcept { m_skipInBetweenValuesToCatchUp.store(skip); }
    bool SkipInBetweenValuesToCatchUp() const noexcept { return m_skipInBetweenValuesToCatchUp.load(); }

    // A connection has messages waiting for the speed limit. The timer thread sends them.
    void WakeForPacedSend() noexcept;

    std::vector<RtpMidi::Participant> Snapshot();
    bool TrySnapshot(_In_ uint32_t const participantId, _Out_ RtpMidi::Participant& snapshot);
    std::vector<std::shared_ptr<RtpMidiConnection>> Connections();

    GUID EntryId() const noexcept { return m_entryId; }
    bool IsRunning() const noexcept { return m_running.load(); }
    uint16_t ControlPort() noexcept { return m_ports.Control().Port(); }
    bool UsedPortFallback() const noexcept { return m_usedPortFallback; }
    bool IsAdvertised() const noexcept { return m_advertised.load(); }
    std::wstring AdvertisedLabel() const { return m_advertiser.RegisteredLabel(); }
    bool AdvertisedLabelWasChanged() const { return m_advertiser.WasRenamed(); }

    // The engine's clock: 100 microsecond ticks, from the same counter as MIDI timestamps
    static uint64_t SessionNow() noexcept;
    static uint64_t SessionTicksToMidiTicks(_In_ uint64_t const sessionTicks) noexcept;

    // ISessionHost. Called by the engine with m_engineLock held.
    void SendControl(_In_ RtpMidi::PeerAddress const& to, _In_ std::vector<uint8_t> const& datagram) override;
    void SendData(_In_ RtpMidi::PeerAddress const& to, _In_ std::vector<uint8_t> const& datagram) override;
    void OnMidi(_In_ RtpMidi::Participant const& participant, _In_ uint64_t localTimestamp, _In_ int64_t senderLeadTicks, _In_ bool recovered, _In_ std::vector<uint8_t> const& bytes) override;
    void OnParticipantChanged(_In_ RtpMidi::Participant const& participant) override;
    void Log(_In_ std::string const& message) override;

private:
    // One queue for both kinds, so a connection's closing Note Offs reach it before it is
    // reported down, and a peer which invites again is reported down before it is reported up.
    struct PendingEvent
    {
        bool IsMidi{ false };
        uint32_t ParticipantId{ 0 };

        uint64_t LocalTicks{ 0 };
        std::vector<uint8_t> Bytes;

        RtpMidi::ParticipantState State{ RtpMidi::ParticipantState::InvitingControl };
        RtpMidi::EndReason Reason{ RtpMidi::EndReason::None };
        bool WeInitiated{ false };
        std::wstring RemoteName;
        RtpMidi::PeerAddress RemoteControl{};
    };

    void OnDatagram(_In_ bool const isControlPort, _In_ RtpMidi::PeerAddress const& from, _In_reads_(size) uint8_t const* data, _In_ size_t const size);

    // The timer thread holds the node for one pass at a time. If the last reference goes at the end
    // of a pass, the node is destroyed on that thread, which then reads nothing of it again.
    static void TickThread(_In_ std::weak_ptr<RtpMidiNode> const weakThis, _In_ std::stop_token const stopToken) noexcept;
    void TickOnce(_Inout_ uint64_t& nextEngineTick);

    // What each connection's speed limit lets go now. MIDI timestamp ticks until more may go, or
    // 0 when nothing is waiting.
    uint64_t SendPacedMidi();
    void ArmPacedSendTimer(_In_ uint64_t const ticks) noexcept;
    void DeliverPending();
    std::shared_ptr<RtpMidiConnection> FindConnection(_In_ uint32_t const participantId);

    Role m_role{ Role::Host };
    GUID m_entryId{};
    std::wstring m_localName;
    bool m_sendRecoveryJournal{ true };
    IListener* m_listener{ nullptr };
    AdmissionCheck m_admissionCheck;

    wil::srwlock m_engineLock;
    std::unique_ptr<RtpMidi::Session> m_session;
    std::vector<PendingEvent> m_pending;

    // False once Stop begins, so a late invitation never becomes a connection. Under m_engineLock.
    bool m_receiving{ false };

    // Serializes delivery, so pieces taken by different threads reach clients in engine order.
    // Always taken before m_engineLock, never after it.
    wil::srwlock m_deliveryLock;

    wil::srwlock m_connectionsLock;
    std::map<uint32_t, std::shared_ptr<RtpMidiConnection>> m_connections;

    RtpMidiNet::PortPair m_ports;
    WindowsMidiServicesInternal::MidiDnssdAdvertiser m_advertiser;
    bool m_usedPortFallback{ false };
    std::atomic<bool> m_advertised{ false };
    std::atomic<bool> m_running{ false };

    std::atomic<uint32_t> m_sendSpeedLimit{ 0 };
    std::atomic<bool> m_skipInBetweenValuesToCatchUp{ true };

    // Wake the timer thread early: for a stop, or for messages waiting on the speed limit.
    // Declared before the thread, so they outlive it.
    wil::unique_event_nothrow m_tickWakeEvent;
    wil::unique_handle m_pacedSendTimer;

    std::jthread m_ticker;
};
