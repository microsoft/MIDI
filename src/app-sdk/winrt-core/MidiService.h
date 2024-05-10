// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================

#pragma once
#include "MidiService.g.h"

namespace winrt::Microsoft::Devices::Midi2::implementation
{
    struct MidiService : MidiServiceT<MidiService>
    {
        MidiService() = default;

        static bool EnsureAvailable();

        static midi2::MidiServiceConfigurationResponse UpdateTransportPluginConfiguration(
            _In_ midi2::IMidiServiceTransportPluginConfiguration const& configurationUpdate) noexcept;

        static midi2::MidiServiceConfigurationResponse UpdateProcessingPluginConfiguration(
            _In_ midi2::IMidiServiceMessageProcessingPluginConfiguration const& configurationUpdate) noexcept;


        static json::JsonObject InternalSendConfigurationJsonAndGetResponse(
            _In_ winrt::guid const& abstractionId, 
            _In_ json::JsonObject const& configObject,
            _In_ bool const isFromConfigurationFile) noexcept;

    private:


    };
}
namespace winrt::Microsoft::Devices::Midi2::factory_implementation
{
    struct MidiService : MidiServiceT<MidiService, implementation::MidiService, winrt::static_lifetime>
    {
    };
}
