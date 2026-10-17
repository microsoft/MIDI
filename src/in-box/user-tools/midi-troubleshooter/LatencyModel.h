// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

// What the DPC latency check found. Plain C++ and the standard library only, so the engine can
// be built and tested without the app around it.
//
// Times are in the trace's own clock (QueryPerformanceCounter ticks for a live check), and
// durations are ticks of that clock. Use TicksToMicroseconds with Results::Trace.Frequency.

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

#if defined(_MSC_VER)
#include <sal.h>
#endif

namespace miditroubleshooter::latency
{
    // Upper limits of the duration buckets, in microseconds. One more bucket holds everything
    // longer than the last limit.
    inline constexpr std::array<uint64_t, 11> DurationBucketLimits{ 10, 25, 50, 100, 250, 500, 1000, 2000, 4000, 8000, 16000 };
    inline constexpr size_t DurationBucketCount{ DurationBucketLimits.size() + 1 };

    using DurationHistogram = std::array<uint64_t, DurationBucketCount>;

    // An audio thread held up for this long or longer is a late moment.
    inline constexpr uint64_t LateMomentMicroseconds{ 1000 };

    // The test thread puts each wake that's at least this late in the trace, with when it should
    // have woken, so the check can see what held up its timer.
    inline constexpr uint64_t ProbeLateWakeMicroseconds{ 500 };

    // How many of the longest waits are kept, each with what caused it.
    inline constexpr size_t MaximumLongestWaits{ 40 };

    inline uint64_t TicksToMicroseconds(_In_ uint64_t const ticks, _In_ uint64_t const frequency) noexcept
    {
        if (frequency == 0)
        {
            return 0;
        }

        // split, so minutes of ticks at 10 MHz can't overflow
        return (ticks / frequency) * 1'000'000 + ((ticks % frequency) * 1'000'000) / frequency;
    }

    inline uint64_t MicrosecondsToTicks(_In_ uint64_t const microseconds, _In_ uint64_t const frequency) noexcept
    {
        return (microseconds / 1'000'000) * frequency + ((microseconds % 1'000'000) * frequency) / 1'000'000;
    }

    inline size_t DurationBucket(_In_ uint64_t const microseconds) noexcept
    {
        for (size_t index = 0; index < DurationBucketLimits.size(); ++index)
        {
            if (microseconds <= DurationBucketLimits[index])
            {
                return index;
            }
        }

        return DurationBucketLimits.size();
    }

    struct DurationStats
    {
        uint64_t Count{ 0 };
        uint64_t TotalTicks{ 0 };
        uint64_t MaxTicks{ 0 };

        // when the longest one ended, and where
        uint64_t MaxTime{ 0 };
        uint32_t MaxProcessor{ 0 };

        DurationHistogram Histogram{};

        void Add(_In_ uint64_t const ticks, _In_ uint64_t const microseconds, _In_ uint64_t const endTime, _In_ uint32_t const processor) noexcept
        {
            ++Count;
            TotalTicks += ticks;

            if (ticks > MaxTicks || Count == 1)
            {
                MaxTicks = ticks;
                MaxTime = endTime;
                MaxProcessor = processor;
            }

            ++Histogram[DurationBucket(microseconds)];
        }

        void Merge(_In_ DurationStats const& other) noexcept
        {
            if (other.Count == 0)
            {
                return;
            }

            if (Count == 0 || other.MaxTicks > MaxTicks)
            {
                MaxTicks = other.MaxTicks;
                MaxTime = other.MaxTime;
                MaxProcessor = other.MaxProcessor;
            }

            Count += other.Count;
            TotalTicks += other.TotalTicks;

            for (size_t index = 0; index < Histogram.size(); ++index)
            {
                Histogram[index] += other.Histogram[index];
            }
        }

        // how many took longer than the limit, counting whole buckets above it
        uint64_t CountOver(_In_ uint64_t const microseconds) const noexcept
        {
            uint64_t count{ 0 };

            for (size_t index = 0; index < Histogram.size(); ++index)
            {
                auto const lower = index == 0 ? 0 : DurationBucketLimits[index - 1];

                if (lower >= microseconds)
                {
                    count += Histogram[index];
                }
            }

            return count;
        }
    };

