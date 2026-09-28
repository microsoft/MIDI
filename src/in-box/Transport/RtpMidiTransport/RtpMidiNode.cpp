// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// One local rtpMIDI port pair.
// ============================================================================

#include "pch.h"

namespace
{
    uint64_t RandomSeed() noexcept
    {
        uint64_t value{ 0 };

        if (!BCRYPT_SUCCESS(BCryptGenRandom(nullptr, reinterpret_cast<PUCHAR>(&value), sizeof(value), BCRYPT_USE_SYSTEM_PREFERRED_RNG)) || value == 0)
        {
            value = internal::GetCurrentMidiTimestamp() * 0x9E3779B97F4A7C15ull;
        }

        return value;
    }

    std::string NameForTheWire(_In_ std::wstring const& name)
    {
        auto utf8 = RtpMidiText::WideToUtf8(name);

        if (utf8.size() > MIDI_RTP_NAME_MAX_UTF8_BYTES)
        {
            // never cut a multi-byte character in half
            size_t cut = MIDI_RTP_NAME_MAX_UTF8_BYTES;
            while (cut > 0 && (static_cast<uint8_t>(utf8[cut]) & 0xC0) == 0x80) cut--;
            utf8.resize(cut);
        }

        return utf8;
    }
}


_Use_decl_annotations_
RtpMidiNode::RtpMidiNode(
    Role const role,
    GUID const& entryId,
    std::wstring const& localName,
    bool const sendRecoveryJournal,
    IListener* const listener) :
    m_role(role),
    m_entryId(entryId),
    m_localName(localName),
    m_sendRecoveryJournal(sendRecoveryJournal),
    m_listener(listener)
{
}

RtpMidiNode::~RtpMidiNode()
{
    Stop();
}


uint64_t
RtpMidiNode::SessionNow() noexcept
{
    auto const ticks = internal::GetCurrentMidiTimestamp();
    auto const frequency = internal::GetMidiTimestampFrequency();

    return (ticks / frequency) * RtpMidi::SessionClockTicksPerSecond +
        ((ticks % frequency) * RtpMidi::SessionClockTicksPerSecond) / frequency;
}

_Use_decl_annotations_
uint64_t
RtpMidiNode::SessionTicksToMidiTimestamp(uint64_t const sessionTicks) noexcept
{
    return SessionTicksToMidiTicks(sessionTicks);
}

_Use_decl_annotations_
uint64_t
RtpMidiNode::SessionTicksToMidiTicks(uint64_t const sessionTicks) noexcept
{
    auto const frequency = internal::GetMidiTimestampFrequency();

    return (sessionTicks / RtpMidi::SessionClockTicksPerSecond) * frequency +
        ((sessionTicks % RtpMidi::SessionClockTicksPerSecond) * frequency) / RtpMidi::SessionClockTicksPerSecond;
}


