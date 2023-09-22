// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================

#include "pch.h"
#include "MidiMessageBuilder.h"
#include "MidiMessageBuilder.g.cpp"

#include "ump_helpers.h"
#include "midi_ump.h"

namespace winrt::Windows::Devices::Midi2::implementation
{
    _Use_decl_annotations_
    midi2::MidiMessage32 MidiMessageBuilder::BuildUtilityMessage(
        internal::MidiTimestamp const timestamp,
        uint8_t const status,
        uint32_t const dataOrReserved) noexcept
    {      
        return midi2::MidiMessage32(
            timestamp,
            (uint32_t)(
                0x00000000 |
                internal::CleanupNibble(status) << 20 |
                internal::CleanupInt20(dataOrReserved))
        );

    }

    _Use_decl_annotations_
    midi2::MidiMessage32 MidiMessageBuilder::BuildSystemMessage(
        internal::MidiTimestamp const timestamp,
        uint8_t const groupIndex,
        uint8_t const status,
        uint8_t const midi1Byte2,
        uint8_t const midi1Byte3) noexcept
    {
        return midi2::MidiMessage32(
            timestamp,
            (uint32_t)(
                0x1 << 28 |
                internal::CleanupNibble(groupIndex) << 24 |
                status << 16 |
                internal::CleanupByte7(midi1Byte2) << 8 |
                internal::CleanupByte7(midi1Byte3))
        );
    }

    _Use_decl_annotations_
    midi2::MidiMessage32 MidiMessageBuilder::BuildMidi1ChannelVoiceMessage(
        internal::MidiTimestamp const timestamp,
        uint8_t const groupIndex,
        midi2::Midi1ChannelVoiceMessageStatus const& status,
        uint8_t const channel,
        uint8_t const byte3,
        uint8_t const byte4) noexcept
    {
        return midi2::MidiMessage32(
            timestamp,
            (uint32_t)(
                0x2 << 28 |
                internal::CleanupNibble(groupIndex) << 24 |
                internal::CleanupNibble((uint8_t)status) << 20 |
                internal::CleanupNibble(channel) << 16 |
                internal::CleanupByte7(byte3) << 8 |
                internal::CleanupByte7(byte4))
        );
    }

    _Use_decl_annotations_
    midi2::MidiMessage64 MidiMessageBuilder::BuildSystemExclusive7Message(
        internal::MidiTimestamp const timestamp,
        uint8_t const groupIndex,
        uint8_t const status,
        uint8_t const numberOfBytes,
        uint8_t const dataByte0,
        uint8_t const dataByte1,
        uint8_t const dataByte2,
        uint8_t const dataByte3,
        uint8_t const dataByte4,
        uint8_t const dataByte5) noexcept
    {
        
        return midi2::MidiMessage64(
            timestamp,
            (uint32_t)(
                0x3 << 28 |
                internal::CleanupNibble(groupIndex) << 24 |
                internal::CleanupNibble(status) << 20 |
                internal::CleanupNibble(numberOfBytes) << 16 |
                internal::CleanupByte7(dataByte0) << 8 |
                internal::CleanupByte7(dataByte1)), 
                
            (uint32_t)(
                internal::CleanupByte7(dataByte2) << 24 |
                internal::CleanupByte7(dataByte3) << 16 |
                internal::CleanupByte7(dataByte4) << 8 |
                internal::CleanupByte7(dataByte5))

            );

    }

    _Use_decl_annotations_
    midi2::MidiMessage64 MidiMessageBuilder::BuildSystemExclusive7MessageFromArray(
        internal::MidiTimestamp const /*timestamp*/,
        uint8_t const /*groupIndex*/,
        uint8_t const /*status*/,
        uint8_t const /*numberOfBytes*/,
        array_view<uint8_t const> const /*dataBytes*/,
        uint32_t const /*arrayStartIndex*/)
    {
        throw hresult_not_implemented();
    }

