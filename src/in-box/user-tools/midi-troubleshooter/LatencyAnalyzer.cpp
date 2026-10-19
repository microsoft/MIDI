// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

// The analyzer's first pass, and the queues that put events in time order for the second.
// See LatencyAnalyzer.h.

#include "LatencyAnalyzer.h"
#include "LatencyKernelEvents.h"

#include <tdh.h>

#include <algorithm>
#include <limits>
#include <queue>
#include <utility>

#pragma comment(lib, "tdh.lib")

namespace miditroubleshooter::latency
{
    namespace
    {
        // recent profile samples each processor keeps until a DPC or an interrupt claims them
        constexpr size_t RecentSampleLimit{ 512 };

        // driver framework calls waiting for the DPC or interrupt they ran inside
        constexpr size_t FrameworkCallLimit{ 64 };

        constexpr size_t IntervalHistory{ 4096 };
        constexpr size_t RunHistory{ 4096 };

        // profile samples each processor keeps for the time-ordered pass: about a quarter of a
        // second of a busy processor, at the usual one sample a millisecond
        constexpr size_t OrderedSampleHistory{ 256 };

        // A DPC or an interrupt longer than this is a damaged event rather than a real one.
        constexpr uint64_t MaximumIntervalMicroseconds{ 1'000'000 };

        // A processor that has logged nothing for this long stops holding back the time-ordered pass.
        constexpr uint64_t SilentProcessorMicroseconds{ 3'000'000 };

        // how often the live status is copied out, in trace time
        constexpr uint64_t SnapshotIntervalMicroseconds{ 250'000 };

        constexpr uint64_t DrainEveryEvents{ 512 };

        // samples whose module wasn't known yet, kept per routine until Finish
        constexpr size_t UnresolvedSampleLimit{ 1024 };

        // A trace can come from anywhere, so nothing in it may grow these without limit. Real
        // traces stay far below every one of them.
        constexpr uint32_t MaximumProcessors{ 4096 };
        constexpr size_t MaximumModules{ 16384 };
        constexpr size_t MaximumRoutines{ 65536 };
        constexpr size_t MaximumThreads{ 1u << 20 };

        // processor time entries all the routines hold between them, about 16 MB
        constexpr size_t MaximumProcessorTickEntries{ 1u << 20 };

        std::wstring FileNamePart(_In_ std::wstring const& path)
        {
            auto const separator = path.find_last_of(L"\\/");

            return separator == std::wstring::npos ? path : path.substr(separator + 1);
        }

        std::wstring AnsiToWide(_In_ std::string_view const text)
        {
            if (text.empty())
            {
                return {};
            }

            auto const length = MultiByteToWideChar(CP_ACP, 0, text.data(), static_cast<int>(text.size()), nullptr, 0);

            if (length <= 0)
            {
                return {};
            }

            std::wstring wide(static_cast<size_t>(length), L'\0');
            MultiByteToWideChar(CP_ACP, 0, text.data(), static_cast<int>(text.size()), wide.data(), length);

            return wide;
        }

        // TDH knows every version of the process event, including its variable-length SID.
        bool TryGetTdhProperty(
            _In_ EVENT_RECORD const& record,
            _In_z_ wchar_t const* const name,
            _Out_ std::vector<BYTE>& value)
        {
            value.clear();

            PROPERTY_DATA_DESCRIPTOR descriptor{};
            descriptor.PropertyName = reinterpret_cast<ULONGLONG>(name);
            descriptor.ArrayIndex = ULONG_MAX;

            auto* const event = const_cast<PEVENT_RECORD>(&record);
            ULONG size{ 0 };

            if (TdhGetPropertySize(event, 0, nullptr, 1, &descriptor, &size) != ERROR_SUCCESS || size == 0 || size > 4096)
            {
                return false;
            }

            value.resize(size);

            if (TdhGetProperty(event, 0, nullptr, 1, &descriptor, size, value.data()) != ERROR_SUCCESS)
            {
                value.clear();
                return false;
            }

            return true;
        }

        std::optional<std::pair<uint32_t, std::wstring>> DecodeProcess(_In_ EVENT_RECORD const& record)
        {
            std::vector<BYTE> idBytes{};
            std::vector<BYTE> nameBytes{};

            if (TryGetTdhProperty(record, L"ProcessId", idBytes) && idBytes.size() >= sizeof(uint32_t) &&
                TryGetTdhProperty(record, L"ImageFileName", nameBytes))
            {
                uint32_t processId{ 0 };
                std::memcpy(&processId, idBytes.data(), sizeof(processId));

                std::string_view name{ reinterpret_cast<char const*>(nameBytes.data()), nameBytes.size() };
                name = name.substr(0, name.find('\0'));

                return std::make_pair(processId, AnsiToWide(name));
            }

            // Without a schema: key, process id, parent, session, exit status, page table base, flags
            // (version 4 and later), then a SID that's either a zero or a TOKEN_USER and the SID.
            kernel::Reader reader{ record };
            auto const pointer = reader.PointerBytes();

            uint32_t processId{ 0 };

            if (!reader.Read(pointer, processId))
            {
                return std::nullopt;
            }

            uint32_t sidOffset = 2 * pointer + 16 + (record.EventHeader.EventDescriptor.Version >= 4 ? 4u : 0u);
            uint32_t firstWord{ 0 };

            if (!reader.Read(sidOffset, firstWord))
            {
                return std::nullopt;
            }

            uint32_t nameOffset{ 0 };

            if (firstWord == 0)
            {
                nameOffset = sidOffset + 4;
            }
            else
            {
                uint8_t subAuthorities{ 0 };

                if (!reader.Read(sidOffset + 2 * pointer + 1, subAuthorities))
                {
                    return std::nullopt;
                }

                nameOffset = sidOffset + 2 * pointer + 8 + 4u * subAuthorities;
            }

            std::string name{};

            for (uint32_t offset = nameOffset; ; ++offset)
            {
                char character{ 0 };

                if (!reader.Read(offset, character) || character == '\0' || name.size() > 260)
                {
                    break;
                }

                name.push_back(character);
            }

            return std::make_pair(processId, AnsiToWide(name));
        }
    }

