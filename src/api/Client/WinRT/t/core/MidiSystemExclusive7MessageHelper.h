// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services WinRT API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once
#include "Utilities.Messages.MidiSystemExclusive7MessageHelper.g.h"

namespace winrt::Windows::Devices::Midi2::Utilities::Messages::implementation
{
    struct MidiSystemExclusive7MessageHelper
    {
        //MidiSystemExclusiveMessageHelper() = default;

        static collections::IVector<uint8_t> GetDataBytesFromMultipleSystemExclusiveMessages(
            _In_ collections::IIterable<midi2::MidiMessage64> const& messages) noexcept;

        static collections::IVector<uint8_t> GetDataBytesFromSingleSystemExclusiveMessage(
            _In_ midi2::MidiMessage64 const& message) noexcept;

        static collections::IVector<uint8_t> GetDataBytesFromSingleSystemExclusiveMessage(
            _In_ uint32_t const word0,
            _In_ uint32_t const word1
        ) noexcept;

        static uint8_t AppendDataBytesFromSingleSystemExclusiveMessage(
            _In_ midi2::MidiMessage64 const& message,
            _In_ collections::IVector<uint8_t> dataBytesToAppendTo) noexcept;

        static uint8_t AppendDataBytesFromSingleSystemExclusiveMessage(
            _In_ uint32_t const word0,
            _In_ uint32_t const word1,
            _In_ collections::IVector<uint8_t> dataBytesToAppendTo) noexcept;

        static uint8_t GetDataByteCountFromSystemExclusiveMessageFirstWord(_In_ uint32_t word0) noexcept;
        static bool MessageIsSystemExclusiveMessage(_In_ uint32_t word0) noexcept;
    };
}
namespace winrt::Windows::Devices::Midi2::Utilities::Messages::factory_implementation
{
    struct MidiSystemExclusive7MessageHelper : MidiSystemExclusive7MessageHelperT<MidiSystemExclusive7MessageHelper, implementation::MidiSystemExclusive7MessageHelper, winrt::static_lifetime>
    {
    };

}
