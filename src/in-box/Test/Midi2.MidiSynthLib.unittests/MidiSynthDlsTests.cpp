// Copyright (c) Microsoft Corporation. All rights reserved.

#include "pch.h"

#include "MidiSynthDlsTests.h"

using namespace WEX::Common;
using namespace WEX::Logging;
using namespace MidiSynth;

namespace
{
    // Builds DLS files byte by byte, so the parser can be pointed at content a real sound set
    // would never contain.
    class DlsBuilder
    {
    public:
        std::vector<std::byte> Bytes;

        void U16(_In_ uint16_t value)
        {
            Bytes.push_back(static_cast<std::byte>(value & 0xFF));
            Bytes.push_back(static_cast<std::byte>((value >> 8) & 0xFF));
        }

        void U32(_In_ uint32_t value)
        {
            for (int shift = 0; shift < 32; shift += 8)
            {
                Bytes.push_back(static_cast<std::byte>((value >> shift) & 0xFF));
            }
        }

        void Tag(_In_ const char* fourCC)
        {
            for (size_t i = 0; i < 4; i++)
            {
                Bytes.push_back(static_cast<std::byte>(fourCC[i]));
            }
        }

        size_t BeginChunk(_In_ const char* id)
        {
            Tag(id);
            U32(0);

            return Bytes.size();
        }

        size_t BeginList(_In_ const char* listType)
        {
            Tag("LIST");
            U32(0);

            const size_t payloadStart = Bytes.size();

            Tag(listType);

            return payloadStart;
        }

        void EndChunk(_In_ size_t payloadStart)
        {
            const auto size = static_cast<uint32_t>(Bytes.size() - payloadStart);

            for (int shift = 0, i = 0; shift < 32; shift += 8, i++)
            {
                Bytes[payloadStart - 4 + i] = static_cast<std::byte>((size >> shift) & 0xFF);
            }

            if ((size & 1u) != 0)
            {
                Bytes.push_back(std::byte{ 0 });
            }
        }
    };

    struct MinimalFileOptions
    {
        uint16_t FormatTag{ 1 };
        uint16_t Channels{ 1 };
        uint16_t BitsPerSample{ 16 };
        uint32_t SampleDataBytes{ 64 };
        uint32_t WaveTableIndex{ 0 };
        uint32_t PoolCueCount{ 1 };
    };

    // One instrument, one region, one wave: the smallest thing the parser will accept.
    std::vector<std::byte> BuildMinimalDls(_In_ MinimalFileOptions const& options)
    {
        DlsBuilder builder;

        builder.Tag("RIFF");
        builder.U32(0);

        const size_t riffPayload = builder.Bytes.size();

        builder.Tag("DLS ");

        const size_t colh = builder.BeginChunk("colh");
        builder.U32(1);
        builder.EndChunk(colh);

        const size_t lins = builder.BeginList("lins");
        const size_t ins = builder.BeginList("ins ");

        const size_t insh = builder.BeginChunk("insh");
        builder.U32(1);
        builder.U32(0);
        builder.U32(0);
        builder.EndChunk(insh);

        const size_t lrgn = builder.BeginList("lrgn");
        const size_t rgn = builder.BeginList("rgn ");

        const size_t rgnh = builder.BeginChunk("rgnh");
        builder.U16(0);
        builder.U16(127);
        builder.U16(0);
        builder.U16(127);
        builder.U16(0);
        builder.U16(0);
        builder.EndChunk(rgnh);

        const size_t wlnk = builder.BeginChunk("wlnk");
        builder.U16(0);
        builder.U16(0);
        builder.U32(0);
        builder.U32(options.WaveTableIndex);
        builder.EndChunk(wlnk);

        builder.EndChunk(rgn);
        builder.EndChunk(lrgn);
        builder.EndChunk(ins);
        builder.EndChunk(lins);

        const size_t ptbl = builder.BeginChunk("ptbl");
        builder.U32(8);
        builder.U32(options.PoolCueCount);

        for (uint32_t i = 0; i < options.PoolCueCount; i++)
        {
            builder.U32(0);
        }

        builder.EndChunk(ptbl);

        const size_t wvpl = builder.BeginList("wvpl");
        const size_t wave = builder.BeginList("wave");

        const uint32_t bytesPerFrame =
            static_cast<uint32_t>(options.Channels) * (options.BitsPerSample / 8u);

        const size_t fmt = builder.BeginChunk("fmt ");
        builder.U16(options.FormatTag);
        builder.U16(options.Channels);
        builder.U32(44100);
        builder.U32(44100u * bytesPerFrame);
        builder.U16(static_cast<uint16_t>(bytesPerFrame));
        builder.U16(options.BitsPerSample);
        builder.EndChunk(fmt);

        const size_t data = builder.BeginChunk("data");

        for (uint32_t i = 0; i < options.SampleDataBytes; i++)
        {
            builder.Bytes.push_back(std::byte{ 0x11 });
        }

        builder.EndChunk(data);

        builder.EndChunk(wave);
        builder.EndChunk(wvpl);
        builder.EndChunk(riffPayload);

        return builder.Bytes;
    }


    DlsParseStatus Load(_In_ std::vector<std::byte> bytes, _Out_ DlsCollection& collection)
    {
        DlsParseLimits limits{};

        return DlsCollection::LoadFromMemory(std::move(bytes), limits, collection);
    }


    DlsParseStatus LoadOptions(_In_ MinimalFileOptions const& options)
    {
        DlsCollection collection;

        return Load(BuildMinimalDls(options), collection);
    }
}


