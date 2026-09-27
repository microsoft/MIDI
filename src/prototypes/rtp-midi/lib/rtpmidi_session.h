// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// PROTOTYPE. One local AppleMIDI session and its participants.
//
// Pure logic: it never touches a socket or a clock. The host hands it datagrams and the current
// session time, and it answers through ISessionHost. That keeps the protocol rules testable
// with a simulated network, and leaves threading and I/O to whoever hosts it.
//
// A session owns one control/data port pair. It can accept invitations and send them, and the
// same pair serves both, the way macOS and rtpMIDI for Windows do it.
// ============================================================================

#pragma once

#include "rtpmidi_journal.h"
#include "rtpmidi_protocol.h"

#include <functional>
#include <memory>

namespace RtpMidi
{
    struct PeerAddress
    {
        uint8_t Family{ 0 };                // 4, 6, or 0 when unset
        std::array<uint8_t, 16> Bytes{};
        uint16_t Port{ 0 };
        uint32_t ScopeId{ 0 };

        bool IsSet() const { return Family != 0; }

        bool SameHost(PeerAddress const& other) const
        {
            return Family == other.Family && Bytes == other.Bytes && ScopeId == other.ScopeId;
        }

        bool operator==(PeerAddress const& other) const { return SameHost(other) && Port == other.Port; }

        PeerAddress WithPort(uint16_t port) const
        {
            auto copy = *this;
            copy.Port = port;
            return copy;
        }

        std::string ToString() const
        {
            char buffer[80]{};

            if (Family == 4)
            {
                snprintf(buffer, sizeof(buffer), "%u.%u.%u.%u:%u", Bytes[0], Bytes[1], Bytes[2], Bytes[3], Port);
                return buffer;
            }

            if (Family == 6)
            {
                std::string text = "[";
                for (int group = 0; group < 8; group++)
                {
                    char part[8]{};
                    snprintf(part, sizeof(part), "%x", (Bytes[group * 2] << 8) | Bytes[group * 2 + 1]);
                    if (group > 0) text += ':';
                    text += part;
                }
                if (ScopeId != 0) { text += '%'; text += std::to_string(ScopeId); }
                text += "]:";
                text += std::to_string(Port);
                return text;
            }

            return "(unset)";
        }
    };

    // All times are session clock ticks, 100 microseconds each.
    struct SessionTimings
    {
        uint64_t InvitationRetryInterval{ 10000 };      // Apple retries once a second
        uint32_t InvitationMaxAttempts{ 12 };
        uint64_t HalfOpenTimeout{ 300000 };             // control accepted, data invitation never came

        // Apple's pattern as an initiator: two exchanges half a second apart, five more 1.5 s
        // apart, then one every 10 s. A responder may end a session silent for 60 s.
        uint64_t SyncFastInterval{ 5000 };
        uint32_t SyncFastCount{ 2 };
        uint64_t SyncMediumInterval{ 15000 };
        uint32_t SyncMediumCount{ 5 };
        uint64_t SyncSteadyInterval{ 100000 };
        uint64_t SyncReplyTimeout{ 30000 };
        uint32_t SyncMaxUnanswered{ 6 };
        uint64_t ResponderSyncTimeout{ 900000 };

        // Clock samples: the offset comes from the retained sample with the smallest round trip,
        // because a slow leg (Wi-Fi power save, a busy AP) skews a sample by half the imbalance.
        // Older samples drop out so drift between the two crystals does not build up.
        uint64_t ClockSampleMaxAge{ 300000 };
        uint64_t SyncRetryAfterPoorSample{ 10000 };

        uint64_t FeedbackInterval{ 10000 };

        uint32_t MaxRejectionsPerSecond{ 10 };
    };

    enum class ParticipantState : uint8_t
    {
        InvitingControl,        // we sent IN to their control port
        InvitingData,           // they accepted on control, we sent IN to their data port
        AwaitingDataInvitation, // they invited our control port, we wait for their data invitation
        Synchronizing,          // both ports accepted, we sent CK0 and wait for CK1
        Connected,
        Ended,
    };

    enum class EndReason : uint8_t
    {
        None,
        LocalRequest,
        RemoteEndedSession,
        Rejected,
        NoAnswer,
        SyncTimeout,
        Replaced,
    };

    inline char const* ParticipantStateName(ParticipantState state)
    {
        switch (state)
        {
        case ParticipantState::InvitingControl: return "inviting (control)";
        case ParticipantState::InvitingData: return "inviting (data)";
        case ParticipantState::AwaitingDataInvitation: return "awaiting data invitation";
        case ParticipantState::Synchronizing: return "synchronizing";
        case ParticipantState::Connected: return "connected";
        case ParticipantState::Ended: return "ended";
        default: return "?";
        }
    }

    inline char const* EndReasonName(EndReason reason)
    {
        switch (reason)
        {
        case EndReason::None: return "none";
        case EndReason::LocalRequest: return "ended here";
        case EndReason::RemoteEndedSession: return "remote sent BY";
        case EndReason::Rejected: return "remote sent NO";
        case EndReason::NoAnswer: return "no answer";
        case EndReason::SyncTimeout: return "clock sync stopped";
        case EndReason::Replaced: return "replaced by a new invitation";
        default: return "?";
        }
    }

    struct ParticipantStats
    {
        uint64_t PacketsReceived{ 0 };
        uint64_t PacketsLost{ 0 };
        uint64_t PacketsOutOfOrder{ 0 };
        uint64_t PacketsSent{ 0 };
        uint64_t MessagesReceived{ 0 };
        uint64_t MalformedPackets{ 0 };
        uint64_t MalformedCommands{ 0 };
        uint64_t DroppedSysExSegments{ 0 };
        uint64_t JournalsSeen{ 0 };
        uint64_t JournalsMalformed{ 0 };
        uint64_t LossEvents{ 0 };
        uint64_t LossEventsCovered{ 0 };
        uint64_t RecoveredNoteOffs{ 0 };
        uint64_t MarkerBitPackets{ 0 };
        uint64_t FeedbackReceived{ 0 };
        uint32_t LastFeedbackField{ 0 };
        uint32_t BitrateLimit{ 0 };
        uint64_t RoundTripTicks{ 0 };           // latest exchange
        uint64_t BestRoundTripTicks{ 0 };       // the sample the offset comes from
        uint64_t ClockOffsetSpreadTicks{ 0 };   // largest minus smallest offset among retained samples
        uint32_t SyncExchanges{ 0 };
        uint32_t SyncExchangesStartedByPeer{ 0 };
    };

