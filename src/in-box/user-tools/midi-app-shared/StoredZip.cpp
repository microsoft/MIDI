// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#include "StoredZip.h"

#include <windows.h>

#include <algorithm>
#include <array>

namespace midiapp
{
    namespace
    {
        constexpr uint32_t LocalHeaderSignature = 0x04034b50;
        constexpr uint32_t CentralHeaderSignature = 0x02014b50;
        constexpr uint32_t EndOfDirectorySignature = 0x06054b50;

        constexpr size_t LocalHeaderSize = 30;
        constexpr size_t CentralHeaderSize = 46;
        constexpr size_t EndOfDirectorySize = 22;

        constexpr uint16_t MethodStored = 0;
        constexpr uint16_t VersionNeeded = 20;

        // Bit 11 marks UTF-8 names; without it they are read in the OEM code page.
        constexpr uint16_t FlagUtf8Names = 0x0800;

        constexpr uint16_t FlagEncrypted = 0x0001;
        constexpr uint16_t FlagDataDescriptor = 0x0008;

        void Put16(_Inout_ std::vector<uint8_t>& bytes, _In_ uint16_t value) noexcept
        {
            bytes.push_back(static_cast<uint8_t>(value & 0xFF));
            bytes.push_back(static_cast<uint8_t>((value >> 8) & 0xFF));
        }

        void Put32(_Inout_ std::vector<uint8_t>& bytes, _In_ uint32_t value) noexcept
        {
            for (int shift = 0; shift < 32; shift += 8)
            {
                bytes.push_back(static_cast<uint8_t>((value >> shift) & 0xFF));
            }
        }

        uint16_t Read16(_In_reads_(2) uint8_t const* at) noexcept
        {
            return static_cast<uint16_t>(at[0] | (at[1] << 8));
        }

        uint32_t Read32(_In_reads_(4) uint8_t const* at) noexcept
        {
            return static_cast<uint32_t>(at[0]) |
                (static_cast<uint32_t>(at[1]) << 8) |
                (static_cast<uint32_t>(at[2]) << 16) |
                (static_cast<uint32_t>(at[3]) << 24);
        }

        std::string ToUtf8(_In_ std::wstring const& text) noexcept
        {
            if (text.empty())
            {
                return {};
            }

            auto const needed = ::WideCharToMultiByte(
                CP_UTF8, 0, text.c_str(), static_cast<int>(text.size()), nullptr, 0, nullptr, nullptr);

            if (needed <= 0)
            {
                return {};
            }

            std::string utf8(static_cast<size_t>(needed), '\0');

            ::WideCharToMultiByte(
                CP_UTF8, 0, text.c_str(), static_cast<int>(text.size()), utf8.data(), needed, nullptr, nullptr);

            return utf8;
        }

        std::wstring FromUtf8(_In_reads_bytes_(size) uint8_t const* data, _In_ size_t size) noexcept
        {
            if (size == 0)
            {
                return {};
            }

            auto const needed = ::MultiByteToWideChar(
                CP_UTF8, MB_ERR_INVALID_CHARS, reinterpret_cast<char const*>(data), static_cast<int>(size), nullptr, 0);

            if (needed <= 0)
            {
                return {};
            }

            std::wstring wide(static_cast<size_t>(needed), L'\0');

            ::MultiByteToWideChar(
                CP_UTF8, MB_ERR_INVALID_CHARS, reinterpret_cast<char const*>(data), static_cast<int>(size), wide.data(), needed);

            return wide;
        }

        struct LocalEntry
        {
            size_t HeaderOffset{ 0 };
            size_t NameOffset{ 0 };
            size_t NameLength{ 0 };
        };

