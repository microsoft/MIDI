// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

// Reads SoundFont 2 files. The file is treated as hostile: every size, count and index it
// declares is checked before it is used, nothing is read past what the file has, and anything
// that does not make sense is dropped rather than guessed at.

#include "pch.h"

#include "SoundFont.h"

namespace SoundFontSynth
{
    namespace
    {
        constexpr uint32_t FourCc(char a, char b, char c, char d) noexcept
        {
            return static_cast<uint32_t>(static_cast<uint8_t>(a)) |
                (static_cast<uint32_t>(static_cast<uint8_t>(b)) << 8) |
                (static_cast<uint32_t>(static_cast<uint8_t>(c)) << 16) |
                (static_cast<uint32_t>(static_cast<uint8_t>(d)) << 24);
        }

        constexpr uint32_t IdRiff = FourCc('R', 'I', 'F', 'F');
        constexpr uint32_t IdList = FourCc('L', 'I', 'S', 'T');
        constexpr uint32_t IdSfbk = FourCc('s', 'f', 'b', 'k');
        constexpr uint32_t IdInfo = FourCc('I', 'N', 'F', 'O');
        constexpr uint32_t IdSdta = FourCc('s', 'd', 't', 'a');
        constexpr uint32_t IdPdta = FourCc('p', 'd', 't', 'a');

        constexpr uint32_t IdIfil = FourCc('i', 'f', 'i', 'l');
        constexpr uint32_t IdIsng = FourCc('i', 's', 'n', 'g');
        constexpr uint32_t IdInam = FourCc('I', 'N', 'A', 'M');
        constexpr uint32_t IdIeng = FourCc('I', 'E', 'N', 'G');
        constexpr uint32_t IdIcop = FourCc('I', 'C', 'O', 'P');
        constexpr uint32_t IdIcmt = FourCc('I', 'C', 'M', 'T');
        constexpr uint32_t IdIsft = FourCc('I', 'S', 'F', 'T');
        constexpr uint32_t IdIcrd = FourCc('I', 'C', 'R', 'D');
        constexpr uint32_t IdIprd = FourCc('I', 'P', 'R', 'D');

        constexpr uint32_t IdSmpl = FourCc('s', 'm', 'p', 'l');
        constexpr uint32_t IdSm24 = FourCc('s', 'm', '2', '4');

        constexpr uint32_t IdPhdr = FourCc('p', 'h', 'd', 'r');
        constexpr uint32_t IdPbag = FourCc('p', 'b', 'a', 'g');
        constexpr uint32_t IdPmod = FourCc('p', 'm', 'o', 'd');
        constexpr uint32_t IdPgen = FourCc('p', 'g', 'e', 'n');
        constexpr uint32_t IdInst = FourCc('i', 'n', 's', 't');
        constexpr uint32_t IdIbag = FourCc('i', 'b', 'a', 'g');
        constexpr uint32_t IdImod = FourCc('i', 'm', 'o', 'd');
        constexpr uint32_t IdIgen = FourCc('i', 'g', 'e', 'n');
        constexpr uint32_t IdShdr = FourCc('s', 'h', 'd', 'r');

        constexpr size_t PhdrRecordBytes = 38;
        constexpr size_t BagRecordBytes = 4;
        constexpr size_t ModRecordBytes = 10;
        constexpr size_t GenRecordBytes = 4;
        constexpr size_t InstRecordBytes = 22;
        constexpr size_t ShdrRecordBytes = 46;
        constexpr size_t NameBytes = 20;

        // Text in INFO is display only, so only this much of it is ever read.
        constexpr uint32_t MaximumInfoTextBytes = 4096;

        // Sample data is read in pieces, so a failed read part way through a large bank is reported
        // rather than leaving a huge request outstanding.
        constexpr size_t SampleReadChunkBytes = 16 * 1024 * 1024;

        // Sample rates outside this cannot be real audio, and a rate of zero would divide by zero.
        constexpr uint32_t MinimumSampleRate = 8;
        constexpr uint32_t MaximumSampleRate = 1000000;

        constexpr uint16_t SampleTypeCompressed = 0x0010;
        constexpr uint16_t SampleTypeRom = 0x8000;

        // Zones refer to samples and instruments with a 16 bit index, so nothing past this is reachable.
        constexpr size_t MaximumAddressableRecords = 65536;

        uint16_t ReadU16(_In_reads_(2) uint8_t const* p) noexcept
        {
            return static_cast<uint16_t>(p[0] | (p[1] << 8));
        }

