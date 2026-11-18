// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#include "pch.h"
#include "LaunchpadXController.h"

namespace midisequencer::launchpad
{
    _Use_decl_annotations_
    LaunchpadXSessionTransport::LaunchpadXSessionTransport(midi2::MidiSession const& session) noexcept :
        m_session(session)
    {
    }

    LaunchpadXSessionTransport::~LaunchpadXSessionTransport()
    {
        Close();
    }

    _Use_decl_annotations_
    LaunchpadEndpointCandidate LaunchpadXSessionTransport::BuildCandidate(
        midiapp::LiveEndpoint const& live,
        uint16_t sourceGroupsOffMask,
        uint16_t destinationGroupsOffMask) noexcept
    {
        LaunchpadEndpointCandidate candidate{};

        try
        {
            candidate.endpointDeviceId = live.EndpointDeviceId;
            candidate.name = live.Name;
            candidate.transportSuppliedName = live.TransportSuppliedName;
            candidate.usbVendorId = live.UsbVendorId;
            candidate.usbProductId = live.UsbProductId;
            candidate.isLoopback = live.IsLoopback;

            for (size_t g = 0; g < 16; ++g)
            {
                uint16_t const bit = static_cast<uint16_t>(1u << g);
                candidate.sourceGroups[g] = live.SourceGroups[g] && ((sourceGroupsOffMask & bit) == 0);
                candidate.destinationGroups[g] = live.DestinationGroups[g] && ((destinationGroupsOffMask & bit) == 0);
                candidate.sourcePortNames[g] = live.SourcePortNames[g];
                candidate.destinationPortNames[g] = live.DestinationPortNames[g];
                candidate.sourceGroupNames[g] = live.SourceGroupNames[g];
                candidate.destinationGroupNames[g] = live.DestinationGroupNames[g];
            }
        }
        catch (...)
        {
        }

        return candidate;
    }

    _Use_decl_annotations_
    bool LaunchpadXSessionTransport::Open(
        std::wstring const& endpointDeviceId,
        uint8_t group,
        OnUmpCallback onUmp,
        OnStateCallback onState) noexcept
    {
        Close();

        try
        {
            if (m_session == nullptr || endpointDeviceId.empty())
            {
                return false;
            }

            auto const connection = m_session.CreateEndpointConnection(winrt::hstring{ endpointDeviceId });
            if (connection == nullptr)
            {
                return false;
            }

            {
                std::scoped_lock guard{ m_lock };
                m_endpointDeviceId = endpointDeviceId;
                m_group = static_cast<uint8_t>(group & 0x0Fu);
                m_onUmp = std::move(onUmp);
                m_onState = std::move(onState);
            }

            std::weak_ptr<LaunchpadXSessionTransport> const weak = weak_from_this();

            auto const msgToken = connection.MessageReceived([weak](auto&&, midi2::MidiMessageReceivedEventArgs const& args)
            {
                if (auto self = weak.lock())
                {
                    self->DeliverMessage(args);
                }
            });

            auto const discToken = connection.EndpointDeviceDisconnected([weak](auto&&, auto&&)
            {
                if (auto self = weak.lock())
                {
                    self->DeliverState(false);
                }
            });

            auto const reconnToken = connection.EndpointDeviceReconnected([weak](auto&&, auto&&)
            {
                if (auto self = weak.lock())
                {
                    self->DeliverState(true);
                }
            });

            if (!connection.Open())
            {
                try
                {
                    connection.MessageReceived(msgToken);
                    connection.EndpointDeviceDisconnected(discToken);
                    connection.EndpointDeviceReconnected(reconnToken);
                    m_session.DisconnectEndpointConnection(connection.ConnectionId());
                }
                catch (...)
                {
                }

                std::scoped_lock guard{ m_lock };
                m_endpointDeviceId.clear();
                m_onUmp = nullptr;
                m_onState = nullptr;
                return false;
            }

            uint32_t maxWords = connection.GetSupportedMaxMidiWordsPerTransmission();
            if (maxWords < 4)
            {
                maxWords = 4;
            }

            std::scoped_lock guard{ m_lock };
            m_connection = connection;
            m_messageToken = msgToken;
            m_disconnectedToken = discToken;
            m_reconnectedToken = reconnToken;
            m_maxWordsPerTransmission = maxWords;
            return true;
        }
        catch (...)
        {
            LOG_CAUGHT_EXCEPTION();
            Close();
            return false;
        }
    }