    _Use_decl_annotations_
    midi2::MidiMessage64 MidiMessageBuilder::BuildSystemExclusive7MessageFromBuffer(
        internal::MidiTimestamp const /*timestamp*/,
        uint8_t const /*groupIndex*/,
        uint8_t const /*status*/,
        uint8_t const /*numberOfBytes*/,
        foundation::IMemoryBuffer const& /*buffer*/,
        uint32_t const /*byteOffsetInBuffer*/)
    {
        throw hresult_not_implemented();
    }

    _Use_decl_annotations_
    midi2::MidiMessage64 MidiMessageBuilder::BuildMidi2ChannelVoiceMessage(
        internal::MidiTimestamp const timestamp,
        uint8_t const groupIndex,
        midi2::Midi2ChannelVoiceMessageStatus const& status,
        uint8_t const channel,
        uint16_t const index,
        uint32_t const data) noexcept
    {
        return midi2::MidiMessage64(
            timestamp,
            (uint32_t)(
                0x4 << 28 |
                internal::CleanupNibble(groupIndex) << 24 |
                internal::CleanupNibble((uint8_t)status) << 20 |
                internal::CleanupNibble(channel) << 16 |
                index), 
            data);
    }

    _Use_decl_annotations_
    midi2::MidiMessage128 MidiMessageBuilder::BuildSystemExclusive8Message(
        internal::MidiTimestamp const timestamp,
        uint8_t const groupIndex,
        midi2::MidiSystemExclusive8Status const& status,
        uint8_t const numberOfValidDataBytesThisMessage,
        uint8_t const streamId,
        uint8_t const dataByte00,
        uint8_t const dataByte01,
        uint8_t const dataByte02,
        uint8_t const dataByte03,
        uint8_t const dataByte04,
        uint8_t const dataByte05,
        uint8_t const dataByte06,
        uint8_t const dataByte07,
        uint8_t const dataByte08,
        uint8_t const dataByte09,
        uint8_t const dataByte10,
        uint8_t const dataByte11,
        uint8_t const dataByte12) noexcept
    {
        return midi2::MidiMessage128(
            timestamp,
            (uint32_t)(
                0x5 << 28 |
                internal::CleanupNibble(groupIndex) << 24 |
                internal::CleanupNibble((uint8_t)status) << 20 |
                internal::CleanupNibble(numberOfValidDataBytesThisMessage) << 16 |
                streamId << 8 |
                dataByte00),
            internal::MidiWordFromBytes(dataByte01, dataByte02, dataByte03, dataByte04),
            internal::MidiWordFromBytes(dataByte05, dataByte06, dataByte07, dataByte08),
            internal::MidiWordFromBytes(dataByte09, dataByte10, dataByte11, dataByte12)
            );
    }

    _Use_decl_annotations_
    midi2::MidiMessage128 MidiMessageBuilder::BuildSystemExclusive8MessageFromArray(
        internal::MidiTimestamp const /*timestamp*/,
        uint8_t const /*groupIndex*/,
        midi2::MidiSystemExclusive8Status const& /*status*/,
        uint8_t const /*numberOfValidDataBytesThisMessage*/,
        uint8_t const /*streamId*/,
        array_view<uint8_t const> const /*dataBytes*/,
        uint32_t const /*arrayStartIndex*/)
    {
        throw hresult_not_implemented();
    }

    _Use_decl_annotations_
    midi2::MidiMessage128 MidiMessageBuilder::BuildSystemExclusive8MessageFromBuffer(
        internal::MidiTimestamp const /*timestamp*/,
        uint8_t const /*groupIndex*/,
        midi2::MidiSystemExclusive8Status const& /*status*/,
        uint8_t const /*numberOfValidDataBytesThisMessage*/,
        foundation::IMemoryBuffer const& /*buffer*/,
        uint32_t const /*byteOffsetInBuffer*/)
    {
        throw hresult_not_implemented();
    }