        bool FindEndOfDirectory(_In_ std::vector<uint8_t> const& zip, _Out_ size_t& at) noexcept
        {
            at = 0;

            if (zip.size() < EndOfDirectorySize)
            {
                return false;
            }

            // The record ends with a comment of up to 64 KB, so look back that far and no further.
            auto const lowest = zip.size() > EndOfDirectorySize + 0xFFFF
                ? zip.size() - EndOfDirectorySize - 0xFFFF
                : 0;

            for (size_t candidate = zip.size() - EndOfDirectorySize + 1; candidate-- > lowest;)
            {
                if (Read32(&zip[candidate]) == EndOfDirectorySignature &&
                    candidate + EndOfDirectorySize + Read16(&zip[candidate + 20]) == zip.size())
                {
                    at = candidate;
                    return true;
                }
            }

            return false;
        }
    }

    _Use_decl_annotations_
    uint32_t ComputeCrc32(uint8_t const* data, size_t size) noexcept
    {
        static auto const table = []()
            {
                std::array<uint32_t, 256> values{};

                for (uint32_t i = 0; i < 256; ++i)
                {
                    auto value = i;

                    for (int bit = 0; bit < 8; ++bit)
                    {
                        value = (value & 1) ? (0xEDB88320u ^ (value >> 1)) : (value >> 1);
                    }

                    values[i] = value;
                }

                return values;
            }();

        uint32_t crc{ 0xFFFFFFFFu };

        for (size_t i = 0; i < size; ++i)
        {
            crc = table[(crc ^ data[i]) & 0xFF] ^ (crc >> 8);
        }

        return crc ^ 0xFFFFFFFFu;
    }

    _Use_decl_annotations_
    std::vector<uint8_t> BuildStoredZip(std::vector<StoredZipEntry> const& entries) noexcept
    {
        try
        {
            std::vector<uint8_t> zip{};
            std::vector<uint8_t> directory{};

            uint32_t count{ 0 };

            for (auto const& entry : entries)
            {
                auto const name = ToUtf8(entry.Name);
                auto const crc = ComputeCrc32(entry.Bytes.data(), entry.Bytes.size());
                auto const size = static_cast<uint32_t>(entry.Bytes.size());
                auto const offset = static_cast<uint32_t>(zip.size());

                Put32(zip, LocalHeaderSignature);
                Put16(zip, VersionNeeded);
                Put16(zip, FlagUtf8Names);
                Put16(zip, MethodStored);
                Put16(zip, 0);
                Put16(zip, 0);
                Put32(zip, crc);
                Put32(zip, size);
                Put32(zip, size);
                Put16(zip, static_cast<uint16_t>(name.size()));
                Put16(zip, 0);

                zip.insert(zip.end(), name.begin(), name.end());
                zip.insert(zip.end(), entry.Bytes.begin(), entry.Bytes.end());

                Put32(directory, CentralHeaderSignature);
                Put16(directory, VersionNeeded);
                Put16(directory, VersionNeeded);
                Put16(directory, FlagUtf8Names);
                Put16(directory, MethodStored);
                Put16(directory, 0);
                Put16(directory, 0);
                Put32(directory, crc);
                Put32(directory, size);
                Put32(directory, size);
                Put16(directory, static_cast<uint16_t>(name.size()));
                Put16(directory, 0);
                Put16(directory, 0);
                Put16(directory, 0);
                Put16(directory, 0);
                Put32(directory, 0);
                Put32(directory, offset);

                directory.insert(directory.end(), name.begin(), name.end());

                count++;
            }

            auto const directoryOffset = static_cast<uint32_t>(zip.size());

            zip.insert(zip.end(), directory.begin(), directory.end());

            Put32(zip, EndOfDirectorySignature);
            Put16(zip, 0);
            Put16(zip, 0);
            Put16(zip, static_cast<uint16_t>(count));
            Put16(zip, static_cast<uint16_t>(count));
            Put32(zip, static_cast<uint32_t>(directory.size()));
            Put32(zip, directoryOffset);
            Put16(zip, 0);

            return zip;
        }
        catch (...)
        {
            return {};
        }
    }

