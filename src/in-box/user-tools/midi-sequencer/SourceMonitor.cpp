// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#include "pch.h"
#include "SourceMonitor.h"
#include "StudioSetup.h"

namespace midisequencer
{
    _Use_decl_annotations_
    SourceMonitor::SourceMonitor(midi2::MidiSession const& session) :
        m_session(session)
    {
    }

    SourceMonitor::~SourceMonitor()
    {
        CloseAll();
    }

    _Use_decl_annotations_
    void SourceMonitor::SetHandler(Handler handler)
    {
        std::scoped_lock guard{ m_handlerLock };
        m_handler = std::move(handler);
    }

    _Use_decl_annotations_
    void SourceMonitor::SetGroupsOff(std::map<std::wstring, uint16_t> groupsOff)
    {
        std::scoped_lock guard{ m_handlerLock };
        m_groupsOff = std::move(groupsOff);
    }

    _Use_decl_annotations_
    void SourceMonitor::Deliver(std::wstring const& endpointId, midi2::MidiMessageReceivedEventArgs const& args) noexcept
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
            uint16_t groupsOff{ 0 };

            {
                std::scoped_lock guard{ m_handlerLock };
                handler = m_handler;

                if (!m_groupsOff.empty())
                {
                    if (auto const found = m_groupsOff.find(endpointId); found != m_groupsOff.end())
                    {
                        groupsOff = found->second;
                    }
                }
            }

            if (groupsOff != 0)
            {
                auto const group = MessageGroup(words[0]);

                if (group >= 0 && (groupsOff & (1u << group)) != 0)
                {
                    return;
                }
            }

            if (handler)
            {
                handler(endpointId, args.Timestamp(), words, count);
            }
        }
        catch (...)
        {
            // A message that can't be read is dropped. The service's thread must never see an exception.
        }
    }

    _Use_decl_annotations_
    void SourceMonitor::Prepare(std::vector<std::wstring> const& endpointIds)
    {
        std::vector<std::wstring> opening{};
        std::vector<Source> closing{};

        {
            std::scoped_lock guard{ m_lock };

            for (auto it = m_sources.begin(); it != m_sources.end();)
            {
                if (std::find(endpointIds.begin(), endpointIds.end(), it->first) == endpointIds.end())
                {
                    closing.push_back(it->second);
                    it = m_sources.erase(it);
                }
                else
                {
                    ++it;
                }
            }

            for (auto const& id : endpointIds)
            {
                if (!id.empty() && !m_sources.contains(id) && std::find(opening.begin(), opening.end(), id) == opening.end())
                {
                    opening.push_back(id);
                }
            }
        }

        for (auto& source : closing)
        {
            try
            {
                source.Connection.MessageReceived(source.Token);
                m_session.DisconnectEndpointConnection(source.Connection.ConnectionId());
            }
            catch (...)
            {
            }
        }

        for (auto const& id : opening)
        {
            if (m_session == nullptr)
            {
                break;
            }

            try
            {
                auto connection = m_session.CreateEndpointConnection(winrt::hstring{ id });

                if (connection == nullptr)
                {
                    continue;
                }

                Source source{};
                source.Connection = connection;
                source.Token = connection.MessageReceived([this, id](auto&&, midi2::MidiMessageReceivedEventArgs const& args)
                {
                    Deliver(id, args);
                });

                if (!connection.Open())
                {
                    connection.MessageReceived(source.Token);
                    m_session.DisconnectEndpointConnection(connection.ConnectionId());
                    continue;
                }

                std::scoped_lock guard{ m_lock };
                m_sources.emplace(id, source);
            }
            catch (...)
            {
                // A source that can't be opened records nothing, like one that's unplugged.
            }
        }
    }

    void SourceMonitor::CloseAll() noexcept
    {
        std::map<std::wstring, Source> closing{};

        {
            std::scoped_lock guard{ m_lock };
            closing.swap(m_sources);
        }

        for (auto& [id, source] : closing)
        {
            try
            {
                source.Connection.MessageReceived(source.Token);

                if (m_session != nullptr)
                {
                    m_session.DisconnectEndpointConnection(source.Connection.ConnectionId());
                }
            }
            catch (...)
            {
            }
        }
    }
}
