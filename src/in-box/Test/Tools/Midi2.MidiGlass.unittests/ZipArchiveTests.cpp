// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#include "ZipArchiveTests.h"

#include "Deflate.h"
#include "ZipArchive.h"

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <compressapi.h>

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <random>
#include <string>
#include <string_view>
#include <vector>

#pragma comment(lib, "cabinet.lib")

using namespace WEX::Common;
using namespace WEX::Logging;
using namespace WEX::TestExecution;

namespace
{
    using midiapp::ZipCompression;
    using midiapp::ZipEntry;
    using midiapp::ZipStatus;

    std::vector<uint8_t> Bytes(_In_ std::string_view text)
    {
        return std::vector<uint8_t>{ text.begin(), text.end() };
    }

    std::vector<uint8_t> Repeated(_In_ std::string_view text, _In_ int count)
    {
        std::string all{};

        for (int i = 0; i < count; ++i)
        {
            all += text;
        }

        return Bytes(all);
    }

    // Note elements like a DAWproject file's, varied enough that deflate has real work to do.
    std::vector<uint8_t> Text(_In_ size_t size, _In_ uint32_t seed)
    {
        std::mt19937 random{ seed };
        std::string text{};

        while (text.size() < size)
        {
            text += "<Note time=\"" + std::to_string(random() % 512) + "\" key=\"" + std::to_string(36 + random() % 48) +
                "\" vel=\"0." + std::to_string(random() % 1000) + "\"/>\r\n";
        }

        text.resize(size);

        return Bytes(text);
    }

    std::vector<uint8_t> Noise(_In_ size_t size, _In_ uint32_t seed)
    {
        std::mt19937 random{ seed };
        std::vector<uint8_t> bytes(size);

        for (auto& byte : bytes)
        {
            byte = static_cast<uint8_t>(random());
        }

        return bytes;
    }

    // MSZIP is deflate cut into pieces of at most 32 KB, each starting with "CK".
    bool WindowsCompress(_In_ std::vector<uint8_t> const& input, _Out_ std::vector<uint8_t>& deflate)
    {
        deflate.clear();

        COMPRESSOR_HANDLE compressor{};

        if (!::CreateCompressor(COMPRESS_ALGORITHM_MSZIP | COMPRESS_RAW, nullptr, &compressor))
        {
            return false;
        }

        std::vector<uint8_t> buffer(input.size() + 1024);
        SIZE_T written{ 0 };

        auto const compressed = ::Compress(compressor, input.data(), input.size(), buffer.data(), buffer.size(), &written);
        ::CloseCompressor(compressor);

        if (!compressed || written < 2 || buffer[0] != 'C' || buffer[1] != 'K')
        {
            return false;
        }

        deflate.assign(buffer.begin() + 2, buffer.begin() + static_cast<std::ptrdiff_t>(written));

        return true;
    }

    bool WindowsDecompress(_In_ std::vector<uint8_t> const& deflate, _In_ size_t size, _Out_ std::vector<uint8_t>& output)
    {
        output.assign(size, 0);

        DECOMPRESSOR_HANDLE decompressor{};

        if (!::CreateDecompressor(COMPRESS_ALGORITHM_MSZIP | COMPRESS_RAW, nullptr, &decompressor))
        {
            return false;
        }

        std::vector<uint8_t> input{ 'C', 'K' };
        input.insert(input.end(), deflate.begin(), deflate.end());

        SIZE_T written{ 0 };

        auto const decompressed = ::Decompress(decompressor, input.data(), input.size(), output.data(), size, &written);
        ::CloseDecompressor(decompressor);

        return decompressed && written == size;
    }

    // Bits for a hand-made stream: values fill each byte from its low end, prefix codes go
    // in from their first bit.
    class BitStream
    {
    public:
        void Put(_In_ uint32_t value, _In_ int count)
        {
            for (int bit = 0; bit < count; ++bit)
            {
                PutBit((value >> bit) & 1u);
            }
        }

        void PutCode(_In_ uint32_t code, _In_ int length)
        {
            for (int bit = length - 1; bit >= 0; --bit)
            {
                PutBit((code >> bit) & 1u);
            }
        }

        std::vector<uint8_t> const& Bytes() const noexcept { return m_bytes; }

