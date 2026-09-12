// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

#include "stdafx.h"
#include "Midi2SchedulerTransform.h"
#include "Midi2MidiSrvTransport.h"

#include "MidiSchedulerTransformTests.h"

#include <initguid.h>
#include "MidiDefs.h"
#include "Feature_Servicing_MIDI2SchedulerV2.h"

#include <vector>
#include <algorithm>
#include <numeric>
#include <random>
#include <atomic>
#include <cmath>
#include <string>
#include <sstream>
#include <iomanip>
#include <chrono>
#include <map>
#include <tlhelp32.h>
#include <timeapi.h>

#pragma comment(lib, "winmm.lib")

bool MidiSchedulerTransformTests::ClassSetup()
{
    PrintStagingStates();

    WEX::TestExecution::SetVerifyOutput verifySettings(WEX::TestExecution::VerifyOutputSettings::LogOnlyFailures);

    return true;
}


namespace
{
    // Only used for its address, to find the module this code was linked into.
    int g_moduleAnchor{ 0 };

    // The registered CLSID resolves to the copy in System32, which is not necessarily the one this
    // build produced. These measurements have to describe the binary under test, so the transform is
    // activated directly out of the test output folder instead of through CoCreateInstance.
    wil::unique_hmodule LoadLocalSchedulerTransform(_Out_ std::wstring& resolvedPath)
    {
        resolvedPath.clear();

        HMODULE selfModule{ nullptr };

        if (!GetModuleHandleExW(
            GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
            reinterpret_cast<LPCWSTR>(&g_moduleAnchor),
            &selfModule))
        {
            return wil::unique_hmodule{};
        }

        wchar_t modulePath[MAX_PATH]{};

        auto const length = GetModuleFileNameW(selfModule, modulePath, ARRAYSIZE(modulePath));

        if (length == 0 || length >= ARRAYSIZE(modulePath))
        {
            return wil::unique_hmodule{};
        }

        std::wstring folder{ modulePath };

        auto const separator = folder.find_last_of(L'\\');

        if (separator == std::wstring::npos)
        {
            return wil::unique_hmodule{};
        }

        folder.resize(separator + 1);

        resolvedPath = folder + L"Midi2.SchedulerTransform.dll";

        return wil::unique_hmodule{ LoadLibraryW(resolvedPath.c_str()) };
    }

    HRESULT CreateSchedulerTransform(
        _In_ HMODULE transformModule,
        _Out_ wil::com_ptr_nothrow<IMidiDataTransform>& transform)
    {
        using DllGetClassObjectProc = HRESULT(STDAPICALLTYPE*)(REFCLSID, REFIID, LPVOID*);

        auto const getClassObject = reinterpret_cast<DllGetClassObjectProc>(
            reinterpret_cast<void*>(GetProcAddress(transformModule, "DllGetClassObject")));

        RETURN_HR_IF_NULL(E_NOINTERFACE, getClassObject);

        wil::com_ptr_nothrow<IClassFactory> factory;
        RETURN_IF_FAILED(getClassObject(__uuidof(Midi2SchedulerTransform), IID_PPV_ARGS(&factory)));

        wil::com_ptr_nothrow<IMidiTransform> transformLib;
        RETURN_IF_FAILED(factory->CreateInstance(nullptr, __uuidof(IMidiTransform), transformLib.put_void()));

        RETURN_IF_FAILED(transformLib->Activate(__uuidof(IMidiDataTransform), transform.put_void()));

        return S_OK;
    }

    double TicksToMicroseconds(_In_ int64_t const ticks, _In_ double const frequency)
    {
        return (static_cast<double>(ticks) * 1000000.0) / frequency;
    }

    int64_t PercentileOfSorted(_In_ std::vector<int64_t> const& sorted, _In_ double const fraction)
    {
        if (sorted.empty()) return 0;

        auto index = static_cast<size_t>(fraction * static_cast<double>(sorted.size() - 1));

        if (index >= sorted.size()) index = sorted.size() - 1;

        return sorted[index];
    }

    uint64_t FileTimeToTicks(_In_ FILETIME const& value)
    {
        ULARGE_INTEGER converted{};
        converted.LowPart = value.dwLowDateTime;
        converted.HighPart = value.dwHighDateTime;
        return converted.QuadPart;
    }

