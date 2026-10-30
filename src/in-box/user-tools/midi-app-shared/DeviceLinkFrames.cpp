// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#include "DeviceLinkFrames.h"

#include <windows.h>

#include <cstring>

namespace midiapp
{
    _Use_decl_annotations_
    std::vector<uint8_t> EncodeDeviceLinkFrame(uint16_t type, std::span<uint8_t const> payload) noexcept
    {
        try
        {
            if (payload.size() > MaximumDeviceLinkFrameBytes)
            {
                return {};
            }

            std::vector<uint8_t> frame(DeviceLinkFrameHeaderBytes + payload.size());

            auto const length = static_cast<uint32_t>(payload.size());

            frame[0] = static_cast<uint8_t>(length);
            frame[1] = static_cast<uint8_t>(length >> 8);
            frame[2] = static_cast<uint8_t>(length >> 16);
            frame[3] = static_cast<uint8_t>(length >> 24);
            frame[4] = static_cast<uint8_t>(type);
            frame[5] = static_cast<uint8_t>(type >> 8);

            if (!payload.empty())
            {
                std::memcpy(frame.data() + DeviceLinkFrameHeaderBytes, payload.data(), payload.size());
            }

            return frame;
        }
        catch (...)
        {
            return {};
        }
    }

    _Use_decl_annotations_
    DeviceLinkFrameStatus ReadDeviceLinkFrame(
        DeviceLinkReadExact const& readExact,
        DeviceLinkFrameCap const& capFor,
        DeviceLinkFrame& frame) noexcept
    {
        frame = DeviceLinkFrame{};

        try
        {
            uint8_t header[DeviceLinkFrameHeaderBytes]{};
            bool closedCleanly{ false };

            if (!readExact(header, sizeof(header), closedCleanly))
            {
                return closedCleanly ? DeviceLinkFrameStatus::Closed : DeviceLinkFrameStatus::Truncated;
            }

            auto const length = static_cast<uint32_t>(header[0]) |
                (static_cast<uint32_t>(header[1]) << 8) |
                (static_cast<uint32_t>(header[2]) << 16) |
                (static_cast<uint32_t>(header[3]) << 24);

            auto const type = static_cast<uint16_t>(header[4] | (header[5] << 8));

            auto const cap = capFor ? capFor(type) : 0;

            if (cap == 0)
            {
                return DeviceLinkFrameStatus::UnexpectedType;
            }

            // Checked before anything is allocated, so a length can't make this reserve memory.
            if (length > cap || length > MaximumDeviceLinkFrameBytes)
            {
                return DeviceLinkFrameStatus::TooLarge;
            }

            frame.Type = type;
            frame.Payload.resize(length);

            if (length > 0 && !readExact(frame.Payload.data(), length, closedCleanly))
            {
                frame = DeviceLinkFrame{};
                return DeviceLinkFrameStatus::Truncated;
            }

            return DeviceLinkFrameStatus::Read;
        }
        catch (...)
        {
            frame = DeviceLinkFrame{};
            return DeviceLinkFrameStatus::Truncated;
        }
    }

    // ------------------------------------------------------------------------ writer

    _Use_decl_annotations_
    void DeviceLinkWriter::U8(uint8_t value)
    {
        m_data.push_back(value);
    }

    _Use_decl_annotations_
    void DeviceLinkWriter::U16(uint16_t value)
    {
        U8(static_cast<uint8_t>(value));
        U8(static_cast<uint8_t>(value >> 8));
    }

    _Use_decl_annotations_
    void DeviceLinkWriter::U32(uint32_t value)
    {
        U16(static_cast<uint16_t>(value));
        U16(static_cast<uint16_t>(value >> 16));
    }

    _Use_decl_annotations_
    void DeviceLinkWriter::U64(uint64_t value)
    {
        U32(static_cast<uint32_t>(value));
        U32(static_cast<uint32_t>(value >> 32));
    }

    _Use_decl_annotations_
    void DeviceLinkWriter::Raw(std::span<uint8_t const> bytes)
    {
        m_data.insert(m_data.end(), bytes.begin(), bytes.end());
    }

    _Use_decl_annotations_
    void DeviceLinkWriter::Bytes(std::span<uint8_t const> bytes)
    {
        U32(static_cast<uint32_t>(bytes.size()));
        Raw(bytes);
    }

