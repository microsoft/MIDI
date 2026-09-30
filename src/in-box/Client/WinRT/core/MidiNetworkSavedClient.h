// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App WinRT API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once
#include "Transports.Network.MidiNetworkSavedClient.g.h"

namespace winrt::Windows::Devices::Midi2::Transports::Network::implementation
{
    struct MidiNetworkSavedClient : MidiNetworkSavedClientT<MidiNetworkSavedClient>
    {
        MidiNetworkSavedClient() = default;

        winrt::guid ClientId() const noexcept { return m_clientId; }

        winrt::hstring Comment() const noexcept { return m_comment; }

        bool IsEnabled() const noexcept { return m_isEnabled; }

        bool CreateOnlyUmpEndpoints() const noexcept { return m_createOnlyUmpEndpoints; }
        uint8_t FallbackMidi1PortCount() const noexcept { return m_fallbackMidi1PortCount; }

        winrt::hstring UmpEndpointName() const noexcept { return m_umpEndpointName; }
        winrt::hstring CustomEndpointName() const noexcept { return m_customEndpointName; }

        network::MidiNetworkClientMatchCriteria MatchCriteria() const noexcept;

        // The saved entry, then each saved change to it in the order the service applies them
        void InternalInitialize(
            _In_ winrt::guid const& clientId,
            _In_ json::JsonObject const& entry,
            _In_ std::vector<json::JsonObject> const& updates) noexcept;

    private:
        winrt::guid m_clientId{};

        winrt::hstring m_comment{};

        bool m_isEnabled{ true };

        bool m_createOnlyUmpEndpoints{ false };
        uint8_t m_fallbackMidi1PortCount{ 1 };

        winrt::hstring m_umpEndpointName{};
        winrt::hstring m_customEndpointName{};

        winrt::hstring m_matchDeviceId{};
        winrt::hstring m_matchProductInstanceId{};
        winrt::hstring m_matchUmpEndpointName{};
        winrt::hstring m_matchDirectHostNameOrIPAddress{};
        uint16_t m_matchDirectPort{ 0 };
    };
}
