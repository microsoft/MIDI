// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================
// MIDI Glass phase 0 spike. Nothing here ships.

#pragma once

namespace gspike
{
    // QueryPerformanceCounter in microseconds. The frequency is read once.
    int64_t NowMicroseconds() noexcept;
    int64_t QpcFrequency() noexcept;
    int64_t QpcToMicroseconds(int64_t counter) noexcept;

    // A fixed-capacity sample set. Fills once, never reallocates, so the act of measuring does
    // not show up in what is being measured.
    class SampleSet
    {
    public:
        explicit SampleSet(size_t capacity);

        void Add(double value) noexcept;
        void Clear() noexcept;

        size_t Count() const noexcept { return m_count; }
        bool Overflowed() const noexcept { return m_overflowed; }

        // Sorts a copy, so calling this does not disturb the order samples arrived in.
        double Percentile(double fraction) const;
        double Mean() const noexcept;
        double Max() const noexcept;
        size_t CountAbove(double threshold) const noexcept;

    private:
        std::vector<double> m_values;
        size_t m_count{ 0 };
        bool m_overflowed{ false };
    };

    struct MemorySnapshot
    {
        uint64_t PrivateBytes{};
        uint64_t WorkingSetBytes{};
    };

    MemorySnapshot TakeMemorySnapshot() noexcept;

    // How busy the whole machine was, and how much processor this process actually got. A run
    // taken while somebody was using the computer is not evidence, and the spike should be able
    // to say so rather than leaving it to be remembered.
    struct CpuSnapshot
    {
        uint64_t SystemIdle{};
        uint64_t SystemKernel{};
        uint64_t SystemUser{};
        uint64_t ProcessKernel{};
        uint64_t ProcessUser{};
    };

    CpuSnapshot TakeCpuSnapshot() noexcept;

    // 0..1. How much of the machine was doing something other than idling, across the interval.
    double SystemBusyFraction(CpuSnapshot const& start, CpuSnapshot const& end) noexcept;

    // Milliseconds of processor time this process used across the interval.
    double ProcessCpuMilliseconds(CpuSnapshot const& start, CpuSnapshot const& end) noexcept;

    // What one run produced. Everything is filled in by MainWindow; nothing computes itself.
    struct RunResult
    {
        std::wstring Mode;
        uint32_t ControlCount{};
        uint32_t AnimatedCount{};
        double DurationSeconds{};

        double BuildMilliseconds{};
        double FirstLayoutMilliseconds{};
        uint32_t XamlElementCount{};
        bool HasAutomationPeers{ false };

        double FrameP50{};
        double FrameP95{};
        double FrameP99{};
        double FrameMax{};
        size_t FrameCount{};
        size_t FramesOver20Ms{};

        double AnimateP50Microseconds{};
        double AnimateP99Microseconds{};
        double SetValueP50Microseconds{};
        double SetValueP99Microseconds{};

        uint64_t PrivateBytesBaseline{};
        uint64_t PrivateBytesAfterBuild{};
        uint64_t WorkingSetAfterBuild{};

        double ThemeSwapMilliseconds{ -1.0 };
        bool ThemeSwapReachedEveryControl{ false };

        // Whether the run is worth believing.
        double SystemBusyPercent{};
        double ProcessCpuMilliseconds{};
        bool LostActivation{ false };
        bool Clean{ false };

        // Input path. -1 means it was never exercised, which is not the same as zero.
        double SendP50Microseconds{ -1.0 };
        double SendP99Microseconds{ -1.0 };
        double PointerToSendP50Microseconds{ -1.0 };
        double PointerToSendP99Microseconds{ -1.0 };
        double HandlerToSendP50Microseconds{ -1.0 };
        double HandlerToSendP99Microseconds{ -1.0 };
        size_t PointerSampleCount{ 0 };
        std::wstring PointerDeviceTypes;
        bool PointerTimestampTrusted{ false };
        std::wstring LatencyEndpoint;
        std::wstring LatencyNote;
    };

    std::wstring FormatResultJson(RunResult const& result);
    std::wstring FormatResultText(RunResult const& result);
    void AppendLineToFile(std::wstring const& path, std::wstring const& line);
}
