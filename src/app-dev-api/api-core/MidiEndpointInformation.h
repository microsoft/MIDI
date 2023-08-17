// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================

#pragma once
#include "MidiEndpointInformation.g.h"


namespace winrt::Windows::Devices::Midi2::implementation
{
    struct MidiEndpointInformation : MidiEndpointInformationT<MidiEndpointInformation>
    {
        MidiEndpointInformation() = default;

        hstring Name();
        hstring ProductInstanceId();
        uint8_t UmpVersionMajor();
        uint8_t UmpVersionMinor();
        hstring UmpVersionString();
        bool HasStaticFunctionBlocks();
        uint8_t FunctionBlockCount();
        bool SupportsMidi10Protocol();
        bool SupportsMidi20Protocol();
        bool SupportsReceivingJRTimestamps();
        bool SupportsSendingJRTimestamps();
    };
}
namespace winrt::Windows::Devices::Midi2::factory_implementation
{
    struct MidiEndpointInformation : MidiEndpointInformationT<MidiEndpointInformation, implementation::MidiEndpointInformation>
    {
    };
}
