// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App WinRT API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

#include "pch.h"
#include "MidiNetworkClientDisconnectConfig.h"
#include "Transports.Network.MidiNetworkClientDisconnectConfig.g.cpp"

namespace winrt::Windows::Devices::Midi2::Transports::Network::implementation
{

    json::JsonObject  MidiNetworkClientDisconnectConfig::ConfigJson() const noexcept
    {
        // Disconnecting is a command, not a configuration file update. It tears down a live
        // session but leaves the client's configuration entry alone, so there is nothing to
        // write here. DisconnectNetworkClientAsync is the supported path.
        //
        // An empty object is returned rather than null so that anything which round-trips this
        // through the generic IMidiServiceTransportPluginConfig surface does not fault.

        return json::JsonObject{};
    }
}
