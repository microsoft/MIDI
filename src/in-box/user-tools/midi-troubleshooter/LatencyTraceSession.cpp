// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#include "LatencyTraceSession.h"
#include "LatencyAnalyzer.h"

#include <algorithm>
#include <format>
#include <vector>

#pragma comment(lib, "advapi32.lib")

namespace miditroubleshooter::latency
{
    namespace
    {
        // {9204610e-12a3-4acd-80a4-93748e5f492c}, this check's own session
        constexpr GUID SessionGuid{ 0x9204610e, 0x12a3, 0x4acd, { 0x80, 0xa4, 0x93, 0x74, 0x8e, 0x5f, 0x49, 0x2c } };

        // From evntrace.h in the Windows SDK, copied so an older SDK still builds this.
        // {d4bbee17-b545-4888-858b-744169015b25}
        constexpr GUID SystemInterruptProvider{ 0xd4bbee17, 0xb545, 0x4888, { 0x85, 0x8b, 0x74, 0x41, 0x69, 0x01, 0x5b, 0x25 } };
        constexpr ULONGLONG InterruptKeywordGeneral{ 0x1 };
        constexpr ULONGLONG InterruptKeywordDpc{ 0x4 };
        constexpr ULONGLONG InterruptKeywordFrameworkDpc{ 0x10 };
        constexpr ULONGLONG InterruptKeywordFrameworkInterrupt{ 0x20 };

        // Large enough for a few seconds of a busy PC if the reader falls behind.
        constexpr ULONG BufferSizeKilobytes{ 256 };
        constexpr ULONG MinimumBufferCount{ 64 };
        constexpr ULONG MaximumBufferCount{ 512 };

        // the reader sees each processor's events at least this often, in seconds
        constexpr ULONG FlushSeconds{ 1 };

        // EVENT_TRACE_PROPERTIES followed by room for the session name and the file name
        struct PropertiesBuffer
        {
            explicit PropertiesBuffer(_In_ std::wstring const& fileName)
            {
                auto const nameBytes = (std::size(KernelSession::SessionName)) * sizeof(wchar_t);
                auto const fileBytes = (fileName.size() + 1) * sizeof(wchar_t);

                m_bytes.resize(sizeof(EVENT_TRACE_PROPERTIES) + nameBytes + (std::max)(fileBytes, static_cast<size_t>(MAX_PATH * sizeof(wchar_t))));

                auto* const properties = Get();
                properties->Wnode.BufferSize = static_cast<ULONG>(m_bytes.size());
                properties->LoggerNameOffset = sizeof(EVENT_TRACE_PROPERTIES);
                properties->LogFileNameOffset = static_cast<ULONG>(sizeof(EVENT_TRACE_PROPERTIES) + nameBytes);

                if (!fileName.empty())
                {
                    std::memcpy(m_bytes.data() + properties->LogFileNameOffset, fileName.c_str(), fileBytes);
                }
            }

            EVENT_TRACE_PROPERTIES* Get() noexcept
            {
                return reinterpret_cast<EVENT_TRACE_PROPERTIES*>(m_bytes.data());
            }

        private:
            std::vector<uint8_t> m_bytes{};
        };

        // Turns a privilege on in this process's token. Returns a Win32 error code, and whether
        // it was off before, so it can be turned off again.
        uint32_t EnablePrivilege(_In_z_ wchar_t const* const name, _Out_ bool& wasDisabled) noexcept
        {
            wasDisabled = false;

            HANDLE token{ nullptr };

            if (!OpenProcessToken(GetCurrentProcess(), TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY, &token))
            {
                return GetLastError();
            }

            TOKEN_PRIVILEGES privileges{};
            privileges.PrivilegeCount = 1;
            privileges.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;

            uint32_t result{ ERROR_SUCCESS };

            if (!LookupPrivilegeValueW(nullptr, name, &privileges.Privileges[0].Luid))
            {
                result = GetLastError();
            }
            else
            {
                TOKEN_PRIVILEGES previous{};
                DWORD previousSize{ sizeof(previous) };

                if (!AdjustTokenPrivileges(token, FALSE, &privileges, sizeof(previous), &previous, &previousSize))
                {
                    result = GetLastError();
                }
                else
                {
                    // ERROR_NOT_ALL_ASSIGNED when the token doesn't hold it at all
                    result = GetLastError();
                    wasDisabled = result == ERROR_SUCCESS && previous.PrivilegeCount == 1 &&
                        (previous.Privileges[0].Attributes & SE_PRIVILEGE_ENABLED) == 0;
                }
            }

            CloseHandle(token);

            return result;
        }

