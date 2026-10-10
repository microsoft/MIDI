// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#include "Deflate.h"

#include <algorithm>
#include <array>
#include <cstring>
#include <utility>

namespace midiapp
{
    namespace
    {
        constexpr int MaximumCodeBits{ 15 };
        constexpr int MaximumCodeLengthBits{ 7 };

        // 286 literal and length codes can appear in a stream; the fixed code defines 288.
        constexpr int LiteralLengthCount{ 288 };
        constexpr int UsableLiteralLengthCount{ 286 };
        constexpr int DistanceCount{ 30 };
        constexpr int CodeLengthCount{ 19 };

        constexpr int EndOfBlock{ 256 };
        constexpr int FirstLengthSymbol{ 257 };

        constexpr size_t WindowSize{ 32768 };
        constexpr size_t MinimumMatch{ 3 };
        constexpr size_t MaximumMatch{ 258 };
        constexpr size_t MaximumStoredBlock{ 65535 };

        constexpr int HashBits{ 15 };
        constexpr size_t HashSize{ size_t{ 1 } << HashBits };

        // How hard the encoder looks for a match. Longer searches find slightly better matches
        // and cost far more time.
        constexpr int MaximumChain{ 64 };
        constexpr size_t GoodEnoughMatch{ 128 };

        constexpr size_t SymbolsPerBlock{ 16384 };

        // A large buffer is compressed a piece at a time, so the positions in the hash chains
        // always fit in 32 bits.
        constexpr size_t PieceSize{ 1024 * 1024 };

        // RFC 1951 section 3.2.5
        constexpr std::array<uint16_t, 29> LengthBase{
            3, 4, 5, 6, 7, 8, 9, 10, 11, 13, 15, 17, 19, 23, 27, 31,
            35, 43, 51, 59, 67, 83, 99, 115, 131, 163, 195, 227, 258 };

        constexpr std::array<uint8_t, 29> LengthExtraBits{
            0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 2, 2, 2, 2,
            3, 3, 3, 3, 4, 4, 4, 4, 5, 5, 5, 5, 0 };

        constexpr std::array<uint16_t, DistanceCount> DistanceBase{
            1, 2, 3, 4, 5, 7, 9, 13, 17, 25, 33, 49, 65, 97, 129, 193,
            257, 385, 513, 769, 1025, 1537, 2049, 3073, 4097, 6145, 8193, 12289, 16385, 24577 };

        constexpr std::array<uint8_t, DistanceCount> DistanceExtraBits{
            0, 0, 0, 0, 1, 1, 2, 2, 3, 3, 4, 4, 5, 5, 6, 6,
            7, 7, 8, 8, 9, 9, 10, 10, 11, 11, 12, 12, 13, 13 };

        // RFC 1951 section 3.2.7: the order the code length code's own lengths are sent in
        constexpr std::array<uint8_t, CodeLengthCount> CodeLengthOrder{
            16, 17, 18, 0, 8, 7, 9, 6, 10, 5, 11, 4, 12, 3, 13, 2, 14, 1, 15 };

        // RFC 1951 section 3.2.6
        constexpr uint8_t FixedDistanceLength{ 5 };

        std::array<uint8_t, LiteralLengthCount> FixedLiteralLengths() noexcept
        {
            std::array<uint8_t, LiteralLengthCount> lengths{};

            for (int symbol = 0; symbol < LiteralLengthCount; ++symbol)
            {
                lengths[symbol] = static_cast<uint8_t>(symbol < 144 ? 8 : symbol < 256 ? 9 : symbol < 280 ? 7 : 8);
            }

            return lengths;
        }

        // ------------------------------------------------------------------------ decoding

        // Bits come out of each byte from its low end first.
        class BitReader
        {
        public:
            BitReader(_In_reads_bytes_opt_(size) uint8_t const* data, _In_ size_t size) noexcept :
                m_data{ data },
                m_size{ size }
            {
            }

            bool Bits(_In_ int count, _Out_ uint32_t& value) noexcept
            {
                while (m_bitCount < count)
                {
                    if (m_position >= m_size)
                    {
                        value = 0;
                        return false;
                    }

                    m_buffer |= static_cast<uint32_t>(m_data[m_position++]) << m_bitCount;
                    m_bitCount += 8;
                }

                value = m_buffer & ((1u << count) - 1u);
                m_buffer >>= count;
                m_bitCount -= count;

                return true;
            }

            // A stored block starts on a byte boundary, so the rest of the current byte is padding.
            // Fewer than 8 bits are ever held, so they're all from that byte.
            void SkipToByte() noexcept
            {
                m_buffer = 0;
                m_bitCount = 0;
            }

