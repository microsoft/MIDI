// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App SDK and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once
#include "ServiceConfig.MidiServiceTransportCommonCommands.g.h"

namespace winrt::Microsoft::Windows::Devices::Midi2::ServiceConfig::implementation
{
    struct MidiServiceTransportCommonCommands
    {
        MidiServiceTransportCommonCommands() = default;

        static winrt::hstring QueryCapabilities() noexcept { return MIDI_CONFIG_JSON_TRANSPORT_COMMAND_QUERY_CAPABILITIES; }
        static winrt::hstring RestartEndpoint() noexcept { return MIDI_CONFIG_JSON_TRANSPORT_COMMAND_RESTART_ENDPOINT; }
        static winrt::hstring DisconnectEndpoint() noexcept { return MIDI_CONFIG_JSON_TRANSPORT_COMMAND_DISCONNECT_ENDPOINT; }
        static winrt::hstring ReconnectEndpoint() noexcept { return MIDI_CONFIG_JSON_TRANSPORT_COMMAND_RECONNECT_ENDPOINT; }
    };
}
namespace winrt::Microsoft::Windows::Devices::Midi2::ServiceConfig::factory_implementation
{
    struct MidiServiceTransportCommonCommands : MidiServiceTransportCommonCommandsT<MidiServiceTransportCommonCommands, implementation::MidiServiceTransportCommonCommands>
    {
    };
}
