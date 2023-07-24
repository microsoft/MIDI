// Copyright(c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App SDK and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================

#pragma once
#include "MidiFunctionBlock.g.h"

namespace winrt::Microsoft::Devices::Midi2::implementation
{
    struct MidiFunctionBlock : MidiFunctionBlockT<MidiFunctionBlock>
    {
        MidiFunctionBlock() = default;

        uint8_t Number();
        hstring Name();
        winrt::Microsoft::Devices::Midi2::MidiFunctionBlockDirection Direction();
        winrt::Microsoft::Devices::Midi2::MidiFunctionBlockUIHint UIHint();
        bool IsMidi10Connection();
        bool IsBandwidthRestricted();
        winrt::Windows::Foundation::Collections::IVectorView<winrt::Microsoft::Devices::Midi2::MidiGroup> IncludedGroups();
        winrt::Windows::Foundation::Collections::IVectorView<winrt::Windows::Devices::Midi2::MidiUmp128> OriginalResponses();
        uint8_t MidiCIMessageVersionFormat();
        uint8_t MaxSysEx8Streams();
    };
}
namespace winrt::Microsoft::Devices::Midi2::factory_implementation
{
    struct MidiFunctionBlock : MidiFunctionBlockT<MidiFunctionBlock, implementation::MidiFunctionBlock>
    {
    };
}
