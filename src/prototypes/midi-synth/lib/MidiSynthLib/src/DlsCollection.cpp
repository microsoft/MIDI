#include "MidiSynth/DlsCollection.h"
#include "MidiSynth/RiffReader.h"

#include <windows.h>

#include <algorithm>
#include <unordered_map>

namespace MidiSynth
{
    namespace
    {
        constexpr uint32_t FourCC_DLS = MakeFourCC("DLS ");
        constexpr uint32_t FourCC_colh = MakeFourCC("colh");
        constexpr uint32_t FourCC_vers = MakeFourCC("vers");
        constexpr uint32_t FourCC_ptbl = MakeFourCC("ptbl");
        constexpr uint32_t FourCC_lins = MakeFourCC("lins");
        constexpr uint32_t FourCC_ins = MakeFourCC("ins ");
        constexpr uint32_t FourCC_insh = MakeFourCC("insh");
        constexpr uint32_t FourCC_lrgn = MakeFourCC("lrgn");
        constexpr uint32_t FourCC_rgn = MakeFourCC("rgn ");
        constexpr uint32_t FourCC_rgn2 = MakeFourCC("rgn2");
        constexpr uint32_t FourCC_rgnh = MakeFourCC("rgnh");
        constexpr uint32_t FourCC_lart = MakeFourCC("lart");
        constexpr uint32_t FourCC_lar2 = MakeFourCC("lar2");
        constexpr uint32_t FourCC_art1 = MakeFourCC("art1");
        constexpr uint32_t FourCC_art2 = MakeFourCC("art2");
        constexpr uint32_t FourCC_wlnk = MakeFourCC("wlnk");
        constexpr uint32_t FourCC_wsmp = MakeFourCC("wsmp");
        constexpr uint32_t FourCC_wvpl = MakeFourCC("wvpl");
        constexpr uint32_t FourCC_wave = MakeFourCC("wave");
        constexpr uint32_t FourCC_fmt = MakeFourCC("fmt ");
        constexpr uint32_t FourCC_data = MakeFourCC("data");
        constexpr uint32_t FourCC_INFO = MakeFourCC("INFO");
        constexpr uint32_t FourCC_INAM = MakeFourCC("INAM");

        constexpr uint32_t PoolCueNull = 0xFFFFFFFFu;

        constexpr uint32_t F_INSTRUMENT_DRUMS = 0x80000000u;

        constexpr size_t ConnectionBlockBytes = 12;
        constexpr size_t PoolCueBytes = 4;
        constexpr size_t SampleLoopBytes = 16;
        constexpr size_t WaveSampleHeaderBytes = 20;
        constexpr size_t ConnectionListHeaderBytes = 8;
        constexpr size_t PoolTableHeaderBytes = 8;

        constexpr size_t MaxNameCharacters = 256;

        constexpr uint16_t WaveFormatPcm = 1;
        constexpr uint16_t RequiredBitsPerSample = 16;

        // INFO text is single byte per the specification. Widening as Latin-1 cannot fail,
        // which keeps a malformed name from becoming a parse failure.
        std::wstring ReadLatin1Name(std::span<const std::byte> payload)
        {
            std::wstring value;

            const size_t limit = (std::min)(payload.size(), MaxNameCharacters);
            value.reserve(limit);

            for (size_t i = 0; i < limit; i++)
            {
                const auto character = static_cast<unsigned char>(payload[i]);

                if (character == 0)
                {
                    break;
                }

                value.push_back(character < 0x20 ? L' ' : static_cast<wchar_t>(character));
            }

            while (!value.empty() && value.back() == L' ')
            {
                value.pop_back();
            }

            return value;
        }

        std::wstring ReadInfoListName(std::span<const std::byte> infoPayload)
        {
            RiffChunkReader reader(infoPayload);
            RiffChunk chunk{};

            while (reader.TryNext(chunk))
            {
                if (chunk.Id == FourCC_INAM)
                {
                    return ReadLatin1Name(chunk.Payload);
                }
            }

            return {};
        }