            bool Bytes(_In_ size_t count, _Out_ uint8_t const*& start) noexcept
            {
                if (count > m_size - m_position)
                {
                    start = nullptr;
                    return false;
                }

                start = m_data + m_position;
                m_position += count;

                return true;
            }

        private:
            uint8_t const* m_data{ nullptr };
            size_t m_size{ 0 };
            size_t m_position{ 0 };
            uint32_t m_buffer{ 0 };
            int m_bitCount{ 0 };
        };

        // A canonical prefix code (RFC 1951 section 3.2.2), kept as how many codes have each
        // length and the symbols in code order, which is all decoding needs.
        struct PrefixCode
        {
            std::array<uint16_t, MaximumCodeBits + 1> CountOfLength{};
            std::array<uint16_t, LiteralLengthCount> SymbolsInCodeOrder{};
        };

        // False for lengths that ask for more codes than the bits allow. A code with room to spare
        // is accepted, and decoding fails only if one of its missing codes actually turns up.
        bool BuildPrefixCode(_In_reads_(count) uint8_t const* lengths, _In_ int count, _Out_ PrefixCode& code) noexcept
        {
            code = PrefixCode{};

            if (count > LiteralLengthCount)
            {
                return false;
            }

            for (int symbol = 0; symbol < count; ++symbol)
            {
                if (lengths[symbol] > MaximumCodeBits)
                {
                    return false;
                }

                code.CountOfLength[lengths[symbol]]++;
            }

            code.CountOfLength[0] = 0;

            int available{ 1 };

            for (int length = 1; length <= MaximumCodeBits; ++length)
            {
                available = (available << 1) - code.CountOfLength[length];

                if (available < 0)
                {
                    return false;
                }
            }

            std::array<uint16_t, MaximumCodeBits + 1> next{};

            for (int length = 1; length < MaximumCodeBits; ++length)
            {
                next[length + 1] = static_cast<uint16_t>(next[length] + code.CountOfLength[length]);
            }

            for (int symbol = 0; symbol < count; ++symbol)
            {
                if (lengths[symbol] != 0)
                {
                    code.SymbolsInCodeOrder[next[lengths[symbol]]++] = static_cast<uint16_t>(symbol);
                }
            }

            return true;
        }

        // The next symbol, or -1 when the bits don't make a code or the data runs out. The codes
        // of one length are consecutive numbers, so a code is found by counting, one bit at a time.
        int DecodeSymbol(_Inout_ BitReader& reader, _In_ PrefixCode const& code) noexcept
        {
            int value{ 0 };
            int first{ 0 };
            int index{ 0 };

            for (int length = 1; length <= MaximumCodeBits; ++length)
            {
                uint32_t bit{ 0 };

                if (!reader.Bits(1, bit))
                {
                    return -1;
                }

                value |= static_cast<int>(bit);

                int const count{ code.CountOfLength[length] };

                if (value - first < count)
                {
                    return code.SymbolsInCodeOrder[static_cast<size_t>(index + value - first)];
                }

                index += count;
                first = (first + count) << 1;
                value <<= 1;
            }

            return -1;
        }

        struct FixedPrefixCodes
        {
            PrefixCode Literals{};
            PrefixCode Distances{};
        };

        FixedPrefixCodes const& FixedCodes() noexcept
        {
            static FixedPrefixCodes const codes = []() noexcept
                {
                    FixedPrefixCodes built{};

                    auto const literalLengths = FixedLiteralLengths();

                    std::array<uint8_t, DistanceCount> distanceLengths{};
                    distanceLengths.fill(FixedDistanceLength);

                    BuildPrefixCode(literalLengths.data(), LiteralLengthCount, built.Literals);
                    BuildPrefixCode(distanceLengths.data(), DistanceCount, built.Distances);

                    return built;
                }();

            return codes;
        }

        bool InflateStored(_Inout_ BitReader& reader, _Inout_ std::vector<uint8_t>& output, _Inout_ size_t& written) noexcept
        {
            reader.SkipToByte();

            uint8_t const* header{ nullptr };

            if (!reader.Bytes(4, header))
            {
                return false;
            }

            auto const length = static_cast<size_t>(header[0] | (header[1] << 8));
            auto const check = static_cast<size_t>(header[2] | (header[3] << 8));

            if ((length ^ 0xFFFFu) != check || length > output.size() - written)
            {
                return false;
            }

            uint8_t const* bytes{ nullptr };

            if (!reader.Bytes(length, bytes))
            {
                return false;
            }

            if (length > 0)
            {
                std::memcpy(output.data() + written, bytes, length);
                written += length;
            }

            return true;
        }

