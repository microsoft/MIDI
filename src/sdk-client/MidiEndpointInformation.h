// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================

#pragma once
#include "MidiEndpointInformation.g.h"


namespace winrt::Microsoft::Devices::Midi2::implementation
{
    struct MidiEndpointInformation : MidiEndpointInformationT<MidiEndpointInformation>
    {
        MidiEndpointInformation() = default;

        hstring Name();
        uint8_t UmpVersionMajor();
        uint8_t UmpVersionMinor();
        bool HasStaticFunctionBlocks();
        uint8_t FunctionBlockCount();
        hstring ProductInstanceId();
        bool SupportsMidi10Protocol();
        bool SupportsMidi20Protocol();
        bool SupportsReceivingJRTimestamps();
        bool SupportsSendingJRTimestamps();
    };
}
namespace winrt::Microsoft::Devices::Midi2::factory_implementation
{
    struct MidiEndpointInformation : MidiEndpointInformationT<MidiEndpointInformation, implementation::MidiEndpointInformation>
    {
    };
}