        bool TryReadWaveSample(
            std::span<const std::byte> payload,
            const DlsParseLimits& limits,
            _Out_ DlsWaveSample& waveSample)
        {
            waveSample = {};

            ByteReader reader(payload);

            uint32_t structureBytes{};
            uint32_t loopCount{};

            if (!reader.TryReadUInt32(structureBytes) ||
                !reader.TryReadUInt16(waveSample.UnityNote) ||
                !reader.TryReadInt16(waveSample.FineTune) ||
                !reader.TryReadInt32(waveSample.Attenuation) ||
                !reader.TryReadUInt32(waveSample.Options) ||
                !reader.TryReadUInt32(loopCount))
            {
                return false;
            }

            if (structureBytes < WaveSampleHeaderBytes)
            {
                return false;
            }

            // The header may grow in a later revision; skip anything past what we understand.
            if (!reader.TrySkip(structureBytes - WaveSampleHeaderBytes))
            {
                return false;
            }

            if (loopCount > limits.MaxSampleLoops || loopCount > reader.Remaining() / SampleLoopBytes)
            {
                return false;
            }

            waveSample.Loops.reserve(loopCount);

            for (uint32_t i = 0; i < loopCount; i++)
            {
                uint32_t loopStructureBytes{};
                DlsSampleLoop loop{};

                if (!reader.TryReadUInt32(loopStructureBytes) ||
                    !reader.TryReadUInt32(loop.LoopType) ||
                    !reader.TryReadUInt32(loop.LoopStart) ||
                    !reader.TryReadUInt32(loop.LoopLength))
                {
                    return false;
                }

                if (loopStructureBytes < SampleLoopBytes ||
                    !reader.TrySkip(loopStructureBytes - SampleLoopBytes))
                {
                    return false;
                }

                waveSample.Loops.push_back(loop);
            }

            return true;
        }

        bool TryReadConnectionList(
            std::span<const std::byte> payload,
            const DlsParseLimits& limits,
            _Inout_ std::vector<DlsConnection>& connections)
        {
            ByteReader reader(payload);

            uint32_t structureBytes{};
            uint32_t connectionCount{};

            if (!reader.TryReadUInt32(structureBytes) || !reader.TryReadUInt32(connectionCount))
            {
                return false;
            }

            if (structureBytes < ConnectionListHeaderBytes ||
                !reader.TrySkip(structureBytes - ConnectionListHeaderBytes))
            {
                return false;
            }

            if (connectionCount > limits.MaxConnections ||
                connectionCount > reader.Remaining() / ConnectionBlockBytes)
            {
                return false;
            }

            connections.reserve(connections.size() + connectionCount);

            for (uint32_t i = 0; i < connectionCount; i++)
            {
                DlsConnection connection{};

                if (!reader.TryReadUInt16(connection.Source) ||
                    !reader.TryReadUInt16(connection.Control) ||
                    !reader.TryReadUInt16(connection.Destination) ||
                    !reader.TryReadUInt16(connection.Transform) ||
                    !reader.TryReadInt32(connection.Scale))
                {
                    return false;
                }

                connections.push_back(connection);
            }

            return true;
        }

        bool TryReadArticulationList(
            std::span<const std::byte> payload,
            const DlsParseLimits& limits,
            _Inout_ std::vector<DlsConnection>& connections)
        {
            RiffChunkReader reader(payload);
            RiffChunk chunk{};

            while (reader.TryNext(chunk))
            {
                if (chunk.Id == FourCC_art1 || chunk.Id == FourCC_art2)
                {
                    if (!TryReadConnectionList(chunk.Payload, limits, connections))
                    {
                        return false;
                    }
                }
            }

            return true;
        }

