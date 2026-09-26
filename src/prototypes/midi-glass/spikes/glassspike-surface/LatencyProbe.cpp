// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================
// MIDI Glass phase 0 spike. Nothing here ships.

#include "pch.h"
#include "LatencyProbe.h"
#include "RunStats.h"

using namespace winrt;

namespace gspike
{
    namespace
    {
        // A loopback endpoint, so the spike never picks an arbitrary instrument off the machine.
        constexpr wchar_t LoopbackA[] = L"\\\\?\\swd#midisrv#midiu_loop_a_default#{e7cce071-3c03-423f-88d3-f1045d02552b}";

        constexpr uint32_t Midi1ChannelVoiceMessageType = 0x2;
        constexpr uint32_t StatusControlChange = 0xB;
    }

    std::wstring LatencyProbe::DefaultEndpointDeviceId()
    {
        return LoopbackA;
    }

    bool LatencyProbe::Open(std::wstring const& endpointDeviceId) noexcept
    {
        try
        {
            Close();

            m_endpointDeviceId = endpointDeviceId.empty() ? DefaultEndpointDeviceId() : endpointDeviceId;

            if (!midi2::MidiApi::EnsureServiceAvailable())
            {
                m_status = L"the MIDI service is not available";
                return false;
            }

            auto session = midi2::MidiSession::Create(L"MIDI Glass spike");

            if (session == nullptr)
            {
                m_status = L"could not create a session";
                return false;
            }

            auto connection = session.CreateEndpointConnection(hstring{ m_endpointDeviceId });

            if (connection == nullptr || !connection.Open())
            {
                m_status = L"could not open the endpoint";
                session.Close();
                return false;
            }

            m_session = session;
            m_connection = connection;
            m_sendImmediately = midi2::MidiClock::TimestampConstantSendImmediately();
            m_status = L"open";

            return true;
        }
        catch (winrt::hresult_error const& ex)
        {
            m_status = std::format(L"open failed, 0x{:08X}", static_cast<uint32_t>(ex.code()));
        }
        catch (...)
        {
            m_status = L"open failed";
        }

        m_connection = nullptr;
        m_session = nullptr;

        return false;
    }

    void LatencyProbe::Close() noexcept
    {
        try
        {
            if (m_session != nullptr && m_connection != nullptr)
            {
                m_session.DisconnectEndpointConnection(m_connection.ConnectionId());
            }
        }
        catch (...)
        {
        }

        m_connection = nullptr;

        try
        {
            if (m_session != nullptr)
            {
                m_session.Close();
            }
        }
        catch (...)
        {
        }

        m_session = nullptr;
    }

    int64_t LatencyProbe::SendControlChange(uint8_t group, uint8_t channel, uint8_t controller, float value) noexcept
    {
        if (m_connection == nullptr)
        {
            return 0;
        }

        // MIDI 1.0 channel voice control change in a UMP. Everything is computed from arguments
        // that are already in registers; nothing here allocates or looks anything up by name.
        const uint32_t data = static_cast<uint32_t>(std::clamp(value, 0.0f, 1.0f) * 127.0f + 0.5f);

        const uint32_t word0 =
            (Midi1ChannelVoiceMessageType << 28) |
            (static_cast<uint32_t>(group & 0x0F) << 24) |
            (StatusControlChange << 20) |
            (static_cast<uint32_t>(channel & 0x0F) << 16) |
            (static_cast<uint32_t>(controller & 0x7F) << 8) |
            (data & 0x7F);

        m_connection.SendSingleMessageWords(m_sendImmediately, word0);

        return NowMicroseconds();
    }
}
