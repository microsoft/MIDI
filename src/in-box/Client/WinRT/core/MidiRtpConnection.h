// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services WinRT API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once
#include "Transports.Rtp.MidiRtpConnection.g.h"

#include "MidiRtpSdkJson.h"

namespace winrt::Windows::Devices::Midi2::Transports::Rtp::implementation
{
    struct MidiRtpConnection : MidiRtpConnectionT<MidiRtpConnection>
    {
        MidiRtpConnection() = default;

        uint32_t ConnectionId() const noexcept { return m_connectionId; }
        winrt::hstring RemoteName() const noexcept { return m_remoteName; }
        winrt::hstring RemoteAddress() const noexcept { return m_remoteAddress; }
        uint16_t RemotePort() const noexcept { return m_remotePort; }
        uint16_t LocalPort() const noexcept { return m_localPort; }
        winrt::hstring RemoteHostName() const noexcept { return m_remoteHostName; }
        bool IsConnected() const noexcept { return m_isConnected; }
        bool ThisPcInvited() const noexcept { return m_thisPcInvited; }
        winrt::hstring EndpointDeviceId() const noexcept { return m_endpointDeviceId; }
        uint64_t CurrentLatencyTicks() const noexcept { return m_currentLatencyTicks; }
        uint64_t BestLatencyTicks() const noexcept { return m_bestLatencyTicks; }
        uint64_t TotalCountNetworkPacketsSent() const noexcept { return m_packetsSent; }
        uint64_t TotalCountNetworkPacketsReceived() const noexcept { return m_packetsReceived; }
        uint64_t TotalCountPacketsLost() const noexcept { return m_packetsLost; }
        uint64_t TotalCountLossesRepairedFromJournal() const noexcept { return m_lossesRepaired; }
        uint64_t TotalCountNoteOffsRecovered() const noexcept { return m_noteOffsRecovered; }
        uint64_t TotalCountMessagesSent() const noexcept { return m_messagesSent; }
        uint64_t TotalCountMessagesReceived() const noexcept { return m_messagesReceived; }

        void InternalInitialize(_In_ json::JsonObject const& source) noexcept;

    private:
        uint32_t m_connectionId{ 0 };
        winrt::hstring m_remoteName{};
        winrt::hstring m_remoteAddress{};
        uint16_t m_remotePort{ 0 };
        uint16_t m_localPort{ 0 };
        winrt::hstring m_remoteHostName{};
        bool m_isConnected{ false };
        bool m_thisPcInvited{ false };
        winrt::hstring m_endpointDeviceId{};
        uint64_t m_currentLatencyTicks{ 0 };
        uint64_t m_bestLatencyTicks{ 0 };
        uint64_t m_packetsSent{ 0 };
        uint64_t m_packetsReceived{ 0 };
        uint64_t m_packetsLost{ 0 };
        uint64_t m_lossesRepaired{ 0 };
        uint64_t m_noteOffsRecovered{ 0 };
        uint64_t m_messagesSent{ 0 };
        uint64_t m_messagesReceived{ 0 };
    };
}