        DlsParseStatus ReadRegion(
            std::span<const std::byte> payload,
            const DlsParseLimits& limits,
            _Out_ DlsRegion& region)
        {
            region = {};

            bool sawHeader = false;
            bool sawWaveLink = false;

            RiffChunkReader reader(payload);
            RiffChunk chunk{};

            while (reader.TryNext(chunk))
            {
                if (chunk.Id == FourCC_rgnh)
                {
                    ByteReader header(chunk.Payload);

                    if (!header.TryReadUInt16(region.KeyLow) ||
                        !header.TryReadUInt16(region.KeyHigh) ||
                        !header.TryReadUInt16(region.VelocityLow) ||
                        !header.TryReadUInt16(region.VelocityHigh) ||
                        !header.TryReadUInt16(region.Options))
                    {
                        return DlsParseStatus::MalformedChunk;
                    }

                    // usKeyGroup is absent on some region headers; treat that as "no key group".
                    (void)header.TryReadUInt16(region.KeyGroup);

                    sawHeader = true;
                }
                else if (chunk.Id == FourCC_wsmp)
                {
                    if (!TryReadWaveSample(chunk.Payload, limits, region.WaveSample))
                    {
                        return DlsParseStatus::MalformedChunk;
                    }

                    region.HasWaveSample = true;
                }
                else if (chunk.Id == FourCC_wlnk)
                {
                    ByteReader link(chunk.Payload);

                    if (!link.TryReadUInt16(region.WaveLink.Options) ||
                        !link.TryReadUInt16(region.WaveLink.PhaseGroup) ||
                        !link.TryReadUInt32(region.WaveLink.Channel) ||
                        !link.TryReadUInt32(region.WaveLink.TableIndex))
                    {
                        return DlsParseStatus::MalformedChunk;
                    }

                    sawWaveLink = true;
                }
                else if (chunk.IsList() && (chunk.ListType == FourCC_lart || chunk.ListType == FourCC_lar2))
                {
                    if (!TryReadArticulationList(chunk.Payload, limits, region.Connections))
                    {
                        return DlsParseStatus::MalformedChunk;
                    }
                }
            }

            if (!sawHeader || !sawWaveLink)
            {
                return DlsParseStatus::MissingRequiredChunk;
            }

            return DlsParseStatus::Ok;
        }

        DlsParseStatus ReadInstrument(
            std::span<const std::byte> payload,
            const DlsParseLimits& limits,
            _Out_ DlsInstrument& instrument)
        {
            instrument = {};

            bool sawHeader = false;

            RiffChunkReader reader(payload);
            RiffChunk chunk{};

            while (reader.TryNext(chunk))
            {
                if (chunk.Id == FourCC_insh)
                {
                    ByteReader header(chunk.Payload);

                    uint32_t regionCount{};
                    uint32_t bank{};
                    uint32_t program{};

                    if (!header.TryReadUInt32(regionCount) ||
                        !header.TryReadUInt32(bank) ||
                        !header.TryReadUInt32(program))
                    {
                        return DlsParseStatus::MalformedChunk;
                    }

                    instrument.IsDrumKit = (bank & F_INSTRUMENT_DRUMS) != 0;
                    instrument.BankLsb = bank & 0x7Fu;
                    instrument.BankMsb = (bank >> 8) & 0x7Fu;
                    instrument.Program = program & 0x7Fu;

                    sawHeader = true;
                }
                else if (chunk.IsList() && chunk.ListType == FourCC_lrgn)
                {
                    RiffChunkReader regionReader(chunk.Payload);
                    RiffChunk regionChunk{};

                    while (regionReader.TryNext(regionChunk))
                    {
                        if (!regionChunk.IsList() ||
                            (regionChunk.ListType != FourCC_rgn && regionChunk.ListType != FourCC_rgn2))
                        {
                            continue;
                        }

                        if (instrument.Regions.size() >= limits.MaxRegionsPerInstrument)
                        {
                            return DlsParseStatus::LimitExceeded;
                        }

                        DlsRegion region{};
                        const auto status = ReadRegion(regionChunk.Payload, limits, region);

                        if (status != DlsParseStatus::Ok)
                        {
                            return status;
                        }

                        instrument.Regions.push_back(std::move(region));
                    }
                }
                else if (chunk.IsList() && (chunk.ListType == FourCC_lart || chunk.ListType == FourCC_lar2))
                {
                    if (!TryReadArticulationList(chunk.Payload, limits, instrument.Connections))
                    {
                        return DlsParseStatus::MalformedChunk;
                    }
                }
                else if (chunk.IsList() && chunk.ListType == FourCC_INFO)
                {
                    instrument.Name = ReadInfoListName(chunk.Payload);
                }
            }

            if (!sawHeader)
            {
                return DlsParseStatus::MissingRequiredChunk;
            }

            return DlsParseStatus::Ok;
        }