        void DisablePrivilege(_In_z_ wchar_t const* const name) noexcept
        {
            HANDLE token{ nullptr };

            if (!OpenProcessToken(GetCurrentProcess(), TOKEN_ADJUST_PRIVILEGES, &token))
            {
                return;
            }

            TOKEN_PRIVILEGES privileges{};
            privileges.PrivilegeCount = 1;
            privileges.Privileges[0].Attributes = 0;

            if (LookupPrivilegeValueW(nullptr, name, &privileges.Privileges[0].Luid))
            {
                AdjustTokenPrivileges(token, FALSE, &privileges, 0, nullptr, nullptr);
            }

            CloseHandle(token);
        }

        std::wstring QueryGroupMask(_In_ TRACEHANDLE const handle)
        {
            // PERFINFO_GROUPMASK is eight ULONGs
            ULONG mask[8]{};
            ULONG returned{ 0 };

            if (TraceQueryInformation(handle, TraceSystemTraceEnableFlagsInfo, mask, sizeof(mask), &returned) != ERROR_SUCCESS)
            {
                return {};
            }

            std::wstring text{};

            for (auto const value : mask)
            {
                text += std::format(L"{}{:08X}", text.empty() ? L"" : L" ", value);
            }

            return text;
        }

        uint32_t QueryProfileInterval() noexcept
        {
            TRACE_PROFILE_INTERVAL interval{};
            ULONG returned{ 0 };

            if (TraceQueryInformation(0, TraceSampledProfileIntervalInfo, &interval, sizeof(interval), &returned) != ERROR_SUCCESS)
            {
                return 0;
            }

            return interval.Interval;
        }
    }

    KernelSession::~KernelSession()
    {
        Stop();
    }

    void KernelSession::StopLeftoverSession() noexcept
    {
        try
        {
            PropertiesBuffer properties{ std::wstring{} };
            ControlTraceW(0, SessionName, properties.Get(), EVENT_TRACE_CONTROL_STOP);
        }
        catch (...)
        {
        }
    }

