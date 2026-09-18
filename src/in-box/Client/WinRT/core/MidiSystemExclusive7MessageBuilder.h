// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App WinRT API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once
#include "Utilities.Messages.MidiSystemExclusive7MessageBuilder.g.h"

namespace winrt::Windows::Devices::Midi2::Utilities::Messages::implementation
{
    struct MidiSystemExclusive7MessageBuilder
    {
        MidiSystemExclusive7MessageBuilder() = default;

        static foundation::Collections::IVector<midi2::MidiMessage64> BuildSystemExclusive7Messages(
            _In_ internal::MidiTimestamp const timestamp,
            _In_ midi2::MidiGroup const& group,
            _In_ foundation::Collections::IIterable<uint8_t> const& dataBytes) noexcept;

        static uint32_t GetMessageCountForDataByteCount(_In_ uint32_t const dataByteCount) noexcept;

        static uint8_t MaxDataBytesPerMessage() noexcept { return 6; }
    };
}

namespace winrt::Windows::Devices::Midi2::Utilities::Messages::factory_implementation
{
    struct MidiSystemExclusive7MessageBuilder : MidiSystemExclusive7MessageBuilderT<MidiSystemExclusive7MessageBuilder, implementation::MidiSystemExclusive7MessageBuilder, winrt::static_lifetime>
    {
    };
}