        uint32_t ReadU32(_In_reads_(4) uint8_t const* p) noexcept
        {
            return static_cast<uint32_t>(p[0]) |
                (static_cast<uint32_t>(p[1]) << 8) |
                (static_cast<uint32_t>(p[2]) << 16) |
                (static_cast<uint32_t>(p[3]) << 24);
        }

        struct ChunkSpan
        {
            uint64_t Offset{ 0 };
            uint32_t Size{ 0 };
            bool Present{ false };
        };

        // Text in these files predates any agreed encoding. Valid UTF-8 is taken as UTF-8 and
        // anything else as Windows-1252, which is what the tools of the time wrote.
        std::wstring DecodeText(_In_reads_(length) uint8_t const* text, _In_ size_t length)
        {
            size_t used = 0;

            while (used < length && text[used] != 0)
            {
                used++;
            }

            while (used > 0 && text[used - 1] == ' ')
            {
                used--;
            }

            if (used == 0)
            {
                return {};
            }

            auto const* const narrow = reinterpret_cast<char const*>(text);
            auto const narrowLength = static_cast<int>(used);

            UINT codePage = CP_UTF8;
            int required = MultiByteToWideChar(codePage, MB_ERR_INVALID_CHARS, narrow, narrowLength, nullptr, 0);

            if (required <= 0)
            {
                codePage = 1252;
                required = MultiByteToWideChar(codePage, 0, narrow, narrowLength, nullptr, 0);
            }

            if (required <= 0)
            {
                return {};
            }

            std::wstring result(static_cast<size_t>(required), L'\0');

            if (MultiByteToWideChar(codePage, 0, narrow, narrowLength, result.data(), required) != required)
            {
                return {};
            }

            for (auto& character : result)
            {
                if (character < 0x20 || character == 0x7F)
                {
                    character = L' ';
                }
            }

            return result;
        }

        bool SourceOperandIsValid(_In_ uint16_t source) noexcept
        {
            // Curve types beyond switch are not defined.
            if ((source >> 10) > 3)
            {
                return false;
            }

            auto const index = static_cast<uint16_t>(source & 0x7F);

            if ((source & 0x80) != 0)
            {
                switch (index)
                {
                case 0:
                case 6:
                case 32:
                case 38:
                case 98:
                case 99:
                case 100:
                case 101:
                    return false;

                default:
                    return index < 120;
                }
            }

            // Linking one modulator to another is not supported, so those are dropped.
            switch (index)
            {
            case 0:
            case 2:
            case 3:
            case 10:
            case 13:
            case 14:
            case 16:
                return true;

            default:
                return false;
            }
        }

        bool ModulatorDestinationIsValid(_In_ uint16_t destination) noexcept
        {
            switch (destination)
            {
            case Gen::ModLfoToPitch:
            case Gen::VibLfoToPitch:
            case Gen::ModEnvToPitch:
            case Gen::InitialFilterFc:
            case Gen::InitialFilterQ:
            case Gen::ModLfoToFilterFc:
            case Gen::ModEnvToFilterFc:
            case Gen::ModLfoToVolume:
            case Gen::ChorusEffectsSend:
            case Gen::ReverbEffectsSend:
            case Gen::Pan:
            case Gen::InitialAttenuation:
            case Gen::CoarseTune:
            case Gen::FineTune:
            case Gen::InitialPitch:
                return true;

            default:
                return destination >= Gen::DelayModLfo && destination <= Gen::KeynumToVolEnvDecay;
            }
        }
    }

    _Use_decl_annotations_
    bool MemoryByteSource::Read(uint64_t offset, void* destination, size_t count) noexcept
    {
        if (destination == nullptr || offset > m_size || count > m_size - offset)
        {
            return false;
        }

        if (count > 0)
        {
            memcpy(destination, m_data + offset, count);
        }

        return true;
    }

    FileByteSource::~FileByteSource()
    {
        if (m_file != INVALID_HANDLE_VALUE)
        {
            CloseHandle(m_file);
        }
    }

    _Use_decl_annotations_
    bool FileByteSource::Open(std::wstring const& path) noexcept
    {
        if (m_file != INVALID_HANDLE_VALUE || path.empty())
        {
            return false;
        }

        // Shared for reading only, so the file cannot be truncated underneath the reads.
        m_file = CreateFileW(
            path.c_str(),
            GENERIC_READ,
            FILE_SHARE_READ,
            nullptr,
            OPEN_EXISTING,
            FILE_ATTRIBUTE_NORMAL | FILE_FLAG_SEQUENTIAL_SCAN,
            nullptr);

        if (m_file == INVALID_HANDLE_VALUE)
        {
            return false;
        }

        LARGE_INTEGER size{};

        if (GetFileType(m_file) != FILE_TYPE_DISK || !GetFileSizeEx(m_file, &size) || size.QuadPart < 0)
        {
            CloseHandle(m_file);
            m_file = INVALID_HANDLE_VALUE;
            return false;
        }

        m_size = static_cast<uint64_t>(size.QuadPart);

        return true;
    }

