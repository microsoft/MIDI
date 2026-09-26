// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================
// MIDI Glass phase 0 spike. Nothing here ships.

#include "pch.h"
#include "RunStats.h"

namespace gspike
{
    namespace
    {
        int64_t ReadFrequency() noexcept
        {
            LARGE_INTEGER frequency{};
            QueryPerformanceFrequency(&frequency);
            return frequency.QuadPart;
        }

        std::wstring EscapeJson(std::wstring const& value)
        {
            std::wstring out;
            out.reserve(value.size() + 8);

            for (wchar_t c : value)
            {
                switch (c)
                {
                case L'"': out += L"\\\""; break;
                case L'\\': out += L"\\\\"; break;
                case L'\n': out += L"\\n"; break;
                case L'\r': out += L"\\r"; break;
                case L'\t': out += L"\\t"; break;
                default:
                    if (c < 0x20)
                    {
                        out += std::format(L"\\u{:04x}", static_cast<uint32_t>(c));
                    }
                    else
                    {
                        out += c;
                    }
                    break;
                }
            }

            return out;
        }
    }

    int64_t QpcFrequency() noexcept
    {
        static const int64_t frequency = ReadFrequency();
        return frequency;
    }

    int64_t QpcToMicroseconds(int64_t counter) noexcept
    {
        const int64_t frequency = QpcFrequency();
        return frequency == 0 ? 0 : (counter * 1000000ll) / frequency;
    }

    int64_t NowMicroseconds() noexcept
    {
        LARGE_INTEGER counter{};
        QueryPerformanceCounter(&counter);
        return QpcToMicroseconds(counter.QuadPart);
    }

    SampleSet::SampleSet(size_t capacity)
    {
        m_values.resize(capacity);
    }

    void SampleSet::Add(double value) noexcept
    {
        if (m_count >= m_values.size())
        {
            m_overflowed = true;
            return;
        }

        m_values[m_count++] = value;
    }

    void SampleSet::Clear() noexcept
    {
        m_count = 0;
        m_overflowed = false;
    }

    double SampleSet::Percentile(double fraction) const
    {
        if (m_count == 0)
        {
            return 0.0;
        }

        std::vector<double> sorted(m_values.begin(), m_values.begin() + static_cast<ptrdiff_t>(m_count));
        std::sort(sorted.begin(), sorted.end());

        const size_t index = std::min(
            m_count - 1,
            static_cast<size_t>(fraction * static_cast<double>(m_count - 1) + 0.5));

        return sorted[index];
    }

    double SampleSet::Mean() const noexcept
    {
        if (m_count == 0)
        {
            return 0.0;
        }

        double total = 0.0;
        for (size_t i = 0; i < m_count; i++)
        {
            total += m_values[i];
        }

        return total / static_cast<double>(m_count);
    }

    double SampleSet::Max() const noexcept
    {
        double highest = 0.0;
        for (size_t i = 0; i < m_count; i++)
        {
            highest = std::max(highest, m_values[i]);
        }

        return highest;
    }

    size_t SampleSet::CountAbove(double threshold) const noexcept
    {
        size_t hits = 0;
        for (size_t i = 0; i < m_count; i++)
        {
            if (m_values[i] > threshold)
            {
                hits++;
            }
        }

        return hits;
    }

    MemorySnapshot TakeMemorySnapshot() noexcept
    {
        MemorySnapshot snapshot{};

        PROCESS_MEMORY_COUNTERS_EX counters{};
        counters.cb = sizeof(counters);

        if (GetProcessMemoryInfo(
            GetCurrentProcess(),
            reinterpret_cast<PROCESS_MEMORY_COUNTERS*>(&counters),
            sizeof(counters)))
        {
            snapshot.PrivateBytes = counters.PrivateUsage;
            snapshot.WorkingSetBytes = counters.WorkingSetSize;
        }

        return snapshot;
    }

    CpuSnapshot TakeCpuSnapshot() noexcept
    {
        CpuSnapshot snapshot{};

        const auto toUint64 = [](FILETIME const& value) noexcept
            {
                return (static_cast<uint64_t>(value.dwHighDateTime) << 32) | value.dwLowDateTime;
            };

        FILETIME idle{}, kernel{}, user{};

        if (GetSystemTimes(&idle, &kernel, &user))
        {
            snapshot.SystemIdle = toUint64(idle);
            snapshot.SystemKernel = toUint64(kernel);
            snapshot.SystemUser = toUint64(user);
        }

        FILETIME created{}, exited{}, processKernel{}, processUser{};

        if (GetProcessTimes(GetCurrentProcess(), &created, &exited, &processKernel, &processUser))
        {
            snapshot.ProcessKernel = toUint64(processKernel);
            snapshot.ProcessUser = toUint64(processUser);
        }

        return snapshot;
    }

