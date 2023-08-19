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
    winrt::Windows::Devices::Midi2::MidiUmp32 MidiMessageBuilder::BuildUtilityMessage(
        _In_ internal::MidiTimestamp const timestamp,
        _In_ uint8_t const reserved,
        _In_ uint8_t const status,
        _In_ uint32_t const dataOrReserved)
    {      
        return MidiUmp32(
            timestamp,
            (uint32_t)(
                0x0 | 
                internal::CleanupNibble(reserved) << 24 |
                internal::CleanupNibble(status) << 20 |
                dataOrReserved)
        );

    }

    winrt::Windows::Devices::Midi2::MidiUmp32 MidiMessageBuilder::BuildSystemMessage(
        _In_ internal::MidiTimestamp const timestamp,
        _In_ uint8_t const groupIndex,
        _In_ uint8_t const status,
        _In_ uint8_t const midi1Byte2,
        _In_ uint8_t const midi1Byte3)
    {
        return MidiUmp32(
            timestamp,
            (uint32_t)(
                0x1 |
                internal::CleanupNibble(groupIndex) << 24 |
                status << 16 |
                midi1Byte2 << 8 |
                midi1Byte3)
        );
    }

    winrt::Windows::Devices::Midi2::MidiUmp32 MidiMessageBuilder::BuildMidi1ChannelVoiceMessage(
        _In_ internal::MidiTimestamp const timestamp,
        _In_ uint8_t const groupIndex,
        _In_ uint8_t const status,
        _In_ uint8_t const channel,
        _In_ uint8_t const byte3,
        _In_ uint8_t const byte4)
    {
        return MidiUmp32(
            timestamp,
            (uint32_t)(
                0x2 << 28 |
                internal::CleanupNibble(groupIndex) << 24 |
                internal::CleanupNibble(status) << 20 |
                internal::CleanupNibble(channel) << 16 |
                internal::CleanupByte7(byte3) << 8 |
                internal::CleanupByte7(byte4))
        );
    }

    winrt::Windows::Devices::Midi2::MidiUmp64 MidiMessageBuilder::BuildSysEx7Message(
        _In_ internal::MidiTimestamp const timestamp,
        _In_ uint8_t const groupIndex,
        _In_ uint8_t const status,
        _In_ uint8_t const numberOfBytes,
        _In_ uint8_t const dataByte0,
        _In_ uint8_t const dataByte1,
        _In_ uint8_t const dataByte2,
        _In_ uint8_t const dataByte3,
        _In_ uint8_t const dataByte4,
        _In_ uint8_t const dataByte5)
    {
        
        return MidiUmp64(
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

    winrt::Windows::Devices::Midi2::MidiUmp64 MidiMessageBuilder::BuildSysEx7MessageFromArray(
        _In_ internal::MidiTimestamp const /*timestamp*/,
        _In_ uint8_t const /*groupIndex*/,
        _In_ uint8_t const /*status*/,
        _In_ uint8_t const /*numberOfBytes*/,
        _In_ array_view<uint8_t const> const /*dataBytes*/,
        _In_ uint32_t const /*arrayStartIndex*/)
    {
        throw hresult_not_implemented();
    }

    winrt::Windows::Devices::Midi2::MidiUmp64 MidiMessageBuilder::BuildSysEx7MessageFromBuffer(
        _In_ internal::MidiTimestamp const /*timestamp*/,
        _In_ uint8_t const /*groupIndex*/,
        _In_ uint8_t const /*status*/,
        _In_ uint8_t const /*numberOfBytes*/,
        _In_ winrt::Windows::Foundation::IMemoryBuffer const& /*buffer*/,
        _In_ uint32_t const /*byteOffsetInBuffer*/)
    {
        throw hresult_not_implemented();
    }

    winrt::Windows::Devices::Midi2::MidiUmp64 MidiMessageBuilder::BuildMidi2ChannelVoiceMessage(
        _In_ internal::MidiTimestamp const timestamp,
        _In_ uint8_t const groupIndex,
        _In_ uint8_t const status,
        _In_ uint8_t const channel,
        _In_ uint16_t const index,
        _In_ uint32_t const data)
    {
        return MidiUmp64(
            timestamp,
            (uint32_t)(
                0x4 << 28 |
                internal::CleanupNibble(groupIndex) << 24 |
                internal::CleanupNibble(status) << 20 |
                internal::CleanupNibble(channel) << 16 |
                index), 
            data);
    }

    winrt::Windows::Devices::Midi2::MidiUmp128 MidiMessageBuilder::BuildSysEx8Message(
        _In_ internal::MidiTimestamp const timestamp,
        _In_ uint8_t const groupIndex,
        _In_ winrt::Windows::Devices::Midi2::MidiSysEx8Status const& status,
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
        _In_ uint8_t const dataByte12)
    {

        return MidiUmp128(
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

    winrt::Windows::Devices::Midi2::MidiUmp128 MidiMessageBuilder::BuildSysEx8MessageFromArray(
        _In_ internal::MidiTimestamp const /*timestamp*/,
        _In_ uint8_t const /*groupIndex*/,
        _In_ winrt::Windows::Devices::Midi2::MidiSysEx8Status const& /*status*/,
        _In_ uint8_t const /*numberOfValidDataBytesThisMessage*/,
        _In_ uint8_t const /*streamId*/,
        _In_ array_view<uint8_t const> const /*dataBytes*/,
        _In_ uint32_t const /*arrayStartIndex*/)
    {
        throw hresult_not_implemented();
    }

    winrt::Windows::Devices::Midi2::MidiUmp128 MidiMessageBuilder::BuildSysEx8MessageFromBuffer(
        _In_ internal::MidiTimestamp const /*timestamp*/,
        _In_ uint8_t const /*groupIndex*/,
        _In_ winrt::Windows::Devices::Midi2::MidiSysEx8Status const& /*status*/,
        _In_ uint8_t const /*numberOfValidDataBytesThisMessage*/,
        _In_ winrt::Windows::Foundation::IMemoryBuffer const& /*buffer*/,
        _In_ uint32_t const /*byteOffsetInBuffer*/)
    {
        throw hresult_not_implemented();
    }

    winrt::Windows::Devices::Midi2::MidiUmp128 MidiMessageBuilder::BuildMixedDataSetChunkHeaderMessage(
        _In_ internal::MidiTimestamp const timestamp,
        _In_ uint8_t const groupIndex,
        _In_ uint8_t const mdsId,
        _In_ uint16_t const numberValidDataBytesInThisChunk,
        _In_ uint16_t const numberChunksInMixedDataSet,
        _In_ uint16_t const numberOfThisChunk,
        _In_ uint16_t const manufacturerId,
        _In_ uint16_t const deviceId,
        _In_ uint16_t const subId1,
        _In_ uint16_t const subId2)
    {
        throw hresult_not_implemented();
    }

    winrt::Windows::Devices::Midi2::MidiUmp128 MidiMessageBuilder::BuildMixedDataSetChunkDataMessage(
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
        _In_ uint8_t const dataByte13)
    {
        throw hresult_not_implemented();
    }

    winrt::Windows::Devices::Midi2::MidiUmp128 MidiMessageBuilder::BuildMixedDataSetChunkDataMessageFromArray(
        _In_ internal::MidiTimestamp const /*timestamp*/,
        _In_ uint8_t const /*groupIndex*/,
        _In_ uint8_t const /*mdsId*/,
        _In_ uint8_t const /*numberOfValidDataBytesThisMessage*/,
        _In_ array_view<uint8_t const> const /*dataBytes*/,
        _In_ uint32_t const /*arrayStartIndex*/)
    {
        throw hresult_not_implemented();
    }

    winrt::Windows::Devices::Midi2::MidiUmp128 MidiMessageBuilder::BuildMixedDataSetChunkDataMessageFromBuffer(
        _In_ internal::MidiTimestamp const /*timestamp*/,
        _In_ uint8_t const /*groupIndex*/,
        _In_ uint8_t const /*mdsId*/,
        _In_ uint8_t const /*numberOfValidDataBytesThisMessage*/,
        _In_ winrt::Windows::Foundation::IMemoryBuffer const& /*buffer*/,
        _In_ uint32_t const /*byteOffsetInBuffer*/)
    {
        throw hresult_not_implemented();
    }

    winrt::Windows::Devices::Midi2::MidiUmp128 MidiMessageBuilder::BuildFlexDataMessage(
        _In_ internal::MidiTimestamp const timestamp,
        _In_ uint8_t const groupIndex,
        _In_ uint8_t const form,
        _In_ uint8_t const address,
        _In_ uint8_t const channel,
        _In_ uint8_t const statusBank,
        _In_ uint8_t const status,
        _In_ uint32_t const word1Data,
        _In_ uint32_t const word2Data,
        _In_ uint32_t const word3Data)
    {
        return MidiUmp128(
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

    winrt::Windows::Devices::Midi2::MidiUmp128 MidiMessageBuilder::BuildUmpStreamMessage(
        _In_ internal::MidiTimestamp const timestamp,
        _In_ uint8_t const form,
        _In_ uint16_t const status,
        _In_ uint16_t const word0RemainingData,
        _In_ uint32_t const word1Data,
        _In_ uint32_t const word2Data,
        _In_ uint32_t const word3Data)
    {       
        return MidiUmp128(
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