    struct Participant
    {
        uint32_t Id{ 0 };
        bool WeInitiated{ false };
        ParticipantState State{ ParticipantState::InvitingControl };
        EndReason Reason{ EndReason::None };

        std::string RemoteName;
        uint32_t RemoteSsrc{ 0 };
        uint32_t InitiatorToken{ 0 };
        PeerAddress RemoteControl{};
        PeerAddress RemoteData{};

        uint64_t Created{ 0 };
        uint64_t LastInvitationSent{ 0 };
        uint32_t InvitationAttempts{ 0 };

        uint64_t LastSyncSent{ 0 };
        bool AwaitingSyncReply{ false };
        uint32_t UnansweredSyncs{ 0 };
        uint32_t CompletedSyncs{ 0 };
        uint64_t LastSyncActivity{ 0 };

        int64_t ClockOffset{ 0 };           // remote clock minus local clock
        bool HaveClockOffset{ false };

        struct ClockSample
        {
            int64_t Offset{ 0 };
            uint64_t RoundTrip{ 0 };
            uint64_t At{ 0 };
        };

        std::array<ClockSample, 8> ClockSamples{};
        uint32_t ClockSampleCount{ 0 };
        bool PoorSampleRetry{ false };

        // the t1 we put in our last CK1, so the peer's CK2 for an exchange it started can be matched
        uint64_t LastSyncReplyStamp{ 0 };
        bool AwaitingPeerSync2{ false };

        bool HaveReceivedRtp{ false };
        uint16_t HighestSequence{ 0 };
        uint32_t SequenceCycles{ 0 };
        bool FeedbackDue{ false };
        uint64_t LastFeedbackSent{ 0 };
        CommandSectionDecoder Decoder{};
        std::array<std::array<bool, 128>, 16> ActiveNotes{};

        uint16_t NextSendSequence{ 0 };

        // running status and an open SysEx belong to one outgoing stream, so each participant has its own
        CommandSectionEncoder Encoder{};

        // sent-side journal: the latest state of each note and the packet that changed it
        struct SentNote
        {
            bool Touched{ false };
            bool On{ false };
            uint8_t Velocity{ 0 };
            uint16_t Sequence{ 0 };
        };

        std::array<std::array<SentNote, 128>, 16> SentNotes{};
        bool HaveSentRtp{ false };
        uint16_t JournalCheckpoint{ 0 };    // oldest packet the journal still has to cover
        uint64_t FeedbackTrims{ 0 };

        ParticipantStats Stats{};

        uint32_t ExtendedHighestSequence() const { return (SequenceCycles << 16) | HighestSequence; }
    };

    class ISessionHost
    {
    public:
        virtual ~ISessionHost() = default;

        virtual void SendControl(PeerAddress const& to, std::vector<uint8_t> const& datagram) = 0;
        virtual void SendData(PeerAddress const& to, std::vector<uint8_t> const& datagram) = 0;

        // senderLeadTicks is the mapped sender time minus arrival: negative is transit delay, positive
        // means the sender scheduled the message ahead. recovered marks synthesized repair messages.
        virtual void OnMidi(Participant const& participant, uint64_t localTimestamp, int64_t senderLeadTicks, bool recovered, std::vector<uint8_t> const& bytes) = 0;

        virtual void OnParticipantChanged(Participant const& participant) = 0;
        virtual void Log(std::string const& message) = 0;

        // diagnostics only: every RTP-MIDI packet accepted from a participant, after decoding
        virtual void OnRtpPacket(Participant const& /*participant*/, DecodedPacket const& /*packet*/, uint8_t const* /*datagram*/, size_t /*size*/) {}
    };

    struct SessionConfig
    {
        std::string LocalName;
        uint32_t Ssrc{ 0 };
        bool AcceptInvitations{ true };
        size_t MaxParticipants{ 16 };

        // optional admission policy, consulted for every invitation to our control port
        std::function<bool(std::string const& remoteName, PeerAddress const& from)> Admit;

        // macOS sends RS with the RTP sequence number in the high 16 bits and filler in the low
        // 16 (measured). The other layout, an extended sequence number, stays selectable.
        bool FeedbackSequenceInHighBits{ true };

        // send a recovery journal (chapter N) with every packet, closed-loop on the peer's RS
        bool SendJournal{ false };

        // macOS starts clock exchanges from both ends of a session. Doing the same as the side
        // that was invited gives a better estimate than waiting for the initiator's schedule.
        bool ResponderStartsSync{ true };

        size_t MaxListBytes{ 1000 };

        SessionTimings Timings{};
    };

    struct SessionStats
    {
        uint64_t UnknownDatagrams{ 0 };
        uint64_t UnexpectedRtp{ 0 };
        uint64_t MalformedCommands{ 0 };
        uint64_t RejectionsSent{ 0 };
        uint64_t RejectionsSuppressed{ 0 };
    };

    class Session
    {
    public:
        Session(SessionConfig config, ISessionHost& host, uint64_t randomSeed) :
            m_config(std::move(config)),
            m_host(host),
            m_random(randomSeed == 0 ? 0x9E3779B97F4A7C15ull : randomSeed),
            m_encoder(m_config.MaxListBytes)
        {
        }

        SessionConfig const& Config() const { return m_config; }
        SessionStats const& Stats() const { return m_stats; }