        bool InflateCodedBlock(
            _Inout_ BitReader& reader,
            _In_ PrefixCode const& literals,
            _In_ PrefixCode const& distances,
            _Inout_ std::vector<uint8_t>& output,
            _Inout_ size_t& written) noexcept
        {
            auto const expected = output.size();
            uint8_t* const out = output.data();

            for (;;)
            {
                auto const symbol = DecodeSymbol(reader, literals);

                if (symbol < 0)
                {
                    return false;
                }

                if (symbol < EndOfBlock)
                {
                    if (written == expected)
                    {
                        return false;
                    }

                    out[written++] = static_cast<uint8_t>(symbol);
                    continue;
                }

                if (symbol == EndOfBlock)
                {
                    return true;
                }

                auto const lengthCode = static_cast<size_t>(symbol - FirstLengthSymbol);

                if (lengthCode >= LengthBase.size())
                {
                    return false;
                }

                uint32_t extra{ 0 };

                if (!reader.Bits(LengthExtraBits[lengthCode], extra))
                {
                    return false;
                }

                size_t const length{ LengthBase[lengthCode] + extra };

                auto const distanceSymbol = DecodeSymbol(reader, distances);

                if (distanceSymbol < 0 || static_cast<size_t>(distanceSymbol) >= DistanceBase.size())
                {
                    return false;
                }

                auto const distanceCode = static_cast<size_t>(distanceSymbol);

                if (!reader.Bits(DistanceExtraBits[distanceCode], extra))
                {
                    return false;
                }

                size_t const distance{ DistanceBase[distanceCode] + extra };

                if (distance > written || length > expected - written)
                {
                    return false;
                }

                // The copy can overlap what it writes, which is how deflate repeats a short run.
                auto from = written - distance;

                for (size_t i = 0; i < length; ++i)
                {
                    out[written++] = out[from++];
                }
            }
        }

        bool ReadDynamicCodes(_Inout_ BitReader& reader, _Out_ PrefixCode& literals, _Out_ PrefixCode& distances) noexcept
        {
            literals = {};
            distances = {};

            uint32_t literalCount{ 0 };
            uint32_t distanceCount{ 0 };
            uint32_t codeLengthCount{ 0 };

            if (!reader.Bits(5, literalCount) || !reader.Bits(5, distanceCount) || !reader.Bits(4, codeLengthCount))
            {
                return false;
            }

            literalCount += 257;
            distanceCount += 1;
            codeLengthCount += 4;

            if (literalCount > UsableLiteralLengthCount || distanceCount > DistanceCount)
            {
                return false;
            }

            std::array<uint8_t, CodeLengthCount> codeLengthLengths{};

            for (uint32_t i = 0; i < codeLengthCount; ++i)
            {
                uint32_t length{ 0 };

                if (!reader.Bits(3, length))
                {
                    return false;
                }

                codeLengthLengths[CodeLengthOrder[i]] = static_cast<uint8_t>(length);
            }

            PrefixCode codeLengthCode{};

            if (!BuildPrefixCode(codeLengthLengths.data(), CodeLengthCount, codeLengthCode))
            {
                return false;
            }

            // A repeat can run on from the literal lengths into the distance lengths.
            std::array<uint8_t, UsableLiteralLengthCount + DistanceCount> lengths{};

            uint32_t const total{ literalCount + distanceCount };
            uint32_t index{ 0 };

            while (index < total)
            {
                auto const symbol = DecodeSymbol(reader, codeLengthCode);

                if (symbol < 0)
                {
                    return false;
                }

                if (symbol < 16)
                {
                    lengths[index++] = static_cast<uint8_t>(symbol);
                    continue;
                }

                uint8_t repeated{ 0 };
                uint32_t repeat{ 0 };

                if (symbol == 16)
                {
                    if (index == 0 || !reader.Bits(2, repeat))
                    {
                        return false;
                    }

                    repeated = lengths[index - 1];
                    repeat += 3;
                }
                else if (symbol == 17)
                {
                    if (!reader.Bits(3, repeat))
                    {
                        return false;
                    }

                    repeat += 3;
                }
                else
                {
                    if (!reader.Bits(7, repeat))
                    {
                        return false;
                    }

                    repeat += 11;
                }

                if (repeat > total - index)
                {
                    return false;
                }

                for (; repeat > 0; --repeat)
                {
                    lengths[index++] = repeated;
                }
            }

            // Without an end of block code, the block could never end.
            if (lengths[EndOfBlock] == 0)
            {
                return false;
            }

            return BuildPrefixCode(lengths.data(), static_cast<int>(literalCount), literals) &&
                BuildPrefixCode(lengths.data() + literalCount, static_cast<int>(distanceCount), distances);
        }