// The baseline has to parse, or every rejection below would prove nothing.
void MidiSynthDlsTests::TestMinimalValidFileIsAccepted()
{
    DlsCollection collection;

    const auto status = Load(BuildMinimalDls(MinimalFileOptions{}), collection);

    VERIFY_ARE_EQUAL(static_cast<int>(DlsParseStatus::Ok), static_cast<int>(status),
        String().Format(L"status was %S", DlsParseStatusToString(status)));

    VERIFY_ARE_EQUAL(static_cast<size_t>(1), collection.Waves().size());
    VERIFY_ARE_EQUAL(static_cast<size_t>(1), collection.Instruments().size());
}


// The regression this suite exists for.
//
// The renderer reads sample data through an int16_t pointer, using a frame count derived from
// BitsPerSample. For 8-bit mono that frame count equals the byte count, so playback read two bytes
// per frame from a buffer holding one: a heap over-read of the whole chunk length.
void MidiSynthDlsTests::TestEightBitMonoIsRejected()
{
    MinimalFileOptions options{};
    options.BitsPerSample = 8;
    options.Channels = 1;

    const auto status = LoadOptions(options);

    VERIFY_ARE_EQUAL(static_cast<int>(DlsParseStatus::UnsupportedWaveFormat), static_cast<int>(status),
        String().Format(L"status was %S", DlsParseStatusToString(status)));
}


void MidiSynthDlsTests::TestUnsupportedBitDepthsAreRejected()
{
    // 24 and 32 bit stay within the buffer but would be read as the wrong samples, and 12 is not a
    // whole number of bytes at all. The renderer supports exactly one format, so it accepts one.
    for (uint16_t bits : { uint16_t{ 4 }, uint16_t{ 12 }, uint16_t{ 24 }, uint16_t{ 32 } })
    {
        MinimalFileOptions options{};
        options.BitsPerSample = bits;
        options.SampleDataBytes = 192;

        const auto status = LoadOptions(options);

        VERIFY_ARE_EQUAL(static_cast<int>(DlsParseStatus::UnsupportedWaveFormat), static_cast<int>(status),
            String().Format(L"%u bits per sample", bits));
    }
}


void MidiSynthDlsTests::TestNonPcmFormatIsRejected()
{
    // 3 is IEEE float, which is the same size as nothing the renderer knows how to read.
    MinimalFileOptions options{};
    options.FormatTag = 3;

    VERIFY_ARE_EQUAL(static_cast<int>(DlsParseStatus::UnsupportedWaveFormat),
        static_cast<int>(LoadOptions(options)));
}


void MidiSynthDlsTests::TestZeroChannelsIsRejected()
{
    // Zero channels would divide by zero when computing the frame count.
    MinimalFileOptions options{};
    options.Channels = 0;

    VERIFY_ARE_EQUAL(static_cast<int>(DlsParseStatus::UnsupportedWaveFormat),
        static_cast<int>(LoadOptions(options)));
}


// A chunk length is the parser's main lever for reading past the end, so every prefix of a valid
// file has to fail cleanly rather than run off the buffer.
void MidiSynthDlsTests::TestEveryTruncationIsRejected()
{
    const auto full = BuildMinimalDls(MinimalFileOptions{});

    for (size_t length = 1; length < full.size(); length++)
    {
        DlsCollection collection;
        std::vector<std::byte> truncated(full.begin(), full.begin() + length);

        const auto status = Load(std::move(truncated), collection);

        VERIFY_ARE_NOT_EQUAL(static_cast<int>(DlsParseStatus::Ok), static_cast<int>(status),
            String().Format(L"truncated to %zu bytes", length));
    }
}


void MidiSynthDlsTests::TestNotRiffIsRejected()
{
    auto bytes = BuildMinimalDls(MinimalFileOptions{});

    bytes[0] = static_cast<std::byte>('J');

    DlsCollection collection;

    VERIFY_ARE_EQUAL(static_cast<int>(DlsParseStatus::NotRiffFile),
        static_cast<int>(Load(std::move(bytes), collection)));
}


void MidiSynthDlsTests::TestWrongFormTypeIsRejected()
{
    auto bytes = BuildMinimalDls(MinimalFileOptions{});

    // The form type sits immediately after the RIFF id and size.
    bytes[8] = static_cast<std::byte>('W');
    bytes[9] = static_cast<std::byte>('A');
    bytes[10] = static_cast<std::byte>('V');
    bytes[11] = static_cast<std::byte>('E');

    DlsCollection collection;

    VERIFY_ARE_EQUAL(static_cast<int>(DlsParseStatus::NotDlsCollection),
        static_cast<int>(Load(std::move(bytes), collection)));
}


// A length that runs past the end of the file is the classic malformed-RIFF attack.
void MidiSynthDlsTests::TestOversizedChunkLengthIsRejected()
{
    auto bytes = BuildMinimalDls(MinimalFileOptions{});

    // Overwrite the outer RIFF length with a value far larger than the file.
    for (int shift = 0, i = 4; shift < 32; shift += 8, i++)
    {
        bytes[i] = static_cast<std::byte>((0x7FFFFFFFu >> shift) & 0xFF);
    }

    DlsCollection collection;

    VERIFY_ARE_NOT_EQUAL(static_cast<int>(DlsParseStatus::Ok),
        static_cast<int>(Load(std::move(bytes), collection)));
}


// The wave link index is read from the file and used to index the pool table.
void MidiSynthDlsTests::TestInvalidWaveIndexIsRejected()
{
    MinimalFileOptions options{};
    options.WaveTableIndex = 999;

    VERIFY_ARE_EQUAL(static_cast<int>(DlsParseStatus::InvalidWaveReference),
        static_cast<int>(LoadOptions(options)));
}
