// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

// Builds the messages the tests push through blocks. Groups, channels and notes are counted
// from 0, the way the words carry them.

#include <array>
#include <cstdint>

namespace patchbaytests
{
    struct Message
    {
        std::array<uint32_t, 4> Words{};
        uint8_t Count{ 1 };

        bool operator==(Message const&) const = default;
    };

    inline Message Midi1(uint8_t group, uint8_t status, uint8_t channel, uint8_t data1, uint8_t data2)
    {
        Message message{};

        message.Words[0] = (0x2u << 28) |
            (static_cast<uint32_t>(group & 0x0F) << 24) |
            (static_cast<uint32_t>(status & 0x0F) << 20) |
            (static_cast<uint32_t>(channel & 0x0F) << 16) |
            (static_cast<uint32_t>(data1 & 0x7F) << 8) |
            static_cast<uint32_t>(data2 & 0x7F);

        message.Count = 1;
        return message;
    }

    inline Message Midi2(uint8_t group, uint8_t status, uint8_t channel, uint8_t index, uint8_t attribute, uint32_t data)
    {
        Message message{};

        message.Words[0] = (0x4u << 28) |
            (static_cast<uint32_t>(group & 0x0F) << 24) |
            (static_cast<uint32_t>(status & 0x0F) << 20) |
            (static_cast<uint32_t>(channel & 0x0F) << 16) |
            (static_cast<uint32_t>(index) << 8) |
            static_cast<uint32_t>(attribute);

        message.Words[1] = data;
        message.Count = 2;
        return message;
    }

    inline Message System(uint8_t group, uint8_t status, uint8_t data1 = 0, uint8_t data2 = 0)
    {
        Message message{};

        message.Words[0] = (0x1u << 28) |
            (static_cast<uint32_t>(group & 0x0F) << 24) |
            (static_cast<uint32_t>(status) << 16) |
            (static_cast<uint32_t>(data1 & 0x7F) << 8) |
            static_cast<uint32_t>(data2 & 0x7F);

        message.Count = 1;
        return message;
    }

    // A jitter reduction timestamp: one word, and no group.
    inline Message Utility()
    {
        Message message{};
        message.Words[0] = (0x0u << 28) | (0x2u << 20) | 0x1234u;
        message.Count = 1;
        return message;
    }

    inline Message SysEx7(uint8_t group)
    {
        Message message{};
        message.Words[0] = (0x3u << 28) | (static_cast<uint32_t>(group & 0x0F) << 24) | (0x0u << 20) | (0x2u << 16) | 0x7E7Fu;
        message.Words[1] = 0x06010000u;
        message.Count = 2;
        return message;
    }

    // Endpoint discovery: four words, and no group.
    inline Message Stream()
    {
        Message message{};
        message.Words[0] = 0xF0000101u;
        message.Words[1] = 0x0000001Fu;
        message.Count = 4;
        return message;
    }
}