        DlsParseStatus ReadWave(
            std::span<const std::byte> payload,
            const DlsParseLimits& limits,
            _Out_ DlsWave& wave)
        {
            wave = {};

            bool sawFormat = false;
            bool sawData = false;

            RiffChunkReader reader(payload);
            RiffChunk chunk{};

            while (reader.TryNext(chunk))
            {
                if (chunk.Id == FourCC_fmt)
                {
                    ByteReader format(chunk.Payload);

                    if (!format.TryReadUInt16(wave.FormatTag) ||
                        !format.TryReadUInt16(wave.Channels) ||
                        !format.TryReadUInt32(wave.SamplesPerSecond) ||
                        !format.TryReadUInt32(wave.AverageBytesPerSecond) ||
                        !format.TryReadUInt16(wave.BlockAlign) ||
                        !format.TryReadUInt16(wave.BitsPerSample))
                    {
                        return DlsParseStatus::MalformedChunk;
                    }

                    sawFormat = true;
                }
                else if (chunk.Id == FourCC_data)
                {
                    wave.SampleData = chunk.Payload;
                    sawData = true;
                }
                else if (chunk.Id == FourCC_wsmp)
                {
                    if (!TryReadWaveSample(chunk.Payload, limits, wave.WaveSample))
                    {
                        return DlsParseStatus::MalformedChunk;
                    }

                    wave.HasWaveSample = true;
                }
                else if (chunk.IsList() && chunk.ListType == FourCC_INFO)
                {
                    wave.Name = ReadInfoListName(chunk.Payload);
                }
            }

            if (!sawFormat || !sawData)
            {
                return DlsParseStatus::MissingRequiredChunk;
            }

            if (wave.Channels == 0 || (wave.BitsPerSample % 8) != 0 || wave.BitsPerSample == 0)
            {
                return DlsParseStatus::UnsupportedWaveFormat;
            }

            // The renderer reads sample data through an int16_t pointer using a frame count derived
            // from BitsPerSample, so 8-bit mono data would be read two bytes at a time past the end
            // of the chunk. Only the format the renderer actually supports is accepted.
            if (wave.FormatTag != WaveFormatPcm || wave.BitsPerSample != RequiredBitsPerSample)
            {
                return DlsParseStatus::UnsupportedWaveFormat;
            }

            // Guards the same pointer cast against a data chunk landing on an odd offset.
            if ((reinterpret_cast<uintptr_t>(wave.SampleData.data()) % alignof(int16_t)) != 0)
            {
                return DlsParseStatus::UnsupportedWaveFormat;
            }

            return DlsParseStatus::Ok;
        }
    }

    const char* DlsParseStatusToString(_In_ DlsParseStatus status) noexcept
    {
        switch (status)
        {
        case DlsParseStatus::Ok: return "Ok";
        case DlsParseStatus::FileNotFound: return "FileNotFound";
        case DlsParseStatus::FileTooLarge: return "FileTooLarge";
        case DlsParseStatus::ReadError: return "ReadError";
        case DlsParseStatus::NotRiffFile: return "NotRiffFile";
        case DlsParseStatus::NotDlsCollection: return "NotDlsCollection";
        case DlsParseStatus::MalformedChunk: return "MalformedChunk";
        case DlsParseStatus::MissingRequiredChunk: return "MissingRequiredChunk";
        case DlsParseStatus::LimitExceeded: return "LimitExceeded";
        case DlsParseStatus::UnsupportedWaveFormat: return "UnsupportedWaveFormat";
        case DlsParseStatus::InvalidWaveReference: return "InvalidWaveReference";
        case DlsParseStatus::NotPermitted: return "NotPermitted";
        }

        return "Unknown";
    }

    namespace
    {
        // INFO strings are single byte and null terminated, from an untrusted file, so the length
        // is taken from the chunk rather than from any terminator inside it.
        std::wstring ToWide(_In_reads_(count) const char* text, _In_ size_t count) noexcept
        {
            while (count > 0 && text[count - 1] == '\0')
            {
                count--;
            }

            if (count == 0)
            {
                return {};
            }

            const int required = MultiByteToWideChar(
                CP_ACP, 0, text, static_cast<int>(count), nullptr, 0);

            if (required <= 0)
            {
                return {};
            }

            std::wstring result(static_cast<size_t>(required), L'\0');

            MultiByteToWideChar(CP_ACP, 0, text, static_cast<int>(count), result.data(), required);

            return result;
        }