    private:
        void PutBit(_In_ uint32_t bit)
        {
            if (m_used == 0)
            {
                m_bytes.push_back(0);
            }

            m_bytes.back() = static_cast<uint8_t>(m_bytes.back() | (bit << m_used));
            m_used = (m_used + 1) % 8;
        }

        std::vector<uint8_t> m_bytes{};
        int m_used{ 0 };
    };

    std::vector<uint8_t> DeflateInPieces(_In_ std::vector<uint8_t> const& input, _In_ uint32_t seed)
    {
        std::mt19937 random{ seed };

        midiapp::DeflateEncoder encoder{};
        std::vector<uint8_t> output{};

        size_t at{ 0 };

        while (at < input.size())
        {
            auto const piece = std::min<size_t>(input.size() - at, random() % 3 == 0 ? 1 + random() % 7 : 1 + random() % 200000);

            VERIFY_IS_TRUE(encoder.Write(input.data() + at, piece, false, output));
            at += piece;
        }

        VERIFY_IS_TRUE(encoder.Write(nullptr, 0, true, output));
        VERIFY_IS_FALSE(encoder.Write(nullptr, 0, true, output));

        return output;
    }

    size_t FindSignature(_In_ std::vector<uint8_t> const& zip, _In_ uint32_t signature, _In_ size_t from)
    {
        for (auto at = from; at + 4 <= zip.size(); ++at)
        {
            if ((zip[at] | (zip[at + 1] << 8) | (zip[at + 2] << 16) | (static_cast<uint32_t>(zip[at + 3]) << 24)) == signature)
            {
                return at;
            }
        }

        return zip.size();
    }

    void Put32At(_Inout_ std::vector<uint8_t>& zip, _In_ size_t at, _In_ uint32_t value)
    {
        for (int i = 0; i < 4; ++i)
        {
            zip[at + i] = static_cast<uint8_t>(value >> (8 * i));
        }
    }

    ZipStatus OpenStatus(_In_ std::vector<uint8_t> const& zip, _In_ uint32_t maximumEntries = 16)
    {
        midiapp::ZipReader reader{};
        return reader.Open(zip, maximumEntries);
    }

    constexpr uint32_t CentralHeaderSignature{ 0x02014b50 };

    std::filesystem::path TestFolder()
    {
        wchar_t temp[MAX_PATH + 1]{};
        ::GetTempPathW(ARRAYSIZE(temp), temp);

        // A folder name no ANSI code page can show, like the paths Windows tar couldn't open.
        return std::filesystem::path{ temp } / L"midi-zip-tests" / L"\u65E5\u672C \U0001F3B9";
    }

    struct RemoveTestFolder
    {
        ~RemoveTestFolder()
        {
            std::error_code ignored{};
            std::filesystem::remove_all(TestFolder().parent_path(), ignored);
        }
    };
}

// ---------------------------------------------------------------- deflate

void ZipArchiveTests::DeflateRoundTrips()
{
    std::vector<std::vector<uint8_t>> const inputs{
        {},
        Bytes("a"),
        Repeated("hello ", 300),
        Text(32768, 1),
        Text(1500000, 2),
        Noise(70000, 3),
        std::vector<uint8_t>(3000000, 0),
    };

    for (auto const& input : inputs)
    {
        std::vector<uint8_t> packed{};
        VERIFY_IS_TRUE(midiapp::Deflate(input.data(), input.size(), packed));

        std::vector<uint8_t> unpacked{};
        VERIFY_IS_TRUE(midiapp::Inflate(packed.data(), packed.size(), input.size(), unpacked));
        VERIFY_IS_TRUE(unpacked == input);

        // How the input is cut into pieces doesn't change what comes back out.
        auto const pieces = DeflateInPieces(input, static_cast<uint32_t>(input.size()));

        VERIFY_IS_TRUE(midiapp::Inflate(pieces.data(), pieces.size(), input.size(), unpacked));
        VERIFY_IS_TRUE(unpacked == input);
    }

    // Text shrinks, and noise doesn't grow by more than the stored blocks' headers.
    std::vector<uint8_t> packed{};
    auto const text = Text(1500000, 2);
    VERIFY_IS_TRUE(midiapp::Deflate(text.data(), text.size(), packed));
    VERIFY_IS_LESS_THAN(packed.size(), text.size() / 4);

    auto const noise = Noise(70000, 3);
    VERIFY_IS_TRUE(midiapp::Deflate(noise.data(), noise.size(), packed));
    VERIFY_IS_LESS_THAN(packed.size(), noise.size() + 64);
}