_Use_decl_annotations_
HRESULT
RtpMidiNode::Start(
    uint16_t const preferredControlPort,
    std::vector<std::pair<uint16_t, uint16_t>> const& fallbackRanges)
{
    try
    {
        RETURN_HR_IF(HRESULT_FROM_WIN32(ERROR_ALREADY_INITIALIZED), m_running.load());

        RETURN_HR_IF(HRESULT_FROM_WIN32(ERROR_ADDRESS_ALREADY_ASSOCIATED),
            !m_ports.Bind(preferredControlPort, fallbackRanges, m_usedPortFallback));

        RtpMidi::SessionConfig config{};
        config.LocalName = NameForTheWire(m_localName);
        config.Ssrc = static_cast<uint32_t>(RandomSeed());
        config.AcceptInvitations = m_role == Role::Host;
        config.MaxParticipants = MIDI_RTP_MAX_HOST_CONNECTIONS;
        config.SendJournal = m_sendRecoveryJournal;

        if (m_admissionCheck)
        {
            config.Admit = [check = m_admissionCheck](std::string const& remoteName, RtpMidi::PeerAddress const& from)
                {
                    return check(RtpMidiText::Utf8ToWide(remoteName), RtpMidiNet::AddressToString(from));
                };
        }

        {
            auto lock = std::scoped_lock{ m_engineLock };
            m_session = std::make_unique<RtpMidi::Session>(std::move(config), *this, RandomSeed());
        }

        std::weak_ptr<RtpMidiNode> weakThis = weak_from_this();

        RETURN_IF_FAILED(m_ports.Control().StartReceiving(
            [weakThis](RtpMidi::PeerAddress const& from, uint8_t const* data, size_t size)
            {
                if (auto self = weakThis.lock()) self->OnDatagram(true, from, data, size);
            },
            L"rtpMIDI control receive"));

        RETURN_IF_FAILED(m_ports.Data().StartReceiving(
            [weakThis](RtpMidi::PeerAddress const& from, uint8_t const* data, size_t size)
            {
                if (auto self = weakThis.lock()) self->OnDatagram(false, from, data, size);
            },
            L"rtpMIDI data receive"));

        m_running = true;
        m_ticker = std::jthread([this](std::stop_token stopToken) { TickLoop(stopToken); });
        SetThreadDescription(m_ticker.native_handle(), L"rtpMIDI timers");

        TraceLoggingWrite(
            MidiRtpMidiTransportTelemetryProvider::Provider(),
            MIDI_TRACE_EVENT_INFO,
            TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
            TraceLoggingLevel(WINEVENT_LEVEL_INFO),
            TraceLoggingPointer(this, "this"),
            TraceLoggingWideString(L"rtpMIDI port pair started", MIDI_TRACE_EVENT_MESSAGE_FIELD),
            TraceLoggingWideString(m_localName.c_str(), "local name"),
            TraceLoggingBool(m_role == Role::Host, "is host"),
            TraceLoggingUInt16(m_ports.Control().Port(), "control port"),
            TraceLoggingBool(m_usedPortFallback, "used port fallback")
        );

        return S_OK;
    }
    CATCH_RETURN();
}


_Use_decl_annotations_
HRESULT
RtpMidiNode::Advertise(std::wstring const& instanceLabel)
{
    RETURN_HR_IF(E_ILLEGAL_METHOD_CALL, m_role != Role::Host);
    RETURN_HR_IF(E_ILLEGAL_STATE_CHANGE, !m_running.load());
    RETURN_HR_IF(E_INVALIDARG, instanceLabel.empty());

    RETURN_IF_FAILED(m_advertiser.Register(instanceLabel, m_ports.Control().Port(), 10000));

    m_advertised = true;

    TraceLoggingWrite(
        MidiRtpMidiTransportTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this"),
        TraceLoggingWideString(L"rtpMIDI host advertised", MIDI_TRACE_EVENT_MESSAGE_FIELD),
        TraceLoggingWideString(instanceLabel.c_str(), "requested label"),
        TraceLoggingWideString(m_advertiser.RegisteredLabel().c_str(), "registered label")
    );

    return S_OK;
}


void
RtpMidiNode::Stop()
{
    if (!m_running.exchange(false)) return;

    try
    {
        {
            auto lock = std::scoped_lock{ m_engineLock };

            if (m_session != nullptr)
            {
                auto const now = SessionNow();
                m_session->EndAll(now);
                m_session->Tick(now);
            }
        }

        // the goodbyes above produced the connection-down notifications, which go out now
        DeliverPending();

        m_ticker.request_stop();

        if (m_ticker.joinable())
        {
            if (m_ticker.get_id() != std::this_thread::get_id()) m_ticker.join();
            else m_ticker.detach();
        }

        m_ports.Close();

        if (m_advertised.exchange(false)) m_advertiser.Unregister();

        {
            auto lock = std::scoped_lock{ m_connectionsLock };
            m_connections.clear();
        }
    }
    CATCH_LOG();
}