    Analyzer::ProcessorState::ProcessorState() :
        Intervals{ IntervalHistory },
        Runs{ RunHistory },
        OrderedSamples{ OrderedSampleHistory }
    {
    }

    _Use_decl_annotations_
    Analyzer::Analyzer(AnalyzerSettings const& settings) :
        m_settings{ settings }
    {
        // a damaged header can claim any number of processors
        if (m_settings.ProcessorCount > MaximumProcessors)
        {
            m_settings.ProcessorCount = 0;
        }

        if (m_settings.ProcessorCount > 0)
        {
            Processor(m_settings.ProcessorCount - 1);
        }
    }

    _Use_decl_annotations_
    Analyzer::ProcessorState& Analyzer::Processor(uint32_t const index)
    {
        // a damaged event can't grow this without limit
        auto const clamped = (std::min)(index, MaximumProcessors - 1);

        if (clamped >= m_processors.size())
        {
            // The two are indexed together, so they grow together or not at all.
            auto const previous = m_results.Processors.size();
            m_results.Processors.resize(clamped + 1);

            try
            {
                m_processors.resize(clamped + 1);
            }
            catch (...)
            {
                m_results.Processors.resize(previous);
                throw;
            }

            for (size_t position = previous; position < m_results.Processors.size(); ++position)
            {
                m_results.Processors[position].Index = static_cast<uint32_t>(position);
            }
        }

        return m_processors[clamped];
    }

    _Use_decl_annotations_
    Analyzer::ThreadState& Analyzer::Thread(uint32_t const threadId)
    {
        if (auto const found = m_threads.find(threadId); found != m_threads.end())
        {
            return found->second;
        }

        // Past the limit, a new thread gets a blank state that keeps nothing, and is never
        // counted as an audio thread.
        if (m_threads.size() >= MaximumThreads)
        {
            m_overflowThread = ThreadState{};
            return m_overflowThread;
        }

        return m_threads[threadId];
    }