    std::wstring DescribePriorityClass(_In_ DWORD const priorityClass)
    {
        switch (priorityClass)
        {
        case REALTIME_PRIORITY_CLASS:       return L"REALTIME";
        case HIGH_PRIORITY_CLASS:           return L"HIGH";
        case ABOVE_NORMAL_PRIORITY_CLASS:   return L"ABOVE_NORMAL";
        case NORMAL_PRIORITY_CLASS:         return L"NORMAL";
        case BELOW_NORMAL_PRIORITY_CLASS:   return L"BELOW_NORMAL";
        case IDLE_PRIORITY_CLASS:           return L"IDLE";
        default:                            return L"unknown";
        }
    }

    // Per-thread CPU for this process only, so the spin can be attributed to a specific thread
    // rather than to whatever else the machine happens to be doing.
    std::map<DWORD, uint64_t> SnapshotThreadCpu()
    {
        std::map<DWORD, uint64_t> result;

        auto const rawSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0);

        if (rawSnapshot == INVALID_HANDLE_VALUE)
        {
            return result;
        }

        wil::unique_handle snapshot{ rawSnapshot };

        auto const processId = GetCurrentProcessId();

        THREADENTRY32 entry{};
        entry.dwSize = sizeof(entry);

        if (Thread32First(snapshot.get(), &entry))
        {
            do
            {
                if (entry.th32OwnerProcessID != processId) continue;

                wil::unique_handle thread{ OpenThread(THREAD_QUERY_LIMITED_INFORMATION, FALSE, entry.th32ThreadID) };

                if (!thread) continue;

                FILETIME threadCreation{}, threadExit{}, threadKernel{}, threadUser{};

                if (GetThreadTimes(thread.get(), &threadCreation, &threadExit, &threadKernel, &threadUser))
                {
                    result[entry.th32ThreadID] = FileTimeToTicks(threadKernel) + FileTimeToTicks(threadUser);
                }

            } while (Thread32Next(snapshot.get(), &entry));
        }

        return result;
    }
}


