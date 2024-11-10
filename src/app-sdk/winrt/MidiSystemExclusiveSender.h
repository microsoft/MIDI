// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App SDK and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once
#include "Utilities.SysExTransfer.MidiSystemExclusiveSender.g.h"

namespace winrt::Microsoft::Windows::Devices::Midi2::Utilities::SysExTransfer::implementation
{
    struct MidiSystemExclusiveSender
    {
        //MidiSystemExclusiveSender() = default;

        static foundation::IAsyncOperationWithProgress<bool, sysex::MidiSystemExclusiveSendProgress> SendDataAsync(
            _In_ midi2::MidiEndpointConnection destination, 
            _In_ streams::IInputStream dataSource,
            _In_ sysex::MidiSystemExclusiveDataReaderFormat sourceReaderFormat,
            _In_ sysex::MidiSystemExclusiveDataFormat sysexDataFormat,
            _In_ uint16_t messageSpacingMilliseconds,
            _In_ bool changeGroup,
            _In_ midi2::MidiGroup newGroup) noexcept;

        static foundation::IAsyncOperationWithProgress<bool, sysex::MidiSystemExclusiveSendProgress> SendDataAsync(
            _In_ midi2::MidiEndpointConnection destination,
            _In_ streams::IInputStream dataSource,
            _In_ sysex::MidiSystemExclusiveDataReaderFormat sourceReaderFormat,
            _In_ sysex::MidiSystemExclusiveDataFormat sysexDataFormat) noexcept;

        static foundation::IAsyncOperationWithProgress<bool, sysex::MidiSystemExclusiveSendProgress> SendDataAsync(
            _In_ midi2::MidiEndpointConnection destination,
            _In_ streams::IInputStream dataSource) noexcept;

    private:

    };
}
namespace winrt::Microsoft::Windows::Devices::Midi2::Utilities::SysExTransfer::factory_implementation
{
    struct MidiSystemExclusiveSender : MidiSystemExclusiveSenderT<MidiSystemExclusiveSender, implementation::MidiSystemExclusiveSender, winrt::static_lifetime>
    {
    };
}