        bool Fail(_Inout_ std::vector<uint8_t>& output) noexcept
        {
            std::vector<uint8_t>{}.swap(output);
            return false;
        }

        // ------------------------------------------------------------------------ encoding

        uint32_t HashOf(_In_reads_(3) uint8_t const* at) noexcept
        {
            auto const value =
                static_cast<uint32_t>(at[0]) |
                (static_cast<uint32_t>(at[1]) << 8) |
                (static_cast<uint32_t>(at[2]) << 16);

            return (value * 2654435761u) >> (32 - HashBits);
        }

        // The index into LengthBase for every match length from 3 to 258.
        std::array<uint8_t, MaximumMatch + 1> const& LengthCodes() noexcept
        {
            static auto const codes = []() noexcept
                {
                    std::array<uint8_t, MaximumMatch + 1> built{};

                    for (size_t code = 0; code < LengthBase.size(); ++code)
                    {
                        size_t const first{ LengthBase[code] };
                        size_t const last{ code + 1 < LengthBase.size() ? size_t{ LengthBase[code + 1] } - 1 : MaximumMatch };

                        for (auto length = first; length <= last; ++length)
                        {
                            built[length] = static_cast<uint8_t>(code);
                        }
                    }

                    return built;
                }();

            return codes;
        }

        size_t DistanceCodeOf(_In_ size_t distance) noexcept
        {
            size_t code{ DistanceBase.size() - 1 };

            while (code > 0 && DistanceBase[code] > distance)
            {
                --code;
            }

            return code;
        }

        // Codes are defined from their first bit, but the stream fills each byte from its low end.
        uint32_t Reverse(_In_ uint32_t value, _In_ int length) noexcept
        {
            uint32_t reversed{ 0 };

            for (int bit = 0; bit < length; ++bit)
            {
                reversed = (reversed << 1) | ((value >> bit) & 1u);
            }

            return reversed;
        }

        // The codes RFC 1951 section 3.2.2 gives for these lengths, ready to write.
        void AssignCodes(_In_reads_(count) uint8_t const* lengths, _In_ int count, _Out_writes_(count) uint32_t* codes) noexcept
        {
            std::array<uint32_t, MaximumCodeBits + 1> countOfLength{};

            for (int symbol = 0; symbol < count; ++symbol)
            {
                countOfLength[lengths[symbol]]++;
            }

            countOfLength[0] = 0;

            std::array<uint32_t, MaximumCodeBits + 1> next{};
            uint32_t code{ 0 };

            for (int length = 1; length <= MaximumCodeBits; ++length)
            {
                code = (code + countOfLength[length - 1]) << 1;
                next[length] = code;
            }

            for (int symbol = 0; symbol < count; ++symbol)
            {
                auto const length = lengths[symbol];

                codes[symbol] = length == 0 ? 0 : Reverse(next[length]++, length);
            }
        }

