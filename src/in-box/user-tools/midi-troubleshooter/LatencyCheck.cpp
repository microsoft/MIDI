// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#include "LatencyCheck.h"
#include "LatencyDriverInfo.h"

#include <windows.h>

#include <chrono>
#include <cwchar>
#include <vector>

namespace miditroubleshooter::latency
{
    namespace
    {
        // After the session stops, Windows hands over what's left in its buffers. This is long
        // enough for a busy PC; past it, whatever hasn't arrived is left out.
        constexpr auto ReaderStopTimeout{ std::chrono::seconds(15) };

        // how often a file analysis says how far it has got
        constexpr uint64_t ProgressEveryEvents{ 65536 };

        // This PC's Windows build, for a live check whose trace header doesn't say.
        uint32_t ThisPcBuild() noexcept
        {
            wchar_t buffer[32]{};
            DWORD size{ sizeof(buffer) };

            if (RegGetValueW(HKEY_LOCAL_MACHINE, L"SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion", L"CurrentBuildNumber",
                RRF_RT_REG_SZ, nullptr, buffer, &size) != ERROR_SUCCESS)
            {
                return 0;
            }

            return static_cast<uint32_t>(std::wcstoul(buffer, nullptr, 10));
        }

        // Which processors are the efficient ones, on a PC that has two kinds. Only this PC's.
        void AddEfficiencyClasses(_Inout_ Results& results) noexcept
        {
            try
            {
                ULONG length{ 0 };
                GetSystemCpuSetInformation(nullptr, 0, &length, GetCurrentProcess(), 0);

                if (length == 0)
                {
                    return;
                }

                std::vector<uint8_t> buffer(length);

                if (!GetSystemCpuSetInformation(reinterpret_cast<PSYSTEM_CPU_SET_INFORMATION>(buffer.data()), length, &length, GetCurrentProcess(), 0))
                {
                    return;
                }

                for (ULONG offset = 0; offset + sizeof(SYSTEM_CPU_SET_INFORMATION::Size) + sizeof(SYSTEM_CPU_SET_INFORMATION::Type) <= length;)
                {
                    auto const* const entry = reinterpret_cast<SYSTEM_CPU_SET_INFORMATION const*>(buffer.data() + offset);

                    if (entry->Size == 0)
                    {
                        break;
                    }

                    // The trace numbers processors across the whole PC, which only matches the
                    // number within a group on a PC with one group.
                    if (entry->Type == CpuSetInformation && entry->CpuSet.Group == 0 &&
                        entry->CpuSet.LogicalProcessorIndex < results.Processors.size())
                    {
                        auto& processor = results.Processors[entry->CpuSet.LogicalProcessorIndex];

                        processor.EfficiencyClass = entry->CpuSet.EfficiencyClass;
                        processor.EfficiencyClassKnown = true;
                    }

                    offset += entry->Size;
                }
            }
            catch (...)
            {
                // the report just leaves them out
            }
        }
    }

    LiveCheck::~LiveCheck()
    {
        Abandon();
    }

    _Use_decl_annotations_
    uint32_t LiveCheck::Start(CheckOptions const& options) noexcept
    {
        // Held through the catch below as well, so a start that fails is cleaned up before the
        // window closing can stop it too.
        std::scoped_lock lifecycle{ m_lifecycleLock };

        // the window closed before this got going
        if (m_abandoned.load())
        {
            return ERROR_CANCELLED;
        }

        try
        {
            AbandonLocked();

            m_options = options;
            m_readerDone = false;
            m_readerStatus = 0;

            SessionOptions sessionOptions{};
            sessionOptions.CollectSamples = options.CollectSamples;
            sessionOptions.TraceFilePath = options.TraceFilePath;
            sessionOptions.MaximumFileMegabytes = options.MaximumFileMegabytes;

            auto status = m_session.Start(sessionOptions);

            if (status != ERROR_SUCCESS)
            {
                return status;
            }

            // The test thread starts first, so the analyzer knows which thread it is from the
            // start. What it does in the meantime waits in the session's buffers. The check
            // still works without it, from whatever audio apps are running.
            if (options.UseProbe)
            {
                m_probe.Start(options.ProbePeriodMicroseconds);
            }

            status = m_reader.OpenLive(KernelSession::SessionName);

            if (status != ERROR_SUCCESS)
            {
                AbandonLocked();
                return status;
            }

            auto const& header = m_reader.Header();

            AnalyzerSettings settings{};
            settings.Frequency = header.Frequency;
            settings.ProcessorCount = header.ProcessorCount;
            settings.PointerSize = header.PointerSize;
            settings.ClockType = header.ClockType;
            settings.TimeOrdered = false;
            settings.ProbeThreadId = m_probe.ThreadId();
            settings.ProbeProcessId = m_probe.ThreadId() != 0 ? GetCurrentProcessId() : 0;

            auto analyzer = std::make_shared<Analyzer>(settings);

            {
                std::scoped_lock lock{ m_analyzerLock };
                m_analyzer = analyzer;
            }

            // The reader thread keeps its own reference, so the analyzer outlives every event
            // it's handed whatever the page does in the meantime.
            m_readerThread = std::thread([this, analyzer]() noexcept
                {
                    try
                    {
                        m_readerStatus = m_reader.Process([&analyzer](EVENT_RECORD const& record)
                            {
                                analyzer->OnEvent(record);
                            });
                    }
                    catch (...)
                    {
                        m_readerStatus = ERROR_GEN_FAILURE;
                    }

                    m_readerDone = true;
                });

            m_started = true;

            return ERROR_SUCCESS;
        }
        catch (...)
        {
            AbandonLocked();
            return ERROR_OUTOFMEMORY;
        }
    }

