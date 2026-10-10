// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

// The zip files the tools read and write: Windows MIDI Glass packages, backups and content packs,
// the MIDI Troubleshooting app's reports and support packages, and DAWproject files. Stored and
// deflated files with UTF-8 names. No encryption, zip64 or zips split across several files.

#include <sal.h>

#include <cstdint>
#include <filesystem>
#include <memory>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace midiapp
{
    struct ZipEntry
    {
        std::wstring Name{};
        std::vector<uint8_t> Bytes{};
    };

    enum class ZipStatus
    {
        Read,
        Written,
        NotAZip,

        // Compressed, where only stored files are accepted.
        Compressed,

        TooManyEntries,
        Damaged,

        // The central directory, which other tools show, doesn't list what the local headers hold.
        DirectoryMismatch,

        // Encrypted, zip64, split across several files, or compressed some way other than deflate.
        Unsupported,

        // Larger than the caller allows, or than a zip without zip64 can hold.
        TooLarge,

        // Not a plain relative path, or already in the zip.
        BadName,

        CannotOpen,
        CannotWrite,
    };

    enum class ZipCompression
    {
        Store,
        Deflate,
    };

    // The names from when this was stored-only, which Windows MIDI Glass still uses.
    using StoredZipEntry = ZipEntry;
    using StoredZipStatus = ZipStatus;

    uint32_t ComputeCrc32(_In_reads_bytes_(size) uint8_t const* data, _In_ size_t size) noexcept;

    // Carries a checksum on across pieces. The first piece passes 0.
    uint32_t UpdateCrc32(_In_ uint32_t crc, _In_reads_bytes_opt_(size) uint8_t const* data, _In_ size_t size) noexcept;

    // ------------------------------------------------------------------------ whole zips in memory

    // UTF-8 names and no timestamps, so the same files always give the same bytes. Names are
    // written as given. Empty when the zip would be too large.
    std::vector<uint8_t> BuildZip(_In_ std::vector<ZipEntry> const& entries, _In_ ZipCompression compression) noexcept;

    std::vector<uint8_t> BuildStoredZip(_In_ std::vector<ZipEntry> const& entries) noexcept;

    // Stored files only, and the local headers and the central directory have to describe exactly
    // the same files, back to back, with nothing hidden between them. A signed pack depends on that,
    // so no decompressor ever sees a stranger's file.
    ZipStatus ReadStoredZip(
        _In_ std::vector<uint8_t> const& zip,
        _In_ uint32_t maximumEntries,
        _Out_ std::vector<ZipEntry>& entries) noexcept;

    // ------------------------------------------------------------------------ zips other apps made

    struct ZipItem
    {
        std::wstring Name{};
        uint32_t Size{ 0 };
        uint32_t CompressedSize{ 0 };
        uint32_t Crc{ 0 };
        uint16_t Method{ 0 };
        uint32_t DataOffset{ 0 };
    };

    // Reads zips the way other apps write them. The central directory says what's in the file, so a
    // file whose sizes come after its data is read too. Folders aren't listed. Opening reads only the
    // directory; a file is read, decompressed and checked against its checksum when it's extracted.
    class ZipReader
    {
    public:
        ZipReader() noexcept = default;
        ~ZipReader();

        ZipReader(ZipReader const&) = delete;
        ZipReader& operator=(ZipReader const&) = delete;

        // The bytes have to stay alive as long as the reader.
        ZipStatus Open(_In_ std::span<uint8_t const> zip, _In_ uint32_t maximumEntries) noexcept;

        ZipStatus OpenFile(_In_ std::filesystem::path const& path, _In_ uint32_t maximumEntries) noexcept;

        std::vector<ZipItem> const& Items() const noexcept { return m_items; }

        ZipStatus Extract(_In_ ZipItem const& item, _In_ size_t maximumSize, _Out_ std::vector<uint8_t>& bytes) const noexcept;

    private:
        ZipStatus ReadDirectory(_In_ uint32_t maximumEntries) noexcept;
        bool ReadAt(_In_ uint64_t offset, _Out_writes_bytes_(size) uint8_t* destination, _In_ size_t size) const noexcept;
        void Close() noexcept;

        std::span<uint8_t const> m_memory{};

        // a file handle, or null when reading from memory
        void* m_file{ nullptr };

        uint64_t m_size{ 0 };
        std::vector<ZipItem> m_items{};
    };

    // Writes a zip file with UTF-8 names. It's written beside its final name and renamed when it's
    // finished, so a failure never leaves half a zip where the customer would find it.
    class ZipWriter
    {
    public:
        ZipWriter() noexcept;
        ~ZipWriter();

        ZipWriter(ZipWriter const&) = delete;
        ZipWriter& operator=(ZipWriter const&) = delete;

        ZipStatus Create(_In_ std::filesystem::path const& path) noexcept;

        // The name is a relative path inside the zip; backslashes become forward slashes. The file is
        // read a piece at a time, so its size doesn't matter to memory, and keeps its last write time.
        ZipStatus AddFile(
            _In_ std::wstring_view name,
            _In_ std::filesystem::path const& source,
            _In_ ZipCompression compression) noexcept;

        ZipStatus AddBytes(
            _In_ std::wstring_view name,
            _In_ std::span<uint8_t const> bytes,
            _In_ ZipCompression compression) noexcept;

        ZipStatus Finish() noexcept;

    private:
        struct State;

        void Abandon() noexcept;

        std::unique_ptr<State> m_state;
    };
}