    _Use_decl_annotations_
    void DeviceLinkWriter::Text(std::wstring_view text)
    {
        auto const utf8 = DeviceLinkUtf8(text);
        Bytes(std::span<uint8_t const>{ reinterpret_cast<uint8_t const*>(utf8.data()), utf8.size() });
    }

    // ------------------------------------------------------------------------ reader

    _Use_decl_annotations_
    bool DeviceLinkReader::Take(size_t size, std::span<uint8_t const>& taken) noexcept
    {
        taken = {};

        if (m_failed || size > m_data.size() - m_offset)
        {
            m_failed = true;
            return false;
        }

        taken = m_data.subspan(m_offset, size);
        m_offset += size;

        return true;
    }

    uint8_t DeviceLinkReader::U8() noexcept
    {
        std::span<uint8_t const> taken{};
        return Take(1, taken) ? taken[0] : 0;
    }

    uint16_t DeviceLinkReader::U16() noexcept
    {
        std::span<uint8_t const> taken{};
        return Take(2, taken) ? static_cast<uint16_t>(taken[0] | (taken[1] << 8)) : 0;
    }

    uint32_t DeviceLinkReader::U32() noexcept
    {
        auto const low = U16();
        auto const high = U16();

        return m_failed ? 0 : (static_cast<uint32_t>(low) | (static_cast<uint32_t>(high) << 16));
    }

    uint64_t DeviceLinkReader::U64() noexcept
    {
        auto const low = U32();
        auto const high = U32();

        return m_failed ? 0 : (static_cast<uint64_t>(low) | (static_cast<uint64_t>(high) << 32));
    }

    _Use_decl_annotations_
    bool DeviceLinkReader::Raw(uint8_t* destination, size_t size) noexcept
    {
        std::span<uint8_t const> taken{};

        if (!Take(size, taken))
        {
            std::memset(destination, 0, size);
            return false;
        }

        if (size > 0)
        {
            std::memcpy(destination, taken.data(), size);
        }

        return true;
    }

    _Use_decl_annotations_
    std::vector<uint8_t> DeviceLinkReader::Bytes(size_t maximumBytes) noexcept
    {
        try
        {
            auto const length = U32();

            if (m_failed || length > maximumBytes)
            {
                m_failed = true;
                return {};
            }

            std::span<uint8_t const> taken{};

            if (!Take(length, taken))
            {
                return {};
            }

            return std::vector<uint8_t>{ taken.begin(), taken.end() };
        }
        catch (...)
        {
            m_failed = true;
            return {};
        }
    }

    _Use_decl_annotations_
    std::wstring DeviceLinkReader::Text(size_t maximumCharacters) noexcept
    {
        try
        {
            // UTF-8 takes at most 4 bytes for a character.
            auto const bytes = Bytes(maximumCharacters * 4);

            if (m_failed)
            {
                return {};
            }

            bool valid{ false };
            auto text = DeviceLinkFromUtf8(bytes, valid);

            if (!valid || text.size() > maximumCharacters)
            {
                m_failed = true;
                return {};
            }

            return text;
        }
        catch (...)
        {
            m_failed = true;
            return {};
        }
    }

    // ------------------------------------------------------------------------ text

    _Use_decl_annotations_
    std::string DeviceLinkUtf8(std::wstring_view text)
    {
        if (text.empty())
        {
            return {};
        }

        auto const needed = ::WideCharToMultiByte(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), nullptr, 0, nullptr, nullptr);

        if (needed <= 0)
        {
            return {};
        }

        std::string utf8(static_cast<size_t>(needed), '\0');
        ::WideCharToMultiByte(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), utf8.data(), needed, nullptr, nullptr);

        return utf8;
    }

    _Use_decl_annotations_
    std::wstring DeviceLinkFromUtf8(std::span<uint8_t const> utf8, bool& valid)
    {
        valid = true;

        if (utf8.empty())
        {
            return {};
        }

        auto const data = reinterpret_cast<char const*>(utf8.data());
        auto const length = static_cast<int>(utf8.size());

        auto const needed = ::MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, data, length, nullptr, 0);

        if (needed <= 0)
        {
            valid = false;
            return {};
        }

        std::wstring text(static_cast<size_t>(needed), L'\0');
        ::MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, data, length, text.data(), needed);

        return text;
    }
}