    _Use_decl_annotations_
    uint32_t KernelSession::Start(SessionOptions const& options) noexcept
    {
        try
        {
            Stop();
            StopLeftoverSession();

            m_info = SessionInfo{};

            // Profile samples need this privilege on in the token of the process that starts the
            // session. Administrators hold it, but it's off until a program asks for it.
            if (options.CollectSamples)
            {
                bool wasDisabled{ false };
                m_info.ProfilePrivilegeError = EnablePrivilege(SE_SYSTEM_PROFILE_NAME, wasDisabled);
                m_info.SamplesEnabled = m_info.ProfilePrivilegeError == ERROR_SUCCESS;
                m_restoreProfilePrivilege = wasDisabled;
            }

            auto const start = [this, &options](bool const withSamples) -> uint32_t
                {
                    PropertiesBuffer buffer{ options.TraceFilePath };
                    auto* const properties = buffer.Get();

                    properties->Wnode.Guid = SessionGuid;
                    properties->Wnode.Flags = WNODE_FLAG_TRACED_GUID;

                    // QueryPerformanceCounter, the same clock the probe uses
                    properties->Wnode.ClientContext = 1;

                    properties->BufferSize = BufferSizeKilobytes;
                    properties->MinimumBuffers = MinimumBufferCount;
                    properties->MaximumBuffers = MaximumBufferCount;
                    properties->FlushTimer = FlushSeconds;
                    properties->LogFileMode = EVENT_TRACE_REAL_TIME_MODE | EVENT_TRACE_SYSTEM_LOGGER_MODE;

                    if (!options.TraceFilePath.empty())
                    {
                        properties->LogFileMode |= EVENT_TRACE_FILE_MODE_SEQUENTIAL;
                        properties->MaximumFileSize = options.MaximumFileMegabytes;
                    }
                    else
                    {
                        properties->LogFileNameOffset = 0;
                    }

                    properties->EnableFlags =
                        EVENT_TRACE_FLAG_PROCESS | EVENT_TRACE_FLAG_THREAD | EVENT_TRACE_FLAG_IMAGE_LOAD |
                        EVENT_TRACE_FLAG_DPC | EVENT_TRACE_FLAG_INTERRUPT |
                        EVENT_TRACE_FLAG_CSWITCH | EVENT_TRACE_FLAG_DISPATCHER;

                    if (withSamples)
                    {
                        properties->EnableFlags |= EVENT_TRACE_FLAG_PROFILE;
                    }

                    if (options.CollectHardFaults)
                    {
                        properties->EnableFlags |= EVENT_TRACE_FLAG_MEMORY_HARD_FAULTS;
                    }

                    TRACEHANDLE handle{ 0 };
                    auto const status = StartTraceW(&handle, SessionName, properties);

                    if (status == ERROR_SUCCESS)
                    {
                        m_handle = handle;
                    }

                    return status;
                };

            auto status = start(m_info.SamplesEnabled);

            // Without samples the check still works; it just can't look inside the frameworks.
            if (status != ERROR_SUCCESS && m_info.SamplesEnabled)
            {
                m_info.SamplesEnabled = false;
                status = start(false);
            }

            if (status != ERROR_SUCCESS)
            {
                m_info.StartError = status;

                if (m_restoreProfilePrivilege)
                {
                    DisablePrivilege(SE_SYSTEM_PROFILE_NAME);
                    m_restoreProfilePrivilege = false;
                }

                return status;
            }

            m_info.Started = true;
            m_info.WritingFile = !options.TraceFilePath.empty();

            // The driver framework's own events name the driver behind each of its DPCs and
            // interrupts. They need the newer way of turning kernel events on, which a Windows
            // older than Windows 11 doesn't have; the check works without them.
            ENABLE_TRACE_PARAMETERS parameters{};
            parameters.Version = ENABLE_TRACE_PARAMETERS_VERSION_2;

            m_info.FrameworkEventsError = EnableTraceEx2(
                m_handle,
                &SystemInterruptProvider,
                EVENT_CONTROL_CODE_ENABLE_PROVIDER,
                TRACE_LEVEL_VERBOSE,
                InterruptKeywordGeneral | InterruptKeywordDpc | InterruptKeywordFrameworkDpc | InterruptKeywordFrameworkInterrupt,
                0,
                0,
                &parameters);

            m_info.FrameworkEventsEnabled = m_info.FrameworkEventsError == ERROR_SUCCESS;

            // the probe names its thread through this provider, so a saved trace says which it was
            m_info.ProbeProviderEnabled = EnableTraceEx2(
                m_handle,
                &LatencyCheckProviderGuid,
                EVENT_CONTROL_CODE_ENABLE_PROVIDER,
                TRACE_LEVEL_VERBOSE,
                0,
                0,
                0,
                &parameters) == ERROR_SUCCESS;

            m_info.GroupMask = QueryGroupMask(m_handle);
            m_info.ProfileInterval = QueryProfileInterval();

            return ERROR_SUCCESS;
        }
        catch (...)
        {
            Stop();
            m_info.StartError = ERROR_OUTOFMEMORY;
            return ERROR_OUTOFMEMORY;
        }
    }

    void KernelSession::Stop() noexcept
    {
        auto const handle = m_handle.exchange(0);

        if (handle == 0)
        {
            return;
        }

        try
        {
            PropertiesBuffer properties{ std::wstring{} };

            if (ControlTraceW(handle, nullptr, properties.Get(), EVENT_TRACE_CONTROL_STOP) == ERROR_SUCCESS)
            {
                m_info.EventsLost = properties.Get()->EventsLost;
                m_info.BuffersLost = properties.Get()->LogBuffersLost;
                m_info.RealTimeBuffersLost = properties.Get()->RealTimeBuffersLost;
            }
        }
        catch (...)
        {
        }

        if (m_restoreProfilePrivilege)
        {
            DisablePrivilege(SE_SYSTEM_PROFILE_NAME);
            m_restoreProfilePrivilege = false;
        }
    }

