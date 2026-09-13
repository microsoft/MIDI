// Bounds-checked RIFF chunk walking. Every length in a RIFF file is attacker-controlled,
// so nothing here advances a cursor without first proving the bytes exist.

#pragma once

#include <sal.h>

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <span>
#include <type_traits>

namespace MidiSynth
{
    // 'RIFF' in file order becomes 0x46464952 when read as a little-endian uint32.
    constexpr uint32_t MakeFourCC(const char (&code)[5]) noexcept
    {
        return static_cast<uint32_t>(static_cast<uint8_t>(code[0])) |
               (static_cast<uint32_t>(static_cast<uint8_t>(code[1])) << 8) |
               (static_cast<uint32_t>(static_cast<uint8_t>(code[2])) << 16) |
               (static_cast<uint32_t>(static_cast<uint8_t>(code[3])) << 24);
    }

    constexpr uint32_t FourCC_RIFF = MakeFourCC("RIFF");
    constexpr uint32_t FourCC_LIST = MakeFourCC("LIST");

    // Sequential little-endian reader over a fixed byte range.
    class ByteReader
    {
    public:
        explicit ByteReader(std::span<const std::byte> data) noexcept
            : m_data(data)
        {
        }

        size_t Remaining() const noexcept { return m_data.size() - m_position; }
        size_t Position() const noexcept { return m_position; }
        bool IsEmpty() const noexcept { return Remaining() == 0; }

        bool TryReadUInt8(_Out_ uint8_t& value) noexcept { return TryReadPod(value); }
        bool TryReadUInt16(_Out_ uint16_t& value) noexcept { return TryReadPod(value); }
        bool TryReadInt16(_Out_ int16_t& value) noexcept { return TryReadPod(value); }
        bool TryReadUInt32(_Out_ uint32_t& value) noexcept { return TryReadPod(value); }
        bool TryReadInt32(_Out_ int32_t& value) noexcept { return TryReadPod(value); }

        bool TryReadBytes(_In_ size_t count, _Out_ std::span<const std::byte>& value) noexcept
        {
            if (count > Remaining())
            {
                return false;
            }

            value = m_data.subspan(m_position, count);
            m_position += count;
            return true;
        }

        bool TrySkip(_In_ size_t count) noexcept
        {
            if (count > Remaining())
            {
                return false;
            }

            m_position += count;
            return true;
        }

    private:
        template <typename T>
        bool TryReadPod(_Out_ T& value) noexcept
        {
            static_assert(std::is_trivially_copyable_v<T>);

            if (sizeof(T) > Remaining())
            {
                value = {};
                return false;
            }

            // memcpy rather than a reinterpret_cast: the source is not guaranteed to be aligned.
            std::memcpy(&value, m_data.data() + m_position, sizeof(T));
            m_position += sizeof(T);
            return true;
        }

        std::span<const std::byte> m_data;
        size_t m_position{ 0 };
    };

    struct RiffChunk
    {
        uint32_t Id{ 0 };

        // Only meaningful when Id is 'RIFF' or 'LIST'; the form type is not part of Payload.
        uint32_t ListType{ 0 };

        std::span<const std::byte> Payload;

        // Offset of this chunk's header from the start of the range being walked. Needed because
        // DLS pool cues address waves by byte offset rather than by index.
        size_t HeaderOffset{ 0 };

        bool IsList() const noexcept { return Id == FourCC_LIST || Id == FourCC_RIFF; }
    };

    // Walks a sequence of chunks. Rejects any chunk whose declared size does not fit.
    class RiffChunkReader
    {
    public:
        explicit RiffChunkReader(std::span<const std::byte> data) noexcept
            : m_reader(data)
        {
        }

        bool TryNext(_Out_ RiffChunk& chunk) noexcept
        {
            chunk = {};

            const size_t headerOffset = m_reader.Position();

            uint32_t id{};
            uint32_t declaredSize{};

            if (!m_reader.TryReadUInt32(id) || !m_reader.TryReadUInt32(declaredSize))
            {
                return false;
            }

            std::span<const std::byte> payload;

            if (!m_reader.TryReadBytes(declaredSize, payload))
            {
                return false;
            }

            chunk.Id = id;
            chunk.HeaderOffset = headerOffset;
            chunk.Payload = payload;

            if (chunk.IsList())
            {
                ByteReader formReader(payload);

                if (!formReader.TryReadUInt32(chunk.ListType))
                {
                    return false;
                }

                chunk.Payload = payload.subspan(sizeof(uint32_t));
            }

            // Chunks are word aligned. A trailing pad byte on the final chunk may be absent,
            // so a failed skip here is the end of the range rather than an error.
            if ((declaredSize & 1u) != 0)
            {
                (void)m_reader.TrySkip(1);
            }

            return true;
        }

    private:
        ByteReader m_reader;
    };
}
