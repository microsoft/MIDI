// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App SDK and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once
#include "MidiSystemExclusiveSender.g.h"

namespace winrt::Microsoft::Windows::Devices::Midi2::Utilities::SysEx::implementation
{
    struct MidiSystemExclusiveSender
    {
        MidiSystemExclusiveSender() = default;

        static foundation::IAsyncOperationWithProgress<bool, uint32_t> SendDataAsync(
            _In_ midi2::MidiEndpointConnection destination, 
            _In_ streams::IDataReader dataSource,
            _In_ sysex::MidiSystemExclusiveDataReaderFormat sourceReaderFormat,
            _In_ sysex::MidiSystemExclusiveDataFormat dataFormat,
            _In_ uint8_t messageSpacingMilliseconds,
            _In_ midi2::MidiGroup newGroup) noexcept;

        static foundation::IAsyncOperationWithProgress<bool, uint32_t> SendDataAsync(
            _In_ midi2::MidiEndpointConnection destination,
            _In_ streams::IDataReader dataSource,
            _In_ sysex::MidiSystemExclusiveDataReaderFormat sourceReaderFormat,
            _In_ sysex::MidiSystemExclusiveDataFormat dataFormat) noexcept;

        static foundation::IAsyncOperationWithProgress<bool, uint32_t> SendDataAsync(
            _In_ midi2::MidiEndpointConnection destination,
            _In_ streams::IDataReader dataSource) noexcept;

    };
}
namespace winrt::Microsoft::Windows::Devices::Midi2::Utilities::SysEx::factory_implementation
{
    struct MidiSystemExclusiveSender : MidiSystemExclusiveSenderT<MidiSystemExclusiveSender, implementation::MidiSystemExclusiveSender, winrt::static_lifetime>
    {
    };
}