    double SystemBusyFraction(CpuSnapshot const& start, CpuSnapshot const& end) noexcept
    {
        // GetSystemTimes folds idle into the kernel total, so total is kernel plus user.
        const uint64_t total = (end.SystemKernel - start.SystemKernel) + (end.SystemUser - start.SystemUser);
        const uint64_t idle = end.SystemIdle - start.SystemIdle;

        if (total == 0 || idle > total)
        {
            return 0.0;
        }

        return 1.0 - (static_cast<double>(idle) / static_cast<double>(total));
    }

    double ProcessCpuMilliseconds(CpuSnapshot const& start, CpuSnapshot const& end) noexcept
    {
        const uint64_t hundredNanoseconds =
            (end.ProcessKernel - start.ProcessKernel) + (end.ProcessUser - start.ProcessUser);

        return static_cast<double>(hundredNanoseconds) / 10000.0;
    }

    std::wstring FormatResultJson(RunResult const& r)
    {
        return std::format(
            LR"({{"mode":"{}","controls":{},"animated":{},"seconds":{:.1f},)"
            LR"("buildMs":{:.2f},"firstLayoutMs":{:.2f},"xamlElements":{},"automationPeers":{},)"
            LR"("frameP50Ms":{:.3f},"frameP95Ms":{:.3f},"frameP99Ms":{:.3f},"frameMaxMs":{:.3f},)"
            LR"("frames":{},"framesOver20Ms":{},)"
            LR"("animateP50Us":{:.2f},"animateP99Us":{:.2f},"setValueP50Us":{:.3f},"setValueP99Us":{:.3f},)"
            LR"("privateBytesBaseline":{},"privateBytesAfterBuild":{},"workingSetAfterBuild":{},)"
            LR"("themeSwapMs":{:.2f},"themeSwapReachedEveryControl":{},)"
            LR"("systemBusyPercent":{:.1f},"processCpuMs":{:.1f},"lostActivation":{},"clean":{},)"
            LR"("sendP50Us":{:.2f},"sendP99Us":{:.2f},)"
            LR"("pointerToSendP50Us":{:.1f},"pointerToSendP99Us":{:.1f},)"
            LR"("handlerToSendP50Us":{:.1f},"handlerToSendP99Us":{:.1f},)"
            LR"("pointerSamples":{},"pointerDeviceTypes":"{}","pointerTimestampTrusted":{},)"
            LR"("latencyEndpoint":"{}","latencyNote":"{}"}})",
            EscapeJson(r.Mode), r.ControlCount, r.AnimatedCount, r.DurationSeconds,
            r.BuildMilliseconds, r.FirstLayoutMilliseconds, r.XamlElementCount, r.HasAutomationPeers ? L"true" : L"false",
            r.FrameP50, r.FrameP95, r.FrameP99, r.FrameMax,
            r.FrameCount, r.FramesOver20Ms,
            r.AnimateP50Microseconds, r.AnimateP99Microseconds, r.SetValueP50Microseconds, r.SetValueP99Microseconds,
            r.PrivateBytesBaseline, r.PrivateBytesAfterBuild, r.WorkingSetAfterBuild,
            r.ThemeSwapMilliseconds, r.ThemeSwapReachedEveryControl ? L"true" : L"false",
            r.SystemBusyPercent, r.ProcessCpuMilliseconds,
            r.LostActivation ? L"true" : L"false", r.Clean ? L"true" : L"false",
            r.SendP50Microseconds, r.SendP99Microseconds,
            r.PointerToSendP50Microseconds, r.PointerToSendP99Microseconds,
            r.HandlerToSendP50Microseconds, r.HandlerToSendP99Microseconds,
            r.PointerSampleCount, EscapeJson(r.PointerDeviceTypes), r.PointerTimestampTrusted ? L"true" : L"false",
            EscapeJson(r.LatencyEndpoint), EscapeJson(r.LatencyNote));
    }