    _Use_decl_annotations_
    StoredZipStatus ReadStoredZip(
        std::vector<uint8_t> const& zip,
        uint32_t maximumEntries,
        std::vector<StoredZipEntry>& entries) noexcept
    {
        entries.clear();

        try
        {
            if (zip.size() < LocalHeaderSize || Read32(zip.data()) != LocalHeaderSignature)
            {
                return StoredZipStatus::NotAZip;
            }

            std::vector<LocalEntry> locals{};

            // The local headers are what the bytes follow, so they are what gets read.
            size_t at{ 0 };

            while (at + LocalHeaderSize <= zip.size() && Read32(&zip[at]) == LocalHeaderSignature)
            {
                auto const flags = Read16(&zip[at + 6]);
                auto const method = Read16(&zip[at + 8]);
                auto const crc = Read32(&zip[at + 14]);
                auto const compressedSize = Read32(&zip[at + 18]);
                auto const size = Read32(&zip[at + 22]);
                auto const nameLength = Read16(&zip[at + 26]);
                auto const extraLength = Read16(&zip[at + 28]);

                if (method != MethodStored || (flags & FlagEncrypted) != 0)
                {
                    entries.clear();
                    return StoredZipStatus::Compressed;
                }

                if ((flags & FlagDataDescriptor) != 0 || compressedSize != size || size == 0xFFFFFFFFu)
                {
                    entries.clear();
                    return StoredZipStatus::Damaged;
                }

                auto const nameAt = at + LocalHeaderSize;
                auto const dataAt = nameAt + nameLength + extraLength;

                if (dataAt > zip.size() || size > zip.size() - dataAt)
                {
                    entries.clear();
                    return StoredZipStatus::Damaged;
                }

                if (entries.size() >= maximumEntries)
                {
                    entries.clear();
                    return StoredZipStatus::TooManyEntries;
                }

                if (ComputeCrc32(zip.data() + dataAt, size) != crc)
                {
                    entries.clear();
                    return StoredZipStatus::Damaged;
                }

                StoredZipEntry entry{};
                entry.Name = FromUtf8(zip.data() + nameAt, nameLength);
                entry.Bytes.assign(zip.begin() + dataAt, zip.begin() + dataAt + size);

                if (entry.Name.empty())
                {
                    entries.clear();
                    return StoredZipStatus::Damaged;
                }

                entries.push_back(std::move(entry));
                locals.push_back(LocalEntry{ at, nameAt, nameLength });

                at = dataAt + size;
            }

            if (entries.empty())
            {
                return StoredZipStatus::NotAZip;
            }

            size_t end{ 0 };

            if (!FindEndOfDirectory(zip, end))
            {
                entries.clear();
                return StoredZipStatus::Damaged;
            }

            auto const count = Read16(&zip[end + 10]);
            auto const directorySize = Read32(&zip[end + 12]);
            auto const directoryOffset = Read32(&zip[end + 16]);

            if (count != entries.size() || directoryOffset != at ||
                static_cast<size_t>(directoryOffset) + directorySize != end)
            {
                entries.clear();
                return StoredZipStatus::DirectoryMismatch;
            }

            size_t walk{ directoryOffset };

            for (auto const& local : locals)
            {
                if (walk + CentralHeaderSize > end || Read32(&zip[walk]) != CentralHeaderSignature)
                {
                    entries.clear();
                    return StoredZipStatus::DirectoryMismatch;
                }

                auto const nameLength = Read16(&zip[walk + 28]);
                auto const extraLength = Read16(&zip[walk + 30]);
                auto const commentLength = Read16(&zip[walk + 32]);
                auto const localOffset = Read32(&zip[walk + 42]);
                auto const nameAt = walk + CentralHeaderSize;

                if (nameAt + nameLength > end || localOffset != local.HeaderOffset ||
                    nameLength != local.NameLength ||
                    !std::equal(zip.begin() + nameAt, zip.begin() + nameAt + nameLength,
                        zip.begin() + local.NameOffset))
                {
                    entries.clear();
                    return StoredZipStatus::DirectoryMismatch;
                }

                walk = nameAt + nameLength + extraLength + commentLength;
            }

            if (walk != end)
            {
                entries.clear();
                return StoredZipStatus::DirectoryMismatch;
            }

            return StoredZipStatus::Read;
        }
        catch (...)
        {
            entries.clear();
            return StoredZipStatus::Damaged;
        }
    }
}
