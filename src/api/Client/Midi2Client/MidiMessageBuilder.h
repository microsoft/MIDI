// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================

#pragma once
#include "MidiMessageBuilder.g.h"

namespace winrt::Windows::Devices::Midi2::implementation
{
    struct MidiMessageBuilder
    {
        MidiMessageBuilder() = default;

        static midi2::MidiUmp32 BuildUtilityMessage(
            _In_ internal::MidiTimestamp const timestamp,
            _In_ uint8_t const status, 
            _In_ uint32_t const dataOrReserved) noexcept;

        static midi2::MidiUmp32 BuildSystemMessage(
            _In_ internal::MidiTimestamp const timestamp,
            _In_ uint8_t const groupIndex,
            _In_ uint8_t const status,
            _In_ uint8_t const midi1Byte2, 
            _In_ uint8_t const midi1Byte3) noexcept;

        static midi2::MidiUmp32 BuildMidi1ChannelVoiceMessage(
            _In_ internal::MidiTimestamp const timestamp,
            _In_ uint8_t const groupIndex,
            _In_ midi2::Midi1ChannelVoiceMessageStatus const& status,
            _In_ uint8_t const channel, 
            _In_ uint8_t const byte3, 
            _In_ uint8_t const byte4) noexcept;

        static midi2::MidiUmp64 BuildSysEx7Message(
            _In_ internal::MidiTimestamp const timestamp,
            _In_ uint8_t const groupIndex,
            _In_ uint8_t const status, 
            _In_ uint8_t const numberOfBytes, 
            _In_ uint8_t const dataByte0, 
            _In_ uint8_t const dataByte1, 
            _In_ uint8_t const dataByte2,
            _In_ uint8_t const dataByte3,
            _In_ uint8_t const dataByte4,
            _In_ uint8_t const dataByte5) noexcept;

        static midi2::MidiUmp64 BuildSysEx7MessageFromArray(
            _In_ internal::MidiTimestamp const timestamp,
            _In_ uint8_t const groupIndex,
            _In_ uint8_t const status,
            _In_ uint8_t const numberOfBytes,
            _In_ array_view<uint8_t const> const dataBytes,
            _In_ uint32_t const arrayStartIndex);

        static midi2::MidiUmp64 BuildSysEx7MessageFromBuffer(
            _In_ internal::MidiTimestamp const timestamp,
            _In_ uint8_t const groupIndex,
            _In_ uint8_t const status,
            _In_ uint8_t const numberOfBytes,
            _In_ foundation::IMemoryBuffer const& buffer,
            _In_ uint32_t const byteOffsetInBuffer);

        static midi2::MidiUmp64 BuildMidi2ChannelVoiceMessage(
            _In_ internal::MidiTimestamp const timestamp,
            _In_ uint8_t const groupIndex,
            _In_ midi2::Midi2ChannelVoiceMessageStatus const& status,
            _In_ uint8_t const channel,
            _In_ uint16_t const index,
            _In_ uint32_t const data) noexcept;

        static midi2::MidiUmp128 BuildSysEx8Message(
            _In_ internal::MidiTimestamp const timestamp,
            _In_ uint8_t const groupIndex,
            _In_ midi2::MidiSysEx8Status const& status, 
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

        static midi2::MidiUmp128 BuildSysEx8MessageFromArray(
            _In_ internal::MidiTimestamp const timestamp,
            _In_ uint8_t const groupIndex,
            _In_ midi2::MidiSysEx8Status const& status, 
            _In_ uint8_t const numberOfValidDataBytesThisMessage,
            _In_ uint8_t const streamId,
            _In_ array_view<uint8_t const> const dataBytes,
            _In_ uint32_t const arrayStartIndex);

        static midi2::MidiUmp128 BuildSysEx8MessageFromBuffer(
            _In_ internal::MidiTimestamp const timestamp,
            _In_ uint8_t const groupIndex,
            _In_ midi2::MidiSysEx8Status const& status,
            _In_ uint8_t const numberOfValidDataBytesThisMessage,
            _In_ foundation::IMemoryBuffer const& buffer,
            _In_ uint32_t const byteOffsetInBuffer);

        static midi2::MidiUmp128 BuildMixedDataSetChunkHeaderMessage(
            _In_ internal::MidiTimestamp const timestamp,
            _In_ uint8_t const groupIndex,
            _In_ uint8_t const mdsId,
            _In_ uint16_t const numberValidDataBytesInThisChunk,
            _In_ uint16_t const numberChunksInMixedDataSet,
            _In_ uint16_t const numberOfThisChunk,
            _In_ uint16_t const manufacturerId,
            _In_ uint16_t const deviceId,
            _In_ uint16_t const subId1,
            _In_ uint16_t const subId2);

        static midi2::MidiUmp128 BuildMixedDataSetChunkDataMessage(
            _In_ internal::MidiTimestamp const timestamp,
            _In_ uint8_t const groupIndex,
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

        static midi2::MidiUmp128 BuildMixedDataSetChunkDataMessageFromArray(
            _In_ internal::MidiTimestamp const timestamp,
            _In_ uint8_t const groupIndex,
            _In_ uint8_t const mdsId,
            _In_ uint8_t const numberOfValidDataBytesThisMessage,
            _In_ array_view<uint8_t const> const dataBytes,
            _In_ uint32_t const arrayStartIndex);

        static midi2::MidiUmp128 BuildMixedDataSetChunkDataMessageFromBuffer(
            _In_ internal::MidiTimestamp const timestamp,
            _In_ uint8_t const groupIndex,
            _In_ uint8_t const mdsId,
            _In_ uint8_t const numberOfValidDataBytesThisMessage,
            _In_ foundation::IMemoryBuffer const& buffer, 
            _In_ uint32_t const byteOffsetInBuffer);

        static midi2::MidiUmp128 BuildFlexDataMessage(
            _In_ internal::MidiTimestamp const timestamp,
            _In_ uint8_t const groupIndex,
            _In_ uint8_t const form,
            _In_ uint8_t const address,
            _In_ uint8_t const channel,
            _In_ uint8_t const statusBank,
            _In_ uint8_t const status,
            _In_ uint32_t const word1Data,
            _In_ uint32_t const word2Data,
            _In_ uint32_t const word3Data);

        static midi2::MidiUmp128 BuildUmpStreamMessage(
            _In_ internal::MidiTimestamp const timestamp,
            _In_ uint8_t const form,
            _In_ uint16_t const status,
            _In_ uint16_t const word0RemainingData,
            _In_ uint32_t const word1Data,
            _In_ uint32_t const word2Data,
            _In_ uint32_t const word3Data) noexcept;
    };
}
namespace winrt::Windows::Devices::Midi2::factory_implementation
{
    struct MidiMessageBuilder : MidiMessageBuilderT<MidiMessageBuilder, implementation::MidiMessageBuilder, winrt::static_lifetime>
    {
    };
}