_Use_decl_annotations_
HRESULT
RtpMidiNode::Invite(RtpMidi::PeerAddress const& remoteControl)
{
    RETURN_HR_IF(E_ILLEGAL_STATE_CHANGE, !m_running.load());

    {
        auto lock = std::scoped_lock{ m_engineLock };
        RETURN_HR_IF_NULL(E_UNEXPECTED, m_session);

        m_session->Invite(remoteControl, SessionNow());
    }

    DeliverPending();

    return S_OK;
}


_Use_decl_annotations_
HRESULT
RtpMidiNode::EndConnection(uint32_t const participantId)
{
    {
        auto lock = std::scoped_lock{ m_engineLock };
        RETURN_HR_IF_NULL(E_UNEXPECTED, m_session);

        m_session->EndParticipant(participantId, SessionNow());
    }

    DeliverPending();

    return S_OK;
}


_Use_decl_annotations_
bool
RtpMidiNode::SendMidi(uint32_t const participantId, uint8_t const* bytes, size_t const count)
{
    if (!m_running.load() || bytes == nullptr || count == 0) return false;

    auto lock = std::scoped_lock{ m_engineLock };
    if (m_session == nullptr) return false;

    return m_session->SendMidiTo(participantId, bytes, count, SessionNow());
}


std::vector<RtpMidi::Participant>
RtpMidiNode::Snapshot()
{
    auto lock = std::scoped_lock{ m_engineLock };
    if (m_session == nullptr) return {};

    return m_session->Snapshot();
}

_Use_decl_annotations_
bool
RtpMidiNode::TrySnapshot(uint32_t const participantId, RtpMidi::Participant& snapshot)
{
    auto lock = std::scoped_lock{ m_engineLock };
    if (m_session == nullptr) return false;

    return m_session->TrySnapshot(participantId, snapshot);
}

std::vector<std::shared_ptr<RtpMidiConnection>>
RtpMidiNode::Connections()
{
    auto lock = std::scoped_lock{ m_connectionsLock };

    std::vector<std::shared_ptr<RtpMidiConnection>> connections;
    for (auto const& entry : m_connections) connections.push_back(entry.second);

    return connections;
}


_Use_decl_annotations_
void
RtpMidiNode::OnDatagram(bool const isControlPort, RtpMidi::PeerAddress const& from, uint8_t const* data, size_t const size)
{
    {
        auto lock = std::scoped_lock{ m_engineLock };
        if (m_session == nullptr) return;

        m_session->OnDatagram(isControlPort, from, data, size, SessionNow());
    }

    DeliverPending();
}


_Use_decl_annotations_
void
RtpMidiNode::TickLoop(std::stop_token stopToken)
{
    while (!stopToken.stop_requested())
    {
        // one failed tick must not stop the timers of every connection on this port pair
        try
        {
            {
                auto lock = std::scoped_lock{ m_engineLock };
                if (m_session != nullptr) m_session->Tick(SessionNow());
            }

            DeliverPending();
        }
        CATCH_LOG();

        std::this_thread::sleep_for(std::chrono::milliseconds(MIDI_RTP_TICK_INTERVAL_MS));
    }
}


_Use_decl_annotations_
std::shared_ptr<RtpMidiConnection>
RtpMidiNode::FindConnection(uint32_t const participantId)
{
    auto lock = std::scoped_lock{ m_connectionsLock };

    auto const it = m_connections.find(participantId);
    return it == m_connections.end() ? nullptr : it->second;
}