    _Use_decl_annotations_
    void Analyzer::SetLostCounts(uint64_t const eventsLost, uint64_t const buffersLost, uint64_t const realTimeBuffersLost) noexcept
    {
        // The session's totals include the losses it also reported as events along the way.
        m_results.Trace.EventsLost = (std::max)(m_results.Trace.EventsLost, eventsLost);
        m_results.Trace.BuffersLost = (std::max)(m_results.Trace.BuffersLost, buffersLost);
        m_results.Trace.RealTimeBuffersLost = (std::max)(m_results.Trace.RealTimeBuffersLost, realTimeBuffersLost);
    }

    _Use_decl_annotations_
    void Analyzer::OnEvent(EVENT_RECORD const& record) noexcept
    {
        try
        {
            if (m_finished)
            {
                return;
            }

            auto const& header = record.EventHeader;
            auto const time = static_cast<uint64_t>(header.TimeStamp.QuadPart);
            auto const processor = static_cast<uint32_t>(GetEventProcessorIndex(&record));

            // Everything here is sized for the processors it has seen, so an event from a
            // processor number no PC has is a damaged one, and is left out.
            if (processor >= MaximumProcessors)
            {
                ++m_results.Trace.EventsSkipped;
                return;
            }

            ++m_results.Trace.EventsProcessed;

            if (time != 0 && (m_firstTime == 0 || time < m_firstTime))
            {
                m_firstTime = time;
            }

            auto& state = Processor(processor);

            state.LatestTime = (std::max)(state.LatestTime, time);
            m_latestTime = (std::max)(m_latestTime, time);

            auto const& provider = header.ProviderId;
            auto const opcode = header.EventDescriptor.Opcode;

            if (kernel::SameGuid(provider, kernel::PerfInfoGuid))
            {
                switch (opcode)
                {
                case kernel::opcode::Dpc:
                    OnInterruptEvent(record, processor, InterruptKind::Dpc);
                    break;

                case kernel::opcode::ThreadedDpc:
                    OnInterruptEvent(record, processor, InterruptKind::ThreadedDpc);
                    break;

                case kernel::opcode::TimerDpc:
                    OnInterruptEvent(record, processor, InterruptKind::TimerDpc);
                    break;

                case kernel::opcode::Isr:
                case kernel::opcode::IsrMsi:
                    OnInterruptEvent(record, processor, InterruptKind::Isr);
                    break;

                case kernel::opcode::WdfDpc:
                    OnFrameworkEvent(record, processor, false);
                    break;

                case kernel::opcode::WdfIsr:
                    OnFrameworkEvent(record, processor, true);
                    break;

                case kernel::opcode::SampledProfile:
                    OnSample(record, processor);
                    break;

                default:
                    break;
                }
            }
            else if (kernel::SameGuid(provider, kernel::ThreadGuid))
            {
                switch (opcode)
                {
                case kernel::opcode::ContextSwitch:
                    if (auto const decoded = kernel::DecodeContextSwitch(record))
                    {
                        ++m_results.Trace.ContextSwitches;

                        OrderedEvent event{};
                        event.Kind = OrderedKind::ContextSwitch;
                        event.Time = time;
                        event.Processor = processor;
                        event.ThreadId = decoded->NewThreadId;
                        event.OtherThreadId = decoded->OldThreadId;
                        event.NewPriority = decoded->NewThreadPriority;
                        event.OldPriority = decoded->OldThreadPriority;
                        event.IdleState = decoded->PreviousCState;

                        Order(event);
                    }
                    break;

                case kernel::opcode::ReadyThread:
                    if (auto const decoded = kernel::DecodeReadyThread(record))
                    {
                        ++m_results.Trace.ReadyThreads;

                        OrderedEvent event{};
                        event.Kind = OrderedKind::ReadyThread;
                        event.Time = time;
                        event.Processor = processor;
                        event.ThreadId = *decoded;

                        Order(event);
                    }
                    break;

                case kernel::opcode::Start:
                case kernel::opcode::End:
                case kernel::opcode::RundownStart:
                case kernel::opcode::RundownEnd:
                    OnThread(record);
                    break;

                case kernel::opcode::ThreadSetName:
                    OnThreadName(record);
                    break;

                default:
                    break;
                }
            }
            else if (kernel::SameGuid(provider, kernel::ImageGuid))
            {
                if (opcode == kernel::opcode::ImageLoad || opcode == kernel::opcode::RundownStart || opcode == kernel::opcode::RundownEnd)
                {
                    OnImage(record);
                }
            }
            else if (kernel::SameGuid(provider, kernel::ProcessGuid))
            {
                if (opcode == kernel::opcode::Start || opcode == kernel::opcode::RundownStart || opcode == kernel::opcode::RundownEnd)
                {
                    OnProcess(record);
                }
            }
            else if (kernel::SameGuid(provider, kernel::PageFaultGuid))
            {
                if (opcode == kernel::opcode::HardFault)
                {
                    if (auto const decoded = kernel::DecodeHardFault(record); decoded && decoded->InitialTime <= time)
                    {
                        ++m_results.Trace.HardFaults;

                        OrderedEvent event{};
                        event.Kind = OrderedKind::HardFault;
                        event.Time = time;
                        event.Start = decoded->InitialTime;
                        event.Processor = processor;
                        event.ThreadId = decoded->ThreadId;

                        Order(event);
                    }
                }
            }
            else if (kernel::SameGuid(provider, kernel::LostEventGuid))
            {
                if (opcode == kernel::opcode::LostBuffer)
                {
                    ++m_results.Trace.RealTimeBuffersLost;
                }
                else
                {
                    ++m_results.Trace.EventsLost;
                }
            }
            else if (kernel::SameGuid(provider, LatencyCheckProviderGuid))
            {
                OnProbeEvent(record);
            }

            if (!m_settings.TimeOrdered && ++m_eventsSinceDrain >= DrainEveryEvents)
            {
                m_eventsSinceDrain = 0;
                DrainOrdered(false);
            }

            UpdateSnapshot(false);
        }
        catch (...)
        {
            SkipEvent();
        }
    }

