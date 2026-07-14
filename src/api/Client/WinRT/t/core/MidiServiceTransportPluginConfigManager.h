// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services WinRT API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================


#pragma once
#include "ServiceConfig.MidiServiceTransportPluginConfigManager.g.h"

namespace winrt::Windows::Devices::Midi2::ServiceConfig::implementation
{
    struct MidiServiceTransportPluginConfigManager
    {
        //MidiServiceConfig() = default;

        static svc::MidiServiceConfigResponse SendUpdate(
            _In_ svc::IMidiServiceTransportPluginConfig const& configUpdate) noexcept;

        static svc::MidiServiceConfigResponse SendUpdate(
            _In_ winrt::guid const& transportId,
            _In_ json::JsonObject const& fullConfigObject) noexcept;

        static svc::MidiServiceConfigResponse SendCommand(
            _In_ svc::MidiServiceTransportCommand const& command) noexcept;

    private:

        static json::JsonObject InternalSendConfigJsonAndGetResponse(
            _In_ winrt::guid const& transportId,
            _In_ json::JsonObject const& configObject) noexcept;

    };
}
namespace winrt::Windows::Devices::Midi2::ServiceConfig::factory_implementation
{
    struct MidiServiceTransportPluginConfigManager : MidiServiceTransportPluginConfigManagerT<MidiServiceTransportPluginConfigManager, implementation::MidiServiceTransportPluginConfigManager, winrt::static_lifetime>
    {
    };
}
