// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

#include "MessageStore.h"

namespace midi2monitor
{
    // ---------------------------------------------------------------------------------------
    // Universal MIDI Packet field accessors. These are deliberately constexpr and free of any
    // WinRT call so that the capture path can classify a message without leaving native code.
    // ---------------------------------------------------------------------------------------

    constexpr uint8_t MessageTypeUtility32 = 0x0;
    constexpr uint8_t MessageTypeSystemCommon32 = 0x1;
    constexpr uint8_t MessageTypeMidi1ChannelVoice32 = 0x2;
    constexpr uint8_t MessageTypeData64 = 0x3;
    constexpr uint8_t MessageTypeMidi2ChannelVoice64 = 0x4;
    constexpr uint8_t MessageTypeData128 = 0x5;
    constexpr uint8_t MessageTypeFlexData128 = 0xD;
    constexpr uint8_t MessageTypeStream128 = 0xF;

    constexpr uint8_t GetMessageTypeFromFirstWord(uint32_t word0) noexcept
    {
        return static_cast<uint8_t>((word0 >> 28) & 0x0F);
    }

    constexpr uint8_t GetGroupIndexFromFirstWord(uint32_t word0) noexcept
    {
        return static_cast<uint8_t>((word0 >> 24) & 0x0F);
    }

    constexpr uint8_t GetChannelIndexFromFirstWord(uint32_t word0) noexcept
    {
        return static_cast<uint8_t>((word0 >> 16) & 0x0F);
    }

    constexpr uint8_t GetWordCountFromMessageType(uint8_t messageType) noexcept
    {
        switch (messageType & 0x0F)
        {
        case 0x0:
        case 0x1:
        case 0x2:
        case 0x6:
        case 0x7:
            return 1;
        case 0x3:
        case 0x4:
        case 0x8:
        case 0x9:
        case 0xA:
            return 2;
        case 0xB:
        case 0xC:
            return 3;
        default:
            return 4;
        }
    }

    constexpr bool MessageTypeHasGroup(uint8_t messageType) noexcept
    {
        switch (messageType & 0x0F)
        {
        case MessageTypeSystemCommon32:
        case MessageTypeMidi1ChannelVoice32:
        case MessageTypeData64:
        case MessageTypeMidi2ChannelVoice64:
        case MessageTypeData128:
        case MessageTypeFlexData128:
            return true;
        default:
            return false;
        }
    }

    constexpr bool MessageTypeHasChannel(uint8_t messageType) noexcept
    {
        switch (messageType & 0x0F)
        {
        case MessageTypeMidi1ChannelVoice32:
        case MessageTypeMidi2ChannelVoice64:
        case MessageTypeFlexData128:
            return true;
        default:
            return false;
        }
    }

    struct UmpInfo
    {
        uint8_t MessageType{ 0 };
        uint8_t WordCount{ 1 };
        bool HasGroup{ false };
        bool HasChannel{ false };
        uint8_t GroupIndex{ 0 };
        uint8_t ChannelIndex{ 0 };
        MessageTraits Traits{ MessageTraits::None };
    };

    UmpInfo InspectMessage(uint32_t word0) noexcept;

    // FlexData channel is only meaningful when the address field says "channel"
    bool FlexDataMessageAddressesChannel(uint32_t word0) noexcept;

    // ---------------------------------------------------------------------------------------
    // Display helpers. These may call into the SDK and are only ever used when a row is
    // actually materialized for display or export.
    // ---------------------------------------------------------------------------------------

    // Decoded content, split into localized labels and their values. Keeping them apart lets the
    // UI colour the values without any parsing, and keeps the labels translatable.
    struct DecodedFields
    {
        static constexpr size_t MaxFields = 4;

        std::array<winrt::hstring, MaxFields> Labels{};
        std::array<winrt::hstring, MaxFields> Values{};
        uint32_t Count{ 0 };

        void Add(winrt::hstring const& label, std::wstring const& value) noexcept
        {
            if (Count >= MaxFields)
            {
                return;
            }

            Labels[Count] = label;
            Values[Count] = winrt::hstring{ value };
            Count++;
        }

        winrt::hstring ToDisplayString() const;
    };

    // Friendly name for the message, from the SDK's MidiMessageHelper
    winrt::hstring GetMessageDisplayName(uint32_t word0) noexcept;

    // Human readable contents of the message, empty when we have no decoder for it
    DecodedFields DecodeMessage(MessageRecord const& record) noexcept;

    // 0-7, used to pick the chiclet colour for the message type
    uint32_t GetMessageTypeColorIndex(uint8_t messageType) noexcept;
}