    void Analyzer::SkipEvent() noexcept
    {
        // The decoders turn a damaged event down without throwing, so an event lands here when
        // memory ran out. The rest of the check carries on, and the verdict says the results
        // are missing parts.
        ++m_results.Trace.EventsSkipped;
        m_results.Trace.AnalysisIncomplete = true;
    }

    _Use_decl_annotations_
    void Analyzer::OnInterruptEvent(EVENT_RECORD const& record, uint32_t const processor, InterruptKind const kind)
    {
        auto const decoded = kernel::DecodeDpc(record);

        if (!decoded)
        {
            return;
        }

        auto const end = static_cast<uint64_t>(record.EventHeader.TimeStamp.QuadPart);
        auto const start = decoded->InitialTime;

        if (start > end || TicksToMicroseconds(end - start, m_settings.Frequency) > MaximumIntervalMicroseconds)
        {
            return;
        }

        auto& state = Processor(processor);
        auto const isIsr = kind == InterruptKind::Isr;

        // The driver framework logs the driver routine it calls from inside its own ISR or DPC,
        // on the same processor, so it's waiting here by the time this ends.
        uint64_t clientRoutine{ 0 };

        for (auto const& call : state.FrameworkCalls)
        {
            if (call.IsIsr == isIsr && call.Time >= start && call.Time <= end)
            {
                clientRoutine = call.Routine;
                break;
            }
        }

        std::erase_if(state.FrameworkCalls, [end](FrameworkCall const& call) { return call.Time <= end; });

        auto const index = RoutineIndex(RoutineKey{ decoded->Routine, clientRoutine, kind });
        auto& routine = m_results.Routines[static_cast<size_t>(index)];

        auto const ticks = end - start;
        auto const microseconds = TicksToMicroseconds(ticks, m_settings.Frequency);

        routine.Durations.Add(ticks, microseconds, end, processor);

        if (auto* const existing = FindProcessorTicks(routine.TicksPerProcessor, processor))
        {
            *existing += ticks;
        }
        else if (m_processorTickEntries < MaximumProcessorTickEntries)
        {
            routine.TicksPerProcessor.emplace_back(processor, ticks);
            ++m_processorTickEntries;
        }

        auto& processorStats = m_results.Processors[processor];

        if (isIsr)
        {
            ++m_results.Trace.Interrupts;
            processorStats.Isr.Add(ticks, microseconds, end, processor);

            if (ticks > m_longestIsrTicks)
            {
                m_longestIsrTicks = ticks;
                m_longestIsrRoutine = index;
            }
        }
        else
        {
            ++m_results.Trace.Dpcs;
            processorStats.Dpc.Add(ticks, microseconds, end, processor);

            if (ticks > m_longestDpcTicks)
            {
                m_longestDpcTicks = ticks;
                m_longestDpcRoutine = index;
            }
        }

        // Profile samples taken while it ran show whose code was running. An interrupt inside a
        // DPC ends first, so it takes its own samples before the DPC sees them.
        for (auto sample = state.RecentSamples.rbegin(); sample != state.RecentSamples.rend(); ++sample)
        {
            if (sample->Time < start)
            {
                break;
            }

            if (sample->Time > end || sample->Claimed)
            {
                continue;
            }

            sample->Claimed = true;

            auto const module = FindModule(sample->InstructionPointer);

            if (module < 0 && kernel::IsKernelAddress(sample->InstructionPointer, m_settings.PointerSize))
            {
                // the module list may not have arrived yet; try again at the end
                auto& unresolved = m_unresolvedSamples[index];

                if (unresolved.size() < UnresolvedSampleLimit)
                {
                    unresolved.push_back(sample->InstructionPointer);
                }

                continue;
            }

            AddSampledModule(routine, module);
        }

        OrderedEvent event{};
        event.Kind = OrderedKind::Interval;
        event.Time = end;
        event.Start = start;
        event.Processor = processor;
        event.Routine = index;
        event.IsIsr = isIsr;

        Order(event);
    }