        uint32_t Invite(PeerAddress const& remoteControl, uint64_t now)
        {
            auto participant = std::make_unique<Participant>();
            participant->Id = ++m_nextId;
            participant->WeInitiated = true;
            participant->State = ParticipantState::InvitingControl;
            participant->InitiatorToken = NextRandom32();
            participant->RemoteControl = remoteControl;
            participant->RemoteData = remoteControl.WithPort(static_cast<uint16_t>(remoteControl.Port + 1));
            participant->Created = now;
            participant->NextSendSequence = static_cast<uint16_t>(NextRandom32());
            participant->Encoder = CommandSectionEncoder{ m_config.MaxListBytes };

            auto& added = *participant;
            m_participants.push_back(std::move(participant));

            SendInvitation(added, true, now);
            m_host.OnParticipantChanged(added);

            return added.Id;
        }

        void OnDatagram(bool isControlPort, PeerAddress const& from, uint8_t const* data, size_t size, uint64_t now)
        {
            AppleMidiCommand command{};

            if (TryGetAppleMidiCommand(data, size, command))
            {
                HandleCommand(isControlPort, from, command, data, size, now);
            }
            else if (!isControlPort)
            {
                HandleRtp(from, data, size, now);
            }
            else
            {
                if (m_stats.UnknownDatagrams++ < 5) m_host.Log("ignored a control port datagram from " + from.ToString() + " that is not AppleMIDI");
            }
        }

        void Tick(uint64_t now)
        {
            auto const& timings = m_config.Timings;

            for (auto& entry : m_participants)
            {
                auto& participant = *entry;

                switch (participant.State)
                {
                case ParticipantState::InvitingControl:
                case ParticipantState::InvitingData:
                    if (now - participant.LastInvitationSent >= timings.InvitationRetryInterval)
                    {
                        if (participant.InvitationAttempts >= timings.InvitationMaxAttempts)
                        {
                            EndParticipant(participant, EndReason::NoAnswer, true, now);
                        }
                        else
                        {
                            SendInvitation(participant, participant.State == ParticipantState::InvitingControl, now);
                        }
                    }
                    break;

                case ParticipantState::AwaitingDataInvitation:
                    if (now - participant.Created >= timings.HalfOpenTimeout)
                    {
                        EndParticipant(participant, EndReason::NoAnswer, false, now);
                    }
                    break;

                case ParticipantState::Synchronizing:
                case ParticipantState::Connected:
                    // as the invited side, wait until the initiator's own first exchange is done
                    if (participant.WeInitiated || (m_config.ResponderStartsSync && participant.HaveClockOffset))
                    {
                        if (participant.AwaitingSyncReply)
                        {
                            if (now - participant.LastSyncSent >= timings.SyncReplyTimeout)
                            {
                                if (participant.WeInitiated)
                                {
                                    participant.UnansweredSyncs++;

                                    if (participant.UnansweredSyncs >= timings.SyncMaxUnanswered)
                                    {
                                        EndParticipant(participant, EndReason::SyncTimeout, true, now);
                                        break;
                                    }

                                    SendSync0(participant, now);
                                }
                                else
                                {
                                    // our own exchanges as the invited side never end a session
                                    participant.AwaitingSyncReply = false;
                                }
                            }
                        }
                        else if (now - participant.LastSyncSent >= SyncInterval(participant))
                        {
                            SendSync0(participant, now);
                        }
                    }

                    if (!participant.WeInitiated && now - participant.LastSyncActivity >= timings.ResponderSyncTimeout)
                    {
                        EndParticipant(participant, EndReason::SyncTimeout, true, now);
                        break;
                    }

                    if (participant.FeedbackDue && now - participant.LastFeedbackSent >= timings.FeedbackInterval)
                    {
                        SendFeedback(participant, now);
                    }
                    break;

                case ParticipantState::Ended:
                    break;
                }
            }

            std::erase_if(m_participants, [](std::unique_ptr<Participant> const& p) { return p->State == ParticipantState::Ended; });
        }

        // A MIDI 1.0 byte stream in any size of piece. Sent to every participant ready for data.
        void SendMidi(uint8_t const* bytes, size_t count, uint64_t now)
        {
            m_encoder.Append(bytes, count);

            auto const lists = m_encoder.TakeLists();
            if (lists.empty()) return;

            for (auto& entry : m_participants)
            {
                if (IsReadyForData(*entry)) SendLists(*entry, lists, now);
            }
        }

        // A MIDI 1.0 byte stream for one participant. False when it is not ready for data.
        bool SendMidiTo(uint32_t participantId, uint8_t const* bytes, size_t count, uint64_t now)
        {
            auto participant = FindById(participantId);
            if (participant == nullptr || !IsReadyForData(*participant)) return false;

            participant->Encoder.Append(bytes, count);

            auto const lists = participant->Encoder.TakeLists();
            if (!lists.empty()) SendLists(*participant, lists, now);

            return true;
        }

        bool TrySnapshot(uint32_t participantId, Participant& snapshot) const
        {
            for (auto const& entry : m_participants)
            {
                if (entry->Id != participantId) continue;

                snapshot = *entry;
                return true;
            }

            return false;
        }

    private:
        static bool IsReadyForData(Participant const& participant)
        {
            return participant.State == ParticipantState::Connected || participant.State == ParticipantState::Synchronizing;
        }

        Participant* FindById(uint32_t id)
        {
            for (auto& entry : m_participants)
            {
                if (entry->Id == id && entry->State != ParticipantState::Ended) return entry.get();
            }

            return nullptr;
        }