    // Time on each processor, as pairs of processor and ticks for only the processors that had
    // any, so a damaged trace naming thousands of processors can't make it huge.
    using ProcessorTicks = std::vector<std::pair<uint32_t, uint64_t>>;

    inline uint64_t* FindProcessorTicks(_Inout_ ProcessorTicks& list, _In_ uint32_t const processor) noexcept
    {
        for (auto& entry : list)
        {
            if (entry.first == processor)
            {
                return &entry.second;
            }
        }

        return nullptr;
    }

    // What a driver is for, which decides the advice. Stable names for each are in the report.
    enum class DriverKind : uint8_t
    {
        Unknown,
        WindowsKernel,
        Hypervisor,
        NetworkStack,
        WiFi,
        Ethernet,
        Bluetooth,
        Graphics,
        Storage,
        UsbController,
        Audio,
        Power,
        Security,
        Input,
        Other
    };

    struct ModuleInfo
    {
        // as the trace recorded it, such as nvlddmkm.sys
        std::wstring FileName{};

        // as the trace recorded it, such as \SystemRoot\System32\drivers\nvlddmkm.sys
        std::wstring TracePath{};

        uint64_t Base{ 0 };
        uint64_t Size{ 0 };
        uint32_t Checksum{ 0 };
        uint32_t TimeDateStamp{ 0 };

        // The rest comes from the driver file on this PC, and only when it's the same file the
        // trace saw. A trace from another PC keeps just the names.
        bool LocalFileMatches{ false };
        std::wstring LocalPath{};
        std::wstring Company{};
        std::wstring Description{};
        std::wstring Version{};

        // the devices that use this driver, and the device class of the first one
        std::vector<std::wstring> Devices{};
        std::wstring DeviceClass{};

        DriverKind Kind{ DriverKind::Unknown };

        // Runs other drivers' code, such as the driver framework, NDIS or Storport. Time spent
        // in one of these is charged to the driver it ran for, when that can be found.
        bool IsFramework{ false };

        bool IsMicrosoft{ false };
    };

    enum class InterruptKind : uint8_t
    {
        Dpc,
        ThreadedDpc,
        TimerDpc,
        Isr
    };

    // One DPC or interrupt service routine.
    struct RoutineStats
    {
        // the routine the kernel called
        uint64_t Routine{ 0 };

        // The routine a framework called in turn, from the framework's own events. Zero when
        // there were none.
        uint64_t ClientRoutine{ 0 };

        int32_t Module{ -1 };
        int32_t ClientModule{ -1 };

        InterruptKind Kind{ InterruptKind::Dpc };

        DurationStats Durations{};

        // time spent in it on each processor
        ProcessorTicks TicksPerProcessor{};

        // Profile samples taken while this routine ran, counted by the module whose code was
        // running. Index into Results::Modules, then the count. -1 is code outside any module.
        std::vector<std::pair<int32_t, uint32_t>> SampledModules{};

        // The module charged for this routine: the client, else the module the samples point to,
        // else the module of the routine itself.
        int32_t Driver{ -1 };

        // how the charged module was chosen
        bool DriverFromSamples{ false };
    };

    struct DriverStats
    {
        int32_t Module{ -1 };

        DurationStats Dpc{};
        DurationStats Isr{};

        // A thread running this driver's code held a processor where timers and DPCs wait,
        // outside of any DPC or interrupt. Only the test thread's late timer shows this.
        DurationStats RaisedIrql{};

        // frameworks this driver's code ran through, index into Results::Modules
        std::vector<int32_t> Frameworks{};

        // DPC and ISR time on each processor, in processor order
        ProcessorTicks TicksPerProcessor{};

        // late moments where this driver was the biggest cause, and the longest of them
        uint64_t LateMoments{ 0 };
        uint64_t WorstWaitTicks{ 0 };
    };

    enum class CauseKind : uint8_t
    {
        Dpc,
        Isr,
        Thread,
        ProcessorIdle,

        // a thread that held the processor where timers and DPCs wait, outside of any DPC or
        // interrupt
        RaisedIrql,

        Unknown
    };

    struct WaitCause
    {
        CauseKind Kind{ CauseKind::Unknown };

        // how much of the wait this accounts for
        uint64_t Ticks{ 0 };

        // A DPC or ISR: index into Results::Routines, and the module charged for it. RaisedIrql:
        // the module the profile samples caught running.
        int32_t Routine{ -1 };
        int32_t Driver{ -1 };