void ZipArchiveTests::WindowsReadsWhatWeCompress()
{
    for (auto const& input : { Bytes("a"), Repeated("hello ", 300), Text(20000, 4), Noise(5000, 5), std::vector<uint8_t>(32768, 7) })
    {
        std::vector<uint8_t> packed{};
        VERIFY_IS_TRUE(midiapp::Deflate(input.data(), input.size(), packed));

        std::vector<uint8_t> unpacked{};
        VERIFY_IS_TRUE(WindowsDecompress(packed, input.size(), unpacked));
        VERIFY_IS_TRUE(unpacked == input);
    }
}

void ZipArchiveTests::WeReadWhatWindowsCompresses()
{
    for (auto const& input : { Bytes("a"), Repeated("hello ", 300), Text(20000, 6), Noise(5000, 7), std::vector<uint8_t>(32768, 7) })
    {
        std::vector<uint8_t> packed{};
        VERIFY_IS_TRUE(WindowsCompress(input, packed));

        std::vector<uint8_t> unpacked{};
        VERIFY_IS_TRUE(midiapp::Inflate(packed.data(), packed.size(), input.size(), unpacked));
        VERIFY_IS_TRUE(unpacked == input);
    }
}

void ZipArchiveTests::DamagedStreamsAreRefused()
{
    std::vector<uint8_t> output{};

    // A block type that doesn't exist.
    VERIFY_IS_FALSE(midiapp::Inflate(std::vector<uint8_t>{ 0x07 }.data(), 1, 0, output));

    // A stored block whose length check doesn't match.
    std::vector<uint8_t> const badStored{ 0x01, 0x03, 0x00, 0x00, 0x00, 'a', 'b', 'c' };
    VERIFY_IS_FALSE(midiapp::Inflate(badStored.data(), badStored.size(), 3, output));

    // The same block done right, then asked for the wrong size.
    std::vector<uint8_t> const goodStored{ 0x01, 0x03, 0x00, 0xFC, 0xFF, 'a', 'b', 'c' };
    VERIFY_IS_TRUE(midiapp::Inflate(goodStored.data(), goodStored.size(), 3, output));
    VERIFY_IS_FALSE(midiapp::Inflate(goodStored.data(), goodStored.size(), 2, output));
    VERIFY_IS_TRUE(output.empty());
    VERIFY_IS_FALSE(midiapp::Inflate(goodStored.data(), goodStored.size(), 4, output));

    // A copy from before the start: fixed codes, length 3 (code 257), distance 1, end of block.
    BitStream reachesBack{};
    reachesBack.Put(1, 1);
    reachesBack.Put(1, 2);
    reachesBack.PutCode(0x01, 7);
    reachesBack.PutCode(0x00, 5);
    reachesBack.PutCode(0x00, 7);
    VERIFY_IS_FALSE(midiapp::Inflate(reachesBack.Bytes().data(), reachesBack.Bytes().size(), 3, output));

    // A dynamic block asking for more literal codes than deflate has.
    BitStream tooManyCodes{};
    tooManyCodes.Put(1, 1);
    tooManyCodes.Put(2, 2);
    tooManyCodes.Put(30, 5);
    tooManyCodes.Put(0, 5);
    tooManyCodes.Put(0, 4);
    tooManyCodes.Put(0, 16);
    VERIFY_IS_FALSE(midiapp::Inflate(tooManyCodes.Bytes().data(), tooManyCodes.Bytes().size(), 1, output));

    // "Repeat the previous length" before there is one.
    BitStream repeatFirst{};
    repeatFirst.Put(1, 1);
    repeatFirst.Put(2, 2);
    repeatFirst.Put(0, 5);
    repeatFirst.Put(0, 5);
    repeatFirst.Put(0, 4);
    repeatFirst.Put(1, 3);
    repeatFirst.Put(0, 3);
    repeatFirst.Put(0, 3);
    repeatFirst.Put(1, 3);
    repeatFirst.PutCode(1, 1);
    repeatFirst.Put(0, 2);
    repeatFirst.Put(0, 16);
    VERIFY_IS_FALSE(midiapp::Inflate(repeatFirst.Bytes().data(), repeatFirst.Bytes().size(), 1, output));

    // Cut short.
    auto const text = Text(50000, 8);
    std::vector<uint8_t> packed{};
    VERIFY_IS_TRUE(midiapp::Deflate(text.data(), text.size(), packed));
    VERIFY_IS_FALSE(midiapp::Inflate(packed.data(), packed.size() / 2, text.size(), output));

    // Random damage either fails or still gives exactly the size asked for, and never crashes.
    std::mt19937 random{ 9 };

    for (int attempt = 0; attempt < 2000; ++attempt)
    {
        auto damaged = packed;

        for (int flip = 0; flip < 1 + attempt % 4; ++flip)
        {
            damaged[random() % damaged.size()] = static_cast<uint8_t>(random());
        }

        if (midiapp::Inflate(damaged.data(), damaged.size(), text.size(), output))
        {
            VERIFY_ARE_EQUAL(text.size(), output.size());
        }
    }
}