        void SendLists(Participant& participant, std::vector<std::vector<uint8_t>> const& lists, uint64_t now)
        {
            auto const timestamp = static_cast<uint32_t>(now);

            for (auto const& list : lists)
            {
                auto const sequence = participant.NextSendSequence;
                std::vector<uint8_t> datagram;

                if (m_config.SendJournal)
                {
                    if (!participant.HaveSentRtp)
                    {
                        // the first packet's checkpoint is itself: an empty history
                        participant.HaveSentRtp = true;
                        participant.JournalCheckpoint = sequence;
                    }

                    // the journal describes the packets before this one, never this one
                    auto const journal = BuildOutgoingJournal(participant, sequence);
                    datagram = BuildRtpMidiPacket(sequence, timestamp, m_config.Ssrc, list, &journal);
                    RecordSentNotes(participant, list, sequence);
                }
                else
                {
                    datagram = BuildRtpMidiPacket(sequence, timestamp, m_config.Ssrc, list);
                }

                participant.NextSendSequence++;
                m_host.SendData(participant.RemoteData, datagram);
                participant.Stats.PacketsSent++;
            }
        }

    public:

        void EndParticipant(uint32_t id, uint64_t now)
        {
            for (auto& entry : m_participants)
            {
                if (entry->Id == id) EndParticipant(*entry, EndReason::LocalRequest, true, now);
            }
        }

        void EndAll(uint64_t now)
        {
            for (auto& entry : m_participants) EndParticipant(*entry, EndReason::LocalRequest, true, now);
        }

        std::vector<Participant> Snapshot() const
        {
            std::vector<Participant> copies;
            for (auto const& entry : m_participants) copies.push_back(*entry);
            return copies;
        }

        size_t ConnectedCount() const
        {
            size_t count = 0;
            for (auto const& entry : m_participants) if (entry->State == ParticipantState::Connected) count++;
            return count;
        }

    private:
        uint32_t NextRandom32()
        {
            // xorshift64*: identifiers only need to be unpredictable enough not to collide
            m_random ^= m_random >> 12;
            m_random ^= m_random << 25;
            m_random ^= m_random >> 27;
            return static_cast<uint32_t>((m_random * 0x2545F4914F6CDD1Dull) >> 32);
        }

        uint64_t SyncInterval(Participant const& participant) const
        {
            auto const& timings = m_config.Timings;

            uint64_t interval = timings.SyncSteadyInterval;

            // the first exchange happens at connection; the fast ones follow it
            if (participant.CompletedSyncs <= timings.SyncFastCount) interval = timings.SyncFastInterval;
            else if (participant.CompletedSyncs <= timings.SyncFastCount + timings.SyncMediumCount) interval = timings.SyncMediumInterval;

            if (participant.PoorSampleRetry) interval = (std::min)(interval, timings.SyncRetryAfterPoorSample);

            return interval;
        }

        void AddClockSample(Participant& participant, int64_t offset, uint64_t roundTrip, uint64_t now)
        {
            auto const& timings = m_config.Timings;
            auto& stats = participant.Stats;

            participant.ClockSamples[participant.ClockSampleCount % participant.ClockSamples.size()] = { offset, roundTrip, now };
            participant.ClockSampleCount++;

            auto const retained = (std::min)(static_cast<size_t>(participant.ClockSampleCount), participant.ClockSamples.size());

            Participant::ClockSample const* best = nullptr;
            int64_t lowest = offset;
            int64_t highest = offset;

            // the sample just added is always young enough, so best is never left empty
            for (size_t i = 0; i < retained; i++)
            {
                auto const& sample = participant.ClockSamples[i];
                if (now - sample.At > timings.ClockSampleMaxAge) continue;

                if (best == nullptr || sample.RoundTrip < best->RoundTrip) best = &sample;
                lowest = (std::min)(lowest, sample.Offset);
                highest = (std::max)(highest, sample.Offset);
            }

            participant.ClockOffset = best->Offset;
            participant.HaveClockOffset = true;

            // a sample far worse than the best one is worth replacing soon
            participant.PoorSampleRetry = roundTrip > best->RoundTrip * 2 + SessionClockTicksPerSecond / 200;

            participant.CompletedSyncs++;
            participant.LastSyncActivity = now;

            stats.RoundTripTicks = roundTrip;
            stats.BestRoundTripTicks = best->RoundTrip;
            stats.ClockOffsetSpreadTicks = static_cast<uint64_t>(highest - lowest);
            stats.SyncExchanges++;
        }

        Participant* FindBySsrc(uint32_t ssrc, PeerAddress const& from)
        {
            for (auto& entry : m_participants)
            {
                auto& participant = *entry;
                if (participant.State == ParticipantState::Ended || participant.RemoteSsrc != ssrc) continue;
                if (from.SameHost(participant.RemoteControl) || from.SameHost(participant.RemoteData)) return &participant;
            }
            return nullptr;
        }

        Participant* FindOurInvitation(uint32_t token, PeerAddress const& from)
        {
            for (auto& entry : m_participants)
            {
                auto& participant = *entry;
                if (participant.State == ParticipantState::Ended || !participant.WeInitiated) continue;
                if (participant.InitiatorToken == token && from.SameHost(participant.RemoteControl)) return &participant;
            }
            return nullptr;
        }

        size_t LiveCount() const
        {
            size_t count = 0;
            for (auto const& entry : m_participants) if (entry->State != ParticipantState::Ended) count++;
            return count;
        }

        void Reply(bool isControlPort, PeerAddress const& to, std::vector<uint8_t> const& datagram)
        {
            if (isControlPort) m_host.SendControl(to, datagram);
            else m_host.SendData(to, datagram);
        }

        // A refusal is the only thing a stranger can make us send, so it is rate limited.
        void Reject(bool isControlPort, PeerAddress const& to, uint32_t token, uint64_t now, char const* why)
        {
            if (now - m_rejectWindowStart >= SessionClockTicksPerSecond)
            {
                m_rejectWindowStart = now;
                m_rejectionsInWindow = 0;
            }

            if (m_rejectionsInWindow >= m_config.Timings.MaxRejectionsPerSecond)
            {
                m_stats.RejectionsSuppressed++;
                return;
            }

            m_rejectionsInWindow++;
            m_stats.RejectionsSent++;

            Reply(isControlPort, to, BuildInvitation(AppleMidiCommand::InvitationRejected, token, m_config.Ssrc, std::string{}));
            m_host.Log(std::string{ "rejected invitation from " } + to.ToString() + ": " + why);
        }

