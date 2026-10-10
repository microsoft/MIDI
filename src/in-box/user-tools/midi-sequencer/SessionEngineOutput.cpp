// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#include "SessionEngineOutput.h"

namespace midi2 = winrt::Windows::Devices::Midi2;

namespace midisequencer
{
    _Use_decl_annotations_
    SessionEngineOutput::SessionEngineOutput(midi2::MidiSession const& session) :
        m_session(session)
    {
    }

    SessionEngineOutput::~SessionEngineOutput()
    {
        CloseAll();
    }

    _Use_decl_annotations_
    void SessionEngineOutput::SetResolver(std::function<std::wstring(EndpointRef const&)> resolver)
    {
        std::scoped_lock guard{ m_lock };
        m_resolver = std::move(resolver);
    }

    _Use_decl_annotations_
    std::wstring SessionEngineOutput::Resolve(EndpointRef const& endpoint) const
    {
        return m_resolver ? m_resolver(endpoint) : endpoint.Id;
    }

    _Use_decl_annotations_
    void SessionEngineOutput::Prepare(std::vector<EndpointRef> const& endpoints)
    {
        std::map<std::pair<std::wstring, std::wstring>, midi2::MidiEndpointConnection> keep{};
        std::vector<std::pair<std::pair<std::wstring, std::wstring>, std::wstring>> open{};

        {
            std::scoped_lock guard{ m_lock };

            for (auto const& endpoint : endpoints)
            {
                auto const key = std::make_pair(endpoint.Name, endpoint.Id);
                auto found = m_connections.find(key);

                if (found != m_connections.end())
                {
                    keep.emplace(key, found->second);
                }
                else if (!keep.contains(key))
                {
                    open.emplace_back(key, Resolve(endpoint));
                }
            }
        }

        // Opening happens without the lock, so Send carries on meanwhile.
        for (auto const& [key, deviceId] : open)
        {
            if (deviceId.empty() || m_session == nullptr)
            {
                continue;
            }

            try
            {
                auto connection = m_session.CreateEndpointConnection(deviceId);

                if (connection != nullptr && connection.Open())
                {
                    keep.emplace(key, connection);
                }
            }
            catch (...)
            {
                // A device that can't be opened stays silent, like one that's unplugged.
            }
        }

        std::vector<midi2::MidiEndpointConnection> closing{};

        {
            std::scoped_lock guard{ m_lock };

            for (auto const& [key, connection] : m_connections)
            {
                if (!keep.contains(key))
                {
                    closing.push_back(connection);
                }
            }

            m_connections = std::move(keep);
        }

        for (auto const& connection : closing)
        {
            try
            {
                m_session.DisconnectEndpointConnection(connection.ConnectionId());
            }
            catch (...)
            {
            }
        }
    }

    void SessionEngineOutput::CloseAll() noexcept
    {
        std::map<std::pair<std::wstring, std::wstring>, midi2::MidiEndpointConnection> closing{};

        {
            std::scoped_lock guard{ m_lock };
            closing.swap(m_connections);
        }

        for (auto const& [key, connection] : closing)
        {
            try
            {
                m_session.DisconnectEndpointConnection(connection.ConnectionId());
            }
            catch (...)
            {
            }
        }
    }

    _Use_decl_annotations_
    void SessionEngineOutput::Send(EndpointRef const& endpoint, uint64_t timestamp, uint32_t const* words, uint8_t wordCount) noexcept
    {
        try
        {
            midi2::MidiEndpointConnection connection{ nullptr };

            {
                std::scoped_lock guard{ m_lock };
                auto found = m_connections.find(std::make_pair(endpoint.Name, endpoint.Id));

                if (found != m_connections.end())
                {
                    connection = found->second;
                }
            }

            if (connection == nullptr)
            {
                ++m_dropped;
                return;
            }

            midi2::MidiSendMessageResults result{};

            switch (wordCount)
            {
            case 1: result = connection.SendSingleMessageWords(timestamp, words[0]); break;
            case 2: result = connection.SendSingleMessageWords(timestamp, words[0], words[1]); break;
            case 3: result = connection.SendSingleMessageWords(timestamp, words[0], words[1], words[2]); break;
            case 4: result = connection.SendSingleMessageWords(timestamp, words[0], words[1], words[2], words[3]); break;
            default: ++m_dropped; return;
            }

            if (!midi2::MidiEndpointConnection::SendMessageSucceeded(result))
            {
                ++m_dropped;
            }
        }
        catch (...)
        {
            ++m_dropped;
        }
    }
}