// ---------------------------------------------------------------- zips

void ZipArchiveTests::TheStoredWriterMakesTheSameBytesAsBefore()
{
    // What the stored-only writer made before the shared zip code replaced it.
    std::vector<uint8_t> const before{
        0x50, 0x4B, 0x03, 0x04, 0x14, 0x00, 0x00, 0x08, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x86, 0xA6,
        0x10, 0x36, 0x05, 0x00, 0x00, 0x00, 0x05, 0x00, 0x00, 0x00, 0x0B, 0x00, 0x00, 0x00, 0x47, 0x72,
        0xC3, 0xBC, 0xC3, 0x9F, 0x65, 0x2E, 0x70, 0x6E, 0x67, 0x68, 0x65, 0x6C, 0x6C, 0x6F, 0x50, 0x4B,
        0x03, 0x04, 0x14, 0x00, 0x00, 0x08, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x09, 0x00, 0x00, 0x00, 0x65, 0x6D, 0x70, 0x74,
        0x79, 0x2E, 0x74, 0x78, 0x74, 0x50, 0x4B, 0x03, 0x04, 0x14, 0x00, 0x00, 0x08, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0xBB, 0x54, 0xFB, 0x01, 0x0C, 0x00, 0x00, 0x00, 0x0C, 0x00, 0x00, 0x00, 0x11,
        0x00, 0x00, 0x00, 0x73, 0x75, 0x62, 0x2F, 0x64, 0x69, 0x72, 0x2F, 0x6E, 0x6F, 0x74, 0x65, 0x73,
        0x2E, 0x74, 0x78, 0x74, 0x73, 0x6F, 0x6D, 0x65, 0x20, 0x6E, 0x6F, 0x74, 0x65, 0x73, 0x0D, 0x0A,
        0x50, 0x4B, 0x01, 0x02, 0x14, 0x00, 0x14, 0x00, 0x00, 0x08, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x86, 0xA6, 0x10, 0x36, 0x05, 0x00, 0x00, 0x00, 0x05, 0x00, 0x00, 0x00, 0x0B, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x47, 0x72,
        0xC3, 0xBC, 0xC3, 0x9F, 0x65, 0x2E, 0x70, 0x6E, 0x67, 0x50, 0x4B, 0x01, 0x02, 0x14, 0x00, 0x14,
        0x00, 0x00, 0x08, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x09, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x2E, 0x00, 0x00, 0x00, 0x65, 0x6D, 0x70, 0x74, 0x79, 0x2E, 0x74, 0x78, 0x74,
        0x50, 0x4B, 0x01, 0x02, 0x14, 0x00, 0x14, 0x00, 0x00, 0x08, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0xBB, 0x54, 0xFB, 0x01, 0x0C, 0x00, 0x00, 0x00, 0x0C, 0x00, 0x00, 0x00, 0x11, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x55, 0x00, 0x00, 0x00, 0x73, 0x75,
        0x62, 0x2F, 0x64, 0x69, 0x72, 0x2F, 0x6E, 0x6F, 0x74, 0x65, 0x73, 0x2E, 0x74, 0x78, 0x74, 0x50,
        0x4B, 0x05, 0x06, 0x00, 0x00, 0x00, 0x00, 0x03, 0x00, 0x03, 0x00, 0xAF, 0x00, 0x00, 0x00, 0x90,
        0x00, 0x00, 0x00, 0x00, 0x00,
    };

    std::vector<ZipEntry> const entries{
        ZipEntry{ L"Gr\u00FC\u00DFe.png", Bytes("hello") },
        ZipEntry{ L"empty.txt", {} },
        ZipEntry{ L"sub/dir/notes.txt", Bytes("some notes\r\n") },
    };

    VERIFY_IS_TRUE(midiapp::BuildStoredZip(entries) == before);
}

