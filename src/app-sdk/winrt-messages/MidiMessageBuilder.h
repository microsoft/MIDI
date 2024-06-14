// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App SDK and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================


#pragma once
#include "MidiMessageBuilder.g.h"

namespace winrt::Microsoft::Windows::Devices::Midi2::Messages::implementation
{
    struct MidiMessageBuilder
    {
        MidiMessageBuilder() = default;

        
        static midi2::MidiMessage32 BuildUtilityMessage(
            _In_ internal::MidiTimestamp const timestamp,
            _In_ uint8_t const status, 
            _In_ uint32_t const dataOrReserved) noexcept;

        static midi2::MidiMessage32 BuildSystemMessage(
            _In_ internal::MidiTimestamp const timestamp,
            _In_ midi2::MidiGroup const& group,
            _In_ uint8_t const status,
            _In_ uint8_t const midi1Byte2, 
            _In_ uint8_t const midi1Byte3) noexcept;

        static midi2::MidiMessage32 BuildMidi1ChannelVoiceMessage(
            _In_ internal::MidiTimestamp const timestamp,
            _In_ midi2::MidiGroup const& group,
            _In_ msgs::Midi1ChannelVoiceMessageStatus const& status,
            _In_ midi2::MidiChannel const& channel,
            _In_ uint8_t const byte3,
            _In_ uint8_t const byte4) noexcept;

        static midi2::MidiMessage64 BuildSystemExclusive7Message(
            _In_ internal::MidiTimestamp const timestamp,
            _In_ midi2::MidiGroup const& group,
            _In_ uint8_t const status,
            _In_ uint8_t const numberOfBytes, 
            _In_ uint8_t const dataByte0, 
            _In_ uint8_t const dataByte1, 
            _In_ uint8_t const dataByte2,
            _In_ uint8_t const dataByte3,
            _In_ uint8_t const dataByte4,
            _In_ uint8_t const dataByte5) noexcept;

        static midi2::MidiMessage64 BuildMidi2ChannelVoiceMessage(
            _In_ internal::MidiTimestamp const timestamp,
            _In_ midi2::MidiGroup const& group,
            _In_ msgs::Midi2ChannelVoiceMessageStatus const& status,
            _In_ midi2::MidiChannel const& channel,
            _In_ uint16_t const index,
            _In_ uint32_t const data) noexcept;

        static midi2::MidiMessage128 BuildSystemExclusive8Message(
            _In_ internal::MidiTimestamp const timestamp,
            _In_ midi2::MidiGroup const& group,
            _In_ msgs::MidiSystemExclusive8Status const& status,
            _In_ uint8_t const numberOfValidDataBytesThisMessage,
            _In_ uint8_t const streamId, 
            _In_ uint8_t const dataByte00,
            _In_ uint8_t const dataByte01,
            _In_ uint8_t const dataByte02,
            _In_ uint8_t const dataByte03,
            _In_ uint8_t const dataByte04,
            _In_ uint8_t const dataByte05,
            _In_ uint8_t const dataByte06,
            _In_ uint8_t const dataByte07,
            _In_ uint8_t const dataByte08,
            _In_ uint8_t const dataByte09,
            _In_ uint8_t const dataByte10,
            _In_ uint8_t const dataByte11,
            _In_ uint8_t const dataByte12) noexcept;

        static midi2::MidiMessage128 BuildMixedDataSetChunkHeaderMessage(
            _In_ internal::MidiTimestamp const timestamp,
            _In_ midi2::MidiGroup const& group,
            _In_ uint8_t const mdsId,
            _In_ uint16_t const numberValidDataBytesInThisChunk,
            _In_ uint16_t const numberChunksInMixedDataSet,
            _In_ uint16_t const numberOfThisChunk,
            _In_ uint16_t const manufacturerId,
            _In_ uint16_t const deviceId,
            _In_ uint16_t const subId1,
            _In_ uint16_t const subId2);

        static midi2::MidiMessage128 BuildMixedDataSetChunkDataMessage(
            _In_ internal::MidiTimestamp const timestamp,
            _In_ midi2::MidiGroup const& group,
            _In_ uint8_t const mdsId,
            _In_ uint8_t const dataByte00,
            _In_ uint8_t const dataByte01,
            _In_ uint8_t const dataByte02,
            _In_ uint8_t const dataByte03,
            _In_ uint8_t const dataByte04,
            _In_ uint8_t const dataByte05,
            _In_ uint8_t const dataByte06,
            _In_ uint8_t const dataByte07,
            _In_ uint8_t const dataByte08,
            _In_ uint8_t const dataByte09,
            _In_ uint8_t const dataByte10,
            _In_ uint8_t const dataByte11,
            _In_ uint8_t const dataByte12,
            _In_ uint8_t const dataByte13);

        static midi2::MidiMessage128 BuildFlexDataMessage(
            _In_ internal::MidiTimestamp const timestamp,
            _In_ midi2::MidiGroup const& group,
            _In_ uint8_t const form,
            _In_ uint8_t const address,
            _In_ midi2::MidiChannel const& channel,
            _In_ uint8_t const statusBank,
            _In_ uint8_t const status,
            _In_ uint32_t const word1Data,
            _In_ uint32_t const word2Data,
            _In_ uint32_t const word3Data);

        static midi2::MidiMessage128 BuildStreamMessage(
            _In_ internal::MidiTimestamp const timestamp,
            _In_ uint8_t const form,
            _In_ uint16_t const status,
            _In_ uint16_t const word0RemainingData,
            _In_ uint32_t const word1Data,
            _In_ uint32_t const word2Data,
            _In_ uint32_t const word3Data) noexcept;
    };
}
namespace winrt::Microsoft::Windows::Devices::Midi2::Messages::factory_implementation
{
    struct MidiMessageBuilder : MidiMessageBuilderT<MidiMessageBuilder, implementation::MidiMessageBuilder, winrt::static_lifetime>
    {
    };
}