        void ReadInfoStrings(_In_ std::span<const std::byte> payload, _Inout_ DlsSoundSetInfo& info) noexcept
        {
            RiffChunkReader chunks(payload);
            RiffChunk chunk;

            while (chunks.TryNext(chunk))
            {
                if (chunk.Payload.empty() || chunk.Payload.size() > 4096)
                {
                    continue;
                }

                const auto text = reinterpret_cast<const char*>(chunk.Payload.data());

                if (chunk.Id == MakeFourCC("INAM"))
                {
                    info.Name = ToWide(text, chunk.Payload.size());
                }
                else if (chunk.Id == MakeFourCC("IENG"))
                {
                    info.Engineer = ToWide(text, chunk.Payload.size());
                }
                else if (chunk.Id == MakeFourCC("ICMT"))
                {
                    info.Comments = ToWide(text, chunk.Payload.size());
                }
            }
        }

        // Resolves symlinks, junctions, short names and relative traversal, none of which a string
        // comparison on the caller's path would catch. Checked on the handle we then read from, so
        // the file cannot be swapped between the check and the read.
        bool HandleIsUnderSystemDirectory(_In_ HANDLE file) noexcept
        {
            std::wstring resolved(MAX_PATH, L'\0');

            DWORD length = GetFinalPathNameByHandleW(
                file, resolved.data(), static_cast<DWORD>(resolved.size()),
                FILE_NAME_NORMALIZED | VOLUME_NAME_DOS);

            if (length == 0)
            {
                return false;
            }

            if (length > resolved.size())
            {
                resolved.assign(length, L'\0');

                length = GetFinalPathNameByHandleW(
                    file, resolved.data(), static_cast<DWORD>(resolved.size()),
                    FILE_NAME_NORMALIZED | VOLUME_NAME_DOS);

                if (length == 0 || length > resolved.size())
                {
                    return false;
                }
            }

            resolved.resize(length);

            // GetFinalPathNameByHandleW returns an extended length prefix; the system directory
            // does not have one.
            constexpr std::wstring_view ExtendedPrefix = L"\\\\?\\";

            if (resolved.starts_with(ExtendedPrefix))
            {
                resolved.erase(0, ExtendedPrefix.size());
            }

            wchar_t systemDirectory[MAX_PATH]{};
            const UINT systemLength = GetSystemDirectoryW(systemDirectory, ARRAYSIZE(systemDirectory));

            if (systemLength == 0 || systemLength >= ARRAYSIZE(systemDirectory))
            {
                return false;
            }

            const std::wstring_view system(systemDirectory, systemLength);

            if (resolved.size() <= system.size())
            {
                return false;
            }

            if (_wcsnicmp(resolved.c_str(), system.data(), system.size()) != 0)
            {
                return false;
            }

            // The next character must be a separator, otherwise "System32Evil\x.dls" would pass.
            return resolved[system.size()] == L'\\';
        }

        DlsParseStatus OpenSoundSetFile(
            _In_ const std::wstring& path,
            _In_ const DlsParseLimits& limits,
            _In_ SoundSetOrigin origin,
            _Out_ HANDLE& openedHandle,
            _Out_ uint64_t& fileBytes) noexcept
        {
            openedHandle = INVALID_HANDLE_VALUE;
            fileBytes = 0;

            const HANDLE rawHandle = CreateFileW(
                path.c_str(),
                GENERIC_READ,
                FILE_SHARE_READ,
                nullptr,
                OPEN_EXISTING,
                FILE_ATTRIBUTE_NORMAL,
                nullptr);

            if (rawHandle == INVALID_HANDLE_VALUE)
            {
                return DlsParseStatus::FileNotFound;
            }

            if (origin == SoundSetOrigin::SystemOnly && !HandleIsUnderSystemDirectory(rawHandle))
            {
                CloseHandle(rawHandle);
                return DlsParseStatus::NotPermitted;
            }

            LARGE_INTEGER size{};

            if (!GetFileSizeEx(rawHandle, &size))
            {
                CloseHandle(rawHandle);
                return DlsParseStatus::ReadError;
            }

            if (size.QuadPart <= 0)
            {
                CloseHandle(rawHandle);
                return DlsParseStatus::NotRiffFile;
            }

            if (static_cast<uint64_t>(size.QuadPart) > limits.MaxFileBytes)
            {
                CloseHandle(rawHandle);
                return DlsParseStatus::FileTooLarge;
            }

            openedHandle = rawHandle;
            fileBytes = static_cast<uint64_t>(size.QuadPart);

            return DlsParseStatus::Ok;
        }
    }