        void SendInvitation(Participant& participant, bool toControlPort, uint64_t now)
        {
            auto const datagram = BuildInvitation(AppleMidiCommand::Invitation, participant.InitiatorToken, m_config.Ssrc, m_config.LocalName);

            if (toControlPort) m_host.SendControl(participant.RemoteControl, datagram);
            else m_host.SendData(participant.RemoteData, datagram);

            participant.LastInvitationSent = now;
            participant.InvitationAttempts++;
        }

        void SendSync0(Participant& participant, uint64_t now)
        {
            m_host.SendData(participant.RemoteData, BuildSynchronization(m_config.Ssrc, 0, { now, 0, 0 }));

            participant.LastSyncSent = now;
            participant.AwaitingSyncReply = true;
        }

        void SendFeedback(Participant& participant, uint64_t now)
        {
            uint32_t const field = m_config.FeedbackSequenceInHighBits ?
                (static_cast<uint32_t>(participant.HighestSequence) << 16) :
                participant.ExtendedHighestSequence();

            m_host.SendControl(participant.RemoteControl, BuildReceiverFeedback(m_config.Ssrc, field));

            participant.LastFeedbackSent = now;
            participant.FeedbackDue = false;
        }

        void Deliver(Participant& participant, uint64_t localTimestamp, int64_t lead, bool recovered, std::vector<uint8_t> const& bytes)
        {
            TrackNotes(participant, bytes);
            participant.Stats.MessagesReceived++;
            m_host.OnMidi(participant, localTimestamp, lead, recovered, bytes);
        }

        void SilenceActiveNotes(Participant& participant, uint64_t when, int64_t lead)
        {
            for (uint8_t channel = 0; channel < 16; channel++)
            {
                for (uint8_t note = 0; note < 128; note++)
                {
                    if (!participant.ActiveNotes[channel][note]) continue;

                    Deliver(participant, when, lead, true, { static_cast<uint8_t>(0x80 | channel), note, 0x40 });
                    participant.Stats.RecoveredNoteOffs++;
                }
            }
        }

        void EndParticipant(Participant& participant, EndReason reason, bool sendEndSession, uint64_t now)
        {
            if (participant.State == ParticipantState::Ended) return;

            if (sendEndSession && participant.RemoteControl.IsSet())
            {
                m_host.SendControl(participant.RemoteControl,
                    BuildInvitation(AppleMidiCommand::EndSession, participant.InitiatorToken, m_config.Ssrc, std::string{}));
            }

            // a session must not end with notes still sounding or a SysEx left open
            std::vector<uint8_t> closing;
            if (participant.Decoder.CloseOpenSysEx(closing)) Deliver(participant, now, 0, true, closing);
            SilenceActiveNotes(participant, now, 0);

            participant.State = ParticipantState::Ended;
            participant.Reason = reason;

            m_host.OnParticipantChanged(participant);
        }

        void HandleCommand(bool isControlPort, PeerAddress const& from, AppleMidiCommand command, uint8_t const* data, size_t size, uint64_t now)
        {
            switch (command)
            {
            case AppleMidiCommand::Invitation:
                if (auto message = ParseInvitation(data, size)) OnInvitation(isControlPort, from, *message, now);
                else m_stats.MalformedCommands++;
                break;

            case AppleMidiCommand::InvitationAccepted:
                if (auto message = ParseInvitation(data, size)) OnAccepted(isControlPort, from, *message, now);
                else m_stats.MalformedCommands++;
                break;

            case AppleMidiCommand::InvitationRejected:
                if (auto message = ParseInvitation(data, size))
                {
                    if (auto participant = FindOurInvitation(message->InitiatorToken, from))
                    {
                        EndParticipant(*participant, EndReason::Rejected, false, now);
                    }
                }
                else m_stats.MalformedCommands++;
                break;

            case AppleMidiCommand::EndSession:
                if (auto message = ParseInvitation(data, size))
                {
                    auto participant = FindBySsrc(message->Ssrc, from);
                    if (participant == nullptr) participant = FindOurInvitation(message->InitiatorToken, from);
                    if (participant != nullptr) EndParticipant(*participant, EndReason::RemoteEndedSession, false, now);
                }
                else m_stats.MalformedCommands++;
                break;

            case AppleMidiCommand::Synchronization:
                if (auto message = ParseSynchronization(data, size)) OnSynchronization(isControlPort, from, *message, now);
                else m_stats.MalformedCommands++;
                break;

            case AppleMidiCommand::ReceiverFeedback:
                if (auto message = ParseReceiverFeedback(data, size))
                {
                    if (auto participant = FindBySsrc(message->Ssrc, from))
                    {
                        participant->Stats.FeedbackReceived++;
                        participant->Stats.LastFeedbackField = message->SequenceField;
                        OnFeedback(*participant, message->SequenceField);
                    }
                }
                else m_stats.MalformedCommands++;
                break;

            case AppleMidiCommand::BitrateReceiveLimit:
                if (auto message = ParseBitrateLimit(data, size))
                {
                    if (auto participant = FindBySsrc(message->Ssrc, from))
                    {
                        participant->Stats.BitrateLimit = message->BitsPerSecond;
                        m_host.Log(participant->RemoteName + " asked for a bit rate limit of " + std::to_string(message->BitsPerSecond));
                    }
                }
                else m_stats.MalformedCommands++;
                break;
            }
        }