    _Use_decl_annotations_
    void Analyzer::AddSampledModule(RoutineStats& routine, int32_t const module)
    {
        for (auto& entry : routine.SampledModules)
        {
            if (entry.first == module)
            {
                ++entry.second;
                return;
            }
        }

        routine.SampledModules.emplace_back(module, 1u);
    }

    _Use_decl_annotations_
    void Analyzer::OnFrameworkEvent(EVENT_RECORD const& record, uint32_t const processor, bool const isIsr)
    {
        auto const routine = kernel::DecodeFrameworkRoutine(record);

        if (!routine)
        {
            return;
        }

        ++m_results.Trace.FrameworkEvents;

        auto& calls = Processor(processor).FrameworkCalls;

        if (calls.size() >= FrameworkCallLimit)
        {
            calls.erase(calls.begin());
        }

        calls.push_back(FrameworkCall{ static_cast<uint64_t>(record.EventHeader.TimeStamp.QuadPart), *routine, isIsr });
    }

    _Use_decl_annotations_
    void Analyzer::OnSample(EVENT_RECORD const& record, uint32_t const processor)
    {
        auto const decoded = kernel::DecodeSample(record);

        if (!decoded)
        {
            return;
        }

        ++m_results.Trace.Samples;

        auto& state = Processor(processor);
        ++m_results.Processors[processor].Samples;

        if (state.RecentSamples.size() >= RecentSampleLimit)
        {
            state.RecentSamples.pop_front();
        }

        auto const time = static_cast<uint64_t>(record.EventHeader.TimeStamp.QuadPart);

        state.RecentSamples.push_back(Sample{ time, decoded->InstructionPointer, false });

        // the time-ordered pass looks at what a processor ran while a timer on it was late
        OrderedEvent event{};
        event.Kind = OrderedKind::Sample;
        event.Time = time;
        event.Processor = processor;
        event.ThreadId = decoded->ThreadId;
        event.Address = decoded->InstructionPointer;

        Order(event);
    }