    _Use_decl_annotations_
    DlsParseStatus DlsCollection::LoadFromFile(
        const std::wstring& path,
        const DlsParseLimits& limits,
        SoundSetOrigin origin,
        DlsCollection& collection)
    {
        collection = {};

        HANDLE rawHandle = INVALID_HANDLE_VALUE;
        uint64_t fileBytes64 = 0;

        const auto opened = OpenSoundSetFile(path, limits, origin, rawHandle, fileBytes64);

        if (opened != DlsParseStatus::Ok)
        {
            return opened;
        }

        struct HandleCloser
        {
            HANDLE Value;
            ~HandleCloser() { CloseHandle(Value); }
        } handle{ rawHandle };

        // Read rather than memory map. A mapped view of a file another process truncates
        // faults on access; a heap copy cannot be pulled out from under the parser.
        std::vector<std::byte> fileBytes(static_cast<size_t>(fileBytes64));

        size_t totalRead = 0;

        while (totalRead < fileBytes.size())
        {
            const DWORD requested = static_cast<DWORD>(
                (std::min)(fileBytes.size() - totalRead, static_cast<size_t>(64u * 1024u * 1024u)));

            DWORD actuallyRead = 0;

            if (!ReadFile(handle.Value, fileBytes.data() + totalRead, requested, &actuallyRead, nullptr))
            {
                return DlsParseStatus::ReadError;
            }

            if (actuallyRead == 0)
            {
                return DlsParseStatus::ReadError;
            }

            totalRead += actuallyRead;
        }

        return LoadFromMemory(std::move(fileBytes), limits, collection);
    }

    _Use_decl_annotations_
    DlsParseStatus DlsCollection::ProbeFile(
        const std::wstring& path,
        const DlsParseLimits& limits,
        SoundSetOrigin origin,
        DlsSoundSetInfo& info)
    {
        info = {};

        HANDLE rawHandle = INVALID_HANDLE_VALUE;
        uint64_t fileBytes = 0;

        const auto opened = OpenSoundSetFile(path, limits, origin, rawHandle, fileBytes);

        if (opened != DlsParseStatus::Ok)
        {
            return opened;
        }

        struct HandleCloser
        {
            HANDLE Value;
            ~HandleCloser() { CloseHandle(Value); }
        } handle{ rawHandle };

        info.FileBytes = fileBytes;

        auto readExact = [&](_Out_writes_bytes_(count) void* buffer, uint32_t count) noexcept
        {
            DWORD actuallyRead = 0;
            return ReadFile(handle.Value, buffer, count, &actuallyRead, nullptr) && actuallyRead == count;
        };

        auto seekTo = [&](uint64_t offset) noexcept
        {
            LARGE_INTEGER move{};
            move.QuadPart = static_cast<LONGLONG>(offset);
            return SetFilePointerEx(handle.Value, move, nullptr, FILE_BEGIN) != FALSE;
        };

        // RIFF header: "RIFF" <size> "DLS ".
        uint32_t header[3]{};

        if (!readExact(header, sizeof(header)))
        {
            return DlsParseStatus::ReadError;
        }

        if (header[0] != MakeFourCC("RIFF") || header[2] != MakeFourCC("DLS "))
        {
            return DlsParseStatus::NotRiffFile;
        }

        // Anything bigger than this in a header chunk is not something worth reading whole.
        constexpr uint32_t MaxInlineChunkBytes = 64u * 1024u;

        uint64_t position = 12;
        bool sawCollectionHeader = false;

        while (position + 8 <= fileBytes)
        {
            if (!seekTo(position))
            {
                return DlsParseStatus::ReadError;
            }

            uint32_t chunk[2]{};

            if (!readExact(chunk, sizeof(chunk)))
            {
                return DlsParseStatus::ReadError;
            }

            const uint32_t id = chunk[0];
            const uint32_t payloadSize = chunk[1];

            if (payloadSize > fileBytes - (position + 8))
            {
                return DlsParseStatus::MalformedChunk;
            }

            if (id == MakeFourCC("colh") && payloadSize >= 4)
            {
                if (!readExact(&info.InstrumentCount, 4))
                {
                    return DlsParseStatus::ReadError;
                }

                sawCollectionHeader = true;
            }
            else if (id == MakeFourCC("vers") && payloadSize >= 8)
            {
                uint16_t parts[4]{};

                if (!readExact(parts, sizeof(parts)))
                {
                    return DlsParseStatus::ReadError;
                }

                info.Version.Minor = parts[0];
                info.Version.Major = parts[1];
                info.Version.Build = parts[2];
                info.Version.Release = parts[3];
            }
            else if (id == MakeFourCC("LIST") && payloadSize >= 4 &&
                     payloadSize <= MaxInlineChunkBytes)
            {
                std::vector<std::byte> listBytes(payloadSize);

                if (!readExact(listBytes.data(), payloadSize))
                {
                    return DlsParseStatus::ReadError;
                }

                uint32_t listType = 0;
                std::memcpy(&listType, listBytes.data(), sizeof(listType));

                if (listType == MakeFourCC("INFO"))
                {
                    ReadInfoStrings(
                        std::span<const std::byte>(listBytes).subspan(sizeof(listType)), info);
                }
            }

            // Chunks are word aligned, and the pad byte is not counted in the size.
            position += 8 + payloadSize + (payloadSize & 1);
        }

        return sawCollectionHeader ? DlsParseStatus::Ok : DlsParseStatus::MissingRequiredChunk;
    }