        // The lengths of an optimal prefix code for these frequencies, none longer than
        // maximumLength. A symbol that's never used gets no code.
        void BuildCodeLengths(
            _In_reads_(count) uint32_t const* frequencies,
            _In_ int count,
            _In_ int maximumLength,
            _Out_writes_(count) uint8_t* lengths)
        {
            std::fill(lengths, lengths + count, uint8_t{ 0 });

            std::vector<std::pair<uint32_t, int>> used{};

            for (int symbol = 0; symbol < count; ++symbol)
            {
                if (frequencies[symbol] != 0)
                {
                    used.emplace_back(frequencies[symbol], symbol);
                }
            }

            // Every code gets at least two symbols, as other encoders do, so no decoder meets a
            // code with a single member.
            for (int symbol = 0; used.size() < 2 && symbol < count; ++symbol)
            {
                if (frequencies[symbol] == 0)
                {
                    used.emplace_back(0u, symbol);
                }
            }

            std::sort(used.begin(), used.end());

            auto const leaves = used.size();
            auto const nodes = 2 * leaves - 1;

            // Huffman's method with two queues: the leaves in order of weight, and the merged
            // nodes, which come out in order of weight too.
            std::vector<uint64_t> weight(nodes, 0);
            std::vector<size_t> parent(nodes, 0);

            for (size_t leaf = 0; leaf < leaves; ++leaf)
            {
                weight[leaf] = used[leaf].first;
            }

            size_t nextLeaf{ 0 };
            size_t nextMerged{ leaves };

            for (auto made = leaves; made < nodes; ++made)
            {
                auto const takeLightest = [&]()
                    {
                        if (nextLeaf < leaves && (nextMerged >= made || weight[nextLeaf] <= weight[nextMerged]))
                        {
                            return nextLeaf++;
                        }

                        return nextMerged++;
                    };

                auto const first = takeLightest();
                auto const second = takeLightest();

                weight[made] = weight[first] + weight[second];
                parent[first] = made;
                parent[second] = made;
            }

            std::vector<int> depth(nodes, 0);

            for (auto node = nodes - 1; node-- > 0;)
            {
                depth[node] = depth[parent[node]] + 1;
            }

            int longest{ 0 };

            for (size_t leaf = 0; leaf < leaves; ++leaf)
            {
                longest = std::max(longest, depth[leaf]);
            }

            if (longest <= maximumLength)
            {
                for (size_t leaf = 0; leaf < leaves; ++leaf)
                {
                    lengths[used[leaf].second] = static_cast<uint8_t>(depth[leaf]);
                }

                return;
            }

            // Too long. Keep the number of codes, cap their lengths, then lengthen shorter codes
            // until the set fits: the sum Kraft's inequality checks has to come back to 1.
            std::vector<uint64_t> countOfLength(static_cast<size_t>(maximumLength) + 1, 0);

            for (size_t leaf = 0; leaf < leaves; ++leaf)
            {
                countOfLength[static_cast<size_t>(std::min(depth[leaf], maximumLength))]++;
            }

            uint64_t total{ 0 };

            for (int length = 1; length <= maximumLength; ++length)
            {
                total += countOfLength[static_cast<size_t>(length)] << (maximumLength - length);
            }

            uint64_t const full{ uint64_t{ 1 } << maximumLength };

            while (total > full)
            {
                // One code leaves the longest length, and a shorter code splits in two one level
                // down, so the count stays the same and the sum drops by one.
                countOfLength[static_cast<size_t>(maximumLength)]--;

                for (auto length = maximumLength - 1; length > 0; --length)
                {
                    if (countOfLength[static_cast<size_t>(length)] > 0)
                    {
                        countOfLength[static_cast<size_t>(length)]--;
                        countOfLength[static_cast<size_t>(length) + 1] += 2;
                        break;
                    }
                }

                total--;
            }

            // The rarest symbols get the longest codes.
            size_t leaf{ 0 };

            for (auto length = maximumLength; length > 0; --length)
            {
                for (uint64_t i = 0; i < countOfLength[static_cast<size_t>(length)]; ++i)
                {
                    lengths[used[leaf++].second] = static_cast<uint8_t>(length);
                }
            }
        }

        // A code length, or a repeat code (16, 17 or 18) with the value of its extra bits.
        struct LengthRun
        {
            uint8_t Symbol{ 0 };
            uint8_t Extra{ 0 };
        };

        // RFC 1951 section 3.2.7
        void EncodeLengthRuns(_In_ std::vector<uint8_t> const& lengths, _Inout_ std::vector<LengthRun>& runs)
        {
            size_t index{ 0 };

            while (index < lengths.size())
            {
                auto const value = lengths[index];
                size_t run{ 1 };

                while (index + run < lengths.size() && lengths[index + run] == value)
                {
                    ++run;
                }

                index += run;

                if (value == 0)
                {
                    while (run >= 11)
                    {
                        auto const take = std::min<size_t>(run, 138);
                        runs.push_back(LengthRun{ 18, static_cast<uint8_t>(take - 11) });
                        run -= take;
                    }

                    if (run >= 3)
                    {
                        runs.push_back(LengthRun{ 17, static_cast<uint8_t>(run - 3) });
                        run = 0;
                    }
                }
                else
                {
                    runs.push_back(LengthRun{ value, 0 });
                    --run;

                    while (run >= 3)
                    {
                        auto const take = std::min<size_t>(run, 6);
                        runs.push_back(LengthRun{ 16, static_cast<uint8_t>(take - 3) });
                        run -= take;
                    }
                }

                for (; run > 0; --run)
                {
                    runs.push_back(LengthRun{ value, 0 });
                }
            }
        }

        int RunExtraBits(_In_ uint8_t symbol) noexcept
        {
            return symbol == 16 ? 2 : symbol == 17 ? 3 : symbol == 18 ? 7 : 0;
        }
    }