    _Use_decl_annotations_
    bool FileByteSource::Read(uint64_t offset, void* destination, size_t count) noexcept
    {
        if (m_file == INVALID_HANDLE_VALUE || destination == nullptr || offset > m_size || count > m_size - offset)
        {
            return false;
        }

        auto* cursor = static_cast<uint8_t*>(destination);

        while (count > 0)
        {
            auto const request = static_cast<DWORD>((std::min)(count, static_cast<size_t>(SampleReadChunkBytes)));

            OVERLAPPED position{};
            position.Offset = static_cast<DWORD>(offset & 0xFFFFFFFFull);
            position.OffsetHigh = static_cast<DWORD>(offset >> 32);

            DWORD read = 0;

            if (!ReadFile(m_file, cursor, request, &read, &position) || read != request)
            {
                return false;
            }

            cursor += read;
            offset += read;
            count -= read;
        }

        return true;
    }

    class Sf2Parser
    {
    public:
        Sf2Parser(
            _In_ IByteSource& source,
            _In_ Sf2LoadLimits const& limits,
            _In_ SoundFont& result,
            _In_ Sf2LoadStatistics& statistics) noexcept :
            m_source(source),
            m_limits(limits),
            m_result(result),
            m_statistics(statistics)
        {
        }

        Sf2LoadStatus Run()
        {
            auto status = WalkTopLevel();

            if (status == Sf2LoadStatus::Ok) status = ReadInfo();
            if (status == Sf2LoadStatus::Ok) status = ReadHydra();
            if (status == Sf2LoadStatus::Ok) status = ParseSamples();
            if (status == Sf2LoadStatus::Ok) status = ParseInstruments();
            if (status == Sf2LoadStatus::Ok) status = ParsePresets();
            if (status == Sf2LoadStatus::Ok) status = ReadSampleData();

            if (status == Sf2LoadStatus::Ok)
            {
                m_result.BuildPresetLookup();
            }

            return status;
        }

    private:
        template <typename Visit>
        Sf2LoadStatus WalkChunks(_In_ uint64_t offset, _In_ uint64_t end, _In_ Visit&& visit)
        {
            while (offset + 8 <= end)
            {
                uint8_t header[8]{};

                if (!m_source.Read(offset, header, sizeof(header)))
                {
                    return Sf2LoadStatus::ReadFailed;
                }

                auto const id = ReadU32(header);
                auto const size = ReadU32(header + 4);
                auto const dataOffset = offset + 8;

                if (static_cast<uint64_t>(size) > end - dataOffset)
                {
                    return Sf2LoadStatus::Truncated;
                }

                auto const status = visit(id, dataOffset, size);

                if (status != Sf2LoadStatus::Ok)
                {
                    return status;
                }

                // Chunks are padded to an even length.
                offset = dataOffset + size + (size & 1u);
            }

            return Sf2LoadStatus::Ok;
        }

        Sf2LoadStatus WalkTopLevel()
        {
            auto const fileSize = m_source.Size();

            if (fileSize > m_limits.MaximumFileBytes)
            {
                return Sf2LoadStatus::TooLarge;
            }

            if (fileSize < 12)
            {
                return Sf2LoadStatus::NotRiff;
            }

            uint8_t header[12]{};

            if (!m_source.Read(0, header, sizeof(header)))
            {
                return Sf2LoadStatus::ReadFailed;
            }

            if (ReadU32(header) != IdRiff)
            {
                return Sf2LoadStatus::NotRiff;
            }

            if (ReadU32(header + 8) != IdSfbk)
            {
                return Sf2LoadStatus::NotSoundFont;
            }

            // A RIFF size that disagrees with the file is common in real banks. Only what is
            // actually in the file is ever read, so the smaller of the two is the boundary.
            auto const riffEnd = (std::min)(static_cast<uint64_t>(ReadU32(header + 4)) + 8, fileSize);

            auto const status = WalkChunks(12, riffEnd, [this](uint32_t id, uint64_t offset, uint32_t size)
                {
                    if (id != IdList || size < 4)
                    {
                        return Sf2LoadStatus::Ok;
                    }

                    uint8_t listType[4]{};

                    if (!m_source.Read(offset, listType, sizeof(listType)))
                    {
                        return Sf2LoadStatus::ReadFailed;
                    }

                    ChunkSpan* target = nullptr;

                    switch (ReadU32(listType))
                    {
                    case IdInfo: target = &m_infoList; break;
                    case IdSdta: target = &m_sdtaList; break;
                    case IdPdta: target = &m_pdtaList; break;
                    default: break;
                    }

                    if (target != nullptr && !target->Present)
                    {
                        target->Offset = offset + 4;
                        target->Size = size - 4;
                        target->Present = true;
                    }

                    return Sf2LoadStatus::Ok;
                });

            if (status != Sf2LoadStatus::Ok)
            {
                return status;
            }

            if (!m_sdtaList.Present || !m_pdtaList.Present)
            {
                return Sf2LoadStatus::MissingChunk;
            }

            return WalkChunks(m_sdtaList.Offset, m_sdtaList.Offset + m_sdtaList.Size,
                [this](uint32_t id, uint64_t offset, uint32_t size)
                {
                    ChunkSpan* target = (id == IdSmpl) ? &m_smpl : (id == IdSm24) ? &m_sm24 : nullptr;

                    if (target != nullptr && !target->Present)
                    {
                        *target = ChunkSpan{ offset, size, true };
                    }

                    return Sf2LoadStatus::Ok;
                });
        }