        void OnInvitation(bool isControlPort, PeerAddress const& from, AppleMidiInvitation const& message, uint64_t now)
        {
            if (message.ProtocolVersion != AppleMidiProtocolVersion)
            {
                Reject(isControlPort, from, message.InitiatorToken, now, "unsupported protocol version");
                return;
            }

            if (!isControlPort)
            {
                auto participant = FindBySsrc(message.Ssrc, from);

                if (participant == nullptr || participant->WeInitiated || participant->InitiatorToken != message.InitiatorToken)
                {
                    Reject(false, from, message.InitiatorToken, now, "data invitation without a control invitation");
                    return;
                }

                participant->RemoteData = from;
                Reply(false, from, BuildInvitation(AppleMidiCommand::InvitationAccepted, message.InitiatorToken, m_config.Ssrc, m_config.LocalName));

                if (participant->State == ParticipantState::AwaitingDataInvitation)
                {
                    participant->State = ParticipantState::Connected;
                    participant->LastSyncActivity = now;
                    m_host.OnParticipantChanged(*participant);
                }

                return;
            }

            if (auto existing = FindBySsrc(message.Ssrc, from))
            {
                // our OK was lost and they asked again: answer again, nothing else changes
                if (!existing->WeInitiated &&
                    existing->State == ParticipantState::AwaitingDataInvitation &&
                    existing->InitiatorToken == message.InitiatorToken)
                {
                    Reply(true, from, BuildInvitation(AppleMidiCommand::InvitationAccepted, message.InitiatorToken, m_config.Ssrc, m_config.LocalName));
                    return;
                }

                EndParticipant(*existing, EndReason::Replaced, false, now);
            }

            // A peer that restarted invites again with a new SSRC. Same host and same name means
            // the old participant is dead; waiting out its sync timeout would leave two.
            for (auto& entry : m_participants)
            {
                auto& stale = *entry;
                if (stale.State == ParticipantState::Ended || stale.WeInitiated) continue;
                if (!from.SameHost(stale.RemoteControl) || stale.RemoteName != message.Name) continue;

                EndParticipant(stale, EndReason::Replaced, false, now);
            }

            if (!m_config.AcceptInvitations)
            {
                Reject(true, from, message.InitiatorToken, now, "this session is not accepting invitations");
                return;
            }

            if (LiveCount() >= m_config.MaxParticipants)
            {
                Reject(true, from, message.InitiatorToken, now, "too many participants");
                return;
            }

            if (m_config.Admit && !m_config.Admit(message.Name, from))
            {
                Reject(true, from, message.InitiatorToken, now, "not admitted by policy");
                return;
            }

            auto participant = std::make_unique<Participant>();
            participant->Id = ++m_nextId;
            participant->WeInitiated = false;
            participant->State = ParticipantState::AwaitingDataInvitation;
            participant->RemoteName = message.Name;
            participant->RemoteSsrc = message.Ssrc;
            participant->InitiatorToken = message.InitiatorToken;
            participant->RemoteControl = from;
            participant->RemoteData = from.WithPort(static_cast<uint16_t>(from.Port + 1));
            participant->Created = now;
            participant->LastSyncActivity = now;
            participant->NextSendSequence = static_cast<uint16_t>(NextRandom32());
            participant->Encoder = CommandSectionEncoder{ m_config.MaxListBytes };

            auto& added = *participant;
            m_participants.push_back(std::move(participant));

            Reply(true, from, BuildInvitation(AppleMidiCommand::InvitationAccepted, message.InitiatorToken, m_config.Ssrc, m_config.LocalName));
            m_host.OnParticipantChanged(added);
        }

        void OnAccepted(bool isControlPort, PeerAddress const& from, AppleMidiInvitation const& message, uint64_t now)
        {
            auto participant = FindOurInvitation(message.InitiatorToken, from);
            if (participant == nullptr) return;

            if (isControlPort && participant->State == ParticipantState::InvitingControl)
            {
                participant->RemoteSsrc = message.Ssrc;
                participant->RemoteName = message.Name;
                participant->State = ParticipantState::InvitingData;
                participant->InvitationAttempts = 0;

                SendInvitation(*participant, false, now);
                m_host.OnParticipantChanged(*participant);
            }
            else if (!isControlPort && participant->State == ParticipantState::InvitingData)
            {
                participant->RemoteData = from;
                participant->State = ParticipantState::Synchronizing;
                participant->LastSyncActivity = now;

                SendSync0(*participant, now);
                m_host.OnParticipantChanged(*participant);
            }
        }

        void OnSynchronization(bool isControlPort, PeerAddress const& from, AppleMidiSynchronization const& message, uint64_t now)
        {
            auto participant = FindBySsrc(message.Ssrc, from);
            if (participant == nullptr) return;

            auto const& timings = m_config.Timings;
            auto const& t = message.Timestamps;

            switch (message.Count)
            {
            case 0:
                Reply(isControlPort, from, BuildSynchronization(m_config.Ssrc, 1, { t[0], now, 0 }));
                participant->LastSyncReplyStamp = now;
                participant->AwaitingPeerSync2 = true;
                participant->LastSyncActivity = now;
                break;

            case 1:
            {
                // only an answer to the CK0 we sent last counts
                if (!participant->AwaitingSyncReply || t[0] != participant->LastSyncSent || now < t[0]) return;

                Reply(isControlPort, from, BuildSynchronization(m_config.Ssrc, 2, { t[0], t[1], now }));

                participant->AwaitingSyncReply = false;
                participant->UnansweredSyncs = 0;

                auto const roundTrip = now - t[0];
                if (roundTrip <= timings.SyncReplyTimeout) AddClockSample(*participant, ClockOffsetFromInitiatorSide(t[0], t[1], now), roundTrip, now);

                if (participant->State == ParticipantState::Synchronizing)
                {
                    participant->State = ParticipantState::Connected;
                    m_host.OnParticipantChanged(*participant);
                }
                break;
            }

            case 2:
            {
                // the end of an exchange the peer started: it must echo the t1 we sent
                if (!participant->AwaitingPeerSync2 || t[1] != participant->LastSyncReplyStamp || t[2] < t[0]) return;

                participant->AwaitingPeerSync2 = false;

                auto const roundTrip = t[2] - t[0];
                if (roundTrip > timings.SyncReplyTimeout) return;

                participant->Stats.SyncExchangesStartedByPeer++;
                AddClockSample(*participant, ClockOffsetFromResponderSide(t[0], t[1], t[2]), roundTrip, now);
                break;
            }

            default:
                break;
            }
        }