_Use_decl_annotations_
void MidiSchedulerTransformTests::RunBaselineScenario(
    wchar_t const* label,
    uint32_t messageCount,
    uint32_t leadMilliseconds,
    uint32_t spacingMicroseconds,
    bool shuffleEnqueueOrder)
{
    LARGE_INTEGER frequency{};
    QueryPerformanceFrequency(&frequency);
    auto const qpf = static_cast<double>(frequency.QuadPart);

    // Control window: process CPU over a fixed idle period with no transform alive at all. This is
    // the noise floor that anything measured below has to beat to mean anything.
    FILETIME controlCreation{}, controlExit{}, controlKernelStart{}, controlUserStart{}, controlKernelEnd{}, controlUserEnd{};
    GetProcessTimes(GetCurrentProcess(), &controlCreation, &controlExit, &controlKernelStart, &controlUserStart);
    Sleep(500);
    GetProcessTimes(GetCurrentProcess(), &controlCreation, &controlExit, &controlKernelEnd, &controlUserEnd);

    auto const idleCpuMilliseconds = static_cast<double>(
        (FileTimeToTicks(controlKernelEnd) - FileTimeToTicks(controlKernelStart))
        + (FileTimeToTicks(controlUserEnd) - FileTimeToTicks(controlUserStart))) / 10000.0;

    std::wstring transformPath;
    auto transformModule = LoadLocalSchedulerTransform(transformPath);
    VERIFY_IS_TRUE(static_cast<bool>(transformModule));

    wil::com_ptr_nothrow<IMidiDataTransform> transform;
    VERIFY_SUCCEEDED(CreateSchedulerTransform(transformModule.get(), transform));

    // Intended timestamp paired with the QPC value captured the moment the callback ran.
    std::vector<std::pair<LONGLONG, LONGLONG>> arrivals(messageCount);
    std::atomic<uint32_t> arrivalCount{ 0 };

    m_MidiInCallback = [&arrivals, &arrivalCount, messageCount]
        (PVOID, UINT32, LONGLONG position, LONGLONG)
        {
            LARGE_INTEGER now{};
            QueryPerformanceCounter(&now);

            auto const index = arrivalCount.fetch_add(1);

            if (index < messageCount)
            {
                arrivals[index] = { position, now.QuadPart };
            }
        };

    // The worker thread raises the priority class of whatever process it runs in, so this has to be
    // captured before Initialize and put back afterwards.
    auto const originalPriorityClass = GetPriorityClass(GetCurrentProcess());

    TRANSFORMCREATIONPARAMS creationParams{};
    creationParams.DataFormatIn = MidiDataFormats::MidiDataFormats_UMP;
    creationParams.DataFormatOut = MidiDataFormats::MidiDataFormats_UMP;

    DWORD mmcssTaskId{ 0 };

    auto const threadCpuBefore = SnapshotThreadCpu();

    // A real endpoint id, so the transform's device property lookup exercises the real path. If the
    // endpoint is absent the lookup fails harmlessly and the latency compensation stays at zero.
    // Override with MIDI_SCHEDULER_TEST_ENDPOINT_ID to measure against a specific device.
    std::wstring endpointId{ L"\\\\?\\swd#midisrv#midiu_loop_a_default#{e7cce071-3c03-423f-88d3-f1045d02552b}" };

    wchar_t endpointOverride[512]{};

    if (GetEnvironmentVariableW(L"MIDI_SCHEDULER_TEST_ENDPOINT_ID", endpointOverride, ARRAYSIZE(endpointOverride)) > 0)
    {
        endpointId = endpointOverride;
    }

    VERIFY_SUCCEEDED(transform->Initialize(endpointId.c_str(), &creationParams, &mmcssTaskId, this, 0, nullptr));

    std::vector<uint32_t> enqueueOrder(messageCount);
    std::iota(enqueueOrder.begin(), enqueueOrder.end(), 0u);

    if (shuffleEnqueueOrder)
    {
        std::mt19937 generator{ 20260911u };
        std::shuffle(enqueueOrder.begin(), enqueueOrder.end(), generator);
    }

    auto const leadTicks = static_cast<LONGLONG>((static_cast<double>(leadMilliseconds) * qpf) / 1000.0);
    auto const spacingTicks = static_cast<LONGLONG>((static_cast<double>(spacingMicroseconds) * qpf) / 1000000.0);

    FILETIME creationTime{}, exitTime{}, kernelStart{}, userStart{}, kernelEnd{}, userEnd{};
    GetProcessTimes(GetCurrentProcess(), &creationTime, &exitTime, &kernelStart, &userStart);

    LARGE_INTEGER wallStart{};
    QueryPerformanceCounter(&wallStart);

    auto const baseTimestamp = wallStart.QuadPart + leadTicks;

    uint32_t scheduledCount{ 0 };
    uint32_t immediateCount{ 0 };
    uint32_t queueFullCount{ 0 };
    uint32_t otherFailureCount{ 0 };

    std::vector<int64_t> enqueueTicks;
    enqueueTicks.reserve(messageCount);

    for (auto const messageIndex : enqueueOrder)
    {
        uint32_t word = 0x20000000u | messageIndex;

        auto const timestamp = baseTimestamp + (static_cast<LONGLONG>(messageIndex) * spacingTicks);

        LARGE_INTEGER enqueueStart{}, enqueueEnd{};
        QueryPerformanceCounter(&enqueueStart);

        auto const hr = transform->SendMidiMessage(MessageOptionFlags_None, &word, static_cast<UINT>(sizeof(word)), timestamp);

        QueryPerformanceCounter(&enqueueEnd);

        enqueueTicks.push_back(enqueueEnd.QuadPart - enqueueStart.QuadPart);

        if (hr == HR_S_MIDI_SENDMSG_SCHEDULED)
        {
            scheduledCount++;
        }
        else if (hr == HR_S_MIDI_SENDMSG_IMMEDIATE)
        {
            immediateCount++;
        }
        else if (hr == HR_E_MIDI_SENDMSG_SCHEDULER_QUEUE_FULL)
        {
            queueFullCount++;
        }
        else
        {
            otherFailureCount++;
        }
    }

    auto const expectedArrivals = scheduledCount + immediateCount;

    auto const spanMilliseconds = static_cast<uint32_t>(
        leadMilliseconds + ((static_cast<uint64_t>(messageCount) * spacingMicroseconds) / 1000ull));

    auto const deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(spanMilliseconds + 15000);

    while (arrivalCount.load() < expectedArrivals && std::chrono::steady_clock::now() < deadline)
    {
        Sleep(10);
    }

    // Sampled here rather than right after Initialize, or the worker thread may not have run yet.
    auto const observedPriorityClass = GetPriorityClass(GetCurrentProcess());

    auto const threadCpuAfter = SnapshotThreadCpu();

    LARGE_INTEGER wallEnd{};
    QueryPerformanceCounter(&wallEnd);

    GetProcessTimes(GetCurrentProcess(), &creationTime, &exitTime, &kernelEnd, &userEnd);

    VERIFY_SUCCEEDED(transform->Shutdown());

    m_MidiInCallback = nullptr;

    SetPriorityClass(GetCurrentProcess(), originalPriorityClass);

    // Parenthesized because windows.h defines min as a macro.
    auto const receivedCount = (std::min)(arrivalCount.load(), messageCount);

    std::vector<int64_t> errorTicks;
    errorTicks.reserve(receivedCount);

    uint32_t outOfOrderCount{ 0 };

    for (uint32_t i = 0; i < receivedCount; i++)
    {
        errorTicks.push_back(arrivals[i].second - arrivals[i].first);

        if (i > 0 && arrivals[i].first < arrivals[i - 1].first)
        {
            outOfOrderCount++;
        }
    }

    double errorMean{ 0.0 };
    double errorStdDev{ 0.0 };

    if (!errorTicks.empty())
    {
        auto const total = std::accumulate(errorTicks.begin(), errorTicks.end(), static_cast<int64_t>(0));
        errorMean = static_cast<double>(total) / static_cast<double>(errorTicks.size());

        double sumSquares{ 0.0 };
        for (auto const value : errorTicks)
        {
            auto const difference = static_cast<double>(value) - errorMean;
            sumSquares += difference * difference;
        }

        errorStdDev = std::sqrt(sumSquares / static_cast<double>(errorTicks.size()));
    }

    auto sortedErrors = errorTicks;
    std::sort(sortedErrors.begin(), sortedErrors.end());

    auto sortedEnqueue = enqueueTicks;
    std::sort(sortedEnqueue.begin(), sortedEnqueue.end());

    double enqueueMean{ 0.0 };
    if (!enqueueTicks.empty())
    {
        auto const total = std::accumulate(enqueueTicks.begin(), enqueueTicks.end(), static_cast<int64_t>(0));
        enqueueMean = static_cast<double>(total) / static_cast<double>(enqueueTicks.size());
    }

    auto const cpuTicks = (FileTimeToTicks(kernelEnd) - FileTimeToTicks(kernelStart))
        + (FileTimeToTicks(userEnd) - FileTimeToTicks(userStart));

    auto const cpuMilliseconds = static_cast<double>(cpuTicks) / 10000.0;
    auto const wallMilliseconds = (static_cast<double>(wallEnd.QuadPart - wallStart.QuadPart) * 1000.0) / qpf;

    std::wstringstream report;
    report << std::fixed << std::setprecision(1);
    report << L"\r\n=== " << label << L" ===\r\n";
    report << L"  transform            : " << transformPath << L"\r\n";
    report << L"  messages / lead / gap: " << messageCount << L" / " << leadMilliseconds << L" ms / " << spacingMicroseconds << L" us\r\n";
    report << L"  enqueue order        : " << (shuffleEnqueueOrder ? L"shuffled" : L"in order") << L"\r\n";
    report << L"  scheduled            : " << scheduledCount << L"\r\n";
    report << L"  sent immediately     : " << immediateCount << L"\r\n";
    report << L"  queue full           : " << queueFullCount << L"\r\n";
    report << L"  other failures       : " << otherFailureCount << L"\r\n";
    report << L"  received             : " << receivedCount << L" of " << expectedArrivals << L" expected\r\n";
    report << L"  out of order         : " << outOfOrderCount << L"\r\n";
    report << L"  priority class       : " << DescribePriorityClass(originalPriorityClass)
           << L" -> " << DescribePriorityClass(observedPriorityClass) << L"\r\n";

    if (!sortedErrors.empty())
    {
        report << L"  timing error (us)    : mean " << errorMean * 1000000.0 / qpf
               << L"  sd " << errorStdDev * 1000000.0 / qpf
               << L"  min " << TicksToMicroseconds(sortedErrors.front(), qpf)
               << L"  p50 " << TicksToMicroseconds(PercentileOfSorted(sortedErrors, 0.50), qpf)
               << L"  p95 " << TicksToMicroseconds(PercentileOfSorted(sortedErrors, 0.95), qpf)
               << L"  p99 " << TicksToMicroseconds(PercentileOfSorted(sortedErrors, 0.99), qpf)
               << L"  max " << TicksToMicroseconds(sortedErrors.back(), qpf) << L"\r\n";
    }

    if (!sortedEnqueue.empty())
    {
        report << L"  enqueue cost (us)    : mean " << enqueueMean * 1000000.0 / qpf
               << L"  p50 " << TicksToMicroseconds(PercentileOfSorted(sortedEnqueue, 0.50), qpf)
               << L"  p99 " << TicksToMicroseconds(PercentileOfSorted(sortedEnqueue, 0.99), qpf)
               << L"  max " << TicksToMicroseconds(sortedEnqueue.back(), qpf) << L"\r\n";
    }

    report << L"  process CPU / wall   : " << cpuMilliseconds << L" ms / " << wallMilliseconds << L" ms  ("
           << (wallMilliseconds > 0.0 ? (cpuMilliseconds * 100.0 / wallMilliseconds) : 0.0) << L"% of one core)\r\n";

    report << L"  idle control         : " << idleCpuMilliseconds << L" ms CPU over a 500 ms window with no transform\r\n";

    // Attribute the CPU to a thread. The scheduler worker is created inside Initialize, so it is
    // absent from the "before" snapshot and shows up here as a new thread.
    double busiestThreadMilliseconds{ 0.0 };
    DWORD busiestThreadId{ 0 };
    bool busiestThreadIsNew{ false };
    uint32_t activeThreadCount{ 0 };
    double attributedMilliseconds{ 0.0 };
    double workerMilliseconds{ 0.0 };

    for (auto const& [threadId, cpuAfter] : threadCpuAfter)
    {
        auto const found = threadCpuBefore.find(threadId);
        auto const cpuBefore = (found == threadCpuBefore.end()) ? 0ull : found->second;

        if (cpuAfter <= cpuBefore) continue;

        auto const deltaMilliseconds = static_cast<double>(cpuAfter - cpuBefore) / 10000.0;

        attributedMilliseconds += deltaMilliseconds;

        if (found == threadCpuBefore.end())
        {
            workerMilliseconds += deltaMilliseconds;
        }

        if (deltaMilliseconds >= 1.0) activeThreadCount++;

        if (deltaMilliseconds > busiestThreadMilliseconds)
        {
            busiestThreadMilliseconds = deltaMilliseconds;
            busiestThreadId = threadId;
            busiestThreadIsNew = (found == threadCpuBefore.end());
        }
    }

    report << L"  threads using >1 ms  : " << activeThreadCount
           << L"   (total attributed " << attributedMilliseconds << L" ms)\r\n";
    report << L"  busiest thread       : " << busiestThreadMilliseconds << L" ms on tid " << busiestThreadId
           << (busiestThreadIsNew ? L"  [created by Initialize = scheduler worker]" : L"  [pre-existing thread]") << L"\r\n";
    report << L"  SCHEDULER WORKER CPU : " << workerMilliseconds << L" ms = "
           << (wallMilliseconds > 0.0 ? (workerMilliseconds * 100.0 / wallMilliseconds) : 0.0)
           << L"% of one core   <-- the number that matters\r\n";

    m_lastWorkerCpuPercent = (wallMilliseconds > 0.0) ? (workerMilliseconds * 100.0 / wallMilliseconds) : 0.0;

    WEX::Logging::Log::Comment(report.str().c_str());

    VERIFY_ARE_EQUAL(receivedCount, expectedArrivals);
}


