// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App WinRT API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

#include "pch.h"
#include "MidiNetworkRemoteClientDisconnectConfig.h"
#include "Transports.Network.MidiNetworkRemoteClientDisconnectConfig.g.cpp"

namespace winrt::Windows::Devices::Midi2::Transports::Network::implementation
{
    _Use_decl_annotations_
    MidiNetworkRemoteClientDisconnectConfig::MidiNetworkRemoteClientDisconnectConfig(
        winrt::guid const& hostId,
        winrt::hstring const& remoteClientName,
        winrt::hstring const& remoteClientProductInstanceId) noexcept
    {
        m_hostId = hostId;
        m_remoteClientName = remoteClientName;
        m_remoteClientProductInstanceId = remoteClientProductInstanceId;
    }

    json::JsonObject MidiNetworkRemoteClientDisconnectConfig::ConfigJson() noexcept
    {
        // Disconnecting a remote client is a command, not a configuration file update. Nothing is
        // recorded anywhere, which is the whole difference between this and a denial. An empty
        // object rather than null, so passing this through the generic
        // IMidiServiceTransportPluginConfig surface does not fault.

        return json::JsonObject{};
    }
}