void ZipArchiveTests::ADeflatedZipRoundTrips()
{
    std::vector<ZipEntry> const entries{
        ZipEntry{ L"project.xml", Text(400000, 10) },
        ZipEntry{ L"Gr\u00FC\u00DFe/empty.txt", {} },
        ZipEntry{ L"audio/noise.bin", Noise(40000, 11) },
    };

    auto const zip = midiapp::BuildZip(entries, ZipCompression::Deflate);
    VERIFY_IS_FALSE(zip.empty());
    VERIFY_IS_LESS_THAN(zip.size(), size_t{ 200000 });

    midiapp::ZipReader reader{};
    VERIFY_IS_TRUE(reader.Open(zip, 16) == ZipStatus::Read);
    VERIFY_ARE_EQUAL(entries.size(), reader.Items().size());

    for (size_t i = 0; i < entries.size(); ++i)
    {
        std::vector<uint8_t> bytes{};

        VERIFY_ARE_EQUAL(entries[i].Name, reader.Items()[i].Name);
        VERIFY_IS_TRUE(reader.Extract(reader.Items()[i], 1024 * 1024, bytes) == ZipStatus::Read);
        VERIFY_IS_TRUE(bytes == entries[i].Bytes);
    }

    // A signed pack still takes stored files only.
    std::vector<ZipEntry> read{};
    VERIFY_IS_TRUE(midiapp::ReadStoredZip(zip, 16, read) == ZipStatus::Compressed);
}

