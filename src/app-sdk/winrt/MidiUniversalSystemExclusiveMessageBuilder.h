// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App SDK and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once
#include "Messages.MidiUniversalSystemExclusiveMessageBuilder.g.h"

namespace winrt::Microsoft::Windows::Devices::Midi2::Messages::implementation
{
    struct MidiUniversalSystemExclusiveMessageBuilder
    {
        MidiUniversalSystemExclusiveMessageBuilder() = default;

        static midi2::MidiMessage64 BuildIdentityRequest(
            _In_ uint64_t const timestamp, 
            _In_ midi2::MidiUniversalSystemExclusiveChannel const& systemExclusiveChannel
        );

        static collections::IVector<midi2::MidiMessage64> BuildIdentityRequestReply(
            _In_ uint64_t timestamp,
            _In_ midi2::MidiDeclaredDeviceIdentity const& identity
        );

        static midi2::MidiMessage64 BuildWait(_In_ uint64_t const timestamp);
        static midi2::MidiMessage64 BuildCancel(_In_ uint64_t const timestamp);
        static midi2::MidiMessage64 BuildNAK(_In_ uint64_t const timestamp);
        static midi2::MidiMessage64 BuildACK(_In_ uint64_t const timestamp);
    };
}
namespace winrt::Microsoft::Windows::Devices::Midi2::Messages::factory_implementation
{
    struct MidiUniversalSystemExclusiveMessageBuilder : MidiUniversalSystemExclusiveMessageBuilderT<MidiUniversalSystemExclusiveMessageBuilder, implementation::MidiUniversalSystemExclusiveMessageBuilder, winrt::static_lifetime>
    {
    };
}
