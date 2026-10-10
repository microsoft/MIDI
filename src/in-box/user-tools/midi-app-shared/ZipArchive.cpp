// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#include "ZipArchive.h"
#include "Deflate.h"

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#include <algorithm>
#include <array>
#include <cstring>
#include <functional>
#include <set>

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
        constexpr uint16_t MethodDeflated = 8;
        constexpr uint16_t VersionNeeded = 20;

        // Bit 11 marks UTF-8 names; without it they are read in the OEM code page.
        constexpr uint16_t FlagUtf8Names = 0x0800;

        constexpr uint16_t FlagEncrypted = 0x0001;
        constexpr uint16_t FlagDataDescriptor = 0x0008;
        constexpr uint16_t FlagStrongEncryption = 0x0040;

        // 0xFFFFFFFF in a size or an offset means "look in the zip64 record instead".
        constexpr uint64_t LargestZip32Value = 0xFFFFFFFEu;
        constexpr size_t LargestZip32Count = 0xFFFE;

        // The directory of a zip this tool would ever need to open is far smaller than this.
        constexpr uint32_t MaximumDirectoryBytes = 64 * 1024 * 1024;

        constexpr DWORD FilePieceSize = 1024 * 1024;

        // The code page zip names were written in before UTF-8, when the flag isn't set.
        constexpr UINT ZipLegacyCodePage = 437;

        void Put16(_Inout_ std::vector<uint8_t>& bytes, _In_ uint16_t value)
        {
            bytes.push_back(static_cast<uint8_t>(value & 0xFF));
            bytes.push_back(static_cast<uint8_t>((value >> 8) & 0xFF));
        }

        void Put32(_Inout_ std::vector<uint8_t>& bytes, _In_ uint32_t value)
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

        std::string ToUtf8(_In_ std::wstring_view text) noexcept
        {
            try
            {
                if (text.empty())
                {
                    return {};
                }

                auto const needed = ::WideCharToMultiByte(
                    CP_UTF8, 0, text.data(), static_cast<int>(text.size()), nullptr, 0, nullptr, nullptr);

                if (needed <= 0)
                {
                    return {};
                }

                std::string utf8(static_cast<size_t>(needed), '\0');

                ::WideCharToMultiByte(
                    CP_UTF8, 0, text.data(), static_cast<int>(text.size()), utf8.data(), needed, nullptr, nullptr);

                return utf8;
            }
            catch (...)
            {
                return {};
            }
        }

        // Empty when the bytes aren't valid in that code page.
        std::wstring FromCodePage(_In_ UINT codePage, _In_reads_bytes_(size) uint8_t const* data, _In_ size_t size) noexcept
        {
            try
            {
                if (size == 0)
                {
                    return {};
                }

                DWORD const flags = codePage == CP_UTF8 ? MB_ERR_INVALID_CHARS : 0;

                auto const needed = ::MultiByteToWideChar(
                    codePage, flags, reinterpret_cast<char const*>(data), static_cast<int>(size), nullptr, 0);

                if (needed <= 0)
                {
                    return {};
                }

                std::wstring wide(static_cast<size_t>(needed), L'\0');

                ::MultiByteToWideChar(
                    codePage, flags, reinterpret_cast<char const*>(data), static_cast<int>(size), wide.data(), needed);

                return wide;
            }
            catch (...)
            {
                return {};
            }
        }

        std::wstring FromUtf8(_In_reads_bytes_(size) uint8_t const* data, _In_ size_t size) noexcept
        {
            return FromCodePage(CP_UTF8, data, size);
        }

        // Many tools write UTF-8 names without setting the flag, so UTF-8 is tried before the old code page.
        std::wstring DecodeName(_In_reads_bytes_(size) uint8_t const* data, _In_ size_t size, _In_ bool utf8) noexcept
        {
            auto name = FromUtf8(data, size);

            if (name.empty() && !utf8)
            {
                name = FromCodePage(ZipLegacyCodePage, data, size);
            }

            // A name with a NUL in it would compare differently in different places.
            if (name.find(L'\0') != std::wstring::npos)
            {
                return {};
            }

            return name;
        }

        // The end record finishes with a comment of up to 64 KB, so look back that far and no further.
        bool FindEndOfDirectory(_In_ std::span<uint8_t const> bytes, _Out_ size_t& at) noexcept
        {
            at = 0;

            if (bytes.size() < EndOfDirectorySize)
            {
                return false;
            }

            auto const lowest = bytes.size() > EndOfDirectorySize + 0xFFFF
                ? bytes.size() - EndOfDirectorySize - 0xFFFF
                : 0;

            for (size_t candidate = bytes.size() - EndOfDirectorySize + 1; candidate-- > lowest;)
            {
                if (Read32(&bytes[candidate]) == EndOfDirectorySignature &&
                    candidate + EndOfDirectorySize + Read16(&bytes[candidate + 20]) == bytes.size())
                {
                    at = candidate;
                    return true;
                }
            }

            return false;
        }

        void ToDosTime(_In_ FILETIME const& utc, _Out_ uint16_t& time, _Out_ uint16_t& date) noexcept
        {
            time = 0;
            date = 0;

            FILETIME local{};
            WORD dosDate{ 0 };
            WORD dosTime{ 0 };

            if (::FileTimeToLocalFileTime(&utc, &local) && ::FileTimeToDosDateTime(&local, &dosDate, &dosTime))
            {
                time = dosTime;
                date = dosDate;
            }
        }

        // A name in a zip is a relative path with forward slashes, and nothing that could climb out
        // of the folder it's extracted into.
        bool ToEntryName(_In_ std::wstring_view name, _Out_ std::string& utf8) noexcept
        {
            utf8.clear();

            try
            {
                std::wstring normalized{ name };
                std::replace(normalized.begin(), normalized.end(), L'\\', L'/');

                if (normalized.empty() || normalized.front() == L'/' || normalized.find(L':') != std::wstring::npos)
                {
                    return false;
                }

                if (std::any_of(normalized.begin(), normalized.end(), [](wchar_t const character) { return character < L' '; }))
                {
                    return false;
                }

                size_t start{ 0 };

                for (;;)
                {
                    auto const end = normalized.find(L'/', start);
                    auto const segment = std::wstring_view{ normalized }.substr(start, end == std::wstring::npos ? std::wstring::npos : end - start);

                    if (segment.empty() || segment == L"." || segment == L"..")
                    {
                        return false;
                    }

                    if (end == std::wstring::npos)
                    {
                        break;
                    }

                    start = end + 1;
                }

                utf8 = ToUtf8(normalized);

                return !utf8.empty() && utf8.size() <= 0xFFFF;
            }
            catch (...)
            {
                utf8.clear();
                return false;
            }
        }

        // ------------------------------------------------------------------------ writing

        struct CentralRecord
        {
            std::string Name{};
            uint32_t Crc{ 0 };
            uint32_t CompressedSize{ 0 };
            uint32_t Size{ 0 };
            uint16_t Method{ 0 };
            uint16_t Time{ 0 };
            uint16_t Date{ 0 };
            uint32_t Offset{ 0 };
        };

        // Where a zip being written goes. Patch fills in a local header once its file's sizes are known.
        class ZipOutput
        {
        public:
            virtual ~ZipOutput() = default;

            virtual bool Append(_In_reads_bytes_opt_(size) uint8_t const* data, _In_ size_t size) = 0;
            virtual bool Patch(_In_ uint64_t offset, _In_reads_bytes_(size) uint8_t const* data, _In_ size_t size) = 0;

            uint64_t Size() const noexcept { return m_size; }

        protected:
            uint64_t m_size{ 0 };
        };

        class MemoryOutput final : public ZipOutput
        {
        public:
            explicit MemoryOutput(_Inout_ std::vector<uint8_t>& bytes) noexcept : m_bytes{ bytes }
            {
            }

            bool Append(_In_reads_bytes_opt_(size) uint8_t const* data, _In_ size_t size) override
            {
                if (size > 0)
                {
                    m_bytes.insert(m_bytes.end(), data, data + size);
                    m_size += size;
                }

                return true;
            }

            bool Patch(_In_ uint64_t offset, _In_reads_bytes_(size) uint8_t const* data, _In_ size_t size) override
            {
                if (offset > m_bytes.size() || size > m_bytes.size() - offset)
                {
                    return false;
                }

                std::memcpy(m_bytes.data() + offset, data, size);

                return true;
            }

        private:
            std::vector<uint8_t>& m_bytes;
        };

        class FileOutput final : public ZipOutput
        {
        public:
            explicit FileOutput(_In_ HANDLE file) noexcept : m_file{ file }
            {
            }

            bool Append(_In_reads_bytes_opt_(size) uint8_t const* data, _In_ size_t size) override
            {
                while (size > 0)
                {
                    auto const chunk = static_cast<DWORD>(std::min<size_t>(size, 1u << 30));
                    DWORD wrote{ 0 };

                    if (!::WriteFile(m_file, data, chunk, &wrote, nullptr) || wrote != chunk)
                    {
                        return false;
                    }

                    data += chunk;
                    size -= chunk;
                    m_size += chunk;
                }

                return true;
            }

            bool Patch(_In_ uint64_t offset, _In_reads_bytes_(size) uint8_t const* data, _In_ size_t size) override
            {
                if (size > MAXDWORD)
                {
                    return false;
                }

                OVERLAPPED at{};
                at.Offset = static_cast<DWORD>(offset & 0xFFFFFFFF);
                at.OffsetHigh = static_cast<DWORD>(offset >> 32);

                DWORD wrote{ 0 };

                if (!::WriteFile(m_file, data, static_cast<DWORD>(size), &wrote, &at) || wrote != size)
                {
                    return false;
                }

                // A write at an offset moves the file pointer there, so it goes back to the end.
                LARGE_INTEGER zero{};
                return ::SetFilePointerEx(m_file, zero, nullptr, FILE_END) != FALSE;
            }

        private:
            HANDLE m_file{ INVALID_HANDLE_VALUE };
        };

        // Hands over the next piece of a file's bytes. An empty piece is the end.
        using PieceSource = std::function<bool(std::span<uint8_t const>& piece)>;

        // The local header goes first with its checksum and sizes blank, then the data, then the
        // blanks are filled in. So the zip needs no data descriptors, which some readers refuse.
        ZipStatus WriteEntry(
            _Inout_ ZipOutput& output,
            _In_ std::string const& name,
            _In_ ZipCompression compression,
            _In_ uint16_t time,
            _In_ uint16_t date,
            _In_ PieceSource const& source,
            _Out_ CentralRecord& record)
        {
            record = {};

            auto const offset = output.Size();

            if (name.size() > 0xFFFF)
            {
                return ZipStatus::BadName;
            }

            if (offset > LargestZip32Value)
            {
                return ZipStatus::TooLarge;
            }

            record.Name = name;
            record.Method = compression == ZipCompression::Deflate ? MethodDeflated : MethodStored;
            record.Time = time;
            record.Date = date;
            record.Offset = static_cast<uint32_t>(offset);

            std::vector<uint8_t> header{};

            Put32(header, LocalHeaderSignature);
            Put16(header, VersionNeeded);
            Put16(header, FlagUtf8Names);
            Put16(header, record.Method);
            Put16(header, time);
            Put16(header, date);
            Put32(header, 0);
            Put32(header, 0);
            Put32(header, 0);
            Put16(header, static_cast<uint16_t>(name.size()));
            Put16(header, 0);

            header.insert(header.end(), name.begin(), name.end());

            if (!output.Append(header.data(), header.size()))
            {
                return ZipStatus::CannotWrite;
            }

            DeflateEncoder encoder{};
            std::vector<uint8_t> packed{};

            uint32_t crc{ 0 };
            uint64_t size{ 0 };
            uint64_t compressedSize{ 0 };

            for (;;)
            {
                std::span<uint8_t const> piece{};

                if (!source(piece))
                {
                    return ZipStatus::CannotOpen;
                }

                bool const end{ piece.empty() };

                crc = UpdateCrc32(crc, piece.data(), piece.size());
                size += piece.size();

                if (size > LargestZip32Value)
                {
                    return ZipStatus::TooLarge;
                }

                if (compression == ZipCompression::Store)
                {
                    if (!output.Append(piece.data(), piece.size()))
                    {
                        return ZipStatus::CannotWrite;
                    }

                    compressedSize += piece.size();
                }
                else
                {
                    packed.clear();

                    if (!encoder.Write(piece.data(), piece.size(), end, packed))
                    {
                        return ZipStatus::CannotWrite;
                    }

                    if (!output.Append(packed.data(), packed.size()))
                    {
                        return ZipStatus::CannotWrite;
                    }

                    compressedSize += packed.size();
                }

                if (compressedSize > LargestZip32Value || output.Size() > LargestZip32Value)
                {
                    return ZipStatus::TooLarge;
                }

                if (end)
                {
                    break;
                }
            }

            record.Crc = crc;
            record.Size = static_cast<uint32_t>(size);
            record.CompressedSize = static_cast<uint32_t>(compressedSize);

            std::vector<uint8_t> sizes{};

            Put32(sizes, record.Crc);
            Put32(sizes, record.CompressedSize);
            Put32(sizes, record.Size);

            return output.Patch(offset + 14, sizes.data(), sizes.size()) ? ZipStatus::Written : ZipStatus::CannotWrite;
        }

        ZipStatus WriteDirectory(_Inout_ ZipOutput& output, _In_ std::vector<CentralRecord> const& records)
        {
            if (records.size() > LargestZip32Count)
            {
                return ZipStatus::TooLarge;
            }

            auto const directoryOffset = output.Size();

            std::vector<uint8_t> directory{};

            for (auto const& record : records)
            {
                Put32(directory, CentralHeaderSignature);
                Put16(directory, VersionNeeded);
                Put16(directory, VersionNeeded);
                Put16(directory, FlagUtf8Names);
                Put16(directory, record.Method);
                Put16(directory, record.Time);
                Put16(directory, record.Date);
                Put32(directory, record.Crc);
                Put32(directory, record.CompressedSize);
                Put32(directory, record.Size);
                Put16(directory, static_cast<uint16_t>(record.Name.size()));
                Put16(directory, 0);
                Put16(directory, 0);
                Put16(directory, 0);
                Put16(directory, 0);
                Put32(directory, 0);
                Put32(directory, record.Offset);

                directory.insert(directory.end(), record.Name.begin(), record.Name.end());
            }

            if (directoryOffset > LargestZip32Value || directory.size() > LargestZip32Value - directoryOffset)
            {
                return ZipStatus::TooLarge;
            }

            auto const directorySize = directory.size();

            Put32(directory, EndOfDirectorySignature);
            Put16(directory, 0);
            Put16(directory, 0);
            Put16(directory, static_cast<uint16_t>(records.size()));
            Put16(directory, static_cast<uint16_t>(records.size()));
            Put32(directory, static_cast<uint32_t>(directorySize));
            Put32(directory, static_cast<uint32_t>(directoryOffset));
            Put16(directory, 0);

            return output.Append(directory.data(), directory.size()) ? ZipStatus::Written : ZipStatus::CannotWrite;
        }

        PieceSource BytesSource(_In_ std::span<uint8_t const> bytes)
        {
            return [bytes, given = false](std::span<uint8_t const>& piece) mutable
                {
                    piece = given ? std::span<uint8_t const>{} : bytes;
                    given = true;
                    return true;
                };
        }
    }

    _Use_decl_annotations_
    uint32_t UpdateCrc32(uint32_t crc, uint8_t const* data, size_t size) noexcept
    {
        static auto const table = []()
            {
                std::array<uint32_t, 256> values{};
                uint32_t index{ 0 };

                for (auto& entry : values)
                {
                    auto value = index++;

                    for (int bit = 0; bit < 8; ++bit)
                    {
                        value = (value & 1) ? (0xEDB88320u ^ (value >> 1)) : (value >> 1);
                    }

                    entry = value;
                }

                return values;
            }();

        if (data == nullptr)
        {
            return crc;
        }

        crc = ~crc;

        for (size_t i = 0; i < size; ++i)
        {
            crc = table[static_cast<uint8_t>(crc ^ data[i])] ^ (crc >> 8);
        }

        return ~crc;
    }

    _Use_decl_annotations_
    uint32_t ComputeCrc32(uint8_t const* data, size_t size) noexcept
    {
        return UpdateCrc32(0, data, size);
    }

    _Use_decl_annotations_
    std::vector<uint8_t> BuildZip(std::vector<ZipEntry> const& entries, ZipCompression compression) noexcept
    {
        try
        {
            std::vector<uint8_t> zip{};
            MemoryOutput output{ zip };

            std::vector<CentralRecord> records{};

            for (auto const& entry : entries)
            {
                CentralRecord record{};

                if (WriteEntry(output, ToUtf8(entry.Name), compression, 0, 0, BytesSource(entry.Bytes), record) != ZipStatus::Written)
                {
                    return {};
                }

                records.push_back(std::move(record));
            }

            if (WriteDirectory(output, records) != ZipStatus::Written)
            {
                return {};
            }

            return zip;
        }
        catch (...)
        {
            return {};
        }
    }

    _Use_decl_annotations_
    std::vector<uint8_t> BuildStoredZip(std::vector<ZipEntry> const& entries) noexcept
    {
        return BuildZip(entries, ZipCompression::Store);
    }

    _Use_decl_annotations_
    ZipStatus ReadStoredZip(
        std::vector<uint8_t> const& zip,
        uint32_t maximumEntries,
        std::vector<ZipEntry>& entries) noexcept
    {
        struct LocalEntry
        {
            size_t HeaderOffset{ 0 };
            size_t NameOffset{ 0 };
            size_t NameLength{ 0 };
        };

        entries.clear();

        try
        {
            if (zip.size() < LocalHeaderSize || Read32(zip.data()) != LocalHeaderSignature)
            {
                return ZipStatus::NotAZip;
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
                    return ZipStatus::Compressed;
                }

                if ((flags & FlagDataDescriptor) != 0 || compressedSize != size || size == 0xFFFFFFFFu)
                {
                    entries.clear();
                    return ZipStatus::Damaged;
                }

                auto const nameAt = at + LocalHeaderSize;
                auto const dataAt = nameAt + nameLength + extraLength;

                if (dataAt > zip.size() || size > zip.size() - dataAt)
                {
                    entries.clear();
                    return ZipStatus::Damaged;
                }

                if (entries.size() >= maximumEntries)
                {
                    entries.clear();
                    return ZipStatus::TooManyEntries;
                }

                if (ComputeCrc32(zip.data() + dataAt, size) != crc)
                {
                    entries.clear();
                    return ZipStatus::Damaged;
                }

                ZipEntry entry{};
                entry.Name = FromUtf8(zip.data() + nameAt, nameLength);
                entry.Bytes.assign(zip.begin() + dataAt, zip.begin() + dataAt + size);

                if (entry.Name.empty())
                {
                    entries.clear();
                    return ZipStatus::Damaged;
                }

                entries.push_back(std::move(entry));
                locals.push_back(LocalEntry{ at, nameAt, nameLength });

                at = dataAt + size;
            }

            if (entries.empty())
            {
                return ZipStatus::NotAZip;
            }

            size_t end{ 0 };

            if (!FindEndOfDirectory(zip, end))
            {
                entries.clear();
                return ZipStatus::Damaged;
            }

            auto const count = Read16(&zip[end + 10]);
            auto const directorySize = Read32(&zip[end + 12]);
            auto const directoryOffset = Read32(&zip[end + 16]);

            if (count != entries.size() || directoryOffset != at ||
                static_cast<size_t>(directoryOffset) + directorySize != end)
            {
                entries.clear();
                return ZipStatus::DirectoryMismatch;
            }

            size_t walk{ directoryOffset };

            for (auto const& local : locals)
            {
                if (walk + CentralHeaderSize > end || Read32(&zip[walk]) != CentralHeaderSignature)
                {
                    entries.clear();
                    return ZipStatus::DirectoryMismatch;
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
                    return ZipStatus::DirectoryMismatch;
                }

                walk = nameAt + nameLength + extraLength + commentLength;
            }

            if (walk != end)
            {
                entries.clear();
                return ZipStatus::DirectoryMismatch;
            }

            return ZipStatus::Read;
        }
        catch (...)
        {
            entries.clear();
            return ZipStatus::Damaged;
        }
    }

    // ------------------------------------------------------------------------ ZipReader

    ZipReader::~ZipReader()
    {
        Close();
    }

    void ZipReader::Close() noexcept
    {
        if (m_file != nullptr)
        {
            ::CloseHandle(static_cast<HANDLE>(m_file));
            m_file = nullptr;
        }

        m_memory = {};
        m_size = 0;
        m_items.clear();
    }

    _Use_decl_annotations_
    ZipStatus ZipReader::Open(std::span<uint8_t const> zip, uint32_t maximumEntries) noexcept
    {
        Close();

        m_memory = zip;
        m_size = zip.size();

        auto const status = ReadDirectory(maximumEntries);

        if (status != ZipStatus::Read)
        {
            Close();
        }

        return status;
    }

    _Use_decl_annotations_
    ZipStatus ZipReader::OpenFile(std::filesystem::path const& path, uint32_t maximumEntries) noexcept
    {
        Close();

        auto const file = ::CreateFileW(
            path.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);

        if (file == INVALID_HANDLE_VALUE)
        {
            return ZipStatus::CannotOpen;
        }

        m_file = file;

        LARGE_INTEGER size{};

        if (!::GetFileSizeEx(file, &size) || size.QuadPart < 0)
        {
            Close();
            return ZipStatus::CannotOpen;
        }

        m_size = static_cast<uint64_t>(size.QuadPart);

        auto const status = ReadDirectory(maximumEntries);

        if (status != ZipStatus::Read)
        {
            Close();
        }

        return status;
    }

    _Use_decl_annotations_
    bool ZipReader::ReadAt(uint64_t offset, uint8_t* destination, size_t size) const noexcept
    {
        if (offset > m_size || size > m_size - offset)
        {
            return false;
        }

        if (m_file == nullptr)
        {
            if (size > 0)
            {
                std::memcpy(destination, m_memory.data() + offset, size);
            }

            return true;
        }

        while (size > 0)
        {
            auto const chunk = static_cast<DWORD>(std::min<size_t>(size, 1u << 30));

            OVERLAPPED at{};
            at.Offset = static_cast<DWORD>(offset & 0xFFFFFFFF);
            at.OffsetHigh = static_cast<DWORD>(offset >> 32);

            DWORD got{ 0 };

            if (!::ReadFile(static_cast<HANDLE>(m_file), destination, chunk, &got, &at) || got != chunk)
            {
                return false;
            }

            destination += chunk;
            offset += chunk;
            size -= chunk;
        }

        return true;
    }

    _Use_decl_annotations_
    ZipStatus ZipReader::ReadDirectory(uint32_t maximumEntries) noexcept
    {
        try
        {
            m_items.clear();

            if (m_size < EndOfDirectorySize)
            {
                return ZipStatus::NotAZip;
            }

            // A zip starts with a file header, or with the end record when it's empty.
            std::array<uint8_t, 4> start{};

            if (!ReadAt(0, start.data(), start.size()))
            {
                return ZipStatus::CannotOpen;
            }

            auto const firstSignature = Read32(start.data());

            if (firstSignature != LocalHeaderSignature && firstSignature != EndOfDirectorySignature)
            {
                return ZipStatus::NotAZip;
            }

            auto const tailSize = static_cast<size_t>(std::min<uint64_t>(m_size, EndOfDirectorySize + 0xFFFF));
            std::vector<uint8_t> tail(tailSize);

            if (!ReadAt(m_size - tailSize, tail.data(), tailSize))
            {
                return ZipStatus::CannotOpen;
            }

            size_t recordAt{ 0 };

            if (!FindEndOfDirectory(tail, recordAt))
            {
                return ZipStatus::Damaged;
            }

            auto const endOfDirectory = m_size - tailSize + recordAt;
            auto const* const record = tail.data() + recordAt;

            auto const disk = Read16(record + 4);
            auto const directoryDisk = Read16(record + 6);
            auto const countOnDisk = Read16(record + 8);
            auto const count = Read16(record + 10);
            auto const directorySize = Read32(record + 12);
            auto const directoryOffset = Read32(record + 16);

            if (disk != 0 || directoryDisk != 0 || countOnDisk != count ||
                count == 0xFFFF || directorySize == 0xFFFFFFFFu || directoryOffset == 0xFFFFFFFFu)
            {
                return ZipStatus::Unsupported;
            }

            if (count > maximumEntries)
            {
                return ZipStatus::TooManyEntries;
            }

            if (static_cast<uint64_t>(directoryOffset) + directorySize > endOfDirectory)
            {
                return ZipStatus::Damaged;
            }

            if (directorySize > MaximumDirectoryBytes)
            {
                return ZipStatus::TooLarge;
            }

            std::vector<uint8_t> directory(directorySize);

            if (!ReadAt(directoryOffset, directory.data(), directory.size()))
            {
                return ZipStatus::CannotOpen;
            }

            struct Span
            {
                uint64_t Start{ 0 };
                uint64_t End{ 0 };
            };

            std::vector<Span> spans{};
            std::vector<ZipItem> items{};

            size_t walk{ 0 };

            for (uint32_t index = 0; index < count; ++index)
            {
                if (directory.size() - walk < CentralHeaderSize || Read32(&directory[walk]) != CentralHeaderSignature)
                {
                    return ZipStatus::Damaged;
                }

                auto const* const header = directory.data() + walk;

                auto const flags = Read16(header + 8);
                auto const method = Read16(header + 10);
                auto const crc = Read32(header + 16);
                auto const compressedSize = Read32(header + 20);
                auto const size = Read32(header + 24);
                auto const nameLength = Read16(header + 28);
                auto const extraLength = Read16(header + 30);
                auto const commentLength = Read16(header + 32);
                auto const diskStart = Read16(header + 34);
                auto const localOffset = Read32(header + 42);

                size_t const entrySize{ CentralHeaderSize + nameLength + extraLength + commentLength };

                if (directory.size() - walk < entrySize)
                {
                    return ZipStatus::Damaged;
                }

                if ((flags & (FlagEncrypted | FlagStrongEncryption)) != 0 ||
                    (method != MethodStored && method != MethodDeflated) ||
                    compressedSize == 0xFFFFFFFFu || size == 0xFFFFFFFFu || localOffset == 0xFFFFFFFFu || diskStart != 0)
                {
                    return ZipStatus::Unsupported;
                }

                if (method == MethodStored && compressedSize != size)
                {
                    return ZipStatus::Damaged;
                }

                auto const* const nameBytes = header + CentralHeaderSize;

                ZipItem item{};
                item.Name = DecodeName(nameBytes, nameLength, (flags & FlagUtf8Names) != 0);
                item.Size = size;
                item.CompressedSize = compressedSize;
                item.Crc = crc;
                item.Method = method;

                if (item.Name.empty())
                {
                    return ZipStatus::Damaged;
                }

                // The local header has to name the same file, the same way, as the directory does.
                std::array<uint8_t, LocalHeaderSize> local{};

                if (!ReadAt(localOffset, local.data(), local.size()) || Read32(local.data()) != LocalHeaderSignature)
                {
                    return ZipStatus::Damaged;
                }

                auto const localMethod = Read16(local.data() + 8);
                auto const localNameLength = Read16(local.data() + 26);
                auto const localExtraLength = Read16(local.data() + 28);

                if (localMethod != method || localNameLength != nameLength)
                {
                    return ZipStatus::DirectoryMismatch;
                }

                std::vector<uint8_t> localName(localNameLength);

                if (!ReadAt(static_cast<uint64_t>(localOffset) + LocalHeaderSize, localName.data(), localName.size()))
                {
                    return ZipStatus::Damaged;
                }

                if (!std::equal(localName.begin(), localName.end(), nameBytes))
                {
                    return ZipStatus::DirectoryMismatch;
                }

                auto const dataOffset = static_cast<uint64_t>(localOffset) + LocalHeaderSize + localNameLength + localExtraLength;

                if (dataOffset + compressedSize > directoryOffset)
                {
                    return ZipStatus::Damaged;
                }

                item.DataOffset = static_cast<uint32_t>(dataOffset);

                spans.push_back(Span{ localOffset, dataOffset + compressedSize });

                bool const folder{ item.Name.back() == L'/' || item.Name.back() == L'\\' };

                if (!folder)
                {
                    items.push_back(std::move(item));
                }

                walk += entrySize;
            }

            if (walk != directory.size())
            {
                return ZipStatus::Damaged;
            }

            // No two files may share bytes, which is how some zip bombs make one block count many times.
            std::sort(spans.begin(), spans.end(), [](Span const& left, Span const& right) { return left.Start < right.Start; });

            for (size_t i = 1; i < spans.size(); ++i)
            {
                if (spans[i].Start < spans[i - 1].End)
                {
                    return ZipStatus::Damaged;
                }
            }

            m_items = std::move(items);

            return ZipStatus::Read;
        }
        catch (...)
        {
            m_items.clear();
            return ZipStatus::Damaged;
        }
    }

    _Use_decl_annotations_
    ZipStatus ZipReader::Extract(ZipItem const& item, size_t maximumSize, std::vector<uint8_t>& bytes) const noexcept
    {
        bytes.clear();

        try
        {
            if (item.Size > maximumSize)
            {
                return ZipStatus::TooLarge;
            }

            // Deflate never needs much more room than the data itself, so anything far larger is
            // refused before it's read.
            if (item.CompressedSize > static_cast<uint64_t>(item.Size) + item.Size / 8 + 1024)
            {
                return ZipStatus::Damaged;
            }

            std::vector<uint8_t> stored(item.CompressedSize);

            if (!ReadAt(item.DataOffset, stored.data(), stored.size()))
            {
                return ZipStatus::Damaged;
            }

            if (item.Method == MethodStored)
            {
                bytes = std::move(stored);
            }
            else if (!Inflate(stored.data(), stored.size(), item.Size, bytes))
            {
                return ZipStatus::Damaged;
            }

            if (bytes.size() != item.Size || ComputeCrc32(bytes.data(), bytes.size()) != item.Crc)
            {
                bytes.clear();
                return ZipStatus::Damaged;
            }

            return ZipStatus::Read;
        }
        catch (...)
        {
            bytes.clear();
            return ZipStatus::TooLarge;
        }
    }

    // ------------------------------------------------------------------------ ZipWriter

    struct ZipWriter::State
    {
        HANDLE File{ INVALID_HANDLE_VALUE };
        std::unique_ptr<FileOutput> Output{};
        std::filesystem::path Path{};
        std::filesystem::path PartialPath{};
        std::vector<CentralRecord> Records{};
        std::set<std::string> Names{};

        // Set once something went wrong partway through a file, after which the zip can't be finished.
        bool Failed{ false };
    };

    ZipWriter::ZipWriter() noexcept = default;

    ZipWriter::~ZipWriter()
    {
        Abandon();
    }

    void ZipWriter::Abandon() noexcept
    {
        if (!m_state)
        {
            return;
        }

        if (m_state->File != INVALID_HANDLE_VALUE)
        {
            ::CloseHandle(m_state->File);
            ::DeleteFileW(m_state->PartialPath.c_str());
        }

        m_state.reset();
    }

    _Use_decl_annotations_
    ZipStatus ZipWriter::Create(std::filesystem::path const& path) noexcept
    {
        Abandon();

        try
        {
            auto state = std::make_unique<State>();

            state->Path = path;
            state->PartialPath = path;
            state->PartialPath += L".partial";

            state->File = ::CreateFileW(
                state->PartialPath.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);

            if (state->File == INVALID_HANDLE_VALUE)
            {
                return ZipStatus::CannotWrite;
            }

            state->Output = std::make_unique<FileOutput>(state->File);
            m_state = std::move(state);

            return ZipStatus::Written;
        }
        catch (...)
        {
            return ZipStatus::CannotWrite;
        }
    }

    _Use_decl_annotations_
    ZipStatus ZipWriter::AddFile(std::wstring_view name, std::filesystem::path const& source, ZipCompression compression) noexcept
    {
        if (!m_state || m_state->Failed)
        {
            return ZipStatus::CannotWrite;
        }

        try
        {
            std::string entryName{};

            if (!ToEntryName(name, entryName) || m_state->Names.contains(entryName))
            {
                return ZipStatus::BadName;
            }

            auto const file = ::CreateFileW(
                source.c_str(),
                GENERIC_READ,
                FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                nullptr,
                OPEN_EXISTING,
                FILE_FLAG_SEQUENTIAL_SCAN,
                nullptr);

            if (file == INVALID_HANDLE_VALUE)
            {
                return ZipStatus::CannotOpen;
            }

            struct CloseOnExit
            {
                HANDLE Handle;
                ~CloseOnExit() { ::CloseHandle(Handle); }
            } const closeOnExit{ file };

            uint16_t time{ 0 };
            uint16_t date{ 0 };
            FILETIME lastWrite{};

            if (::GetFileTime(file, nullptr, nullptr, &lastWrite))
            {
                ToDosTime(lastWrite, time, date);
            }

            std::vector<uint8_t> buffer(FilePieceSize);

            PieceSource const read = [file, &buffer](std::span<uint8_t const>& piece)
                {
                    DWORD got{ 0 };

                    if (!::ReadFile(file, buffer.data(), FilePieceSize, &got, nullptr))
                    {
                        return false;
                    }

                    piece = std::span<uint8_t const>{ buffer.data(), got };
                    return true;
                };

            CentralRecord record{};
            auto const status = WriteEntry(*m_state->Output, entryName, compression, time, date, read, record);

            if (status != ZipStatus::Written)
            {
                m_state->Failed = true;
                return status;
            }

            m_state->Names.insert(entryName);
            m_state->Records.push_back(std::move(record));

            return ZipStatus::Written;
        }
        catch (...)
        {
            m_state->Failed = true;
            return ZipStatus::CannotWrite;
        }
    }

    _Use_decl_annotations_
    ZipStatus ZipWriter::AddBytes(std::wstring_view name, std::span<uint8_t const> bytes, ZipCompression compression) noexcept
    {
        if (!m_state || m_state->Failed)
        {
            return ZipStatus::CannotWrite;
        }

        try
        {
            std::string entryName{};

            if (!ToEntryName(name, entryName) || m_state->Names.contains(entryName))
            {
                return ZipStatus::BadName;
            }

            CentralRecord record{};
            auto const status = WriteEntry(*m_state->Output, entryName, compression, 0, 0, BytesSource(bytes), record);

            if (status != ZipStatus::Written)
            {
                m_state->Failed = true;
                return status;
            }

            m_state->Names.insert(entryName);
            m_state->Records.push_back(std::move(record));

            return ZipStatus::Written;
        }
        catch (...)
        {
            m_state->Failed = true;
            return ZipStatus::CannotWrite;
        }
    }

    ZipStatus ZipWriter::Finish() noexcept
    {
        if (!m_state || m_state->Failed)
        {
            Abandon();
            return ZipStatus::CannotWrite;
        }

        try
        {
            auto const status = WriteDirectory(*m_state->Output, m_state->Records);

            if (status != ZipStatus::Written)
            {
                Abandon();
                return status;
            }

            auto const closed = ::CloseHandle(m_state->File) != FALSE;
            m_state->File = INVALID_HANDLE_VALUE;

            if (!closed || !::MoveFileExW(m_state->PartialPath.c_str(), m_state->Path.c_str(), MOVEFILE_REPLACE_EXISTING))
            {
                ::DeleteFileW(m_state->PartialPath.c_str());
                m_state.reset();

                return ZipStatus::CannotWrite;
            }

            m_state.reset();

            return ZipStatus::Written;
        }
        catch (...)
        {
            Abandon();
            return ZipStatus::CannotWrite;
        }
    }
}