        Sf2LoadStatus ReadInfoText(_In_ uint64_t offset, _In_ uint32_t size, _Out_ std::wstring& text)
        {
            text.clear();

            auto const length = (std::min)(size, MaximumInfoTextBytes);

            if (length == 0)
            {
                return Sf2LoadStatus::Ok;
            }

            std::vector<uint8_t> buffer(length);

            if (!m_source.Read(offset, buffer.data(), buffer.size()))
            {
                return Sf2LoadStatus::ReadFailed;
            }

            text = DecodeText(buffer.data(), buffer.size());

            return Sf2LoadStatus::Ok;
        }

        Sf2LoadStatus ReadInfo()
        {
            if (!m_infoList.Present)
            {
                return Sf2LoadStatus::Ok;
            }

            auto& info = m_result.m_info;

            auto const status = WalkChunks(m_infoList.Offset, m_infoList.Offset + m_infoList.Size,
                [this, &info](uint32_t id, uint64_t offset, uint32_t size)
                {
                    switch (id)
                    {
                    case IdIfil:
                    {
                        if (size < 4)
                        {
                            return Sf2LoadStatus::MalformedChunk;
                        }

                        uint8_t version[4]{};

                        if (!m_source.Read(offset, version, sizeof(version)))
                        {
                            return Sf2LoadStatus::ReadFailed;
                        }

                        info.VersionMajor = ReadU16(version);
                        info.VersionMinor = ReadU16(version + 2);
                        return Sf2LoadStatus::Ok;
                    }

                    case IdInam: return ReadInfoText(offset, size, info.Name);
                    case IdIsng: return ReadInfoText(offset, size, info.SoundEngine);
                    case IdIeng: return ReadInfoText(offset, size, info.Engineers);
                    case IdIcop: return ReadInfoText(offset, size, info.Copyright);
                    case IdIcmt: return ReadInfoText(offset, size, info.Comment);
                    case IdIsft: return ReadInfoText(offset, size, info.Software);
                    case IdIcrd: return ReadInfoText(offset, size, info.CreationDate);
                    case IdIprd: return ReadInfoText(offset, size, info.Product);

                    default:
                        return Sf2LoadStatus::Ok;
                    }
                });

            if (status != Sf2LoadStatus::Ok)
            {
                return status;
            }

            // Version 3 is the Ogg Vorbis compressed variant, which needs a decoder this does not have.
            if (info.VersionMajor >= 3)
            {
                return Sf2LoadStatus::CompressedSamples;
            }

            return Sf2LoadStatus::Ok;
        }

