// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

#include "Sf2Types.h"

#include <windows.h>

#include <string>
#include <vector>

namespace SoundFontSynth
{
    enum class Sf2LoadStatus : int32_t
    {
        Ok = 0,
        CannotOpen,
        TooLarge,
        ReadFailed,
        NotRiff,
        NotSoundFont,
        Truncated,
        MissingChunk,
        MalformedChunk,
        BadIndex,
        NoPresets,
        CompressedSamples,
        OutOfMemory,
    };

    // The file is untrusted, so every size it declares is checked against these before anything
    // is allocated for it.
    struct Sf2LoadLimits
    {
        // RIFF sizes are 32 bit, so no well formed file is larger than 4 GiB. Most of a SoundFont
        // is held in memory while it plays, so the default stops well short of that.
        uint64_t MaximumFileBytes{ 2ull * 1024 * 1024 * 1024 };

        // The preset, instrument and sample tables. Real files keep these to a few megabytes.
        uint32_t MaximumHydraBytes{ 64u * 1024 * 1024 };
    };

    // What loading had to leave out, so the customer can be told a file is only partly usable.
    struct Sf2LoadStatistics
    {
        uint32_t PresetsDropped{ 0 };
        uint32_t ZonesDropped{ 0 };
        uint32_t GeneratorsDropped{ 0 };
        uint32_t ModulatorsDropped{ 0 };
        uint32_t SamplesUnusable{ 0 };
        bool Uses24BitSamples{ false };
    };

    struct IByteSource
    {
        virtual ~IByteSource() = default;

        virtual uint64_t Size() const noexcept = 0;

        // Reads exactly count bytes or fails. A short read is a failure.
        virtual bool Read(
            _In_ uint64_t offset,
            _Out_writes_bytes_(count) void* destination,
            _In_ size_t count) noexcept = 0;
    };

    class MemoryByteSource final : public IByteSource
    {
    public:
        MemoryByteSource(_In_reads_bytes_(size) uint8_t const* data, _In_ size_t size) noexcept :
            m_data(data),
            m_size(size)
        {
        }

        uint64_t Size() const noexcept override { return m_size; }

        bool Read(
            _In_ uint64_t offset,
            _Out_writes_bytes_(count) void* destination,
            _In_ size_t count) noexcept override;

    private:
        uint8_t const* m_data{ nullptr };
        size_t m_size{ 0 };
    };

    class FileByteSource final : public IByteSource
    {
    public:
        FileByteSource() noexcept = default;
        ~FileByteSource();

        FileByteSource(FileByteSource const&) = delete;
        FileByteSource& operator=(FileByteSource const&) = delete;

        // Refuses anything that is not a file on disk, so a device or pipe name cannot make a
        // read block forever.
        bool Open(_In_ std::wstring const& path) noexcept;

        uint64_t Size() const noexcept override { return m_size; }

        bool Read(
            _In_ uint64_t offset,
            _Out_writes_bytes_(count) void* destination,
            _In_ size_t count) noexcept override;

    private:
        HANDLE m_file{ INVALID_HANDLE_VALUE };
        uint64_t m_size{ 0 };
    };

    // A loaded SoundFont 2 bank. Immutable once Load returns, so one copy can be shared by every
    // synthesizer that plays it, on any thread.
    class SoundFont
    {
    public:
        static Sf2LoadStatus Load(
            _In_ IByteSource& source,
            _In_ Sf2LoadLimits const& limits,
            _Out_ SoundFont& result,
            _Out_opt_ Sf2LoadStatistics* statistics) noexcept;

        static Sf2LoadStatus LoadFromFile(
            _In_ std::wstring const& path,
            _In_ Sf2LoadLimits const& limits,
            _Out_ SoundFont& result,
            _Out_opt_ Sf2LoadStatistics* statistics) noexcept;

        Sf2Info const& Info() const noexcept { return m_info; }
        std::vector<Sf2Preset> const& Presets() const noexcept { return m_presets; }
        std::vector<Sf2Instrument> const& Instruments() const noexcept { return m_instruments; }
        std::vector<Sf2Sample> const& Samples() const noexcept { return m_samples; }

        std::vector<int16_t> const& SampleData() const noexcept { return m_sampleData; }

        // The low eight bits of 24 bit samples, one per sample point, or empty for a 16 bit bank.
        std::vector<uint8_t> const& SampleData24() const noexcept { return m_sampleData24; }

        // -1 when the bank has no such preset. Bank 128 holds the percussion kits.
        int32_t FindPreset(_In_ uint16_t bank, _In_ uint16_t program) const noexcept;

        static constexpr uint16_t PercussionBank = 128;

    private:
        void BuildPresetLookup();

        Sf2Info m_info{};
        std::vector<Sf2Preset> m_presets{};
        std::vector<Sf2Instrument> m_instruments{};
        std::vector<Sf2Sample> m_samples{};
        std::vector<int16_t> m_sampleData{};
        std::vector<uint8_t> m_sampleData24{};

        // Bank 0 to 128 by program 0 to 127, holding a preset index or -1.
        std::vector<int32_t> m_presetLookup{};

        friend class Sf2Parser;
    };
}
