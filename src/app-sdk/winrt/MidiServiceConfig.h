// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App SDK and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================


#pragma once
#include "ServiceConfig.MidiServiceConfig.g.h"

namespace winrt::Microsoft::Windows::Devices::Midi2::ServiceConfig::implementation
{
    struct MidiServiceConfig : MidiServiceConfigT<MidiServiceConfig>
    {
        MidiServiceConfig() = default;

        static svc::MidiServiceConfigResponse UpdateTransportPluginConfig(
            _In_ svc::IMidiServiceTransportPluginConfig const& configUpdate) noexcept;

        static svc::MidiServiceConfigResponse UpdateProcessingPluginConfig(
            _In_ svc::IMidiServiceMessageProcessingPluginConfig const& configUpdate) noexcept;


        static json::JsonObject InternalSendConfigJsonAndGetResponse(
            _In_ winrt::guid const& abstractionId, 
            _In_ json::JsonObject const& configObject,
            _In_ bool const isFromCurrentConfigFile) noexcept;

    private:


    };
}
namespace winrt::Microsoft::Windows::Devices::Midi2::ServiceConfig::factory_implementation
{
    struct MidiServiceConfig : MidiServiceConfigT<MidiServiceConfig, implementation::MidiServiceConfig, winrt::static_lifetime>
    {
    };
}