        Sf2LoadStatus ReadHydra()
        {
            struct Entry
            {
                uint32_t Id;
                std::vector<uint8_t>* Data;
                bool Found;
            };

            Entry entries[]
            {
                { IdPhdr, &m_phdr, false },
                { IdPbag, &m_pbag, false },
                { IdPmod, &m_pmod, false },
                { IdPgen, &m_pgen, false },
                { IdInst, &m_inst, false },
                { IdIbag, &m_ibag, false },
                { IdImod, &m_imod, false },
                { IdIgen, &m_igen, false },
                { IdShdr, &m_shdr, false },
            };

            uint64_t total = 0;

            auto const status = WalkChunks(m_pdtaList.Offset, m_pdtaList.Offset + m_pdtaList.Size,
                [this, &entries, &total](uint32_t id, uint64_t offset, uint32_t size)
                {
                    for (auto& entry : entries)
                    {
                        if (entry.Id != id || entry.Found)
                        {
                            continue;
                        }

                        total += size;

                        if (total > m_limits.MaximumHydraBytes)
                        {
                            return Sf2LoadStatus::TooLarge;
                        }

                        entry.Data->resize(size);

                        if (size > 0 && !m_source.Read(offset, entry.Data->data(), size))
                        {
                            return Sf2LoadStatus::ReadFailed;
                        }

                        entry.Found = true;
                        break;
                    }

                    return Sf2LoadStatus::Ok;
                });

            if (status != Sf2LoadStatus::Ok)
            {
                return status;
            }

            for (auto const& entry : entries)
            {
                if (!entry.Found)
                {
                    return Sf2LoadStatus::MissingChunk;
                }
            }

            // Every table ends with a terminal record, and the header tables need at least one
            // real record before it.
            if (m_phdr.size() % PhdrRecordBytes != 0 || m_phdr.size() / PhdrRecordBytes < 2 ||
                m_inst.size() % InstRecordBytes != 0 || m_inst.size() / InstRecordBytes < 2 ||
                m_shdr.size() % ShdrRecordBytes != 0 || m_shdr.size() / ShdrRecordBytes < 2 ||
                m_pbag.size() % BagRecordBytes != 0 || m_pbag.size() / BagRecordBytes < 1 ||
                m_ibag.size() % BagRecordBytes != 0 || m_ibag.size() / BagRecordBytes < 1 ||
                m_pgen.size() % GenRecordBytes != 0 || m_pgen.size() / GenRecordBytes < 1 ||
                m_igen.size() % GenRecordBytes != 0 || m_igen.size() / GenRecordBytes < 1 ||
                m_pmod.size() % ModRecordBytes != 0 ||
                m_imod.size() % ModRecordBytes != 0)
            {
                return Sf2LoadStatus::MalformedChunk;
            }

            return Sf2LoadStatus::Ok;
        }

        Sf2LoadStatus ParseSamples()
        {
            if (!m_smpl.Present)
            {
                return Sf2LoadStatus::MissingChunk;
            }

            auto const samplePoints = static_cast<uint64_t>(m_smpl.Size / 2);
            auto const count = (std::min)(m_shdr.size() / ShdrRecordBytes - 1, MaximumAddressableRecords);

            m_result.m_samples.resize(count);

            for (size_t i = 0; i < count; i++)
            {
                auto const* record = m_shdr.data() + i * ShdrRecordBytes;
                auto& sample = m_result.m_samples[i];

                sample.Name = DecodeText(record, NameBytes);

                auto const start = ReadU32(record + 20);
                auto const end = ReadU32(record + 24);
                auto const loopStart = ReadU32(record + 28);
                auto const loopEnd = ReadU32(record + 32);
                auto const rate = ReadU32(record + 36);
                auto const originalPitch = record[40];
                auto const correction = static_cast<int8_t>(record[41]);
                auto const type = ReadU16(record + 44);

                sample.Usable =
                    (type & (SampleTypeRom | SampleTypeCompressed)) == 0 &&
                    start < end &&
                    end <= samplePoints &&
                    rate >= MinimumSampleRate &&
                    rate <= MaximumSampleRate;

                if (!sample.Usable)
                {
                    m_statistics.SamplesUnusable++;
                    continue;
                }

                sample.Start = start;
                sample.End = end;
                sample.SampleRate = rate;

                // 255 is the specification's value for a sample with no pitch, played unshifted.
                sample.OriginalPitch = (originalPitch <= 127) ? originalPitch : 60;
                sample.PitchCorrection = correction;

                sample.LoopValid =
                    loopStart >= start &&
                    loopStart < loopEnd &&
                    loopEnd <= end &&
                    loopEnd - loopStart >= 2;

                sample.LoopStart = sample.LoopValid ? loopStart : start;
                sample.LoopEnd = sample.LoopValid ? loopEnd : end;
            }

            return Sf2LoadStatus::Ok;
        }