        std::pair<uint64_t, int64_t> MapTimestamp(Participant const& participant, uint32_t rtpTimestamp, uint64_t now) const
        {
            if (!participant.HaveClockOffset) return { now, 0 };

            auto const senderNow = static_cast<uint64_t>(static_cast<int64_t>(now) + participant.ClockOffset);
            auto const sender = UnwrapTimestamp(rtpTimestamp, senderNow);
            auto const local = static_cast<int64_t>(sender) - participant.ClockOffset;

            return { static_cast<uint64_t>(local), local - static_cast<int64_t>(now) };
        }

        static void TrackNoteState(std::array<std::array<bool, 128>, 16>& notes, std::vector<uint8_t> const& bytes)
        {
            if (bytes.size() < 3) return;

            auto const status = bytes[0];
            auto const channel = status & 0x0F;

            switch (status & 0xF0)
            {
            case 0x90:
                notes[channel][bytes[1] & 0x7F] = bytes[2] != 0;
                break;
            case 0x80:
                notes[channel][bytes[1] & 0x7F] = false;
                break;
            case 0xB0:
                // All Sound Off and All Notes Off
                if (bytes[1] == 120 || bytes[1] == 123) notes[channel].fill(false);
                break;
            default:
                break;
            }
        }

        static void TrackNotes(Participant& participant, std::vector<uint8_t> const& bytes)
        {
            TrackNoteState(participant.ActiveNotes, bytes);
        }

        void RepairFromJournal(Participant& participant, DecodedPacket const& packet, uint8_t const* datagram, uint16_t previousHighest, uint64_t when, int64_t lead)
        {
            participant.Stats.LossEvents++;

            bool covered = false;

            if (packet.HasJournal)
            {
                participant.Stats.JournalsSeen++;

                RecoveryJournal journal{};

                if (ParseRecoveryJournal(datagram + packet.JournalOffset, packet.JournalSize, journal))
                {
                    // it covers the loss if its history starts at or before the first packet lost
                    auto const firstLost = static_cast<uint16_t>(previousHighest + 1);
                    covered = static_cast<int16_t>(journal.CheckpointSequence - firstLost) <= 0;

                    if (covered)
                    {
                        participant.Stats.LossEventsCovered++;

                        for (auto const& channel : journal.Channels)
                        {
                            for (auto const note : channel.NoteOffs)
                            {
                                if (!participant.ActiveNotes[channel.Channel][note]) continue;

                                Deliver(participant, when, lead, true, { static_cast<uint8_t>(0x80 | channel.Channel), note, 0x40 });
                                participant.Stats.RecoveredNoteOffs++;
                            }
                        }
                    }
                }
                else
                {
                    participant.Stats.JournalsMalformed++;
                }
            }

            // no usable history: stuck notes are worse than cut ones (RFC 6295 C.2.2.3)
            if (!covered) SilenceActiveNotes(participant, when, lead);
        }

        void HandleRtp(PeerAddress const& from, uint8_t const* data, size_t size, uint64_t now)
        {
            RtpHeader header{};

            if (!ParseRtpHeader(data, size, header) || header.PayloadType != RtpMidiPayloadType)
            {
                if (m_stats.UnknownDatagrams++ < 5) m_host.Log("ignored a data port datagram from " + from.ToString() + " that is not RTP-MIDI");
                return;
            }

            auto participant = FindBySsrc(header.Ssrc, from);

            if (participant == nullptr ||
                (participant->State != ParticipantState::Connected && participant->State != ParticipantState::Synchronizing))
            {
                if (m_stats.UnexpectedRtp++ < 5)
                {
                    char ssrc[16]{};
                    snprintf(ssrc, sizeof(ssrc), "%08X", header.Ssrc);
                    m_host.Log("ignored RTP-MIDI from " + from.ToString() + " ssrc " + ssrc + ": no connected participant matches");
                }
                return;
            }

            auto& stats = participant->Stats;

            if (header.Marker) stats.MarkerBitPackets++;

            bool lossEvent = false;
            uint16_t previousHighest = participant->HighestSequence;

            if (!participant->HaveReceivedRtp)
            {
                participant->HaveReceivedRtp = true;
                participant->HighestSequence = header.SequenceNumber;
            }
            else
            {
                auto const delta = static_cast<int16_t>(header.SequenceNumber - participant->HighestSequence);

                // late or repeated: ignoring it is the safe option (RFC 6295 section 4)
                if (delta <= 0)
                {
                    stats.PacketsOutOfOrder++;
                    return;
                }

                if (delta > 1)
                {
                    lossEvent = true;
                    stats.PacketsLost += static_cast<uint64_t>(delta - 1);
                }

                if (header.SequenceNumber < participant->HighestSequence) participant->SequenceCycles++;
                participant->HighestSequence = header.SequenceNumber;
            }

            stats.PacketsReceived++;
            participant->FeedbackDue = true;

            // a SysEx cut off by the loss has to be closed before this packet is decoded
            std::vector<uint8_t> closing;
            bool const closedSysEx = lossEvent && participant->Decoder.CloseOpenSysEx(closing);

            DecodedPacket packet{};
            bool const decoded = participant->Decoder.Decode(data, size, packet) == DecodeStatus::Ok;

            // Repairs stand for what came before this packet, so they must not be stamped after its
            // first event, or a timestamp-ordered consumer ends a note the packet just restarted.
            uint64_t repairTime = now;
            if (decoded && !packet.Events.empty()) repairTime = (std::min)(now, MapTimestamp(*participant, packet.Events.front().Timestamp, now).first);
            auto const repairLead = static_cast<int64_t>(repairTime) - static_cast<int64_t>(now);

            if (closedSysEx) Deliver(*participant, repairTime, repairLead, true, closing);

            if (!decoded)
            {
                stats.MalformedPackets++;

                if (lossEvent)
                {
                    stats.LossEvents++;
                    SilenceActiveNotes(*participant, repairTime, repairLead);
                }
                return;
            }

            stats.MalformedCommands += packet.MalformedCommands;
            stats.DroppedSysExSegments += packet.DroppedSysExSegments;

            m_host.OnRtpPacket(*participant, packet, data, size);

            // the journal describes what came before this packet, so repairs go first
            if (lossEvent) RepairFromJournal(*participant, packet, data, previousHighest, repairTime, repairLead);
            else if (packet.HasJournal) stats.JournalsSeen++;

            for (auto const& event : packet.Events)
            {
                auto const [local, lead] = MapTimestamp(*participant, event.Timestamp, now);
                Deliver(*participant, local, lead, false, event.Bytes);
            }
        }