void MidiSchedulerTransformTests::BaselineTimingByLeadTime()
{
    // Walks across the 2 second sleep floor in CalculateSafeSleepTime.
    RunBaselineScenario(L"Lead 5 ms", 200, 5, 5000, false);
    RunBaselineScenario(L"Lead 50 ms", 200, 50, 5000, false);
    RunBaselineScenario(L"Lead 500 ms", 200, 500, 5000, false);
    RunBaselineScenario(L"Lead 2000 ms", 200, 2000, 5000, false);
    RunBaselineScenario(L"Lead 10000 ms", 200, 10000, 5000, false);
}

void MidiSchedulerTransformTests::BaselineTimingByQueueDepth()
{
    RunBaselineScenario(L"Depth 10", 10, 3000, 1000, false);
    RunBaselineScenario(L"Depth 500", 500, 3000, 1000, false);
    RunBaselineScenario(L"Depth 5000", 5000, 3000, 1000, false);
    RunBaselineScenario(L"Depth 9000", 9000, 3000, 1000, false);
}

void MidiSchedulerTransformTests::BaselineIdenticalTimestampBurst()
{
    // Every message shares one timestamp, which is the drain-a-run path in the worker.
    RunBaselineScenario(L"Burst 500 same timestamp", 500, 3000, 0, false);
    RunBaselineScenario(L"Burst 5000 same timestamp", 5000, 3000, 0, false);
}