        // another thread that ran instead, or the thread that held the processor
        uint32_t ThreadId{ 0 };
        uint32_t ProcessId{ 0 };
        int32_t Priority{ 0 };

        // the processor was asleep in this C-state, where 0 is the lightest
        uint32_t IdleState{ 0 };
    };

    enum class MomentKind : uint8_t
    {
        // ready to run, and kept waiting
        WaitedToRun,

        // running, and stopped by a DPC or an interrupt
        Interrupted,

        // The test thread's timer went off late, because the processor that runs the timer was
        // busy. An audio app's thread is held up the same way when its audio device's interrupt
        // is answered late.
        WokeLate
    };

    struct LateMoment
    {
        MomentKind Kind{ MomentKind::WaitedToRun };

        uint64_t StartTime{ 0 };
        uint64_t WaitTicks{ 0 };

        // where the thread ran, and where it was made ready
        uint32_t Processor{ 0 };
        uint32_t ReadyProcessor{ 0 };

        uint32_t ThreadId{ 0 };
        uint32_t ProcessId{ 0 };

        // biggest first
        std::vector<WaitCause> Causes{};
    };

    enum class AudioThreadKind : uint8_t
    {
        // the check's own test thread
        Probe,

        // A thread that ran at real-time priority, 16 or higher, nearly every time it ran. An
        // ordinary app only gets there through the Multimedia Class Scheduler Service (MMCSS),
        // which audio apps use for the threads that make their sound.
        Audio,

        // the same, in the Windows MIDI Services service
        MidiService
    };

    struct AudioThreadStats
    {
        uint32_t ThreadId{ 0 };
        uint32_t ProcessId{ 0 };
        std::wstring ThreadName{};

        AudioThreadKind Kind{ AudioThreadKind::Audio };

        int32_t HighestPriority{ 0 };

        // the priority it ran at most often
        int32_t TypicalPriority{ 0 };

        // how many times it started running, and how many of those were at real-time priority
        uint64_t Runs{ 0 };
        uint64_t RealTimeRuns{ 0 };

        // ready to running
        DurationStats Waits{};

        // stopped by a DPC or an interrupt while running
        DurationStats Interruptions{};

        // waited for the disk
        DurationStats HardFaults{};

        // the test thread only: how late its timer went off, for the wakes that were late
        DurationStats LateWakes{};

        // The test thread only: late wakes where the processor that runs the timer ran one
        // thread and took no profile samples, so the trace can't show what held it.
        DurationStats UnseenStalls{};

        // held up for LateMomentMicroseconds or more, and the longest of those
        uint64_t LateMoments{ 0 };
        uint64_t WorstLateTicks{ 0 };

        bool IsProbe() const noexcept { return Kind == AudioThreadKind::Probe; }
    };

    struct ProcessInfo
    {
        uint32_t ProcessId{ 0 };
        std::wstring Name{};
    };

    struct ProcessorStats
    {
        uint32_t Index{ 0 };
        DurationStats Dpc{};
        DurationStats Isr{};
        uint64_t Samples{ 0 };

        // 0 is the most efficient kind of core. Only known for a check run on this PC.
        uint32_t EfficiencyClass{ 0 };
        bool EfficiencyClassKnown{ false };
    };

    struct ProbeResults
    {
        bool Ran{ false };

        // Running with MMCSS "Pro Audio" priority, like an audio app's audio thread. Without
        // it, the thread ran at the highest ordinary priority instead.
        bool UsedMmcss{ false };
        uint32_t MmcssError{ 0 };

        // Woken by a high-resolution timer. Without one, the wake times are only as exact as the
        // system timer, so how late each wake was says little.
        bool HighResolutionTimer{ false };

        // why it couldn't run, as a Win32 error
        uint32_t Error{ 0 };

        uint32_t ThreadId{ 0 };
        uint32_t PeriodMicroseconds{ 1000 };
        uint64_t Wakes{ 0 };

        // periods it skipped because it was still running late from the one before
        uint64_t MissedWakes{ 0 };

        // How much later than asked for each wake was, in ticks of the trace's clock. Every
        // timer wakes a little late, so the median is what's normal for this PC, and the
        // difference between it and the longest is what a driver added.
        DurationStats Lateness{};
        uint64_t MedianLatenessMicroseconds{ 0 };
        uint64_t Percentile99LatenessMicroseconds{ 0 };
    };

