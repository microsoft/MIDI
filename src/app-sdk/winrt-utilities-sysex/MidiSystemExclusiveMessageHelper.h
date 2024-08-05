// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App SDK and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once
#include "MidiSystemExclusiveMessageHelper.g.h"

namespace winrt::Microsoft::Windows::Devices::Midi2::Utilities::SysEx::implementation
{
    struct MidiSystemExclusiveMessageHelper
    {
        MidiSystemExclusiveMessageHelper() = default;

        static collections::IVector<uint8_t> GetDataBytesFromMultipleSystemExclusive7Messages(
            _In_ collections::IIterable<midi2::MidiMessage64> const& messages) noexcept;

        static collections::IVector<uint8_t> GetDataBytesFromSingleSystemExclusive7Message(
            _In_ midi2::MidiMessage64 const& message) noexcept;

        static collections::IVector<uint8_t> GetDataBytesFromSingleSystemExclusive7Message(
            _In_ uint32_t const word0,
            _In_ uint32_t const word1
        ) noexcept;

        static uint8_t AppendDataBytesFromSingleSystemExclusive7Message(
            _In_ midi2::MidiMessage64 const& message,
            _In_ collections::IVector<uint8_t> dataBytesToAppendTo) noexcept;

        static uint8_t AppendDataBytesFromSingleSystemExclusive7Message(
            _In_ uint32_t const word0,
            _In_ uint32_t const word1,
            _In_ collections::IVector<uint8_t> dataBytesToAppendTo) noexcept;

        static uint8_t GetDataByteCountFromSystemExclusive7MessageFirstWord(_In_ uint32_t word0) noexcept;
        static bool MessageIsSystemExclusive7Message(_In_ uint32_t word0) noexcept;
    };
}
namespace winrt::Microsoft::Windows::Devices::Midi2::Utilities::SysEx::factory_implementation
{
    struct MidiSystemExclusiveMessageHelper : MidiSystemExclusiveMessageHelperT<MidiSystemExclusiveMessageHelper, implementation::MidiSystemExclusiveMessageHelper, winrt::static_lifetime>
    {
    };

}