        // A bag's generators and modulators run up to the next bag's, so each index has to be in
        // order and inside its table. One index out of place would make a zone read another
        // zone's data, or past the end of the table.
        static bool BagIndicesAreValid(
            _In_ std::vector<uint8_t> const& bags,
            _In_ size_t generatorCount,
            _In_ size_t modulatorCount) noexcept
        {
            auto const count = bags.size() / BagRecordBytes;

            for (size_t i = 0; i < count; i++)
            {
                auto const generator = ReadU16(bags.data() + i * BagRecordBytes);
                auto const modulator = ReadU16(bags.data() + i * BagRecordBytes + 2);

                if (generator > generatorCount || modulator > modulatorCount)
                {
                    return false;
                }

                if (i + 1 < count)
                {
                    if (ReadU16(bags.data() + (i + 1) * BagRecordBytes) < generator ||
                        ReadU16(bags.data() + (i + 1) * BagRecordBytes + 2) < modulator)
                    {
                        return false;
                    }
                }
            }

            return true;
        }

        static bool HeaderBagIndicesAreValid(
            _In_ std::vector<uint8_t> const& headers,
            _In_ size_t recordBytes,
            _In_ size_t bagIndexOffset,
            _In_ size_t bagCount) noexcept
        {
            auto const count = headers.size() / recordBytes;
            uint16_t previous = 0;

            for (size_t i = 0; i < count; i++)
            {
                auto const bag = ReadU16(headers.data() + i * recordBytes + bagIndexOffset);

                // The terminal bag only marks where the last real one ends, so it is never the
                // start of a zone. Every header must leave room for the bag after its last one.
                if (bag < previous || bag >= bagCount)
                {
                    return false;
                }

                previous = bag;
            }

            return true;
        }

        void ReadZone(
            _In_ std::vector<uint8_t> const& bags,
            _In_ std::vector<uint8_t> const& generators,
            _In_ std::vector<uint8_t> const& modulators,
            _In_ size_t bagIndex,
            _In_ bool presetLevel,
            _Out_ Sf2Zone& zone,
            _Out_ bool& hasLink)
        {
            zone = Sf2Zone{};
            hasLink = false;

            auto const* bag = bags.data() + bagIndex * BagRecordBytes;
            auto const* next = bag + BagRecordBytes;

            auto const generatorStart = ReadU16(bag);
            auto const generatorEnd = ReadU16(next);
            auto const modulatorStart = ReadU16(bag + 2);
            auto const modulatorEnd = ReadU16(next + 2);

            auto const terminator = presetLevel ? Gen::Instrument : Gen::SampleId;
            auto const generatorCount = generators.size() / GenRecordBytes;

            for (size_t g = generatorStart; g < generatorEnd && g < generatorCount; g++)
            {
                auto const* record = generators.data() + g * GenRecordBytes;
                auto const generator = ReadU16(record);
                auto const amount = ReadU16(record + 2);

                if (generator == Gen::KeyRange || generator == Gen::VelRange)
                {
                    auto const low = static_cast<uint8_t>((std::min)(amount & 0xFF, 127));
                    auto const high = static_cast<uint8_t>((std::min)(amount >> 8, 127));

                    if (generator == Gen::KeyRange)
                    {
                        zone.KeyLow = low;
                        zone.KeyHigh = high;
                    }
                    else
                    {
                        zone.VelocityLow = low;
                        zone.VelocityHigh = high;
                    }

                    continue;
                }

                // Everything after the instrument or sample generator is ignored, per the specification.
                if (generator == terminator)
                {
                    zone.Link = amount;
                    hasLink = true;
                    break;
                }

                if (generator >= Gen::Count ||
                    !(presetLevel ? GeneratorAllowedAtPresetLevel(generator) : GeneratorIsSettable(generator)) ||
                    generator == Gen::Instrument)
                {
                    m_statistics.GeneratorsDropped++;
                    continue;
                }

                zone.Values[generator] = static_cast<int16_t>(amount);
                zone.SetMask |= (1ull << generator);
            }

            auto const modulatorCount = modulators.size() / ModRecordBytes;

            for (size_t m = modulatorStart; m < modulatorEnd && m < modulatorCount; m++)
            {
                auto const* record = modulators.data() + m * ModRecordBytes;

                Sf2Modulator modulator{};
                modulator.Source = ReadU16(record);
                modulator.Destination = ReadU16(record + 2);
                modulator.Amount = static_cast<int16_t>(ReadU16(record + 4));
                modulator.AmountSource = ReadU16(record + 6);
                modulator.Transform = ReadU16(record + 8);

                bool valid =
                    modulator.Amount != 0 &&
                    SourceOperandIsValid(modulator.Source) &&
                    SourceOperandIsValid(modulator.AmountSource) &&
                    ModulatorDestinationIsValid(modulator.Destination) &&
                    (modulator.Transform == 0 || modulator.Transform == 2) &&
                    zone.Modulators.size() < MaximumModulatorsPerZone;

                for (auto const& existing : zone.Modulators)
                {
                    if (valid && existing.IsSameAs(modulator))
                    {
                        valid = false;
                    }
                }

                if (!valid)
                {
                    m_statistics.ModulatorsDropped++;
                    continue;
                }

                zone.Modulators.push_back(modulator);
            }
        }