void MidiSchedulerTransformTests::BaselineShuffledEnqueueOrder()
{
    RunBaselineScenario(L"Shuffled 500", 500, 3000, 1000, true);
    RunBaselineScenario(L"Shuffled 5000", 5000, 3000, 1000, true);
}

void MidiSchedulerTransformTests::SchedulerDoesNotBusyWait()
{
    if (!Feature_Servicing_MIDI2SchedulerV2::IsEnabled())
    {
        WEX::Logging::Log::Comment(L"Feature_Servicing_MIDI2SchedulerV2 is disabled. The original scheduler busy waits by design, so this is skipped.");
        return;
    }

    RunBaselineScenario(L"Busy wait regression guard", 200, 1000, 5000, false);

    // The original scheduler measured 84-94% of a core for this scenario and the replacement
    // measures 2-20%. The threshold sits in the gap on purpose, so a loaded machine cannot make
    // this flaky: a busy machine gives the worker less CPU, never more.
    VERIFY_IS_LESS_THAN(m_lastWorkerCpuPercent, 60.0);
}


namespace
{
    enum class WakeupMechanism
    {
        HighResolutionTimer,
        DefaultTimer,
        EventWithTimeout
    };

    struct WakeupStats
    {
        double MeanUs{};
        double P50Us{};
        double P99Us{};
        double MaxUs{};
    };