void ZipArchiveTests::ZipsFromOtherToolsOpen()
{
    // Windows tar: deflated, and the checksum and sizes come after the data.
    std::vector<uint8_t> const fromTar{
        0x50, 0x4B, 0x03, 0x04, 0x14, 0x00, 0x08, 0x00, 0x08, 0x00, 0x78, 0x6A, 0x4A, 0x5D, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x09, 0x00, 0x20, 0x00, 0x6E, 0x6F,
        0x74, 0x65, 0x73, 0x2E, 0x74, 0x78, 0x74, 0x75, 0x78, 0x0B, 0x00, 0x01, 0x04, 0x00, 0x00, 0x00,
        0x00, 0x04, 0x00, 0x00, 0x00, 0x00, 0x55, 0x54, 0x0D, 0x00, 0x07, 0xB4, 0x73, 0xCA, 0x6A, 0xB4,
        0x73, 0xCA, 0x6A, 0xB4, 0x73, 0xCA, 0x6A, 0xF3, 0xCB, 0x2F, 0x49, 0x2D, 0x56, 0x48, 0x2B, 0xCA,
        0xCF, 0x55, 0x28, 0x49, 0x2C, 0xE2, 0xE5, 0xF2, 0x1B, 0xE5, 0x93, 0xC4, 0x07, 0x00, 0x50, 0x4B,
        0x07, 0x08, 0x9F, 0xCA, 0x49, 0x6A, 0x17, 0x00, 0x00, 0x00, 0x40, 0x01, 0x00, 0x00, 0x50, 0x4B,
        0x01, 0x02, 0x14, 0x03, 0x14, 0x00, 0x08, 0x00, 0x08, 0x00, 0x78, 0x6A, 0x4A, 0x5D, 0x9F, 0xCA,
        0x49, 0x6A, 0x17, 0x00, 0x00, 0x00, 0x40, 0x01, 0x00, 0x00, 0x09, 0x00, 0x18, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xB6, 0x81, 0x00, 0x00, 0x00, 0x00, 0x6E, 0x6F, 0x74, 0x65,
        0x73, 0x2E, 0x74, 0x78, 0x74, 0x75, 0x78, 0x0B, 0x00, 0x01, 0x04, 0x00, 0x00, 0x00, 0x00, 0x04,
        0x00, 0x00, 0x00, 0x00, 0x55, 0x54, 0x05, 0x00, 0x01, 0xB4, 0x73, 0xCA, 0x6A, 0x50, 0x4B, 0x05,
        0x06, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x01, 0x00, 0x4F, 0x00, 0x00, 0x00, 0x6E, 0x00, 0x00,
        0x00, 0x00, 0x00,
    };

    // .NET: deflated, with a UTF-8 folder name.
    std::vector<uint8_t> const fromDotNet{
        0x50, 0x4B, 0x03, 0x04, 0x14, 0x00, 0x00, 0x08, 0x08, 0x00, 0x78, 0x6A, 0x4A, 0x5D, 0xF5, 0x4A,
        0xCB, 0x69, 0x24, 0x00, 0x00, 0x00, 0x5E, 0x01, 0x00, 0x00, 0x13, 0x00, 0x00, 0x00, 0x47, 0x72,
        0xC3, 0xBC, 0xC3, 0x9F, 0x65, 0x2F, 0x70, 0x72, 0x6F, 0x6A, 0x65, 0x63, 0x74, 0x2E, 0x78, 0x6D,
        0x6C, 0xB3, 0x09, 0x28, 0xCA, 0xCF, 0x4A, 0x4D, 0x2E, 0xB1, 0xB3, 0xF1, 0xCB, 0x2F, 0x49, 0x55,
        0xC8, 0x4E, 0xAD, 0xB4, 0x55, 0x32, 0x33, 0x50, 0xD2, 0xB7, 0xB3, 0xD1, 0x87, 0xCB, 0x8C, 0x2A,
        0x51, 0x20, 0x33, 0x5C, 0x00, 0x50, 0x4B, 0x01, 0x02, 0x14, 0x00, 0x14, 0x00, 0x00, 0x08, 0x08,
        0x00, 0x78, 0x6A, 0x4A, 0x5D, 0xF5, 0x4A, 0xCB, 0x69, 0x24, 0x00, 0x00, 0x00, 0x5E, 0x01, 0x00,
        0x00, 0x13, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x47, 0x72, 0xC3, 0xBC, 0xC3, 0x9F, 0x65, 0x2F, 0x70, 0x72, 0x6F, 0x6A, 0x65,
        0x63, 0x74, 0x2E, 0x78, 0x6D, 0x6C, 0x50, 0x4B, 0x05, 0x06, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00,
        0x01, 0x00, 0x41, 0x00, 0x00, 0x00, 0x55, 0x00, 0x00, 0x00, 0x00, 0x00,
    };

    struct Expected
    {
        std::vector<uint8_t> const& Zip;
        std::wstring Name;
        std::vector<uint8_t> Bytes;
    };

    std::vector<Expected> const zips{
        Expected{ fromTar, L"notes.txt", Repeated("Notes from tar\r\n", 20) },
        Expected{ fromDotNet, L"Gr\u00FC\u00DFe/project.xml", Repeated("<Project><Note key=\"60\"/></Project>", 10) },
    };

    for (auto const& expected : zips)
    {
        midiapp::ZipReader reader{};
        VERIFY_IS_TRUE(reader.Open(expected.Zip, 16) == ZipStatus::Read);
        VERIFY_ARE_EQUAL(size_t{ 1 }, reader.Items().size());
        VERIFY_ARE_EQUAL(expected.Name, reader.Items()[0].Name);

        std::vector<uint8_t> bytes{};
        VERIFY_IS_TRUE(reader.Extract(reader.Items()[0], 4096, bytes) == ZipStatus::Read);
        VERIFY_IS_TRUE(bytes == expected.Bytes);
    }
}

void ZipArchiveTests::OldCodePageNamesAreRead()
{
    // Without the UTF-8 flag, a name that isn't UTF-8 is in the old IBM PC code page:
    // 0x81 is u with an umlaut there, and 0xE1 is sharp s.
    auto zip = midiapp::BuildZip({ ZipEntry{ L"Gr__e.txt", Bytes("x") } }, ZipCompression::Store);
    std::string_view const placeholder{ "Gr__e.txt" };

    for (auto at = std::search(zip.begin(), zip.end(), placeholder.begin(), placeholder.end());
        at != zip.end();
        at = std::search(at + 1, zip.end(), placeholder.begin(), placeholder.end()))
    {
        at[2] = 0x81;
        at[3] = 0xE1;
    }

    zip[7] = 0;

    auto const central = FindSignature(zip, CentralHeaderSignature, 0);
    zip[central + 9] = 0;

    midiapp::ZipReader reader{};
    VERIFY_IS_TRUE(reader.Open(zip, 16) == ZipStatus::Read);
    VERIFY_ARE_EQUAL(std::wstring{ L"Gr\u00FC\u00DFe.txt" }, reader.Items()[0].Name);
}