    struct TraceInfo
    {
        // recorded by this app just now, rather than read from a file
        bool Live{ false };
        std::wstring SourcePath{};

        uint64_t Frequency{ 0 };
        uint64_t StartTime{ 0 };
        uint64_t EndTime{ 0 };
        uint32_t ProcessorCount{ 0 };
        uint32_t PointerSize{ 8 };

        // 1 is QueryPerformanceCounter, 2 is system time, 3 is the CPU cycle counter
        uint32_t ClockType{ 1 };

        uint32_t WindowsBuild{ 0 };

        uint64_t EventsProcessed{ 0 };
        uint64_t EventsLost{ 0 };
        uint64_t BuffersLost{ 0 };
        uint64_t RealTimeBuffersLost{ 0 };

        // events too damaged to use, such as one from a processor number no PC has
        uint64_t EventsSkipped{ 0 };

        // The check ran out of memory while working out the results, so they're missing parts.
        bool AnalysisIncomplete{ false };

        uint64_t Dpcs{ 0 };
        uint64_t Interrupts{ 0 };
        uint64_t Samples{ 0 };
        uint64_t ContextSwitches{ 0 };
        uint64_t ReadyThreads{ 0 };
        uint64_t FrameworkEvents{ 0 };
        uint64_t HardFaults{ 0 };

        // events that came in too late to be put in time order with the rest
        uint64_t OutOfOrder{ 0 };

        // set when the trace had none of these, so the report can say what it couldn't check
        bool HasDpcs() const noexcept { return Dpcs > 0; }
        bool HasScheduling() const noexcept { return ContextSwitches > 0 && ReadyThreads > 0; }
    };

    struct Results
    {
        TraceInfo Trace{};
        ProbeResults Probe{};

        std::vector<ModuleInfo> Modules{};
        std::vector<RoutineStats> Routines{};

        // the drivers that held a processor longest first
        std::vector<DriverStats> Drivers{};

        std::vector<ProcessorStats> Processors{};
        std::vector<ProcessInfo> Processes{};
        std::vector<AudioThreadStats> AudioThreads{};

        // Threads that ran at real-time priority only some of the time, so they weren't counted
        // as audio threads. Windows lifts an ordinary thread for a moment when a real-time thread
        // is waiting on something it holds. The ones that ran that way most, for the report.
        std::vector<AudioThreadStats> OtherRealTimeThreads{};
        uint64_t OtherRealTimeThreadCount{ 0 };

        // longest first
        std::vector<LateMoment> LongestWaits{};

        // every wait and interruption of every audio thread
        DurationStats AllWaits{};
        DurationStats AllInterruptions{};

        // waits and interruptions of LateMomentMicroseconds or more
        uint64_t LateMomentCount{ 0 };

        uint64_t Microseconds(_In_ uint64_t const ticks) const noexcept
        {
            return TicksToMicroseconds(ticks, Trace.Frequency);
        }

        double Seconds() const noexcept
        {
            return Trace.Frequency == 0 || Trace.EndTime <= Trace.StartTime ?
                0.0 :
                static_cast<double>(Trace.EndTime - Trace.StartTime) / static_cast<double>(Trace.Frequency);
        }

        ModuleInfo const* Module(_In_ int32_t const index) const noexcept
        {
            return index >= 0 && static_cast<size_t>(index) < Modules.size() ? &Modules[static_cast<size_t>(index)] : nullptr;
        }

        ProcessInfo const* Process(_In_ uint32_t const processId) const noexcept
        {
            for (auto const& process : Processes)
            {
                if (process.ProcessId == processId)
                {
                    return &process;
                }
            }

            return nullptr;
        }
    };

    // What the page shows while a check runs. Copied out under a lock, so it's all values.
    struct LiveStatus
    {
        uint64_t ElapsedMilliseconds{ 0 };
        uint64_t EventsProcessed{ 0 };

        uint64_t LongestDpcMicroseconds{ 0 };
        std::wstring LongestDpcModule{};

        uint64_t LongestIsrMicroseconds{ 0 };
        std::wstring LongestIsrModule{};

        uint64_t LongestWaitMicroseconds{ 0 };
        uint64_t LateMoments{ 0 };

        uint64_t ProbeWakes{ 0 };
    };
}