    _Use_decl_annotations_
    bool Inflate(uint8_t const* data, size_t size, size_t expectedSize, std::vector<uint8_t>& output) noexcept
    {
        output.clear();

        try
        {
            output.resize(expectedSize);
        }
        catch (...)
        {
            return Fail(output);
        }

        BitReader reader{ data, size };

        size_t written{ 0 };
        bool last{ false };

        while (!last)
        {
            uint32_t header{ 0 };

            if (!reader.Bits(3, header))
            {
                return Fail(output);
            }

            last = (header & 1u) != 0;

            switch (header >> 1)
            {
            case 0:
                if (!InflateStored(reader, output, written))
                {
                    return Fail(output);
                }
                break;

            case 1:
                if (!InflateCodedBlock(reader, FixedCodes().Literals, FixedCodes().Distances, output, written))
                {
                    return Fail(output);
                }
                break;

            case 2:
            {
                PrefixCode literals{};
                PrefixCode distances{};

                if (!ReadDynamicCodes(reader, literals, distances) ||
                    !InflateCodedBlock(reader, literals, distances, output, written))
                {
                    return Fail(output);
                }
                break;
            }

            default:
                return Fail(output);
            }
        }

        if (written != expectedSize)
        {
            return Fail(output);
        }

        return true;
    }

    _Use_decl_annotations_
    bool DeflateEncoder::Write(uint8_t const* data, size_t size, bool last, std::vector<uint8_t>& output) noexcept
    {
        if (m_finished || (data == nullptr && size != 0))
        {
            return false;
        }

        if (size == 0 && !last)
        {
            return true;
        }

        try
        {
            do
            {
                auto const piece = std::min(size, PieceSize);

                // The last 32 KB of what came before stays, so matches can reach back into it.
                auto const keep = std::min(m_buffer.size(), WindowSize);

                m_buffer.erase(m_buffer.begin(), m_buffer.end() - static_cast<std::ptrdiff_t>(keep));

                if (piece > 0)
                {
                    m_buffer.insert(m_buffer.end(), data, data + piece);
                    data += piece;
                    size -= piece;
                }

                CompressPiece(keep, last && size == 0, output);
            } while (size > 0);

            if (last)
            {
                m_finished = true;
            }

            return true;
        }
        catch (...)
        {
            m_finished = true;
            return false;
        }
    }

    _Use_decl_annotations_
    void DeflateEncoder::CompressPiece(size_t start, bool last, std::vector<uint8_t>& output)
    {
        auto const end = m_buffer.size();

        m_head.assign(HashSize, -1);
        m_previous.resize(end);

        for (size_t position = 0; position < start; ++position)
        {
            Remember(position);
        }

        m_symbols.clear();

        size_t blockStart{ start };
        size_t position{ start };

        while (position < end)
        {
            size_t length{ 0 };
            size_t distance{ 0 };

            FindMatch(position, end, length, distance);

            if (length >= MinimumMatch)
            {
                m_symbols.push_back(Symbol{ static_cast<uint16_t>(length), static_cast<uint16_t>(distance) });

                for (size_t i = 0; i < length; ++i)
                {
                    Remember(position + i);
                }

                position += length;
            }
            else
            {
                m_symbols.push_back(Symbol{ m_buffer[position], 0 });

                Remember(position);
                ++position;
            }

            if (m_symbols.size() >= SymbolsPerBlock && position < end)
            {
                FlushBlock(blockStart, position, false, output);
                blockStart = position;
            }
        }

        if (!m_symbols.empty())
        {
            FlushBlock(blockStart, end, last, output);
        }
        else if (last)
        {
            // Nothing left, so the final block is an empty one: fixed codes, then end of block,
            // which is seven zero bits in the fixed code.
            PutBits(1, 1, output);
            PutBits(1, 2, output);
            PutBits(0, 7, output);
        }

        if (last)
        {
            PutToByte(output);
        }
    }

    _Use_decl_annotations_
    void DeflateEncoder::Remember(size_t position) noexcept
    {
        if (position + MinimumMatch > m_buffer.size())
        {
            return;
        }

        auto const hash = HashOf(m_buffer.data() + position);

        m_previous[position] = m_head[hash];
        m_head[hash] = static_cast<int32_t>(position);
    }

    _Use_decl_annotations_
    void DeflateEncoder::FindMatch(size_t position, size_t end, size_t& length, size_t& distance) const noexcept
    {
        length = 0;
        distance = 0;

        if (position + MinimumMatch > end)
        {
            return;
        }

        auto const limit = std::min(MaximumMatch, end - position);
        auto const* const here = m_buffer.data() + position;

        auto candidate = m_head[HashOf(here)];
        int chain{ MaximumChain };

        while (candidate >= 0 && chain-- > 0)
        {
            auto const back = position - static_cast<size_t>(candidate);

            if (back > WindowSize)
            {
                break;
            }

            auto const* const there = m_buffer.data() + candidate;

            if (there[length] == here[length] && there[0] == here[0])
            {
                size_t matched{ 0 };

                while (matched < limit && there[matched] == here[matched])
                {
                    ++matched;
                }

                if (matched > length)
                {
                    length = matched;
                    distance = back;

                    if (matched >= GoodEnoughMatch || matched == limit)
                    {
                        break;
                    }
                }
            }

            candidate = m_previous[static_cast<size_t>(candidate)];
        }

        if (length < MinimumMatch)
        {
            length = 0;
            distance = 0;
        }
    }