    // Overshoot only: how much LATER than requested the thread actually resumed.
    WakeupStats MeasureWakeupAccuracy(
        _In_ WakeupMechanism const mechanism,
        _In_ double const requestedMilliseconds,
        _In_ uint32_t const iterations,
        _In_ double const qpf)
    {
        wil::unique_handle waitObject;

        switch (mechanism)
        {
        case WakeupMechanism::HighResolutionTimer:
            waitObject.reset(CreateWaitableTimerExW(nullptr, nullptr,
                CREATE_WAITABLE_TIMER_MANUAL_RESET | CREATE_WAITABLE_TIMER_HIGH_RESOLUTION, TIMER_ALL_ACCESS));
            break;

        case WakeupMechanism::DefaultTimer:
            waitObject.reset(CreateWaitableTimerExW(nullptr, nullptr,
                CREATE_WAITABLE_TIMER_MANUAL_RESET, TIMER_ALL_ACCESS));
            break;

        default:
            waitObject.reset(CreateEventW(nullptr, TRUE, FALSE, nullptr));
            break;
        }

        WakeupStats stats{};

        if (!waitObject) return stats;

        auto const requestedTicks = static_cast<int64_t>((requestedMilliseconds * qpf) / 1000.0);

        std::vector<int64_t> overshoot;
        overshoot.reserve(iterations);

        for (uint32_t i = 0; i < iterations; i++)
        {
            LARGE_INTEGER before{};
            QueryPerformanceCounter(&before);

            if (mechanism == WakeupMechanism::EventWithTimeout)
            {
                WaitForSingleObject(waitObject.get(), static_cast<DWORD>(requestedMilliseconds));
            }
            else
            {
                LARGE_INTEGER dueTime{};
                dueTime.QuadPart = -static_cast<LONGLONG>(requestedMilliseconds * 10000.0);

                SetWaitableTimer(waitObject.get(), &dueTime, 0, nullptr, nullptr, FALSE);
                WaitForSingleObject(waitObject.get(), INFINITE);
            }

            LARGE_INTEGER after{};
            QueryPerformanceCounter(&after);

            overshoot.push_back((after.QuadPart - before.QuadPart) - requestedTicks);
        }

        std::sort(overshoot.begin(), overshoot.end());

        auto const total = std::accumulate(overshoot.begin(), overshoot.end(), static_cast<int64_t>(0));

        stats.MeanUs = TicksToMicroseconds(total / static_cast<int64_t>(overshoot.size()), qpf);
        stats.P50Us = TicksToMicroseconds(PercentileOfSorted(overshoot, 0.50), qpf);
        stats.P99Us = TicksToMicroseconds(PercentileOfSorted(overshoot, 0.99), qpf);
        stats.MaxUs = TicksToMicroseconds(overshoot.back(), qpf);

        return stats;
    }

