// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App WinRT API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once
#include "Utilities.SysExTransfer.MidiSystemExclusiveSender.g.h"

namespace winrt::Windows::Devices::Midi2::Utilities::SysExTransfer::implementation
{
    struct MidiSystemExclusiveSender
    {
        //MidiSystemExclusiveSender() = default;

        static foundation::IAsyncOperationWithProgress<bool, sysex::MidiSystemExclusiveSendProgress> SendBinarySysEx7ByteDataAsync(
            _In_ midi2::MidiEndpointConnection destinationConnection, 
            _In_ midi2::MidiGroup destinationGroup,
            _In_ streams::IInputStream dataSource,
            _In_ uint32_t preferredSingleTransferMessageCount,
            _In_ uint16_t transferSpacingMilliseconds,
            _In_ msgs::MidiBytestreamToUmpMessageConverterState converterState
            );


    private:

    };
}
namespace winrt::Windows::Devices::Midi2::Utilities::SysExTransfer::factory_implementation
{
    struct MidiSystemExclusiveSender : MidiSystemExclusiveSenderT<MidiSystemExclusiveSender, implementation::MidiSystemExclusiveSender, winrt::static_lifetime>
    {
    };
}