void
RtpMidiNode::DeliverPending()
{
    try
    {
        auto delivery = std::scoped_lock{ m_deliveryLock };

        std::vector<PendingEvent> events;

        {
            auto lock = std::scoped_lock{ m_engineLock };
            events.swap(m_pending);
        }

        for (auto& event : events)
        {
            if (event.IsMidi)
            {
                if (auto connection = FindConnection(event.ParticipantId))
                {
                    connection->DeliverFromNetwork(event.Bytes, SessionTicksToMidiTimestamp(event.LocalTicks));
                }

                continue;
            }

            if (event.State == RtpMidi::ParticipantState::Connected || event.State == RtpMidi::ParticipantState::Synchronizing)
            {
                std::shared_ptr<RtpMidiConnection> connection{ nullptr };

                {
                    auto lock = std::scoped_lock{ m_connectionsLock };

                    if (m_connections.find(event.ParticipantId) == m_connections.end())
                    {
                        connection = std::make_shared<RtpMidiConnection>(
                            weak_from_this(), m_entryId, m_role == Role::Host, event.ParticipantId, event.RemoteName, event.RemoteControl);

                        m_connections.emplace(event.ParticipantId, connection);
                    }
                }

                if (connection != nullptr && m_listener != nullptr) m_listener->OnConnectionUp(connection);
            }
            else if (event.State == RtpMidi::ParticipantState::Ended)
            {
                std::shared_ptr<RtpMidiConnection> connection{ nullptr };

                {
                    auto lock = std::scoped_lock{ m_connectionsLock };

                    auto const it = m_connections.find(event.ParticipantId);
                    if (it != m_connections.end())
                    {
                        connection = std::move(it->second);
                        m_connections.erase(it);
                    }
                }

                if (m_listener != nullptr)
                {
                    if (connection != nullptr) m_listener->OnConnectionDown(connection, event.Reason);

                    // whether or not it ever connected, the client can now decide whether to try again
                    if (event.WeInitiated) m_listener->OnInvitationEnded(this, event.Reason);
                }
            }
        }
    }
    CATCH_LOG();
}


_Use_decl_annotations_
void
RtpMidiNode::SendControl(RtpMidi::PeerAddress const& to, std::vector<uint8_t> const& datagram)
{
    m_ports.Control().Send(to, datagram);
}

_Use_decl_annotations_
void
RtpMidiNode::SendData(RtpMidi::PeerAddress const& to, std::vector<uint8_t> const& datagram)
{
    m_ports.Data().Send(to, datagram);
}

_Use_decl_annotations_
void
RtpMidiNode::OnMidi(RtpMidi::Participant const& participant, uint64_t localTimestamp, int64_t senderLeadTicks, bool, std::vector<uint8_t> const& bytes)
{
    PendingEvent event{};
    event.IsMidi = true;
    event.ParticipantId = participant.Id;

    // never later than arrival: a sender may stamp ahead, and the clock mapping has error
    event.LocalTicks = senderLeadTicks > 0 ? localTimestamp - static_cast<uint64_t>(senderLeadTicks) : localTimestamp;
    event.Bytes = bytes;

    m_pending.push_back(std::move(event));
}

_Use_decl_annotations_
void
RtpMidiNode::OnParticipantChanged(RtpMidi::Participant const& participant)
{
    // only these three change what exists outside the engine
    if (participant.State != RtpMidi::ParticipantState::Connected &&
        participant.State != RtpMidi::ParticipantState::Synchronizing &&
        participant.State != RtpMidi::ParticipantState::Ended)
    {
        return;
    }

    PendingEvent event{};
    event.ParticipantId = participant.Id;
    event.State = participant.State;
    event.Reason = participant.Reason;
    event.WeInitiated = participant.WeInitiated;
    event.RemoteName = RtpMidiText::Utf8ToWide(participant.RemoteName);
    event.RemoteControl = participant.RemoteControl;

    m_pending.push_back(std::move(event));
}

_Use_decl_annotations_
void
RtpMidiNode::Log(std::string const& message)
{
    TraceLoggingWrite(
        MidiRtpMidiTransportTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this"),
        TraceLoggingString(message.c_str(), MIDI_TRACE_EVENT_MESSAGE_FIELD),
        TraceLoggingWideString(m_localName.c_str(), "local name")
    );
}