    wchar_t const* DescribeMechanism(_In_ WakeupMechanism const mechanism)
    {
        switch (mechanism)
        {
        case WakeupMechanism::HighResolutionTimer:  return L"high-res timer";
        case WakeupMechanism::DefaultTimer:         return L"default timer ";
        default:                                    return L"event timeout ";
        }
    }
}

void MidiSchedulerTransformTests::ProbeWakeupAccuracy()
{
    LARGE_INTEGER frequency{};
    QueryPerformanceFrequency(&frequency);
    auto const qpf = static_cast<double>(frequency.QuadPart);

    // The production worker runs elevated, so measure under the same conditions.
    auto const originalThreadPriority = GetThreadPriority(GetCurrentThread());
    SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_TIME_CRITICAL);

    double const delays[]{ 1.0, 2.0, 5.0, 10.0, 50.0 };

    WakeupMechanism const mechanisms[]{
        WakeupMechanism::HighResolutionTimer,
        WakeupMechanism::DefaultTimer,
        WakeupMechanism::EventWithTimeout };

    std::wstringstream report;
    report << std::fixed << std::setprecision(1);
    report << L"\r\n=== Wakeup overshoot, microseconds later than requested ===\r\n";
    report << L"  mechanism        requested     mean      p50      p99      max\r\n";

    for (auto const mechanism : mechanisms)
    {
        for (auto const delay : delays)
        {
            auto const iterations = (delay >= 50.0) ? 40u : 150u;

            auto const stats = MeasureWakeupAccuracy(mechanism, delay, iterations, qpf);

            report << L"  " << DescribeMechanism(mechanism)
                   << std::setw(9) << delay << L" ms"
                   << std::setw(9) << stats.MeanUs
                   << std::setw(9) << stats.P50Us
                   << std::setw(9) << stats.P99Us
                   << std::setw(9) << stats.MaxUs << L"\r\n";
        }
    }

    // Same sweep while the process holds a 1 ms timer period, to show whether the high resolution
    // timer needs it. If it does not, midisrv never has to raise the global timer rate.
    auto const periodResult = timeBeginPeriod(1);

    report << L"\r\n  --- with timeBeginPeriod(1) held by this process ---\r\n";

    for (auto const mechanism : mechanisms)
    {
        auto const stats = MeasureWakeupAccuracy(mechanism, 2.0, 150u, qpf);

        report << L"  " << DescribeMechanism(mechanism)
               << std::setw(9) << 2.0 << L" ms"
               << std::setw(9) << stats.MeanUs
               << std::setw(9) << stats.P50Us
               << std::setw(9) << stats.P99Us
               << std::setw(9) << stats.MaxUs << L"\r\n";
    }

    if (periodResult == TIMERR_NOERROR)
    {
        timeEndPeriod(1);
    }

    SetThreadPriority(GetCurrentThread(), originalThreadPriority);

    WEX::Logging::Log::Comment(report.str().c_str());
}