    _Use_decl_annotations_
    midi2::MidiMessage128 MidiMessageBuilder::BuildMixedDataSetChunkHeaderMessage(
        internal::MidiTimestamp const /*timestamp*/,
        uint8_t const /*groupIndex*/,
        uint8_t const /*mdsId*/,
        uint16_t const /*numberValidDataBytesInThisChunk*/,
        uint16_t const /*numberChunksInMixedDataSet*/,
        uint16_t const /*numberOfThisChunk*/,
        uint16_t const /*manufacturerId*/,
        uint16_t const /*deviceId*/,
        uint16_t const /*subId1*/,
        uint16_t const /*subId2*/)
    {
        throw hresult_not_implemented();
    }

    _Use_decl_annotations_
    midi2::MidiMessage128 MidiMessageBuilder::BuildMixedDataSetChunkDataMessage(
        internal::MidiTimestamp const /*timestamp*/,
        uint8_t const /*groupIndex*/,
        uint8_t const /*mdsId*/,
        uint8_t const /*dataByte00*/,
        uint8_t const /*dataByte01*/,
        uint8_t const /*dataByte02*/,
        uint8_t const /*dataByte03*/,
        uint8_t const /*dataByte04*/,
        uint8_t const /*dataByte05*/,
        uint8_t const /*dataByte06*/,
        uint8_t const /*dataByte07*/,
        uint8_t const /*dataByte08*/,
        uint8_t const /*dataByte09*/,
        uint8_t const /*dataByte10*/,
        uint8_t const /*dataByte11*/,
        uint8_t const /*dataByte12*/,
        uint8_t const /*dataByte13*/)
    {
        throw hresult_not_implemented();
    }

    _Use_decl_annotations_
    midi2::MidiMessage128 MidiMessageBuilder::BuildMixedDataSetChunkDataMessageFromArray(
        internal::MidiTimestamp const /*timestamp*/,
        uint8_t const /*groupIndex*/,
        uint8_t const /*mdsId*/,
        uint8_t const /*numberOfValidDataBytesThisMessage*/,
        array_view<uint8_t const> const /*dataBytes*/,
        uint32_t const /*arrayStartIndex*/)
    {
        throw hresult_not_implemented();
    }

    _Use_decl_annotations_
    midi2::MidiMessage128 MidiMessageBuilder::BuildMixedDataSetChunkDataMessageFromBuffer(
        internal::MidiTimestamp const /*timestamp*/,
        uint8_t const /*groupIndex*/,
        uint8_t const /*mdsId*/,
        uint8_t const /*numberOfValidDataBytesThisMessage*/,
        foundation::IMemoryBuffer const& /*buffer*/,
        uint32_t const /*byteOffsetInBuffer*/)
    {
        throw hresult_not_implemented();
    }

    _Use_decl_annotations_
    midi2::MidiMessage128 MidiMessageBuilder::BuildFlexDataMessage(
        internal::MidiTimestamp const timestamp,
        uint8_t const groupIndex,
        uint8_t const form,
        uint8_t const address,
        uint8_t const channel,
        uint8_t const statusBank,
        uint8_t const status,
        uint32_t const word1Data,
        uint32_t const word2Data,
        uint32_t const word3Data)
    {
        return midi2::MidiMessage128(
            timestamp,
            (uint32_t)(
                0xD << 28 |
                internal::CleanupNibble(groupIndex) << 24 |
                internal::CleanupCrumb(form) << 22 |
                internal::CleanupCrumb(address) << 20 |
                internal::CleanupNibble(channel) << 16 |
                statusBank << 8 |
                status),
            word1Data,
            word2Data,
            word3Data
        );
    }

    _Use_decl_annotations_
    midi2::MidiMessage128 MidiMessageBuilder::BuildStreamMessage(
        internal::MidiTimestamp const timestamp,
        uint8_t const form,
        uint16_t const status,
        uint16_t const word0RemainingData,
        uint32_t const word1Data,
        uint32_t const word2Data,
        uint32_t const word3Data) noexcept
    {       
        return midi2::MidiMessage128(
            timestamp, 
            (uint32_t)(
                0xF << 28 |
                internal::CleanupCrumb(form) << 26 |
                internal::CleanupInt10(status) << 16 |
                word0RemainingData), 
            word1Data,
            word2Data,
            word3Data
            );
    }

}
