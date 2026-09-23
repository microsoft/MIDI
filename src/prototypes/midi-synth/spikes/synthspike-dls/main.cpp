// Spike: parse a DLS collection and report what is in it. Used to validate the parser against
// the in-box gm.dls before any synthesis work depends on it.

#include "MidiSynth/DlsCollection.h"
#include "MidiSynth/ProgramList.h"

#include "MidiCiMessage.h"

#include <windows.h>

#include <algorithm>
#include <cstdio>
#include <map>
#include <set>
#include <string>

using namespace MidiSynth;

namespace
{
    std::string ToUtf8(_In_ const std::wstring& value)
    {
        if (value.empty())
        {
            return {};
        }

        const int required = WideCharToMultiByte(
            CP_UTF8, 0, value.c_str(), static_cast<int>(value.size()), nullptr, 0, nullptr, nullptr);

        if (required <= 0)
        {
            return {};
        }

        std::string result(static_cast<size_t>(required), '\0');

        WideCharToMultiByte(
            CP_UTF8, 0, value.c_str(), static_cast<int>(value.size()), result.data(), required, nullptr, nullptr);

        return result;
    }

    std::wstring DefaultDlsPath()
    {
        wchar_t systemDirectory[MAX_PATH]{};

        if (GetSystemDirectoryW(systemDirectory, ARRAYSIZE(systemDirectory)) == 0)
        {
            return L"gm.dls";
        }

        return std::wstring(systemDirectory) + L"\\drivers\\gm.dls";
    }

    void PrintSummary(_In_ const DlsCollection& collection)
    {
        printf("File\n");
        printf("  bytes                 %zu\n", collection.FileByteCount());
        printf("  version               %u.%u.%u.%u\n",
            collection.Version().Major,
            collection.Version().Minor,
            collection.Version().Release,
            collection.Version().Build);

        if (!collection.Name().empty())
        {
            printf("  name                  %s\n", ToUtf8(collection.Name()).c_str());
        }

        printf("  instruments declared   %u\n", collection.DeclaredInstrumentCount());
        printf("  instruments parsed     %zu\n", collection.Instruments().size());
        printf("  waves parsed           %zu\n", collection.Waves().size());
    }

    void PrintWaveStatistics(_In_ const DlsCollection& collection)
    {
        size_t totalSampleBytes = 0;
        size_t loopedWaves = 0;
        std::set<uint32_t> sampleRates;
        std::set<uint16_t> bitDepths;
        std::set<uint16_t> channelCounts;
        std::set<uint16_t> formatTags;

        int32_t minFineTune = INT32_MAX;
        int32_t maxFineTune = INT32_MIN;
        int32_t minAttenuation = INT32_MAX;
        int32_t maxAttenuation = INT32_MIN;
        uint16_t minUnityNote = UINT16_MAX;
        uint16_t maxUnityNote = 0;

        for (const auto& wave : collection.Waves())
        {
            totalSampleBytes += wave.SampleData.size();
            sampleRates.insert(wave.SamplesPerSecond);
            bitDepths.insert(wave.BitsPerSample);
            channelCounts.insert(wave.Channels);
            formatTags.insert(wave.FormatTag);

            if (wave.HasWaveSample)
            {
                minFineTune = (std::min)(minFineTune, static_cast<int32_t>(wave.WaveSample.FineTune));
                maxFineTune = (std::max)(maxFineTune, static_cast<int32_t>(wave.WaveSample.FineTune));
                minAttenuation = (std::min)(minAttenuation, wave.WaveSample.Attenuation);
                maxAttenuation = (std::max)(maxAttenuation, wave.WaveSample.Attenuation);
                minUnityNote = (std::min)(minUnityNote, wave.WaveSample.UnityNote);
                maxUnityNote = (std::max)(maxUnityNote, wave.WaveSample.UnityNote);

                if (!wave.WaveSample.Loops.empty())
                {
                    loopedWaves++;
                }
            }
        }

        printf("\nWave pool\n");
        printf("  total sample bytes     %zu (%.2f MB)\n",
            totalSampleBytes, static_cast<double>(totalSampleBytes) / (1024.0 * 1024.0));
        printf("  looped / one shot      %zu / %zu\n", loopedWaves, collection.Waves().size() - loopedWaves);

        printf("  format tags            ");
        for (const auto tag : formatTags) { printf("%u ", tag); }

        printf("\n  channel counts         ");
        for (const auto channels : channelCounts) { printf("%u ", channels); }

        printf("\n  bit depths             ");
        for (const auto bits : bitDepths) { printf("%u ", bits); }

        printf("\n  sample rates           ");
        for (const auto rate : sampleRates) { printf("%u ", rate); }

        printf("\n  wsmp unity note range  %u to %u\n", minUnityNote, maxUnityNote);
        printf("  wsmp fine tune range   %d to %d\n", minFineTune, maxFineTune);
        printf("  wsmp attenuation range %d to %d (%.2f to %.2f dB at 1/655360 dB per unit)\n",
            minAttenuation, maxAttenuation,
            minAttenuation / 655360.0, maxAttenuation / 655360.0);
    }