    LiveStatus LiveCheck::Status() const
    {
        std::shared_ptr<Analyzer> analyzer{};

        {
            std::scoped_lock lock{ m_analyzerLock };
            analyzer = m_analyzer;
        }

        return analyzer != nullptr ? analyzer->Snapshot() : LiveStatus{};
    }

    _Use_decl_annotations_
    void LiveCheck::StopReader(bool const deliverEverything) noexcept
    {
        if (deliverEverything)
        {
            // Stopping the session flushes its buffers to the reader, and then the reader's
            // Process call returns by itself.
            auto const deadline = std::chrono::steady_clock::now() + ReaderStopTimeout;

            while (!m_readerDone.load() && std::chrono::steady_clock::now() < deadline)
            {
                Sleep(20);
            }
        }

        m_reader.Cancel();

        if (m_readerThread.joinable())
        {
            try
            {
                m_readerThread.join();
            }
            catch (...)
            {
            }
        }
    }

    CheckOutcome LiveCheck::Finish() noexcept
    {
        CheckOutcome outcome{};

        std::scoped_lock lifecycle{ m_lifecycleLock };

        std::shared_ptr<Analyzer> analyzer{};

        {
            std::scoped_lock lock{ m_analyzerLock };
            analyzer = m_analyzer;
        }

        if (!m_started || analyzer == nullptr)
        {
            outcome.Error = ERROR_INVALID_STATE;
            return outcome;
        }

        try
        {
            // the test thread's last wakes go in the trace before it closes
            m_probe.Stop();
            m_session.Stop();

            StopReader(true);
            m_started = false;

            // nothing else touches it now that the reader is done
            {
                std::scoped_lock lock{ m_analyzerLock };
                m_analyzer.reset();
            }

            auto const& session = m_session.Info();

            analyzer->SetLostCounts(session.EventsLost, session.BuffersLost, session.RealTimeBuffersLost);
            analyzer->Finish();

            auto results = analyzer->TakeResults();
            analyzer.reset();

            results.Trace.Live = true;
            results.Trace.SourcePath = m_options.TraceFilePath;
            results.Trace.WindowsBuild = m_reader.Header().WindowsBuild != 0 ? m_reader.Header().WindowsBuild : ThisPcBuild();
            results.Probe = m_probe.Results();

            AddEfficiencyClasses(results);
            AddDriverDetails(results);

            outcome.Summary = Assess(results);
            outcome.Data = std::move(results);
            outcome.Session = session;
        }
        catch (...)
        {
            AbandonLocked();
            outcome.Error = ERROR_OUTOFMEMORY;
        }

        return outcome;
    }

    void LiveCheck::Abandon() noexcept
    {
        m_abandoned = true;

        std::scoped_lock lifecycle{ m_lifecycleLock };

        AbandonLocked();
    }

    void LiveCheck::AbandonLocked() noexcept
    {
        m_probe.Stop();
        m_session.Stop();
        StopReader(false);

        {
            std::scoped_lock lock{ m_analyzerLock };
            m_analyzer.reset();
        }

        m_started = false;
    }

    _Use_decl_annotations_
    CheckOutcome AnalyzeTraceFile(std::wstring const& path, std::atomic<bool> const& cancel, std::atomic<uint64_t>& eventsRead) noexcept
    {
        CheckOutcome outcome{};

        try
        {
            TraceReader reader{};
            auto status = reader.OpenFile(path);

            if (status != ERROR_SUCCESS)
            {
                outcome.Error = status;
                return outcome;
            }

            auto const& header = reader.Header();

            AnalyzerSettings settings{};
            settings.Frequency = header.Frequency;
            settings.ProcessorCount = header.ProcessorCount;
            settings.PointerSize = header.PointerSize;
            settings.ClockType = header.ClockType;

            // ProcessTrace hands over a file's events in time order already
            settings.TimeOrdered = true;

            auto analyzer = std::make_unique<Analyzer>(settings);

            bool canceled{ false };
            uint64_t count{ 0 };

            status = reader.Process([&](EVENT_RECORD const& record)
                {
                    if (canceled)
                    {
                        return;
                    }

                    if (cancel.load(std::memory_order_relaxed))
                    {
                        canceled = true;
                        reader.Cancel();
                        return;
                    }

                    analyzer->OnEvent(record);

                    if (++count % ProgressEveryEvents == 0)
                    {
                        eventsRead.store(count, std::memory_order_relaxed);
                    }
                });

            eventsRead.store(count, std::memory_order_relaxed);

            if (canceled || cancel.load())
            {
                outcome.Canceled = true;
                return outcome;
            }

            // a file that ends early still has its events up to there
            if (status != ERROR_SUCCESS && count == 0)
            {
                outcome.Error = status;
                return outcome;
            }

            analyzer->SetLostCounts(header.EventsLost, header.BuffersLost, 0);
            analyzer->Finish();

            auto results = analyzer->TakeResults();

            results.Trace.Live = false;
            results.Trace.SourcePath = path;
            results.Trace.WindowsBuild = header.WindowsBuild;

            // Only a trace from this PC gets these, because AddDriverDetails checks that each
            // driver file is the very one the trace saw.
            AddDriverDetails(results);

            outcome.Summary = Assess(results);
            outcome.Data = std::move(results);
        }
        catch (...)
        {
            outcome.Error = ERROR_OUTOFMEMORY;
        }

        return outcome;
    }
}
