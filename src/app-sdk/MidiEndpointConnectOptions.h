// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services Client SDK and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================


#pragma once
#include "MidiEndpointConnectOptions.g.h"


namespace winrt::Microsoft::Devices::Midi2::implementation
{
    struct MidiEndpointConnectOptions : MidiEndpointConnectOptionsT<MidiEndpointConnectOptions>
    {
        MidiEndpointConnectOptions() = default;

        static winrt::Microsoft::Devices::Midi2::MidiEndpointConnectOptions Default();

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
