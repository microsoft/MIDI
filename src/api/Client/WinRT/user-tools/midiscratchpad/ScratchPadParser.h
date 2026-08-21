// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

namespace midiscratchpad
{
    struct ParsedInput
    {
        bool HasError{ false };

        // 1-based, so it can be reported the way the customer counts lines
        uint32_t ErrorLine{ 0 };
        std::wstring ErrorResourceKey{};
        std::wstring ErrorToken{};

        std::vector<uint8_t> Bytes{};
        std::vector<uint32_t> Words{};

        // UMP mode only: how many complete packets the words form
        uint32_t PacketCount{ 0 };

        bool IsEmpty() const noexcept { return Bytes.empty() && Words.empty(); }
    };

    // Hex bytes separated by whitespace. '#' or '//' begins a comment that runs to end of line.
    ParsedInput ParseMidi1Bytes(std::wstring_view text) noexcept;

    // 32 bit UMP words, eight hex digits each with an optional 0x prefix. Also checks that the
    // words form whole packets, using the message type in each packet's first word.
    ParsedInput ParseUmpWords(std::wstring_view text) noexcept;
}