    _Use_decl_annotations_
    void DeflateEncoder::FlushBlock(size_t blockStart, size_t blockEnd, bool final, std::vector<uint8_t>& output)
    {
        auto const& lengthCodes = LengthCodes();

        std::array<uint32_t, LiteralLengthCount> literalFrequency{};
        std::array<uint32_t, DistanceCount> distanceFrequency{};

        uint64_t extraBits{ 0 };

        for (auto const& symbol : m_symbols)
        {
            if (symbol.Distance == 0)
            {
                literalFrequency[symbol.LiteralOrLength]++;
            }
            else
            {
                auto const lengthCode = lengthCodes[symbol.LiteralOrLength];
                auto const distanceCode = DistanceCodeOf(symbol.Distance);

                literalFrequency[FirstLengthSymbol + lengthCode]++;
                distanceFrequency[distanceCode]++;

                extraBits += LengthExtraBits[lengthCode] + DistanceExtraBits[distanceCode];
            }
        }

        literalFrequency[EndOfBlock] = 1;

        // ---- the dynamic code this block would send

        std::array<uint8_t, LiteralLengthCount> literalLengths{};
        std::array<uint8_t, DistanceCount> distanceLengths{};

        BuildCodeLengths(literalFrequency.data(), UsableLiteralLengthCount, MaximumCodeBits, literalLengths.data());
        BuildCodeLengths(distanceFrequency.data(), DistanceCount, MaximumCodeBits, distanceLengths.data());

        int literalCount{ UsableLiteralLengthCount };

        while (literalCount > FirstLengthSymbol && literalLengths[static_cast<size_t>(literalCount) - 1] == 0)
        {
            --literalCount;
        }

        int distanceCount{ DistanceCount };

        while (distanceCount > 1 && distanceLengths[static_cast<size_t>(distanceCount) - 1] == 0)
        {
            --distanceCount;
        }

        std::vector<uint8_t> sentLengths{};
        sentLengths.insert(sentLengths.end(), literalLengths.begin(), literalLengths.begin() + literalCount);
        sentLengths.insert(sentLengths.end(), distanceLengths.begin(), distanceLengths.begin() + distanceCount);

        std::vector<LengthRun> runs{};
        EncodeLengthRuns(sentLengths, runs);

        std::array<uint32_t, CodeLengthCount> runFrequency{};

        for (auto const& run : runs)
        {
            runFrequency[run.Symbol]++;
        }

        std::array<uint8_t, CodeLengthCount> runLengths{};
        BuildCodeLengths(runFrequency.data(), CodeLengthCount, MaximumCodeLengthBits, runLengths.data());

        int runLengthCount{ CodeLengthCount };

        while (runLengthCount > 4 && runLengths[CodeLengthOrder[static_cast<size_t>(runLengthCount) - 1]] == 0)
        {
            --runLengthCount;
        }

        auto const fixedLiteralLengths = FixedLiteralLengths();

        uint64_t dynamicBits{ 3 + 5 + 5 + 4 + 3 * static_cast<uint64_t>(runLengthCount) + extraBits };
        uint64_t fixedBits{ 3 + extraBits };

        for (auto const& run : runs)
        {
            dynamicBits += runLengths[run.Symbol] + RunExtraBits(run.Symbol);
        }

        for (int symbol = 0; symbol < LiteralLengthCount; ++symbol)
        {
            dynamicBits += static_cast<uint64_t>(literalFrequency[symbol]) * literalLengths[symbol];
            fixedBits += static_cast<uint64_t>(literalFrequency[symbol]) * fixedLiteralLengths[symbol];
        }

        for (int symbol = 0; symbol < DistanceCount; ++symbol)
        {
            dynamicBits += static_cast<uint64_t>(distanceFrequency[symbol]) * distanceLengths[symbol];
            fixedBits += static_cast<uint64_t>(distanceFrequency[symbol]) * FixedDistanceLength;
        }

        // Each stored block costs its header, up to 7 bits to reach a byte boundary, and the length twice.
        auto const rawBytes = static_cast<uint64_t>(blockEnd - blockStart);
        auto const storedBlocks = std::max<uint64_t>(1, (rawBytes + MaximumStoredBlock - 1) / MaximumStoredBlock);
        auto const storedBits = storedBlocks * (3 + 7 + 32) + rawBytes * 8;

        if (storedBits < dynamicBits && storedBits < fixedBits)
        {
            auto at = blockStart;

            do
            {
                auto const chunk = std::min(blockEnd - at, MaximumStoredBlock);
                bool const lastChunk{ at + chunk == blockEnd };

                PutBits(final && lastChunk ? 1u : 0u, 1, output);
                PutBits(0, 2, output);
                PutToByte(output);
                PutBits(static_cast<uint32_t>(chunk), 16, output);
                PutBits(static_cast<uint32_t>(chunk ^ 0xFFFFu), 16, output);

                output.insert(output.end(), m_buffer.begin() + static_cast<std::ptrdiff_t>(at), m_buffer.begin() + static_cast<std::ptrdiff_t>(at + chunk));

                at += chunk;
            } while (at < blockEnd);

            m_symbols.clear();
            return;
        }

        bool const dynamic{ dynamicBits < fixedBits };

        std::array<uint32_t, LiteralLengthCount> literalCodes{};
        std::array<uint32_t, DistanceCount> distanceCodes{};

        if (!dynamic)
        {
            literalLengths = fixedLiteralLengths;
            distanceLengths.fill(FixedDistanceLength);
        }

        AssignCodes(literalLengths.data(), LiteralLengthCount, literalCodes.data());
        AssignCodes(distanceLengths.data(), DistanceCount, distanceCodes.data());

        PutBits(final ? 1u : 0u, 1, output);
        PutBits(dynamic ? 2u : 1u, 2, output);

        if (dynamic)
        {
            PutBits(static_cast<uint32_t>(literalCount - FirstLengthSymbol), 5, output);
            PutBits(static_cast<uint32_t>(distanceCount - 1), 5, output);
            PutBits(static_cast<uint32_t>(runLengthCount - 4), 4, output);

            for (int i = 0; i < runLengthCount; ++i)
            {
                PutBits(runLengths[CodeLengthOrder[static_cast<size_t>(i)]], 3, output);
            }

            std::array<uint32_t, CodeLengthCount> runCodes{};
            AssignCodes(runLengths.data(), CodeLengthCount, runCodes.data());

            for (auto const& run : runs)
            {
                PutBits(runCodes[run.Symbol], runLengths[run.Symbol], output);
                PutBits(run.Extra, RunExtraBits(run.Symbol), output);
            }
        }

        for (auto const& symbol : m_symbols)
        {
            if (symbol.Distance == 0)
            {
                PutBits(literalCodes[symbol.LiteralOrLength], literalLengths[symbol.LiteralOrLength], output);
                continue;
            }

            auto const lengthCode = lengthCodes[symbol.LiteralOrLength];
            auto const lengthSymbol = static_cast<size_t>(FirstLengthSymbol) + lengthCode;

            PutBits(literalCodes[lengthSymbol], literalLengths[lengthSymbol], output);
            PutBits(static_cast<uint32_t>(symbol.LiteralOrLength - LengthBase[lengthCode]), LengthExtraBits[lengthCode], output);

            auto const distanceCode = DistanceCodeOf(symbol.Distance);

            PutBits(distanceCodes[distanceCode], distanceLengths[distanceCode], output);
            PutBits(static_cast<uint32_t>(symbol.Distance - DistanceBase[distanceCode]), DistanceExtraBits[distanceCode], output);
        }

        PutBits(literalCodes[EndOfBlock], literalLengths[EndOfBlock], output);

        m_symbols.clear();
    }

    _Use_decl_annotations_
    void DeflateEncoder::PutBits(uint32_t value, int count, std::vector<uint8_t>& output)
    {
        m_bitBuffer |= static_cast<uint64_t>(value) << m_bitCount;
        m_bitCount += count;

        while (m_bitCount >= 8)
        {
            output.push_back(static_cast<uint8_t>(m_bitBuffer & 0xFF));
            m_bitBuffer >>= 8;
            m_bitCount -= 8;
        }
    }

    _Use_decl_annotations_
    void DeflateEncoder::PutToByte(std::vector<uint8_t>& output)
    {
        if (m_bitCount > 0)
        {
            output.push_back(static_cast<uint8_t>(m_bitBuffer & 0xFF));
        }

        m_bitBuffer = 0;
        m_bitCount = 0;
    }

    _Use_decl_annotations_
    bool Deflate(uint8_t const* data, size_t size, std::vector<uint8_t>& output) noexcept
    {
        output.clear();

        DeflateEncoder encoder{};

        return encoder.Write(data, size, true, output);
    }
}