        Sf2LoadStatus ParseInstruments()
        {
            auto const bagCount = m_ibag.size() / BagRecordBytes;
            auto const generatorCount = m_igen.size() / GenRecordBytes;
            auto const modulatorCount = m_imod.size() / ModRecordBytes;

            if (!HeaderBagIndicesAreValid(m_inst, InstRecordBytes, 20, bagCount) ||
                !BagIndicesAreValid(m_ibag, generatorCount, modulatorCount))
            {
                return Sf2LoadStatus::BadIndex;
            }

            auto const count = (std::min)(m_inst.size() / InstRecordBytes - 1, MaximumAddressableRecords);
            auto const sampleCount = m_result.m_samples.size();

            m_result.m_instruments.resize(count);

            for (size_t i = 0; i < count; i++)
            {
                auto const* record = m_inst.data() + i * InstRecordBytes;
                auto& instrument = m_result.m_instruments[i];

                instrument.Name = DecodeText(record, NameBytes);

                auto const firstBag = ReadU16(record + 20);
                auto const endBag = ReadU16(record + InstRecordBytes + 20);

                for (size_t bag = firstBag; bag < endBag; bag++)
                {
                    Sf2Zone zone{};
                    bool hasLink = false;

                    ReadZone(m_ibag, m_igen, m_imod, bag, false, zone, hasLink);

                    if (!hasLink)
                    {
                        // Only the first zone can be the global zone; any other without a sample is
                        // ignored, per the specification.
                        if (bag == firstBag)
                        {
                            instrument.GlobalZone = std::move(zone);
                            instrument.HasGlobalZone = true;
                        }
                        else
                        {
                            m_statistics.ZonesDropped++;
                        }

                        continue;
                    }

                    if (zone.Link >= sampleCount || !m_result.m_samples[zone.Link].Usable ||
                        zone.KeyLow > zone.KeyHigh || zone.VelocityLow > zone.VelocityHigh)
                    {
                        m_statistics.ZonesDropped++;
                        continue;
                    }

                    instrument.Zones.push_back(std::move(zone));
                }
            }

            return Sf2LoadStatus::Ok;
        }

        Sf2LoadStatus ParsePresets()
        {
            auto const bagCount = m_pbag.size() / BagRecordBytes;
            auto const generatorCount = m_pgen.size() / GenRecordBytes;
            auto const modulatorCount = m_pmod.size() / ModRecordBytes;

            if (!HeaderBagIndicesAreValid(m_phdr, PhdrRecordBytes, 24, bagCount) ||
                !BagIndicesAreValid(m_pbag, generatorCount, modulatorCount))
            {
                return Sf2LoadStatus::BadIndex;
            }

            auto const count = m_phdr.size() / PhdrRecordBytes - 1;
            auto const instrumentCount = m_result.m_instruments.size();

            m_result.m_presets.reserve(count);

            for (size_t i = 0; i < count; i++)
            {
                auto const* record = m_phdr.data() + i * PhdrRecordBytes;

                Sf2Preset preset{};

                preset.Name = DecodeText(record, NameBytes);
                preset.Program = ReadU16(record + 20);
                preset.Bank = ReadU16(record + 22);

                auto const firstBag = ReadU16(record + 24);
                auto const endBag = ReadU16(record + PhdrRecordBytes + 24);

                for (size_t bag = firstBag; bag < endBag; bag++)
                {
                    Sf2Zone zone{};
                    bool hasLink = false;

                    ReadZone(m_pbag, m_pgen, m_pmod, bag, true, zone, hasLink);

                    if (!hasLink)
                    {
                        if (bag == firstBag)
                        {
                            preset.GlobalZone = std::move(zone);
                            preset.HasGlobalZone = true;
                        }
                        else
                        {
                            m_statistics.ZonesDropped++;
                        }

                        continue;
                    }

                    if (zone.Link >= instrumentCount || m_result.m_instruments[zone.Link].Zones.empty() ||
                        zone.KeyLow > zone.KeyHigh || zone.VelocityLow > zone.VelocityHigh)
                    {
                        m_statistics.ZonesDropped++;
                        continue;
                    }

                    preset.Zones.push_back(std::move(zone));
                }

                // A preset nothing can select, or that has nothing to play, is left out entirely.
                if (preset.Zones.empty() || preset.Program > 127 || preset.Bank > SoundFont::PercussionBank)
                {
                    m_statistics.PresetsDropped++;
                    continue;
                }

                m_result.m_presets.push_back(std::move(preset));
            }

            if (m_result.m_presets.empty())
            {
                return Sf2LoadStatus::NoPresets;
            }

            return Sf2LoadStatus::Ok;
        }

