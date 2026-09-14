// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App WinRT API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

#include "pch.h"
#include "MidiNetworkRemoteClientForgetConfig.h"
#include "Transports.Network.MidiNetworkRemoteClientForgetConfig.g.cpp"

namespace winrt::Windows::Devices::Midi2::Transports::Network::implementation
{
    _Use_decl_annotations_
    MidiNetworkRemoteClientForgetConfig::MidiNetworkRemoteClientForgetConfig(
        winrt::guid const& hostId,
        winrt::hstring const& remoteClientName,
        winrt::hstring const& remoteClientProductInstanceId) noexcept
    {
        m_hostId = hostId;
        m_remoteClientName = remoteClientName;
        m_remoteClientProductInstanceId = remoteClientProductInstanceId;
    }

    json::JsonObject MidiNetworkRemoteClientForgetConfig::ConfigJson() noexcept
    {
        // Forgetting is a command against the running service, not a configuration file update.
        // Rewriting the saved lists is MidiNetworkHostKnownClientsConfig's job. An empty object
        // rather than null, so passing this through the generic
        // IMidiServiceTransportPluginConfig surface does not fault.

        return json::JsonObject{};
    }
}
