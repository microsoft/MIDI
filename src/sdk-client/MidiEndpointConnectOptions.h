// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================

#pragma once
#include "MidiEndpointConnectOptions.g.h"


namespace winrt::Microsoft::Devices::Midi2::implementation
{
    struct MidiEndpointConnectOptions : MidiEndpointConnectOptionsT<MidiEndpointConnectOptions>
    {
        MidiEndpointConnectOptions() = default;

        bool DisableProcessingFunctionBlockInformationMessages();
        void DisableProcessingFunctionBlockInformationMessages(bool value);
        bool DisableProcessingEndpointInformationMessages();
        void DisableProcessingEndpointInformationMessages(bool value);
    };
}
namespace winrt::Microsoft::Devices::Midi2::factory_implementation
{
    struct MidiEndpointConnectOptions : MidiEndpointConnectOptionsT<MidiEndpointConnectOptions, implementation::MidiEndpointConnectOptions>
    {
    };
}
