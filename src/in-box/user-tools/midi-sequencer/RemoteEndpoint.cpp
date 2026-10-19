// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#include "pch.h"
#include "RemoteEndpoint.h"

namespace midisequencer
{
    _Use_decl_annotations_
    RemoteEndpoint::RemoteEndpoint(midi2::MidiSession const& session) :
        m_session(session)
    {
    }

    RemoteEndpoint::~RemoteEndpoint()
    {
        Close();
    }

    _Use_decl_annotations_
    bool RemoteEndpoint::Open(std::wstring const& name, std::wstring const& description, std::wstring const& blockName, Handler handler, std::function<void()> connected)
    {
        Close();

        try
        {
            if (m_session == nullptr || !midi2virtual::MidiVirtualDeviceManager::IsTransportAvailable())
            {
                return false;
            }

            midi2enum::MidiDeclaredEndpointInfo info{};
            info.Name(winrt::hstring{ name });

            // Unique to this process, so two sequencer windows make two endpoints.
            info.ProductInstanceId(winrt::hstring{ std::format(L"MIDI-SEQUENCER-{}", ::GetCurrentProcessId()) });
            info.SupportsMidi10Protocol(true);
            info.SupportsMidi20Protocol(true);
            info.SupportsReceivingJitterReductionTimestamps(false);
            info.SupportsSendingJitterReductionTimestamps(false);
            info.SpecificationVersionMajor(1);
            info.SpecificationVersionMinor(1);
            info.HasStaticFunctionBlocks(true);

            midi2virtual::MidiVirtualDeviceCreationConfig config{ info.Name(), winrt::hstring{ description }, L"Microsoft", info };

            // One block over every group: tracks past the fifteenth are on groups 2 and up.
            midi2enum::MidiFunctionBlock block{};
            block.Number(0);
            block.Name(winrt::hstring{ blockName });
            block.IsActive(true);
            block.Direction(midi2enum::MidiFunctionBlockDirection::Bidirectional);
            block.UIHint(midi2enum::MidiFunctionBlockUIHint::Bidirectional);
            block.RepresentsMidi10Connection(midi2enum::MidiFunctionBlockRepresentsMidi10Connection::Not10);
            block.FirstGroup(midi2::MidiGroup{ static_cast<uint8_t>(0) });
            block.GroupCount(16);
            config.FunctionBlocks().Append(block);

            auto const device = midi2virtual::MidiVirtualDeviceManager::CreateVirtualDevice(config);

            if (device == nullptr)
            {
                return false;
            }

            auto const connection = m_session.CreateEndpointConnection(device.DeviceEndpointDeviceId());

            if (connection == nullptr)
            {
                return false;
            }

            {
                std::scoped_lock guard{ m_lock };
                m_handler = std::move(handler);
                m_connected = std::move(connected);
            }

            // An older SDK doesn't have this event; the window then sends everything now and again.
            winrt::event_token inUse{};

            try
            {
                inUse = device.ClientEndpointInUseChanged([this](auto&&, midi2virtual::MidiVirtualDeviceClientEndpointInUseChangedEventArgs const& args)
                {
                    try
                    {
                        if (!args.IsClientEndpointInUse())
                        {
                            return;
                        }

                        std::function<void()> notify{};

                        {
                            std::scoped_lock guard{ m_lock };
                            notify = m_connected;
                        }

                        if (notify)
                        {
                            notify();
                        }
                    }
                    catch (...)
                    {
                    }
                });
            }
            catch (...)
            {
                inUse = {};
            }

            auto const token = connection.MessageReceived([this](auto&&, midi2::MidiMessageReceivedEventArgs const& args)
            {
                Deliver(args);
            });

            // The device has to be on the connection before it opens, or the endpoint other apps
            // see is never made.
            if (connection.AddMessageProcessingPlugin(device) != midi2::MidiMessageProcessingPluginAddResult::Succeeded || !connection.Open())
            {
                connection.MessageReceived(token);
                m_session.DisconnectEndpointConnection(connection.ConnectionId());
                return false;
            }

            std::scoped_lock guard{ m_lock };
            m_device = device;
            m_connection = connection;
            m_token = token;
            m_inUseToken = inUse;
            return true;
        }
        catch (...)
        {
            LOG_CAUGHT_EXCEPTION();
            return false;
        }
    }

    void RemoteEndpoint::Close() noexcept
    {
        midi2virtual::MidiVirtualDevice device{ nullptr };
        midi2::MidiEndpointConnection connection{ nullptr };
        winrt::event_token token{};
        winrt::event_token inUse{};

        {
            std::scoped_lock guard{ m_lock };
            device = std::exchange(m_device, nullptr);
            connection = std::exchange(m_connection, nullptr);
            token = std::exchange(m_token, {});
            inUse = std::exchange(m_inUseToken, {});
            m_handler = nullptr;
            m_connected = nullptr;
        }

        try
        {
            if (device != nullptr && inUse)
            {
                device.ClientEndpointInUseChanged(inUse);
            }
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

    bool RemoteEndpoint::KnowsWhenAppsConnect() const noexcept
    {
        std::scoped_lock guard{ m_lock };
        return static_cast<bool>(m_inUseToken);
    }

    _Use_decl_annotations_
    void RemoteEndpoint::Send(uint32_t const* words, uint8_t wordCount) noexcept
    {
        midi2::MidiEndpointConnection connection{ nullptr };

        {
            std::scoped_lock guard{ m_lock };
            connection = m_connection;
        }

        if (connection == nullptr || words == nullptr)
        {
            return;
        }

        try
        {
            switch (wordCount)
            {
            case 1: connection.SendSingleMessageWords(0, words[0]); break;
            case 2: connection.SendSingleMessageWords(0, words[0], words[1]); break;
            case 3: connection.SendSingleMessageWords(0, words[0], words[1], words[2]); break;
            case 4: connection.SendSingleMessageWords(0, words[0], words[1], words[2], words[3]); break;
            default: break;
            }
        }
        catch (...)
        {
        }
    }

    _Use_decl_annotations_
    void RemoteEndpoint::Deliver(midi2::MidiMessageReceivedEventArgs const& args) noexcept
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
                handler(words, count);
            }
        }
        catch (...)
        {
            // The service's thread must never see an exception.
        }
    }
}