    void PrintInstrumentStatistics(_In_ const DlsCollection& collection)
    {
        size_t melodic = 0;
        size_t drumKits = 0;
        size_t totalRegions = 0;
        size_t maxRegions = 0;
        size_t instrumentsWithArticulation = 0;
        size_t regionsWithArticulation = 0;
        std::set<uint32_t> melodicBanks;

        for (const auto& instrument : collection.Instruments())
        {
            if (instrument.IsDrumKit)
            {
                drumKits++;
            }
            else
            {
                melodic++;
                melodicBanks.insert((instrument.BankMsb << 8) | instrument.BankLsb);
            }

            totalRegions += instrument.Regions.size();
            maxRegions = (std::max)(maxRegions, instrument.Regions.size());

            if (!instrument.Connections.empty())
            {
                instrumentsWithArticulation++;
            }

            for (const auto& region : instrument.Regions)
            {
                if (!region.Connections.empty())
                {
                    regionsWithArticulation++;
                }
            }
        }

        printf("\nInstruments\n");
        printf("  melodic / drum kits    %zu / %zu\n", melodic, drumKits);
        printf("  regions total / max    %zu / %zu\n", totalRegions, maxRegions);
        printf("  with articulation      %zu instruments, %zu regions\n",
            instrumentsWithArticulation, regionsWithArticulation);

        printf("  melodic banks (msb:lsb) ");
        for (const auto bank : melodicBanks) { printf("%u:%u ", bank >> 8, bank & 0xFFu); }
        printf("\n");
    }

    void PrintInstrumentList(_In_ const DlsCollection& collection)
    {
        printf("\n%-5s %-5s %-5s %-4s %-7s %s\n", "index", "bank", "bank", "prog", "regions", "name");
        printf("%-5s %-5s %-5s %-4s %-7s %s\n", "", "msb", "lsb", "", "", "");

        size_t index = 0;

        for (const auto& instrument : collection.Instruments())
        {
            printf("%-5zu %-5u %-5u %-4u %-7zu %s%s\n",
                index,
                instrument.BankMsb,
                instrument.BankLsb,
                instrument.Program,
                instrument.Regions.size(),
                ToUtf8(instrument.Name).c_str(),
                instrument.IsDrumKit ? "   [drum kit]" : "");

            index++;
        }
    }

