// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

#include <sal.h>

#include <cstddef>
#include <cstdint>

namespace SoundFontSynth
{
    // Words needed to carry a System Exclusive payload of this many bytes as 64-bit SysEx7 packets.
    constexpr size_t SysEx7WordCount(_In_ size_t payloadBytes) noexcept
    {
        return (payloadBytes == 0) ? 2 : ((payloadBytes + 5) / 6) * 2;
    }

    // Splits a payload, without F0 and F7, into UMP data messages on one group. Returns the words
    // written, or zero when the buffer is too small or a byte has the top bit set.
    inline size_t BuildSysEx7Packets(
        _In_ uint8_t group,
        _In_reads_(payloadBytes) uint8_t const* payload,
        _In_ size_t payloadBytes,
        _Out_writes_(capacityWords) uint32_t* words,
        _In_ size_t capacityWords) noexcept
    {
        auto const needed = SysEx7WordCount(payloadBytes);

        if (words == nullptr || capacityWords < needed || (payload == nullptr && payloadBytes > 0))
        {
            return 0;
        }

        for (size_t i = 0; i < payloadBytes; i++)
        {
            if ((payload[i] & 0x80) != 0)
            {
                return 0;
            }
        }

        constexpr uint32_t StatusComplete = 0x0;
        constexpr uint32_t StatusStart = 0x1;
        constexpr uint32_t StatusContinue = 0x2;
        constexpr uint32_t StatusEnd = 0x3;

        size_t written{ 0 };
        size_t offset{ 0 };

        do
        {
            auto const remaining = payloadBytes - offset;
            auto const count = (remaining > 6) ? 6 : remaining;
            auto const first = (offset == 0);
            auto const last = (offset + count >= payloadBytes);

            auto const status = (first && last) ? StatusComplete : first ? StatusStart : last ? StatusEnd : StatusContinue;

            uint8_t bytes[6]{};

            for (size_t i = 0; i < count; i++)
            {
                bytes[i] = payload[offset + i];
            }

            words[written++] =
                (0x3u << 28) |
                (static_cast<uint32_t>(group & 0x0F) << 24) |
                (status << 20) |
                (static_cast<uint32_t>(count) << 16) |
                (static_cast<uint32_t>(bytes[0]) << 8) |
                static_cast<uint32_t>(bytes[1]);

            words[written++] =
                (static_cast<uint32_t>(bytes[2]) << 24) |
                (static_cast<uint32_t>(bytes[3]) << 16) |
                (static_cast<uint32_t>(bytes[4]) << 8) |
                static_cast<uint32_t>(bytes[5]);

            offset += count;
        } while (offset < payloadBytes);

        return written;
    }
}