        // Closed loop: the journal covers everything since the last packet the peer confirmed.
        std::vector<uint8_t> BuildOutgoingJournal(Participant const& participant, uint16_t sequence) const
        {
            auto const previous = static_cast<uint16_t>(sequence - 1);

            std::vector<ChannelJournal> channels;

            for (uint8_t channel = 0; channel < 16; channel++)
            {
                ChannelJournal journal{};
                journal.Channel = channel;

                for (uint8_t note = 0; note < 128; note++)
                {
                    auto const& sent = participant.SentNotes[channel][note];
                    if (!sent.Touched) continue;

                    bool const inPrevious = sent.Sequence == previous;

                    if (sent.On)
                    {
                        journal.NoteOns.push_back(JournalNoteOn{ note, sent.Velocity, true, inPrevious });
                    }
                    else
                    {
                        journal.NoteOffs.push_back(note);
                        if (inPrevious) journal.NoteOffInPreviousPacket = true;
                    }
                }

                if (!journal.NoteOns.empty() || !journal.NoteOffs.empty()) channels.push_back(std::move(journal));
            }

            return BuildRecoveryJournal(participant.JournalCheckpoint, channels);
        }

        void RecordSentNotes(Participant& participant, std::vector<uint8_t> const& list, uint16_t sequence)
        {
            // walk the list we just built: status bytes are always present, deltas are single zeros
            size_t position = 0;

            while (position < list.size())
            {
                auto const status = list[position];
                auto const dataBytes = DataByteCount(status);

                if (dataBytes == SysExLength)
                {
                    size_t scan = position + 1;
                    while (scan < list.size() && list[scan] < 0x80) scan++;
                    position = scan + 1;
                }
                else if (dataBytes >= 0)
                {
                    if ((status & 0xE0) == 0x80 && position + 2 < list.size())
                    {
                        auto const channel = status & 0x0F;
                        auto const note = list[position + 1] & 0x7F;
                        auto const velocity = static_cast<uint8_t>(list[position + 2] & 0x7F);

                        auto& sent = participant.SentNotes[channel][note];
                        sent.Touched = true;
                        sent.On = (status & 0xF0) == 0x90 && velocity != 0;
                        sent.Velocity = velocity;
                        sent.Sequence = sequence;
                    }
                    else if ((status & 0xF0) == 0xB0 && position + 2 < list.size() && (list[position + 1] == 120 || list[position + 1] == 123))
                    {
                        // All Sound Off and All Notes Off end every note on the channel
                        for (auto& sent : participant.SentNotes[status & 0x0F])
                        {
                            if (sent.Touched && sent.On) { sent.On = false; sent.Sequence = sequence; }
                        }
                    }

                    position += 1 + static_cast<size_t>(dataBytes);
                }
                else
                {
                    break;
                }

                if (position < list.size()) position++;
            }
        }

        // RS confirms receipt through a sequence number. Its layout differs between peers, so the
        // half that falls inside the window of packets actually sent is the one believed.
        void OnFeedback(Participant& participant, uint32_t field)
        {
            if (!participant.HaveSentRtp) return;

            auto const lastSent = static_cast<uint16_t>(participant.NextSendSequence - 1);
            auto const oldestAllowed = static_cast<uint16_t>(participant.JournalCheckpoint - 1);

            // distance behind the last packet sent, or -1 if outside [checkpoint - 1, last sent]
            auto const distance = [&](uint16_t candidate) -> int
            {
                auto const behindLast = static_cast<int16_t>(lastSent - candidate);
                auto const pastOldest = static_cast<int16_t>(candidate - oldestAllowed);
                return (behindLast >= 0 && pastOldest >= 0) ? behindLast : -1;
            };

            auto const low = static_cast<uint16_t>(field & 0xFFFF);
            auto const high = static_cast<uint16_t>(field >> 16);

            auto const lowDistance = distance(low);
            auto const highDistance = distance(high);

            // A receiver confirms the newest packet it has, so the closer candidate wins. The
            // other half is a rollover count or zero padding, depending on the peer.
            uint16_t acknowledged{ 0 };

            if (lowDistance >= 0 && (highDistance < 0 || lowDistance <= highDistance)) acknowledged = low;
            else if (highDistance >= 0) acknowledged = high;
            else return;

            auto const checkpoint = static_cast<uint16_t>(acknowledged + 1);
            participant.JournalCheckpoint = checkpoint;
            participant.FeedbackTrims++;

            // anything that changed at or before the confirmed packet no longer needs protecting
            for (auto& channel : participant.SentNotes)
            {
                for (auto& sent : channel)
                {
                    if (sent.Touched && static_cast<int16_t>(sent.Sequence - checkpoint) < 0) sent.Touched = false;
                }
            }
        }

        SessionConfig m_config;
        ISessionHost& m_host;
        uint64_t m_random{ 0 };
        uint32_t m_nextId{ 0 };
        std::vector<std::unique_ptr<Participant>> m_participants;
        CommandSectionEncoder m_encoder;
        SessionStats m_stats{};
        uint64_t m_rejectWindowStart{ 0 };
        uint32_t m_rejectionsInWindow{ 0 };
    };
}