    TraceReader::~TraceReader()
    {
        Cancel();
    }

    _Use_decl_annotations_
    uint32_t TraceReader::Open(EVENT_TRACE_LOGFILEW& logFile) noexcept
    {
        logFile.ProcessTraceMode |= PROCESS_TRACE_MODE_EVENT_RECORD | PROCESS_TRACE_MODE_RAW_TIMESTAMP;
        logFile.EventRecordCallback = &TraceReader::OnEventRecord;
        logFile.Context = this;

        auto const handle = OpenTraceW(&logFile);

        if (handle == INVALID_PROCESSTRACE_HANDLE)
        {
            return GetLastError();
        }

        m_handle = handle;

        auto const& header = logFile.LogfileHeader;

        m_header.ClockType = header.ReservedFlags == 0 ? 1 : header.ReservedFlags;
        m_header.ProcessorCount = header.NumberOfProcessors;
        m_header.PointerSize = header.PointerSize == 0 ? static_cast<uint32_t>(sizeof(void*)) : header.PointerSize;
        m_header.WindowsBuild = header.ProviderVersion;
        m_header.EventsLost = header.EventsLost;
        m_header.BuffersLost = header.BuffersLost;

        switch (m_header.ClockType)
        {
        case 2:
            // system time, in 100 ns units
            m_header.Frequency = 10'000'000;
            break;

        case 3:
            // the processor's cycle counter
            m_header.Frequency = static_cast<uint64_t>(header.CpuSpeedInMHz) * 1'000'000;
            break;

        default:
            m_header.Frequency = static_cast<uint64_t>(header.PerfFreq.QuadPart);
            break;
        }

        // a live session on this PC: the clock is this PC's
        if (m_header.Frequency == 0)
        {
            LARGE_INTEGER frequency{};
            QueryPerformanceFrequency(&frequency);
            m_header.Frequency = static_cast<uint64_t>(frequency.QuadPart);
        }

        if (m_header.ProcessorCount == 0)
        {
            m_header.ProcessorCount = GetActiveProcessorCount(ALL_PROCESSOR_GROUPS);
        }

        return ERROR_SUCCESS;
    }

    _Use_decl_annotations_
    uint32_t TraceReader::OpenFile(std::wstring const& path) noexcept
    {
        EVENT_TRACE_LOGFILEW logFile{};
        logFile.LogFileName = const_cast<LPWSTR>(path.c_str());

        return Open(logFile);
    }

    _Use_decl_annotations_
    uint32_t TraceReader::OpenLive(std::wstring const& sessionName) noexcept
    {
        EVENT_TRACE_LOGFILEW logFile{};
        logFile.LoggerName = const_cast<LPWSTR>(sessionName.c_str());
        logFile.ProcessTraceMode = PROCESS_TRACE_MODE_REAL_TIME;

        return Open(logFile);
    }

    _Use_decl_annotations_
    uint32_t TraceReader::Process(Callback callback) noexcept
    {
        PROCESSTRACE_HANDLE handle = m_handle;

        if (handle == INVALID_PROCESSTRACE_HANDLE)
        {
            return ERROR_INVALID_HANDLE;
        }

        m_callback = std::move(callback);

        auto const status = ProcessTrace(&handle, 1, nullptr, nullptr);

        // Cancel may have closed it already
        if (auto const open = m_handle.exchange(INVALID_PROCESSTRACE_HANDLE); open != INVALID_PROCESSTRACE_HANDLE)
        {
            CloseTrace(open);
        }

        return status;
    }

    void TraceReader::Cancel() noexcept
    {
        if (auto const open = m_handle.exchange(INVALID_PROCESSTRACE_HANDLE); open != INVALID_PROCESSTRACE_HANDLE)
        {
            CloseTrace(open);
        }
    }

    _Use_decl_annotations_
    void WINAPI TraceReader::OnEventRecord(PEVENT_RECORD record)
    {
        if (record == nullptr || record->UserContext == nullptr)
        {
            return;
        }

        auto* const reader = static_cast<TraceReader*>(record->UserContext);

        try
        {
            if (reader->m_callback)
            {
                reader->m_callback(*record);
            }
        }
        catch (...)
        {
            // nothing may leave a trace callback
        }
    }
}