    _Use_decl_annotations_
    void Analyzer::OnImage(EVENT_RECORD const& record) noexcept
    {
        try
        {
            auto const decoded = kernel::DecodeImage(record);

            // Only drivers and the kernel. DPCs and interrupts never run in an application's code.
            if (!decoded || decoded->Size == 0 || !kernel::IsKernelAddress(decoded->Base, kernel::PointerSize(record)))
            {
                return;
            }

            // the rundown at the end lists everything again
            if (auto const found = m_moduleBases.find(decoded->Base); found != m_moduleBases.end() &&
                m_results.Modules[static_cast<size_t>(found->second)].Size == decoded->Size)
            {
                return;
            }

            if (m_results.Modules.size() >= MaximumModules)
            {
                return;
            }

            ModuleInfo module{};
            module.TracePath = decoded->FileName;
            module.FileName = FileNamePart(decoded->FileName);
            module.Base = decoded->Base;
            module.Size = decoded->Size;
            module.Checksum = decoded->Checksum;
            module.TimeDateStamp = decoded->TimeDateStamp;

            m_results.Modules.push_back(std::move(module));
            m_moduleBases[decoded->Base] = static_cast<int32_t>(m_results.Modules.size() - 1);
            m_moduleOrderStale = true;
        }
        catch (...)
        {
            SkipEvent();
        }
    }

    _Use_decl_annotations_
    void Analyzer::OnProcess(EVENT_RECORD const& record) noexcept
    {
        try
        {
            if (auto const decoded = DecodeProcess(record))
            {
                m_processNames[decoded->first] = decoded->second;
            }
        }
        catch (...)
        {
            SkipEvent();
        }
    }

    _Use_decl_annotations_
    void Analyzer::OnThread(EVENT_RECORD const& record) noexcept
    {
        try
        {
            if (auto const decoded = kernel::DecodeThread(record))
            {
                auto& thread = Thread(decoded->ThreadId);

                thread.ProcessId = decoded->ProcessId;
                thread.ProcessKnown = true;
            }
        }
        catch (...)
        {
            SkipEvent();
        }
    }

    _Use_decl_annotations_
    void Analyzer::OnThreadName(EVENT_RECORD const& record) noexcept
    {
        try
        {
            if (auto const decoded = kernel::DecodeThreadName(record))
            {
                auto& thread = Thread(decoded->ThreadId);

                thread.Name = decoded->Name;

                if (!thread.ProcessKnown)
                {
                    thread.ProcessId = decoded->ProcessId;
                    thread.ProcessKnown = true;
                }
            }
        }
        catch (...)
        {
            SkipEvent();
        }
    }

    _Use_decl_annotations_
    void Analyzer::OnProbeEvent(EVENT_RECORD const& record) noexcept
    {
        try
        {
            // the probe says which thread it is, so a saved trace can be checked again later
            std::vector<BYTE> value{};

            if (TryGetTdhProperty(record, L"ProbeThreadId", value) && value.size() >= sizeof(uint32_t))
            {
                uint32_t threadId{ 0 };
                std::memcpy(&threadId, value.data(), sizeof(threadId));

                if (m_settings.ProbeThreadId == 0)
                {
                    m_settings.ProbeThreadId = threadId;
                    m_settings.ProbeProcessId = record.EventHeader.ProcessId;
                }

                return;
            }

            // a late wake, and when it should have been
            if (TryGetTdhProperty(record, L"Deadline", value) && value.size() >= sizeof(uint64_t) &&
                (m_settings.ProbeThreadId == 0 || record.EventHeader.ThreadId == m_settings.ProbeThreadId))
            {
                uint64_t deadline{ 0 };
                std::memcpy(&deadline, value.data(), sizeof(deadline));

                auto const time = static_cast<uint64_t>(record.EventHeader.TimeStamp.QuadPart);

                if (deadline == 0 || deadline >= time)
                {
                    return;
                }

                OrderedEvent event{};
                event.Kind = OrderedKind::ProbeLateWake;
                event.Time = time;
                event.Start = deadline;
                event.Processor = static_cast<uint32_t>(GetEventProcessorIndex(&record));
                event.ThreadId = record.EventHeader.ThreadId;

                Order(event);
            }
        }
        catch (...)
        {
            SkipEvent();
        }
    }