    // A note whose key and velocity match no region produces no sound at all, silently. That is
    // indistinguishable from latency when playing, so it is worth proving it cannot happen.
    void PrintCoverage(_In_ const DlsCollection& collection)
    {
        printf("\nRegion coverage\n");

        size_t instrumentsWithGaps = 0;
        size_t totalUncoveredCells = 0;

        for (size_t index = 0; index < collection.Instruments().size(); index++)
        {
            const auto& instrument = collection.Instruments()[index];

            std::vector<bool> covered(128 * 128, false);

            for (const auto& region : instrument.Regions)
            {
                const auto keyLow = (std::min)(region.KeyLow, static_cast<uint16_t>(127));
                const auto keyHigh = (std::min)(region.KeyHigh, static_cast<uint16_t>(127));
                const auto velocityLow = (std::min)(region.VelocityLow, static_cast<uint16_t>(127));
                const auto velocityHigh = (std::min)(region.VelocityHigh, static_cast<uint16_t>(127));

                for (uint16_t key = keyLow; key <= keyHigh; key++)
                {
                    for (uint16_t velocity = velocityLow; velocity <= velocityHigh; velocity++)
                    {
                        covered[static_cast<size_t>(key) * 128 + velocity] = true;
                    }
                }
            }

            size_t uncovered = 0;
            int unplayableKeys = 0;
            int firstPlayableKey = -1;
            int lastPlayableKey = -1;
            int partialKeys = 0;

            // Velocity zero is a note off by convention, so it is not a playable gap.
            for (uint16_t key = 0; key < 128; key++)
            {
                int covedVelocities = 0;

                for (uint16_t velocity = 1; velocity < 128; velocity++)
                {
                    if (covered[static_cast<size_t>(key) * 128 + velocity])
                    {
                        covedVelocities++;
                    }
                    else
                    {
                        uncovered++;
                    }
                }

                if (covedVelocities == 0)
                {
                    unplayableKeys++;
                }
                else
                {
                    if (firstPlayableKey < 0)
                    {
                        firstPlayableKey = key;
                    }

                    lastPlayableKey = key;

                    if (covedVelocities < 127)
                    {
                        partialKeys++;
                    }
                }
            }

            if (uncovered > 0)
            {
                instrumentsWithGaps++;
                totalUncoveredCells += uncovered;

                if (instrumentsWithGaps <= 12)
                {
                    printf("  %-4zu %-24s plays keys %d-%d, %d keys silent, %d partial%s\n",
                        index,
                        ToUtf8(instrument.Name).c_str(),
                        firstPlayableKey,
                        lastPlayableKey,
                        unplayableKeys,
                        partialKeys,
                        instrument.IsDrumKit ? "  [drum kit, expected]" : "  [MELODIC, INVESTIGATE]");
                }
            }
        }

        printf("\n  instruments with gaps        %zu of %zu\n",
            instrumentsWithGaps, collection.Instruments().size());
        printf("  uncovered key/velocity cells %zu\n", totalUncoveredCells);

        if (instrumentsWithGaps > 12)
        {
            printf("  (only the first 12 listed)\n");
        }
    }

    void PrintRegions(_In_ const DlsCollection& collection, _In_ size_t instrumentIndex)
    {
        if (instrumentIndex >= collection.Instruments().size())
        {
            printf("error: instrument index %zu is out of range (0 to %zu)\n",
                instrumentIndex, collection.Instruments().size() - 1);
            return;
        }

        const auto& instrument = collection.Instruments()[instrumentIndex];

        printf("\nInstrument %zu: %s%s\n",
            instrumentIndex,
            ToUtf8(instrument.Name).c_str(),
            instrument.IsDrumKit ? " [drum kit]" : "");
        printf("  bank %u:%u  program %u  connections %zu\n",
            instrument.BankMsb, instrument.BankLsb, instrument.Program, instrument.Connections.size());

        printf("\n  %-4s %-9s %-9s %-6s %-6s %-8s %-7s %-6s %s\n",
            "rgn", "keys", "velocity", "group", "wave", "rate", "frames", "loops", "unity/fine");

        size_t regionIndex = 0;

        for (const auto& region : instrument.Regions)
        {
            const auto& wave = collection.Waves()[region.WaveIndex];

            const DlsWaveSample& waveSample =
                region.HasWaveSample ? region.WaveSample : wave.WaveSample;

            char keys[16]{};
            char velocity[16]{};
            (void)snprintf(keys, sizeof(keys), "%u-%u", region.KeyLow, region.KeyHigh);
            (void)snprintf(velocity, sizeof(velocity), "%u-%u", region.VelocityLow, region.VelocityHigh);

            printf("  %-4zu %-9s %-9s %-6u %-6zu %-8u %-7u %-6zu %u/%d\n",
                regionIndex,
                keys,
                velocity,
                region.KeyGroup,
                region.WaveIndex,
                wave.SamplesPerSecond,
                wave.FrameCount(),
                waveSample.Loops.size(),
                waveSample.UnityNote,
                waveSample.FineTune);

            regionIndex++;
        }
    }
}

