// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================
//
// Classifies messages for the capture path, and hands the Details column's description to the
// shared describer in midi-app-shared\UmpDescriber, which Windows MIDI Patchbay uses too.
//

#include "pch.h"
#include "MidiMessageDecoder.h"
#include "StringResources.h"

namespace midi2monitor
{
    namespace
    {
        // MIDI 1.0 System Common / System Real Time status bytes
        constexpr uint8_t StatusTimingClock = 0xF8;
        constexpr uint8_t StatusActiveSensing = 0xFE;
        constexpr uint8_t FirstSystemRealTimeStatus = 0xF8;

        // Utility message statuses that carry timing
        constexpr uint8_t UtilityStatusJitterReductionClock = 0x1;
        constexpr uint8_t UtilityStatusJitterReductionTimestamp = 0x2;

        // Field labels come from resources. They are read once and cached because the decoder
        // runs for every visible row.
        midiapp::UmpDescriptionLabels const& Labels() noexcept
        {
            static midiapp::UmpDescriptionLabels labels
            {
                resources::GetString(L"DecodeLabelNote"),
                resources::GetString(L"DecodeLabelVelocity"),
                resources::GetString(L"DecodeLabelPressure"),
                resources::GetString(L"DecodeLabelController"),
                resources::GetString(L"DecodeLabelValue"),
                resources::GetString(L"DecodeLabelProgram"),
                resources::GetString(L"DecodeLabelBank"),
                resources::GetString(L"DecodeLabelBend"),
                resources::GetString(L"DecodeLabelIndex"),
                resources::GetString(L"DecodeLabelBytes"),
                resources::GetString(L"DecodeLabelStatus"),
                resources::GetString(L"DecodeLabelSong"),
                resources::GetString(L"DecodeLabelPosition"),
                resources::GetString(L"DecodeLabelFrame"),
                resources::GetString(L"DecodeLabelAttribute"),
                resources::GetString(L"DecodeLabelText"),
                resources::GetString(L"DecodeLabelName"),
                resources::GetString(L"DecodeLabelTicks"),
                resources::GetString(L"DecodeLabelTempo"),
                resources::GetString(L"DecodeLabelStreamId"),
                resources::GetString(L"DecodeLabelFunctionBlock"),
                resources::GetString(L"DecodeLabelForm"),
                resources::GetString(L"DecodeLabelTimeSignature"),
                resources::GetString(L"DecodeLabelKeySignature"),
                resources::GetString(L"DecodeLabelPerNote"),
                resources::GetString(L"DecodeLabelOptions"),
                resources::GetString(L"DecodeLabelNoteOffValue"),
                resources::GetString(L"DecodeTimeSignatureNonStandardFormat")
            };

            return labels;
        }
    }
    _Use_decl_annotations_
    UmpInfo InspectMessage(uint32_t word0) noexcept
    {
        UmpInfo info{};

        info.MessageType = GetMessageTypeFromFirstWord(word0);
        info.WordCount = GetWordCountFromMessageType(info.MessageType);
        info.HasGroup = MessageTypeHasGroup(info.MessageType);
        info.GroupIndex = info.HasGroup ? GetGroupIndexFromFirstWord(word0) : uint8_t{ 0 };

        if (info.MessageType == MessageTypeFlexData128)
        {
            info.HasChannel = FlexDataMessageAddressesChannel(word0);
        }
        else
        {
            info.HasChannel = MessageTypeHasChannel(info.MessageType);
        }

        info.ChannelIndex = info.HasChannel ? GetChannelIndexFromFirstWord(word0) : uint8_t{ 0 };

        if (info.MessageType == MessageTypeSystemCommon32)
        {
            auto const status = static_cast<uint8_t>((word0 >> 16) & 0xFF);

            if (status >= FirstSystemRealTimeStatus)
            {
                info.Traits = info.Traits | MessageTraits::SystemRealTime;
            }

            if (status == StatusTimingClock)
            {
                info.Traits = info.Traits | MessageTraits::Clock;
            }
            else if (status == StatusActiveSensing)
            {
                info.Traits = info.Traits | MessageTraits::ActiveSense;
            }
        }
        else if (info.MessageType == MessageTypeUtility32)
        {
            auto const status = static_cast<uint8_t>((word0 >> 20) & 0x0F);

            if (status == UtilityStatusJitterReductionClock ||
                status == UtilityStatusJitterReductionTimestamp)
            {
                info.Traits = info.Traits | MessageTraits::Clock;
            }
        }

        return info;
    }

    _Use_decl_annotations_
    bool FlexDataMessageAddressesChannel(uint32_t word0) noexcept
    {
        // address field is bits 23-22. 0 = channel, 1 = group
        return ((word0 >> 22) & 0x03) == 0;
    }

    _Use_decl_annotations_
    winrt::hstring GetMessageDisplayName(uint32_t word0) noexcept
    {
        try
        {
            return midi2msg::MidiMessageHelper::GetMessageDisplayNameFromFirstWord(word0);
        }
        MIDI_MONITOR_CATCH_AND_LOG(L"Unable to retrieve the message display name.")

        return {};
    }

    _Use_decl_annotations_
    DecodedFields DecodeMessage(MessageRecord const& record) noexcept
    {
        if (record.Kind != RecordKind::MidiMessage || record.WordCount == 0)
        {
            return {};
        }

        return midiapp::DescribeUmp(record.Words, record.WordCount, Labels());
    }

    _Use_decl_annotations_
    uint32_t GetMessageTypeColorIndex(uint8_t messageType) noexcept
    {
        return midiapp::UmpMessageTypeColorIndex(messageType);
    }
}