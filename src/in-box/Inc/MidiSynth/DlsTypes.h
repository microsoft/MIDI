// Parsed model of a DLS Level 1 collection, per the MMA "Downloadable Sounds Level 1"
// specification (version 1.1). DLS Level 2 files parse as far as their Level 1 subset.

#pragma once

#include <sal.h>

#include <cstdint>
#include <span>
#include <string>
#include <vector>

namespace MidiSynth
{
    enum class DlsParseStatus
    {
        Ok,
        FileNotFound,
        FileTooLarge,
        ReadError,
        NotRiffFile,
        NotDlsCollection,
        MalformedChunk,
        MissingRequiredChunk,
        LimitExceeded,
        UnsupportedWaveFormat,
        InvalidWaveReference,

        // The file is readable but the configured origin policy does not allow it.
        NotPermitted,
    };

    const char* DlsParseStatusToString(_In_ DlsParseStatus status) noexcept;

    // Caps on attacker-controlled counts. A conforming file is far below every one of these;
    // they exist so a malformed file fails fast instead of driving an allocation.
    struct DlsParseLimits
    {
        size_t MaxFileBytes{ 512u * 1024u * 1024u };
        size_t MaxInstruments{ 8192 };
        size_t MaxRegionsPerInstrument{ 8192 };
        size_t MaxConnections{ 4096 };
        size_t MaxSampleLoops{ 8 };
        size_t MaxWaves{ 65536 };
    };

    // art1/art2 connection block. Source, control, destination and transform are the
    // CONN_SRC_*, CONN_DST_* and CONN_TRN_* values from the specification.
    struct DlsConnection
    {
        uint16_t Source{ 0 };
        uint16_t Control{ 0 };
        uint16_t Destination{ 0 };
        uint16_t Transform{ 0 };
        int32_t Scale{ 0 };
    };

    struct DlsSampleLoop
    {
        uint32_t LoopType{ 0 };     // WLOOP_TYPE_FORWARD == 0
        uint32_t LoopStart{ 0 };    // in samples, from the start of the wave data
        uint32_t LoopLength{ 0 };   // in samples
    };

    // wsmp. Present on a wave, and optionally overridden per region.
    struct DlsWaveSample
    {
        uint16_t UnityNote{ 60 };
        int16_t FineTune{ 0 };          // relative pitch, 1/65536 semitone units
        int32_t Attenuation{ 0 };       // relative gain, 1/655360 dB units
        uint32_t Options{ 0 };
        std::vector<DlsSampleLoop> Loops;
    };

    // wlnk
    struct DlsWaveLink
    {
        uint16_t Options{ 0 };
        uint16_t PhaseGroup{ 0 };
        uint32_t Channel{ 0 };
        uint32_t TableIndex{ 0 };       // index into the pool table, zero based
    };

    struct DlsRegion
    {
        uint16_t KeyLow{ 0 };
        uint16_t KeyHigh{ 127 };
        uint16_t VelocityLow{ 0 };
        uint16_t VelocityHigh{ 127 };
        uint16_t Options{ 0 };
        uint16_t KeyGroup{ 0 };

        bool HasWaveSample{ false };
        DlsWaveSample WaveSample;

        DlsWaveLink WaveLink;

        // Region-level articulation. Empty for most melodic regions, which inherit the
        // instrument-level list instead.
        std::vector<DlsConnection> Connections;

        // Resolved during parse from WaveLink.TableIndex. Indexes DlsCollection::Waves.
        size_t WaveIndex{ 0 };
    };

    struct DlsInstrument
    {
        uint32_t BankMsb{ 0 };          // CC0
        uint32_t BankLsb{ 0 };          // CC32
        uint32_t Program{ 0 };
        bool IsDrumKit{ false };

        std::wstring Name;

        std::vector<DlsRegion> Regions;
        std::vector<DlsConnection> Connections;
    };

    struct DlsWave
    {
        uint16_t FormatTag{ 0 };
        uint16_t Channels{ 0 };
        uint32_t SamplesPerSecond{ 0 };
        uint32_t AverageBytesPerSecond{ 0 };
        uint16_t BlockAlign{ 0 };
        uint16_t BitsPerSample{ 0 };

        bool HasWaveSample{ false };
        DlsWaveSample WaveSample;

        // Points into the owning collection's file buffer. Valid for the lifetime of
        // the DlsCollection that produced it.
        std::span<const std::byte> SampleData;

        std::wstring Name;

        uint32_t FrameCount() const noexcept
        {
            const uint32_t bytesPerFrame = static_cast<uint32_t>(Channels) * (BitsPerSample / 8u);
            return bytesPerFrame == 0 ? 0u : static_cast<uint32_t>(SampleData.size() / bytesPerFrame);
        }
    };

    struct DlsVersion
    {
        uint16_t Major{ 0 };
        uint16_t Minor{ 0 };
        uint16_t Release{ 0 };
        uint16_t Build{ 0 };
    };
}
