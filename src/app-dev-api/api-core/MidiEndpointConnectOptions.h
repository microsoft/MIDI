// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================



#pragma once
#include "MidiEndpointConnectOptions.g.h"


namespace winrt::Windows::Devices::Midi2::implementation
{
    struct MidiEndpointConnectOptions : MidiEndpointConnectOptionsT<MidiEndpointConnectOptions>
    {
        MidiEndpointConnectOptions() = default;

        static winrt::Windows::Devices::Midi2::MidiEndpointConnectOptions Default();

        bool DisableProcessingFunctionBlockInformationMessages();
        void DisableProcessingFunctionBlockInformationMessages(bool value);
        bool DisableProcessingEndpointInformationMessages();
        void DisableProcessingEndpointInformationMessages(bool value);

        bool UseSessionLevelMessageReceiveHandler();
        void UseSessionLevelMessageReceiveHandler(bool value);

    private:
        bool _disableProcessingFunctionBlockInformationMessages = false;
        bool _disableProcessingEndpointInformationMessages = false;
        bool _useSessionLevelMessageReceiveHandler = false;

    };
}
namespace winrt::Windows::Devices::Midi2::factory_implementation
{
    struct MidiEndpointConnectOptions : MidiEndpointConnectOptionsT<MidiEndpointConnectOptions, implementation::MidiEndpointConnectOptions>
    {
    };
}
