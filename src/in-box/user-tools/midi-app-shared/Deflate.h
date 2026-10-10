// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

// Deflate (RFC 1951) for the zip files the tools read and write, so no tool needs a compression
// library. Raw deflate only, without a zlib or gzip wrapper, because that's what a zip holds.

#include <sal.h>

#include <cstddef>
#include <cstdint>
#include <vector>

namespace midiapp
{
    // Decompresses a whole stream, which must come to exactly expectedSize bytes. False for a
    // damaged stream, including one that would run past expectedSize; nothing past it is written.
    bool Inflate(
        _In_reads_bytes_opt_(size) uint8_t const* data,
        _In_ size_t size,
        _In_ size_t expectedSize,
        _Out_ std::vector<uint8_t>& output) noexcept;

    // Compresses a stream handed over in pieces, so a large file never has to be in memory at once.
    class DeflateEncoder
    {
    public:
        // Appends the compressed form of the next piece to output. The last piece, which can be
        // empty, is passed with last set. False when memory runs out, or after the last piece.
        bool Write(
            _In_reads_bytes_opt_(size) uint8_t const* data,
            _In_ size_t size,
            _In_ bool last,
            _Inout_ std::vector<uint8_t>& output) noexcept;

    private:
        struct Symbol
        {
            uint16_t LiteralOrLength{ 0 };

            // 0 for a literal
            uint16_t Distance{ 0 };
        };

        void CompressPiece(_In_ size_t start, _In_ bool last, _Inout_ std::vector<uint8_t>& output);
        void Remember(_In_ size_t position) noexcept;
        void FindMatch(_In_ size_t position, _In_ size_t end, _Out_ size_t& length, _Out_ size_t& distance) const noexcept;
        void FlushBlock(_In_ size_t blockStart, _In_ size_t blockEnd, _In_ bool final, _Inout_ std::vector<uint8_t>& output);
        void PutBits(_In_ uint32_t value, _In_ int count, _Inout_ std::vector<uint8_t>& output);
        void PutToByte(_Inout_ std::vector<uint8_t>& output);

        // the history kept from earlier pieces, then the piece being compressed
        std::vector<uint8_t> m_buffer{};

        std::vector<int32_t> m_head{};
        std::vector<int32_t> m_previous{};
        std::vector<Symbol> m_symbols{};

        uint64_t m_bitBuffer{ 0 };
        int m_bitCount{ 0 };
        bool m_finished{ false };
    };

    // The whole of data, compressed in one call.
    bool Deflate(
        _In_reads_bytes_opt_(size) uint8_t const* data,
        _In_ size_t size,
        _Out_ std::vector<uint8_t>& output) noexcept;
}