int wmain(int argc, wchar_t** argv)
{
    SetConsoleOutputCP(CP_UTF8);

    std::wstring path;
    bool listInstruments = false;
    bool showWaves = false;
    bool showCoverage = false;
    bool showProgramList = false;
    bool haveRegionIndex = false;
    size_t regionIndex = 0;

    for (int i = 1; i < argc; i++)
    {
        const std::wstring argument = argv[i];

        if (argument == L"--instruments")
        {
            listInstruments = true;
        }
        else if (argument == L"--waves")
        {
            showWaves = true;
        }
        else if (argument == L"--coverage")
        {
            showCoverage = true;
        }
        else if (argument == L"--programlist")
        {
            showProgramList = true;
        }
        else if (argument == L"--regions" && i + 1 < argc)
        {
            regionIndex = static_cast<size_t>(_wtoi64(argv[++i]));
            haveRegionIndex = true;
        }
        else if (argument.rfind(L"--", 0) == 0)
        {
            printf("usage: synthspike-dls [path.dls] [--instruments] [--waves] [--coverage]\n");
            printf("                      [--regions <index>] [--programlist]\n");
            return 2;
        }
        else if (path.empty())
        {
            path = argument;
        }
    }

    if (path.empty())
    {
        path = DefaultDlsPath();
    }

    // Proves the shipping policy rejects anything outside the system directory.
    {
        DlsSoundSetInfo systemInfo;

        const auto systemStatus =
            DlsCollection::ProbeFile(path, DlsParseLimits{}, SoundSetOrigin::SystemOnly, systemInfo);

        printf("System only policy: %s\n\n", DlsParseStatusToString(systemStatus));
    }

    printf("Reading %s\n\n", ToUtf8(path).c_str());

    const DlsParseLimits limits{};

    // Metadata only, which is what a settings app listing installed sound sets would call.
    {
        DlsSoundSetInfo info;

        const LARGE_INTEGER probeFrequency = [] { LARGE_INTEGER f{}; QueryPerformanceFrequency(&f); return f; }();
        LARGE_INTEGER probeStart{};
        LARGE_INTEGER probeEnd{};

        QueryPerformanceCounter(&probeStart);
        const auto probeStatus = DlsCollection::ProbeFile(path, limits, SoundSetOrigin::AnyPath, info);
        QueryPerformanceCounter(&probeEnd);

        const double probeMilliseconds = 1000.0
            * static_cast<double>(probeEnd.QuadPart - probeStart.QuadPart)
            / static_cast<double>(probeFrequency.QuadPart);

        if (probeStatus == DlsParseStatus::Ok)
        {
            printf("Probe (header only, %.2f ms)\n", probeMilliseconds);
            printf("  name          %s\n", ToUtf8(info.Name).c_str());
            printf("  version       %u.%u.%u.%u\n",
                info.Version.Major, info.Version.Minor, info.Version.Release, info.Version.Build);
            printf("  instruments   %u\n", info.InstrumentCount);
            printf("  file bytes    %llu\n\n", info.FileBytes);
        }
        else
        {
            printf("Probe failed: %s\n\n", DlsParseStatusToString(probeStatus));
        }
    }

    DlsCollection collection;

    const LARGE_INTEGER frequency = [] { LARGE_INTEGER f{}; QueryPerformanceFrequency(&f); return f; }();
    LARGE_INTEGER start{};
    LARGE_INTEGER end{};

    QueryPerformanceCounter(&start);
    const auto status = DlsCollection::LoadFromFile(path, limits, SoundSetOrigin::AnyPath, collection);
    QueryPerformanceCounter(&end);

    if (status != DlsParseStatus::Ok)
    {
        printf("parse failed: %s\n", DlsParseStatusToString(status));
        return 1;
    }

    const double elapsedMilliseconds =
        1000.0 * static_cast<double>(end.QuadPart - start.QuadPart) / static_cast<double>(frequency.QuadPart);

    printf("Parsed in %.2f ms\n\n", elapsedMilliseconds);

    PrintSummary(collection);
    PrintInstrumentStatistics(collection);

    if (showWaves)
    {
        PrintWaveStatistics(collection);
    }

    if (showCoverage)
    {
        PrintCoverage(collection);
    }

    if (listInstruments)
    {
        PrintInstrumentList(collection);
    }

    if (haveRegionIndex)
    {
        PrintRegions(collection, regionIndex);
    }

    if (showProgramList)
    {
        const auto json = BuildProgramListJson(collection, ProgramListKind::Melodic);

        printf("\nProperty Exchange ProgramList\n");
        printf("  bytes                 %zu\n", json.size());

        // What a responder would have to do to answer a device that declared 512 bytes.
        const auto perChunk = WindowsMidiServicesCapabilityInquiry::MaximumPropertyDataBytesPerChunk(512, 14);
        const auto chunks = WindowsMidiServicesCapabilityInquiry::ChunkCountForDataSize(json.size(), perChunk);

        printf("  chunks at 512 bytes   %u  (%u data bytes each)\n", chunks, perChunk);

        size_t nonAscii = 0;

        for (const auto character : json)
        {
            if (static_cast<uint8_t>(character) > 0x7F) { nonAscii++; }
        }

        printf("  bytes over 0x7F       %zu  (any would truncate the system exclusive)\n", nonAscii);

        printf("\n  first 400 bytes\n    %.400s\n", json.data());
    }

    return 0;
}