void ZipArchiveTests::HostileZipsAreRefused()
{
    auto const zip = midiapp::BuildZip(
        { ZipEntry{ L"a.txt", Text(1000, 12) }, ZipEntry{ L"b.txt", Text(1000, 13) } },
        ZipCompression::Deflate);

    auto const firstCentral = FindSignature(zip, CentralHeaderSignature, 0);
    auto const secondCentral = FindSignature(zip, CentralHeaderSignature, firstCentral + 4);
    VERIFY_IS_LESS_THAN(secondCentral, zip.size());

    VERIFY_IS_TRUE(OpenStatus(zip) == ZipStatus::Read);
    VERIFY_IS_TRUE(OpenStatus(zip, 1) == ZipStatus::TooManyEntries);
    VERIFY_IS_TRUE(OpenStatus(Bytes("not a zip at all, just some words")) == ZipStatus::NotAZip);
    VERIFY_IS_TRUE(OpenStatus({}) == ZipStatus::NotAZip);
    VERIFY_IS_TRUE(OpenStatus(std::vector<uint8_t>{ zip.begin(), zip.end() - 1 }) == ZipStatus::Damaged);

    auto encrypted = zip;
    encrypted[firstCentral + 8] = static_cast<uint8_t>(encrypted[firstCentral + 8] | 0x01);
    VERIFY_IS_TRUE(OpenStatus(encrypted) == ZipStatus::Unsupported);

    auto otherMethod = zip;
    otherMethod[firstCentral + 10] = 12;
    VERIFY_IS_TRUE(OpenStatus(otherMethod) == ZipStatus::Unsupported);

    auto zip64 = zip;
    Put32At(zip64, firstCentral + 20, 0xFFFFFFFFu);
    VERIFY_IS_TRUE(OpenStatus(zip64) == ZipStatus::Unsupported);

    auto pastTheDirectory = zip;
    Put32At(pastTheDirectory, firstCentral + 20, static_cast<uint32_t>(zip.size()));
    VERIFY_IS_TRUE(OpenStatus(pastTheDirectory) == ZipStatus::Damaged);

    auto otherName = zip;
    otherName[secondCentral + 46] = 'c';
    VERIFY_IS_TRUE(OpenStatus(otherName) == ZipStatus::DirectoryMismatch);

    // Two directory entries pointing at the same bytes, the way some zip bombs count one block many times.
    auto twins = midiapp::BuildZip(
        { ZipEntry{ L"same.txt", Text(1000, 14) }, ZipEntry{ L"same.txt", Text(1000, 14) } },
        ZipCompression::Deflate);
    auto const twinCentral = FindSignature(twins, CentralHeaderSignature, FindSignature(twins, CentralHeaderSignature, 0) + 4);
    Put32At(twins, twinCentral + 42, 0);
    VERIFY_IS_TRUE(OpenStatus(twins) == ZipStatus::Damaged);

    // A checksum that doesn't match is caught when the file is read.
    auto wrongChecksum = zip;
    wrongChecksum[firstCentral + 16] = static_cast<uint8_t>(wrongChecksum[firstCentral + 16] ^ 0xFF);

    midiapp::ZipReader reader{};
    VERIFY_IS_TRUE(reader.Open(wrongChecksum, 16) == ZipStatus::Read);

    std::vector<uint8_t> bytes{};
    VERIFY_IS_TRUE(reader.Extract(reader.Items()[0], 4096, bytes) == ZipStatus::Damaged);
    VERIFY_IS_TRUE(bytes.empty());
    VERIFY_IS_TRUE(reader.Extract(reader.Items()[1], 4096, bytes) == ZipStatus::Read);
    VERIFY_IS_TRUE(reader.Extract(reader.Items()[1], 999, bytes) == ZipStatus::TooLarge);
}

