// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

namespace midisysextool
{
    enum class SysExByteKind : uint8_t
    {
        Data = 0,
        Start = 1,          // F0, or a SysEx7 start word
        End = 2,            // F7, or a SysEx7 end word
        Disallowed = 3,     // a status byte that has no business inside a SysEx stream
        Complete = 4        // a SysEx7 word pair carrying a whole message, UMP view only
    };

    // Start/Continue/End/Complete, from the status nibble of a message type 3 word.
    SysExByteKind ClassifySysEx7Word(uint32_t word0) noexcept;

    struct SysExStats
    {
        uint64_t TotalBytes{ 0 };
        uint32_t CompleteMessages{ 0 };
        uint32_t DisallowedBytes{ 0 };
        uint32_t IgnoredRealTimeBytes{ 0 };
        bool IsInsideMessage{ false };
    };

    // Accumulates an incoming SysEx dump. Dumps run to hundreds of kilobytes, so the buffer is
    // reserved up front from the customer's setting and then grows geometrically: every byte
    // has to be retained for saving, and reallocating per message would dominate the cost.
    class SysExBuffer
    {
    public:
        void Reset(uint32_t initialBytes) noexcept;

        // Appends one MIDI 1.0 byte, classifying it and tracking F0/F7 framing.
        SysExByteKind AppendByte(uint8_t value) noexcept;

        // Appends bytes as handed over by MidiSystemExclusiveReceiver, framing already restored.
        void AppendBytes(uint8_t const* data, size_t count) noexcept;

        // Records a message type 3 word pair for the UMP view. The receiver reports bytes only,
        // so the words are collected separately and are display state, not part of the dump.
        void AppendDisplayWords(uint32_t word0, uint32_t word1) noexcept;

        std::vector<uint8_t> const& Bytes() const noexcept { return m_bytes; }
        std::vector<uint32_t> const& Words() const noexcept { return m_words; }

        SysExStats const& Stats() const noexcept { return m_stats; }

        bool IsEmpty() const noexcept { return m_bytes.empty(); }

        // true when every message that was opened was also closed
        bool IsWellFormed() const noexcept { return !m_stats.IsInsideMessage && m_stats.DisallowedBytes == 0; }

        bool WriteToFile(std::wstring const& path) const noexcept;
        bool ReadFromFile(std::wstring const& path) noexcept;

    private:
        std::vector<uint8_t> m_bytes{};
        std::vector<uint32_t> m_words{};
        SysExStats m_stats{};
    };

    // System real time may legally appear between SysEx data bytes and is simply passed through.
    bool IsAllowedRealTimeStatus(uint8_t value) noexcept;
}
