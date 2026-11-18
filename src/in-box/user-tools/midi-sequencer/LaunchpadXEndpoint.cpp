// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#include "pch.h"
#include "LaunchpadXEndpoint.h"

namespace midisequencer
{
    _Use_decl_annotations_
    LaunchpadXEndpoint::LaunchpadXEndpoint(midi2::MidiSession const& session) :
        m_session(session)
    {
    }

    LaunchpadXEndpoint::~LaunchpadXEndpoint()
    {
        Close();
    }

    _Use_decl_annotations_
    bool LaunchpadXEndpoint::Open(std::wstring const& endpointId, Handler handler)
    {
        Close();

        try
        {
            if (m_session == nullptr || endpointId.empty())
            {
                return false;
            }

            auto const connection = m_session.CreateEndpointConnection(winrt::hstring{ endpointId });

            if (connection == nullptr)
            {
                return false;
            }

            {
                std::scoped_lock guard{ m_lock };
                m_handler = std::move(handler);
            }

            // Weak, so a message still on its way when this is closed and freed finds nothing.
            auto const token = connection.MessageReceived([weak = weak_from_this()](auto&&, midi2::MidiMessageReceivedEventArgs const& args)
            {
                if (auto self = weak.lock())
                {
                    self->Deliver(args);
                }
            });

            if (!connection.Open())
            {
                connection.MessageReceived(token);
                m_session.DisconnectEndpointConnection(connection.ConnectionId());

                std::scoped_lock guard{ m_lock };
                m_handler = nullptr;
                return false;
            }

            std::scoped_lock guard{ m_lock };
            m_connection = connection;
            m_token = token;
            m_endpointId = endpointId;
            return true;
        }
        catch (...)
        {
            LOG_CAUGHT_EXCEPTION();
            return false;
        }
    }

    void LaunchpadXEndpoint::Close() noexcept
    {
        midi2::MidiEndpointConnection connection{ nullptr };
        winrt::event_token token{};

        try
        {
            std::scoped_lock guard{ m_lock };
            connection = std::exchange(m_connection, nullptr);
            token = std::exchange(m_token, {});
            m_handler = nullptr;
            m_endpointId.clear();
        }
        catch (...)
        {
        }

        if (connection == nullptr)
        {
            return;
        }

        try
        {
            connection.MessageReceived(token);

            if (m_session != nullptr)
            {
                m_session.DisconnectEndpointConnection(connection.ConnectionId());
            }
        }
        catch (...)
        {
        }
    }

    std::wstring LaunchpadXEndpoint::EndpointId() const
    {
        std::scoped_lock guard{ m_lock };
        return m_endpointId;
    }

    _Use_decl_annotations_
    bool LaunchpadXEndpoint::SendUmpPackets(std::span<const launchpad::UmpPacket> packets) noexcept
    {
        try
        {
            midi2::MidiEndpointConnection connection{ nullptr };

            {
                std::scoped_lock guard{ m_lock };
                connection = m_connection;
            }

            if (connection == nullptr)
            {
                return false;
            }

            for (auto const& packet : packets)
            {
                midi2::MidiSendMessageResults result{};

                switch (packet.wordCount)
                {
                case 1: result = connection.SendSingleMessageWords(0, packet.words[0]); break;
                case 2: result = connection.SendSingleMessageWords(0, packet.words[0], packet.words[1]); break;
                case 3: result = connection.SendSingleMessageWords(0, packet.words[0], packet.words[1], packet.words[2]); break;
                case 4: result = connection.SendSingleMessageWords(0, packet.words[0], packet.words[1], packet.words[2], packet.words[3]); break;
                default: return false;
                }

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

    _Use_decl_annotations_
    void LaunchpadXEndpoint::Deliver(midi2::MidiMessageReceivedEventArgs const& args) noexcept
    {
        try
        {
            uint32_t words[4]{};
            auto const count = args.FillWords(words[0], words[1], words[2], words[3]);

            if (count == 0 || count > 4)
            {
                return;
            }

            Handler handler{};

            {
                std::scoped_lock guard{ m_lock };
                handler = m_handler;
            }

            if (handler)
            {
                handler(words, count, args.Timestamp());
            }
        }
        catch (...)
        {
            // The service's thread must never see an exception.
        }
    }
}