    std::wstring FormatResultText(RunResult const& r)
    {
        std::wstring text;

        text += std::format(L"{} - {} controls, {} animating, {:.0f} s\n",
            r.Mode, r.ControlCount, r.AnimatedCount, r.DurationSeconds);
        text += std::format(L"  build {:.1f} ms, first layout {:.1f} ms, {} XAML elements, automation peers {}\n",
            r.BuildMilliseconds, r.FirstLayoutMilliseconds, r.XamlElementCount, r.HasAutomationPeers ? L"yes" : L"no");
        text += std::format(L"  frame p50 {:.2f} ms, p95 {:.2f} ms, p99 {:.2f} ms, max {:.2f} ms, {} of {} over 20 ms\n",
            r.FrameP50, r.FrameP95, r.FrameP99, r.FrameMax, r.FramesOver20Ms, r.FrameCount);
        text += std::format(L"  UI thread per frame, update plus layout: p50 {:.1f} us, p99 {:.1f} us\n",
            r.AnimateP50Microseconds, r.AnimateP99Microseconds);
        text += std::format(L"  one value pushed into one control: p50 {:.3f} us, p99 {:.3f} us (call cost, layout excluded)\n",
            r.SetValueP50Microseconds, r.SetValueP99Microseconds);
        text += std::format(L"  private bytes {:.1f} MB baseline, {:.1f} MB after build (delta {:.1f} MB), working set {:.1f} MB\n",
            static_cast<double>(r.PrivateBytesBaseline) / 1048576.0,
            static_cast<double>(r.PrivateBytesAfterBuild) / 1048576.0,
            static_cast<double>(r.PrivateBytesAfterBuild - r.PrivateBytesBaseline) / 1048576.0,
            static_cast<double>(r.WorkingSetAfterBuild) / 1048576.0);

        if (r.ThemeSwapMilliseconds >= 0.0)
        {
            text += std::format(L"  theme swap {:.1f} ms, reached every control {}\n",
                r.ThemeSwapMilliseconds, r.ThemeSwapReachedEveryControl ? L"yes" : L"NO");
        }

        text += std::format(L"  machine {:.0f} percent busy, this process used {:.0f} ms of processor{}\n",
            r.SystemBusyPercent, r.ProcessCpuMilliseconds,
            r.LostActivation ? L", window lost focus during the run" : L"");

        if (!r.Clean)
        {
            text += L"  !! the machine was in use during this run. Treat the frame numbers as suspect. !!\n";
        }

        if (r.SendP50Microseconds >= 0.0)
        {
            text += std::format(L"  SendSingleMessageWords on its own: p50 {:.2f} us, p99 {:.2f} us\n",
                r.SendP50Microseconds, r.SendP99Microseconds);
        }

        if (r.PointerSampleCount == 0)
        {
            text += L"  input to send: not measured, nothing was touched\n";
        }
        else
        {
            text += std::format(L"  input to send p50 {:.0f} us, p99 {:.0f} us over {} moves ({}{})\n",
                r.PointerToSendP50Microseconds, r.PointerToSendP99Microseconds,
                r.PointerSampleCount, r.PointerDeviceTypes,
                r.PointerTimestampTrusted ? L"" : L", system timestamp not on the QPC timebase");
            text += std::format(L"  handler to send p50 {:.1f} us, p99 {:.1f} us\n",
                r.HandlerToSendP50Microseconds, r.HandlerToSendP99Microseconds);
        }

        if (!r.LatencyNote.empty())
        {
            text += std::format(L"  {}\n", r.LatencyNote);
        }

        return text;
    }

    void AppendLineToFile(std::wstring const& path, std::wstring const& line)
    {
        if (path.empty())
        {
            return;
        }

        wil::unique_hfile file{ CreateFileW(
            path.c_str(),
            FILE_APPEND_DATA,
            FILE_SHARE_READ,
            nullptr,
            OPEN_ALWAYS,
            FILE_ATTRIBUTE_NORMAL,
            nullptr) };

        if (!file)
        {
            return;
        }

        std::wstring text = line;
        text += L"\r\n";

        const int required = WideCharToMultiByte(CP_UTF8, 0, text.c_str(), static_cast<int>(text.size()), nullptr, 0, nullptr, nullptr);
        if (required <= 0)
        {
            return;
        }

        std::string utf8(static_cast<size_t>(required), '\0');
        WideCharToMultiByte(CP_UTF8, 0, text.c_str(), static_cast<int>(text.size()), utf8.data(), required, nullptr, nullptr);

        DWORD written = 0;
        WriteFile(file.get(), utf8.data(), static_cast<DWORD>(utf8.size()), &written, nullptr);
    }
}