    _Use_decl_annotations_
    int32_t Analyzer::FindModule(uint64_t const address) noexcept
    {
        try
        {
            if (m_moduleOrderStale)
            {
                m_moduleOrder.resize(m_results.Modules.size());

                for (size_t index = 0; index < m_moduleOrder.size(); ++index)
                {
                    m_moduleOrder[index] = static_cast<int32_t>(index);
                }

                std::sort(m_moduleOrder.begin(), m_moduleOrder.end(), [this](int32_t const left, int32_t const right)
                    {
                        return m_results.Modules[static_cast<size_t>(left)].Base < m_results.Modules[static_cast<size_t>(right)].Base;
                    });

                m_moduleOrderStale = false;
            }
        }
        catch (...)
        {
            // An address looked up before the end is looked up again then. At the end, the
            // results are left without the driver it belongs to.
            if (m_finished)
            {
                m_results.Trace.AnalysisIncomplete = true;
            }

            return -1;
        }

        auto found = std::upper_bound(m_moduleOrder.begin(), m_moduleOrder.end(), address, [this](uint64_t const value, int32_t const index)
            {
                return value < m_results.Modules[static_cast<size_t>(index)].Base;
            });

        if (found == m_moduleOrder.begin())
        {
            return -1;
        }

        --found;

        auto const& module = m_results.Modules[static_cast<size_t>(*found)];

        return address - module.Base < module.Size ? *found : -1;
    }

    _Use_decl_annotations_
    int32_t Analyzer::RoutineIndex(RoutineKey const& key)
    {
        if (auto const found = m_routineIndexes.find(key); found != m_routineIndexes.end())
        {
            return found->second;
        }

        RoutineStats routine{};
        routine.Kind = key.Kind;

        // Past the limit, the rest of each kind share one entry with no routine of its own.
        auto const full = m_results.Routines.size() >= MaximumRoutines;
        auto& overflow = m_overflowRoutines[static_cast<size_t>(key.Kind) % m_overflowRoutines.size()];

        if (full && overflow >= 0)
        {
            return overflow;
        }

        if (!full)
        {
            routine.Routine = key.Routine;
            routine.ClientRoutine = key.ClientRoutine;
            routine.Module = FindModule(key.Routine);
            routine.ClientModule = key.ClientRoutine != 0 ? FindModule(key.ClientRoutine) : -1;
        }

        m_results.Routines.push_back(std::move(routine));

        auto const index = static_cast<int32_t>(m_results.Routines.size() - 1);

        if (full)
        {
            overflow = index;
        }
        else
        {
            m_routineIndexes.emplace(key, index);
        }

        return index;
    }

    _Use_decl_annotations_
    void Analyzer::Order(OrderedEvent const& event)
    {
        if (m_settings.TimeOrdered)
        {
            OnOrdered(event);
            return;
        }

        Processor(event.Processor).Queue.push_back(event);
    }