        Sf2LoadStatus ReadSampleData()
        {
            auto const points = static_cast<size_t>(m_smpl.Size / 2);

            m_result.m_sampleData.resize(points);

            if (points > 0 && !m_source.Read(m_smpl.Offset, m_result.m_sampleData.data(), points * sizeof(int16_t)))
            {
                return Sf2LoadStatus::ReadFailed;
            }

            auto const& info = m_result.m_info;
            auto const versionSupports24Bit =
                info.VersionMajor > 2 || (info.VersionMajor == 2 && info.VersionMinor >= 4);

            // The specification says a 24 bit chunk of the wrong size is to be ignored, not refused.
            if (m_sm24.Present && versionSupports24Bit &&
                (m_sm24.Size == points || m_sm24.Size == points + (points & 1)))
            {
                m_result.m_sampleData24.resize(points);

                if (points > 0 && !m_source.Read(m_sm24.Offset, m_result.m_sampleData24.data(), points))
                {
                    return Sf2LoadStatus::ReadFailed;
                }

                m_statistics.Uses24BitSamples = true;
            }

            return Sf2LoadStatus::Ok;
        }

        IByteSource& m_source;
        Sf2LoadLimits const& m_limits;
        SoundFont& m_result;
        Sf2LoadStatistics& m_statistics;

        ChunkSpan m_infoList{};
        ChunkSpan m_sdtaList{};
        ChunkSpan m_pdtaList{};
        ChunkSpan m_smpl{};
        ChunkSpan m_sm24{};

        std::vector<uint8_t> m_phdr{};
        std::vector<uint8_t> m_pbag{};
        std::vector<uint8_t> m_pmod{};
        std::vector<uint8_t> m_pgen{};
        std::vector<uint8_t> m_inst{};
        std::vector<uint8_t> m_ibag{};
        std::vector<uint8_t> m_imod{};
        std::vector<uint8_t> m_igen{};
        std::vector<uint8_t> m_shdr{};
    };

    _Use_decl_annotations_
    Sf2LoadStatus SoundFont::Load(
        IByteSource& source,
        Sf2LoadLimits const& limits,
        SoundFont& result,
        Sf2LoadStatistics* statistics) noexcept
    {
        Sf2LoadStatistics localStatistics{};

        result = SoundFont{};

        try
        {
            Sf2Parser parser{ source, limits, result, localStatistics };

            auto const status = parser.Run();

            if (status != Sf2LoadStatus::Ok)
            {
                result = SoundFont{};
            }

            if (statistics != nullptr)
            {
                *statistics = localStatistics;
            }

            return status;
        }
        catch (std::bad_alloc const&)
        {
            result = SoundFont{};
            return Sf2LoadStatus::OutOfMemory;
        }
        catch (...)
        {
            result = SoundFont{};
            return Sf2LoadStatus::ReadFailed;
        }
    }

    _Use_decl_annotations_
    Sf2LoadStatus SoundFont::LoadFromFile(
        std::wstring const& path,
        Sf2LoadLimits const& limits,
        SoundFont& result,
        Sf2LoadStatistics* statistics) noexcept
    {
        result = SoundFont{};

        if (statistics != nullptr)
        {
            *statistics = Sf2LoadStatistics{};
        }

        FileByteSource source{};

        if (!source.Open(path))
        {
            return Sf2LoadStatus::CannotOpen;
        }

        return Load(source, limits, result, statistics);
    }

    void SoundFont::BuildPresetLookup()
    {
        m_presetLookup.assign(static_cast<size_t>(PercussionBank + 1) * 128, -1);

        for (size_t i = 0; i < m_presets.size(); i++)
        {
            auto const& preset = m_presets[i];
            auto const key = static_cast<size_t>(preset.Bank) * 128 + preset.Program;

            // The first of two presets with the same address wins, which is what a bank editor shows.
            if (m_presetLookup[key] < 0)
            {
                m_presetLookup[key] = static_cast<int32_t>(i);
            }
        }
    }

    _Use_decl_annotations_
    int32_t SoundFont::FindPreset(uint16_t bank, uint16_t program) const noexcept
    {
        if (bank > PercussionBank || program > 127 || m_presetLookup.empty())
        {
            return -1;
        }

        return m_presetLookup[static_cast<size_t>(bank) * 128 + program];
    }
}