    _Use_decl_annotations_
    DlsParseStatus DlsCollection::LoadFromMemory(
        std::vector<std::byte>&& fileBytes,
        const DlsParseLimits& limits,
        DlsCollection& collection)
    {
        collection = {};
        collection.m_fileBytes = std::move(fileBytes);

        const auto status = collection.Parse(limits);

        if (status != DlsParseStatus::Ok)
        {
            collection = {};
        }

        return status;
    }

    _Use_decl_annotations_
    DlsParseStatus DlsCollection::Parse(const DlsParseLimits& limits)
    {
        RiffChunkReader fileReader{ std::span<const std::byte>(m_fileBytes) };
        RiffChunk root{};

        if (!fileReader.TryNext(root) || root.Id != FourCC_RIFF)
        {
            return DlsParseStatus::NotRiffFile;
        }

        if (root.ListType != FourCC_DLS)
        {
            return DlsParseStatus::NotDlsCollection;
        }

        // Collect the top level chunks first. The specification orders lins before ptbl and wvpl,
        // but region wave references cannot be resolved until the pool and the waves are both read.
        std::span<const std::byte> instrumentListPayload;
        std::span<const std::byte> wavePoolPayload;
        std::span<const std::byte> poolTablePayload;

        bool sawCollectionHeader = false;

        RiffChunkReader reader(root.Payload);
        RiffChunk chunk{};

        while (reader.TryNext(chunk))
        {
            if (chunk.Id == FourCC_colh)
            {
                ByteReader header(chunk.Payload);

                if (!header.TryReadUInt32(m_declaredInstrumentCount))
                {
                    return DlsParseStatus::MalformedChunk;
                }

                sawCollectionHeader = true;
            }
            else if (chunk.Id == FourCC_vers)
            {
                ByteReader version(chunk.Payload);

                uint32_t versionMostSignificant{};
                uint32_t versionLeastSignificant{};

                if (version.TryReadUInt32(versionMostSignificant) &&
                    version.TryReadUInt32(versionLeastSignificant))
                {
                    m_version.Major = static_cast<uint16_t>(versionMostSignificant >> 16);
                    m_version.Minor = static_cast<uint16_t>(versionMostSignificant & 0xFFFFu);
                    m_version.Release = static_cast<uint16_t>(versionLeastSignificant >> 16);
                    m_version.Build = static_cast<uint16_t>(versionLeastSignificant & 0xFFFFu);
                }
            }
            else if (chunk.Id == FourCC_ptbl)
            {
                poolTablePayload = chunk.Payload;
            }
            else if (chunk.IsList() && chunk.ListType == FourCC_lins)
            {
                instrumentListPayload = chunk.Payload;
            }
            else if (chunk.IsList() && chunk.ListType == FourCC_wvpl)
            {
                wavePoolPayload = chunk.Payload;
            }
            else if (chunk.IsList() && chunk.ListType == FourCC_INFO)
            {
                m_name = ReadInfoListName(chunk.Payload);
            }
        }

        if (!sawCollectionHeader || instrumentListPayload.empty() || wavePoolPayload.empty())
        {
            return DlsParseStatus::MissingRequiredChunk;
        }

        // Waves, recording where each one starts so pool cues can be resolved to an index.
        std::unordered_map<uint32_t, size_t> waveOffsetToIndex;

        {
            RiffChunkReader waveReader(wavePoolPayload);
            RiffChunk waveChunk{};

            while (waveReader.TryNext(waveChunk))
            {
                if (!waveChunk.IsList() || waveChunk.ListType != FourCC_wave)
                {
                    continue;
                }

                if (m_waves.size() >= limits.MaxWaves)
                {
                    return DlsParseStatus::LimitExceeded;
                }

                DlsWave wave{};
                const auto status = ReadWave(waveChunk.Payload, limits, wave);

                if (status != DlsParseStatus::Ok)
                {
                    return status;
                }

                waveOffsetToIndex.emplace(static_cast<uint32_t>(waveChunk.HeaderOffset), m_waves.size());
                m_waves.push_back(std::move(wave));
            }
        }

        // Pool table.
        std::vector<uint32_t> poolCues;

        if (!poolTablePayload.empty())
        {
            ByteReader pool(poolTablePayload);

            uint32_t structureBytes{};
            uint32_t cueCount{};

            if (!pool.TryReadUInt32(structureBytes) || !pool.TryReadUInt32(cueCount))
            {
                return DlsParseStatus::MalformedChunk;
            }

            if (structureBytes < PoolTableHeaderBytes ||
                !pool.TrySkip(structureBytes - PoolTableHeaderBytes))
            {
                return DlsParseStatus::MalformedChunk;
            }

            if (cueCount > limits.MaxWaves || cueCount > pool.Remaining() / PoolCueBytes)
            {
                return DlsParseStatus::MalformedChunk;
            }

            poolCues.reserve(cueCount);

            for (uint32_t i = 0; i < cueCount; i++)
            {
                uint32_t offset{};

                if (!pool.TryReadUInt32(offset))
                {
                    return DlsParseStatus::MalformedChunk;
                }

                poolCues.push_back(offset);
            }
        }

        // Instruments.
        {
            RiffChunkReader instrumentReader(instrumentListPayload);
            RiffChunk instrumentChunk{};

            while (instrumentReader.TryNext(instrumentChunk))
            {
                if (!instrumentChunk.IsList() || instrumentChunk.ListType != FourCC_ins)
                {
                    continue;
                }

                if (m_instruments.size() >= limits.MaxInstruments)
                {
                    return DlsParseStatus::LimitExceeded;
                }

                DlsInstrument instrument{};
                const auto status = ReadInstrument(instrumentChunk.Payload, limits, instrument);

                if (status != DlsParseStatus::Ok)
                {
                    return status;
                }

                m_instruments.push_back(std::move(instrument));
            }
        }

        // Resolve every region's wave reference now, so nothing downstream has to revalidate it.
        for (auto& instrument : m_instruments)
        {
            for (auto& region : instrument.Regions)
            {
                const uint32_t tableIndex = region.WaveLink.TableIndex;

                if (tableIndex >= poolCues.size())
                {
                    return DlsParseStatus::InvalidWaveReference;
                }

                const uint32_t offset = poolCues[tableIndex];

                if (offset == PoolCueNull)
                {
                    return DlsParseStatus::InvalidWaveReference;
                }

                const auto found = waveOffsetToIndex.find(offset);

                if (found == waveOffsetToIndex.end())
                {
                    return DlsParseStatus::InvalidWaveReference;
                }

                region.WaveIndex = found->second;
            }
        }

        return DlsParseStatus::Ok;
    }

    _Use_decl_annotations_
    const DlsInstrument* DlsCollection::FindInstrument(
        uint32_t bankMsb,
        uint32_t bankLsb,
        uint32_t program,
        bool isDrumKit) const noexcept
    {
        for (const auto& instrument : m_instruments)
        {
            if (instrument.IsDrumKit == isDrumKit &&
                instrument.Program == program &&
                instrument.BankMsb == bankMsb &&
                instrument.BankLsb == bankLsb)
            {
                return &instrument;
            }
        }

        return nullptr;
    }
}