    _Use_decl_annotations_
    void Analyzer::DrainOrdered(bool const everything)
    {
        auto watermark = (std::numeric_limits<uint64_t>::max)();

        if (!everything)
        {
            // Every processor that has logged anything lately has passed this time, so nothing
            // older can still be on its way.
            auto const silence = MicrosecondsToTicks(SilentProcessorMicroseconds, m_settings.Frequency);
            bool any{ false };

            for (auto const& processor : m_processors)
            {
                if (processor.LatestTime == 0 || m_latestTime - processor.LatestTime > silence)
                {
                    continue;
                }

                watermark = (std::min)(watermark, processor.LatestTime);
                any = true;
            }

            if (!any)
            {
                return;
            }
        }

        using Head = std::pair<uint64_t, uint32_t>;
        std::priority_queue<Head, std::vector<Head>, std::greater<Head>> heads{};

        for (uint32_t index = 0; index < m_processors.size(); ++index)
        {
            if (!m_processors[index].Queue.empty())
            {
                heads.emplace(m_processors[index].Queue.front().Time, index);
            }
        }

        while (!heads.empty())
        {
            auto const [time, index] = heads.top();

            if (time > watermark)
            {
                break;
            }

            heads.pop();

            auto& queue = m_processors[index].Queue;
            auto const event = queue.front();
            queue.pop_front();

            if (event.Time < m_orderedTime)
            {
                ++m_results.Trace.OutOfOrder;
            }
            else
            {
                m_orderedTime = event.Time;
            }

            OnOrdered(event);

            if (!queue.empty())
            {
                heads.emplace(queue.front().Time, index);
            }
        }
    }

    std::wstring Analyzer::RoutineDisplayName(_In_ int32_t const index)
    {
        if (index < 0 || static_cast<size_t>(index) >= m_results.Routines.size())
        {
            return {};
        }

        auto& routine = m_results.Routines[static_cast<size_t>(index)];

        if (routine.ClientRoutine != 0 && routine.ClientModule < 0)
        {
            routine.ClientModule = FindModule(routine.ClientRoutine);
        }

        if (routine.Module < 0)
        {
            routine.Module = FindModule(routine.Routine);
        }

        auto const module = routine.ClientModule >= 0 ? routine.ClientModule : routine.Module;

        return module >= 0 ? m_results.Modules[static_cast<size_t>(module)].FileName : std::wstring{};
    }

    _Use_decl_annotations_
    void Analyzer::UpdateSnapshot(bool const force) noexcept
    {
        try
        {
            if (!force && m_latestTime - m_snapshotTime < MicrosecondsToTicks(SnapshotIntervalMicroseconds, m_settings.Frequency))
            {
                return;
            }

            m_snapshotTime = m_latestTime;

            LiveStatus status{};
            status.ElapsedMilliseconds = m_latestTime > m_firstTime ? TicksToMicroseconds(m_latestTime - m_firstTime, m_settings.Frequency) / 1000 : 0;
            status.EventsProcessed = m_results.Trace.EventsProcessed;
            status.LongestDpcMicroseconds = TicksToMicroseconds(m_longestDpcTicks, m_settings.Frequency);
            status.LongestDpcModule = RoutineDisplayName(m_longestDpcRoutine);
            status.LongestIsrMicroseconds = TicksToMicroseconds(m_longestIsrTicks, m_settings.Frequency);
            status.LongestIsrModule = RoutineDisplayName(m_longestIsrRoutine);

            // the threads that look like audio threads so far
            uint64_t longestWait{ 0 };

            for (auto const& audio : m_results.AudioThreads)
            {
                if (!CountsAsAudio(audio))
                {
                    continue;
                }

                longestWait = (std::max)({ longestWait, audio.Waits.MaxTicks, audio.Interruptions.MaxTicks });
                status.LateMoments += audio.LateMoments;

                if (audio.IsProbe())
                {
                    status.ProbeWakes = audio.Waits.Count;
                }
            }

            status.LongestWaitMicroseconds = TicksToMicroseconds(longestWait, m_settings.Frequency);

            std::scoped_lock lock{ m_snapshotLock };
            m_snapshot = std::move(status);
        }
        catch (...)
        {
        }
    }

    LiveStatus Analyzer::Snapshot() const
    {
        std::scoped_lock lock{ m_snapshotLock };
        return m_snapshot;
    }
}
