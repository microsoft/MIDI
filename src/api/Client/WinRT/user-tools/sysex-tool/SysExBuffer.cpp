// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#include "pch.h"
#include "SysExBuffer.h"

namespace midisysextool
{
    // F8 clock, FA start, FB continue, FC stop, FE active sensing, FF reset
    bool IsAllowedRealTimeStatus(uint8_t value) noexcept
    {
        return value >= 0xF8;
    }

    SysExByteKind ClassifySysEx7Word(uint32_t word0) noexcept
    {
        switch ((word0 >> 20) & 0x0F)
        {
        case 0: return SysExByteKind::Complete;
        case 1: return SysExByteKind::Start;
        case 3: return SysExByteKind::End;
        default: return SysExByteKind::Data;
        }
    }

    void SysExBuffer::Reset(uint32_t initialBytes) noexcept
    {
        try
        {
            m_bytes.clear();
            m_words.clear();
            m_stats = {};

            m_bytes.reserve(initialBytes);

            // roughly four data bytes per 64 bit packet, so two words per four bytes
            m_words.reserve(initialBytes / 2);
        }
        catch (...)
        {
            LOG_CAUGHT_EXCEPTION();
        }
    }

    SysExByteKind SysExBuffer::AppendByte(uint8_t value) noexcept
    {
        try
        {
            if (value == 0xF0)
            {
                m_bytes.push_back(value);
                m_stats.TotalBytes++;
                m_stats.IsInsideMessage = true;
                return SysExByteKind::Start;
            }

            if (value == 0xF7)
            {
                m_bytes.push_back(value);
                m_stats.TotalBytes++;

                if (m_stats.IsInsideMessage)
                {
                    m_stats.CompleteMessages++;
                }

                m_stats.IsInsideMessage = false;
                return SysExByteKind::End;
            }

            if (IsAllowedRealTimeStatus(value))
            {
                // legal between data bytes, and not part of the dump
                m_stats.IgnoredRealTimeBytes++;
                return SysExByteKind::Data;
            }

            if ((value & 0x80) != 0)
            {
                // any other status byte terminates a SysEx stream, so the data is suspect
                m_bytes.push_back(value);
                m_stats.TotalBytes++;
                m_stats.DisallowedBytes++;
                return SysExByteKind::Disallowed;
            }

            m_bytes.push_back(value);
            m_stats.TotalBytes++;
            return SysExByteKind::Data;
        }
        catch (...)
        {
            LOG_CAUGHT_EXCEPTION();
            return SysExByteKind::Data;
        }
    }

    _Use_decl_annotations_
    void SysExBuffer::AppendBytes(uint8_t const* data, size_t count) noexcept
    {
        try
        {
            if (data == nullptr)
            {
                return;
            }

            for (size_t i = 0; i < count; i++)
            {
                AppendByte(data[i]);
            }
        }
        catch (...)
        {
            LOG_CAUGHT_EXCEPTION();
        }
    }

    void SysExBuffer::AppendDisplayWords(uint32_t word0, uint32_t word1) noexcept
    {
        try
        {
            m_words.push_back(word0);
            m_words.push_back(word1);
        }
        catch (...)
        {
            LOG_CAUGHT_EXCEPTION();
        }
    }

    bool SysExBuffer::WriteToFile(std::wstring const& path) const noexcept
    {
        try
        {
            wil::unique_hfile file{ ::CreateFileW(path.c_str(), GENERIC_WRITE, 0, nullptr,
                CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr) };

            if (!file)
            {
                LOG_LAST_ERROR();
                return false;
            }

            DWORD written{ 0 };

            if (!::WriteFile(file.get(), m_bytes.data(), static_cast<DWORD>(m_bytes.size()), &written, nullptr))
            {
                LOG_LAST_ERROR();
                return false;
            }

            return written == m_bytes.size();
        }
        catch (...)
        {
            LOG_CAUGHT_EXCEPTION();
            return false;
        }
    }

    bool SysExBuffer::ReadFromFile(std::wstring const& path) noexcept
    {
        try
        {
            wil::unique_hfile file{ ::CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr,
                OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr) };

            if (!file)
            {
                LOG_LAST_ERROR();
                return false;
            }

            LARGE_INTEGER size{};

            if (!::GetFileSizeEx(file.get(), &size) || size.QuadPart <= 0 || size.QuadPart > 0x7FFFFFFF)
            {
                return false;
            }

            Reset(static_cast<uint32_t>(size.QuadPart));

            std::vector<uint8_t> raw(static_cast<size_t>(size.QuadPart));
            DWORD read{ 0 };

            if (!::ReadFile(file.get(), raw.data(), static_cast<DWORD>(raw.size()), &read, nullptr))
            {
                LOG_LAST_ERROR();
                return false;
            }

            raw.resize(read);

            for (auto const value : raw)
            {
                AppendByte(value);
            }

            return true;
        }
        catch (...)
        {
            LOG_CAUGHT_EXCEPTION();
            return false;
        }
    }
}