void ZipArchiveTests::AZipFileCanHaveAnyName()
{
    auto const folder = TestFolder();
    RemoveTestFolder const removeTestFolder{};

    std::error_code ignored{};
    std::filesystem::remove_all(folder, ignored);
    VERIFY_IS_TRUE(std::filesystem::create_directories(folder));

    auto const source = folder / L"\u30EC\u30DD\u30FC\u30C8.txt";
    auto const text = Text(300000, 15);

    {
        std::ofstream file{ source, std::ios::binary };
        file.write(reinterpret_cast<char const*>(text.data()), static_cast<std::streamsize>(text.size()));
    }

    auto const zipPath = folder / L"\u4FDD\u5B58.zip";

    midiapp::ZipWriter writer{};
    VERIFY_IS_TRUE(writer.Create(zipPath) == ZipStatus::Written);
    VERIFY_IS_TRUE(writer.AddFile(L"logs\\\u30EC\u30DD\u30FC\u30C8.txt", source, ZipCompression::Deflate) == ZipStatus::Written);
    VERIFY_IS_TRUE(writer.AddBytes(L"summary.txt", Bytes("summary"), ZipCompression::Store) == ZipStatus::Written);
    VERIFY_IS_TRUE(writer.AddFile(L"missing.txt", folder / L"not here.txt", ZipCompression::Deflate) == ZipStatus::CannotOpen);
    VERIFY_IS_TRUE(writer.Finish() == ZipStatus::Written);

    VERIFY_IS_TRUE(std::filesystem::exists(zipPath));
    VERIFY_IS_FALSE(std::filesystem::exists(std::filesystem::path{ zipPath }.concat(L".partial")));

    midiapp::ZipReader reader{};
    VERIFY_IS_TRUE(reader.OpenFile(zipPath, 16) == ZipStatus::Read);
    VERIFY_ARE_EQUAL(size_t{ 2 }, reader.Items().size());
    VERIFY_ARE_EQUAL(std::wstring{ L"logs/\u30EC\u30DD\u30FC\u30C8.txt" }, reader.Items()[0].Name);

    std::vector<uint8_t> bytes{};
    VERIFY_IS_TRUE(reader.Extract(reader.Items()[0], text.size(), bytes) == ZipStatus::Read);
    VERIFY_IS_TRUE(bytes == text);
    VERIFY_IS_TRUE(reader.Extract(reader.Items()[1], 100, bytes) == ZipStatus::Read);
    VERIFY_IS_TRUE(bytes == Bytes("summary"));
}

void ZipArchiveTests::AnUnfinishedZipLeavesNothingBehind()
{
    auto const folder = TestFolder();
    RemoveTestFolder const removeTestFolder{};

    std::error_code ignored{};
    std::filesystem::create_directories(folder, ignored);

    auto const zipPath = folder / L"unfinished.zip";

    {
        midiapp::ZipWriter writer{};
        VERIFY_IS_TRUE(writer.Create(zipPath) == ZipStatus::Written);
        VERIFY_IS_TRUE(writer.AddBytes(L"a.txt", Bytes("a"), ZipCompression::Deflate) == ZipStatus::Written);
    }

    VERIFY_IS_FALSE(std::filesystem::exists(zipPath));
    VERIFY_IS_FALSE(std::filesystem::exists(std::filesystem::path{ zipPath }.concat(L".partial")));
}

void ZipArchiveTests::NamesThatCouldClimbOutAreRefused()
{
    auto const folder = TestFolder();
    RemoveTestFolder const removeTestFolder{};

    std::error_code ignored{};
    std::filesystem::create_directories(folder, ignored);

    midiapp::ZipWriter writer{};
    VERIFY_IS_TRUE(writer.Create(folder / L"names.zip") == ZipStatus::Written);

    for (auto const* name : { L"", L"../up.txt", L"a/../../up.txt", L"/root.txt", L"C:/drive.txt", L"a//b.txt", L"a/./b.txt", L"folder/" })
    {
        VERIFY_IS_TRUE(writer.AddBytes(name, Bytes("x"), ZipCompression::Store) == ZipStatus::BadName, name);
    }

    VERIFY_IS_TRUE(writer.AddBytes(L"dir\\file.txt", Bytes("x"), ZipCompression::Store) == ZipStatus::Written);
    VERIFY_IS_TRUE(writer.AddBytes(L"dir/file.txt", Bytes("y"), ZipCompression::Store) == ZipStatus::BadName);
    VERIFY_IS_TRUE(writer.Finish() == ZipStatus::Written);

    midiapp::ZipReader reader{};
    VERIFY_IS_TRUE(reader.OpenFile(folder / L"names.zip", 16) == ZipStatus::Read);
    VERIFY_ARE_EQUAL(size_t{ 1 }, reader.Items().size());
    VERIFY_ARE_EQUAL(std::wstring{ L"dir/file.txt" }, reader.Items()[0].Name);
}