    void LaunchpadXSessionTransport::Close() noexcept
    {
        midi2::MidiEndpointConnection connection{ nullptr };
        winrt::event_token msgToken{};
        winrt::event_token discToken{};
        winrt::event_token reconnToken{};

        {
            std::scoped_lock guard{ m_lock };
            connection = std::exchange(m_connection, nullptr);
            msgToken = std::exchange(m_messageToken, {});
            discToken = std::exchange(m_disconnectedToken, {});
            reconnToken = std::exchange(m_reconnectedToken, {});
            m_endpointDeviceId.clear();
            m_group = 0;
            m_onUmp = nullptr;
            m_onState = nullptr;
        }

        if (connection == nullptr)
        {
            return;
        }

        try
        {
            if (msgToken)
            {
                connection.MessageReceived(msgToken);
            }

            if (discToken)
            {
                connection.EndpointDeviceDisconnected(discToken);
            }

            if (reconnToken)
            {
                connection.EndpointDeviceReconnected(reconnToken);
            }

            if (m_session != nullptr)
            {
                m_session.DisconnectEndpointConnection(connection.ConnectionId());
            }
        }
        catch (...)
        {
        }
    }

    _Use_decl_annotations_
    bool LaunchpadXSessionTransport::SendUmpPackets(std::span<const UmpPacket> packets) noexcept
    {
        if (packets.empty())
        {
            return true;
        }

        midi2::MidiEndpointConnection connection{ nullptr };
        uint32_t maxWords = 684;

        {
            std::scoped_lock guard{ m_lock };
            connection = m_connection;
            maxWords = m_maxWordsPerTransmission;
        }

        if (connection == nullptr)
        {
            return false;
        }

        try
        {
            if (packets.size() == 1)
            {
                auto const& packet = packets[0];
                midi2::MidiSendMessageResults result = midi2::MidiSendMessageResults::Failed;

                switch (packet.wordCount)
                {
                case 1:
                    result = connection.SendSingleMessageWords(0, packet.words[0]);
                    break;
                case 2:
                    result = connection.SendSingleMessageWords(0, packet.words[0], packet.words[1]);
                    break;
                case 3:
                    result = connection.SendSingleMessageWords(0, packet.words[0], packet.words[1], packet.words[2]);
                    break;
                case 4:
                    result = connection.SendSingleMessageWords(0, packet.words[0], packet.words[1], packet.words[2], packet.words[3]);
                    break;
                default:
                    return false;
                }

                return midi2::MidiEndpointConnection::SendMessageSucceeded(result);
            }

            std::vector<uint32_t> chunkWords{};
            chunkWords.reserve(std::min<size_t>(packets.size() * 2, maxWords));

            for (auto const& packet : packets)
            {
                if (packet.wordCount == 0 || packet.wordCount > 4)
                {
                    return false;
                }

                if (!chunkWords.empty() && chunkWords.size() + packet.wordCount > maxWords)
                {
                    auto const result = connection.SendMultipleMessagesWordArray(
                        0,
                        0,
                        static_cast<uint32_t>(chunkWords.size()),
                        winrt::array_view<uint32_t const>{ chunkWords.data(), chunkWords.data() + chunkWords.size() });

                    if (!midi2::MidiEndpointConnection::SendMessageSucceeded(result))
                    {
                        return false;
                    }

                    chunkWords.clear();
                }

                for (uint8_t i = 0; i < packet.wordCount; ++i)
                {
                    chunkWords.push_back(packet.words[i]);
                }
            }

            if (!chunkWords.empty())
            {
                auto const result = connection.SendMultipleMessagesWordArray(
                    0,
                    0,
                    static_cast<uint32_t>(chunkWords.size()),
                    winrt::array_view<uint32_t const>{ chunkWords.data(), chunkWords.data() + chunkWords.size() });

                if (!midi2::MidiEndpointConnection::SendMessageSucceeded(result))
                {
                    return false;
                }
            }

            return true;
        }
        catch (...)
        {
            return false;
        }
    }

    std::wstring LaunchpadXSessionTransport::EndpointDeviceId() const
    {
        std::scoped_lock guard{ m_lock };
        return m_endpointDeviceId;
    }

    uint8_t LaunchpadXSessionTransport::Group() const noexcept
    {
        std::scoped_lock guard{ m_lock };
        return m_group;
    }

    _Use_decl_annotations_
    void LaunchpadXSessionTransport::DeliverMessage(midi2::MidiMessageReceivedEventArgs const& args) noexcept
    {
        try
        {
            uint32_t words[4]{};
            auto const count = args.FillWords(words[0], words[1], words[2], words[3]);
            if (count == 0 || count > 4)
            {
                return;
            }

            OnUmpCallback callback{};
            {
                std::scoped_lock guard{ m_lock };
                callback = m_onUmp;
            }

            if (callback)
            {
                callback(words, count, args.Timestamp());
            }
        }
        catch (...)
        {
        }
    }

    _Use_decl_annotations_
    void LaunchpadXSessionTransport::DeliverState(bool reconnected) noexcept
    {
        try
        {
            OnStateCallback callback{};
            {
                std::scoped_lock guard{ m_lock };
                callback = m_onState;
            }

            if (callback)
            {
                callback(reconnected);
            }
        }
        catch (...)
        {
        }
    }
}
