// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services WinRT API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

#include "pch.h"
#include "mididiag_output.h"

#include <userenv.h>

#pragma comment(lib, "userenv.lib")

namespace mididiag
{
    namespace
    {
        // Recursive, because the closing sections are written by functions that already hold it.
        // Timed, because the watchdog must not wait forever on a thread stuck in a write.
        std::recursive_timed_mutex g_outputLock{};

        bool g_anyOutput{ false };
        bool g_lastLineBlank{ false };

        std::mutex g_findingsLock{};
        std::vector<std::pair<std::wstring, std::wstring>> g_findings{};

        struct SectionTiming
        {
            std::wstring Name{};
            uint64_t ElapsedMilliseconds{ 0 };
        };

        std::vector<SectionTiming> g_sectionTimings{};
        std::wstring g_currentSection{};
        std::chrono::steady_clock::time_point g_currentSectionStart{};
        std::chrono::steady_clock::time_point const g_reportStart{ std::chrono::steady_clock::now() };

        // in parentheses so the max macro from windows.h stays out of it
        constexpr auto NoDeadline{ (std::chrono::steady_clock::time_point::max)() };

        std::mutex g_watchdogLock{};
        std::condition_variable_any g_watchdogWake{};
        std::wstring g_watchdogSection{};
        std::chrono::steady_clock::time_point g_watchdogDeadline{ NoDeadline };
        std::chrono::milliseconds g_watchdogTimeout{ 0 };
        bool g_watchdogServicePhase{ false };
        uint64_t g_watchdogGeneration{ 0 };
        ReportPhase g_phase{ ReportPhase::Local };
        std::chrono::milliseconds g_phaseTimeout{ 0 };
        std::jthread g_watchdogThread{};

        uint64_t MillisecondsSince(_In_ std::chrono::steady_clock::time_point const start)
        {
            return static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now() - start).count());
        }

        // A device can put any character in its name, and a line break would let it write
        // a line of its own into a report that scripts read.
        std::wstring CleanText(_In_ std::wstring_view const text)
        {
            std::wstring cleaned{ text };

            for (auto& ch : cleaned)
            {
                if (ch < 0x20 || ch == 0x7F || (ch >= 0x80 && ch <= 0x9F))
                {
                    ch = L'?';
                }
            }

            return cleaned;
        }

        std::wstring QuoteIfNeeded(_In_ std::wstring const& value)
        {
            if (!value.empty() && value.find_first_of(L" =\"") == std::wstring::npos)
            {
                return value;
            }

            std::wstring quoted{ L"\"" };

            for (auto const ch : value)
            {
                quoted += ch;

                if (ch == L'"')
                {
                    quoted += L'"';
                }
            }

            quoted += L'"';

            return quoted;
        }

        std::wstring LowerCopy(_In_ std::wstring_view const text)
        {
            std::wstring lower{ text };

            for (auto& ch : lower)
            {
                ch = static_cast<wchar_t>(::towlower(ch));
            }

            return lower;
        }

        std::wstring WithoutTrailingSeparator(_In_ std::wstring const& path)
        {
            auto trimmed = path;

            while (!trimmed.empty() && (trimmed.back() == L'\\' || trimmed.back() == L'/'))
            {
                trimmed.pop_back();
            }

            return trimmed;
        }

        struct ProfileFolders
        {
            std::wstring UserProfileLower{};
            std::wstring ProfilesRootLower{};
        };

        ProfileFolders const& GetProfileFolders()
        {
            static ProfileFolders const folders = []()
                {
                    ProfileFolders result{};

                    wchar_t buffer[MAX_PATH * 2]{};

                    auto const length = ::GetEnvironmentVariableW(L"USERPROFILE", buffer, ARRAYSIZE(buffer));

                    if (length > 0 && length < ARRAYSIZE(buffer))
                    {
                        result.UserProfileLower = LowerCopy(WithoutTrailingSeparator(buffer));
                    }

                    DWORD size{ ARRAYSIZE(buffer) };

                    if (::GetProfilesDirectoryW(buffer, &size))
                    {
                        result.ProfilesRootLower = LowerCopy(WithoutTrailingSeparator(buffer));
                    }

                    return result;
                }();

            return folders;
        }

        bool IsPathBoundary(_In_ std::wstring const& text, _In_ size_t const position)
        {
            return position >= text.size() || text[position] == L'\\' || text[position] == L'/';
        }

        void ReplaceCurrentUserProfile(_Inout_ std::wstring& text, _In_ std::wstring const& profileLower)
        {
            if (profileLower.empty())
            {
                return;
            }

            constexpr std::wstring_view replacement{ L"%USERPROFILE%" };

            auto lower = LowerCopy(text);
            size_t position{ 0 };

            while ((position = lower.find(profileLower, position)) != std::wstring::npos)
            {
                // C:\Users\Pete must not match the start of C:\Users\Peter
                if (!IsPathBoundary(lower, position + profileLower.size()))
                {
                    position += profileLower.size();
                    continue;
                }

                text.replace(position, profileLower.size(), replacement);
                lower.replace(position, profileLower.size(), LowerCopy(replacement));
                position += replacement.size();
            }
        }

        // other people's folders, and the one person's folder when the report runs as another account
        void ReplaceOtherProfiles(_Inout_ std::wstring& text, _In_ std::wstring const& rootLower)
        {
            if (rootLower.empty())
            {
                return;
            }

            constexpr std::wstring_view replacement{ L"<user>" };

            auto const needle = rootLower + L"\\";
            auto lower = LowerCopy(text);
            size_t position{ 0 };

            while ((position = lower.find(needle, position)) != std::wstring::npos)
            {
                auto const start = position + needle.size();
                auto end = lower.find_first_of(L"\\/", start);

                if (end == std::wstring::npos)
                {
                    end = lower.size();
                }

                auto const folder = std::wstring_view{ lower }.substr(start, end - start);

                if (folder.empty() || folder == L"public" || folder == L"default" || folder == L"default user" ||
                    folder == L"all users" || folder == LowerCopy(replacement))
                {
                    position = end;
                    continue;
                }

                text.replace(start, end - start, replacement);
                lower.replace(start, end - start, LowerCopy(replacement));
                position = start + replacement.size();
            }
        }

        void WriteFieldText(_In_ std::wstring_view const label, _In_ std::wstring const& value, _In_ fmt::text_style const& valueStyle)
        {
            std::scoped_lock lock{ g_outputLock };

            fmt::print(L"{}", Styled(fmt::format(L"{:<{}}", label, MIDIDIAG_MAX_FIELD_LABEL_WIDTH), fieldLabelTextStyle));
            fmt::print(L"{}", Styled(std::wstring{ MIDIDIAG_FIELD_SEPARATOR }, separatorTextStyle));
            fmt::println(L"{}", Styled(value, valueStyle));

            g_anyOutput = true;
            g_lastLineBlank = false;
        }

        // The header alone: no timing, and the watchdog is left as it is
        void WriteSectionHeader(_In_ std::wstring_view const sectionName)
        {
            std::scoped_lock lock{ g_outputLock };

            if (g_anyOutput && !g_lastLineBlank)
            {
                fmt::println(L"");
            }

            fmt::println(L"{}", Styled(std::wstring{ MIDIDIAG_SECTION_HEADER_PREFIX } + std::wstring{ sectionName }, infoTextStyle));

            g_anyOutput = true;
            g_lastLineBlank = false;

            // redirected output is buffered, so send the last section and this header before a crash can lose them
            fflush(stdout);
        }

        void EndCurrentSectionTiming()
        {
            std::scoped_lock lock{ g_outputLock };

            if (!g_currentSection.empty())
            {
                g_sectionTimings.push_back({ g_currentSection, MillisecondsSince(g_currentSectionStart) });
                g_currentSection.clear();
            }
        }

        void ArmWatchdog(_In_ std::wstring_view const sectionName, _In_ std::chrono::milliseconds const timeout)
        {
            {
                std::scoped_lock lock{ g_watchdogLock };

                g_watchdogSection = sectionName;
                g_watchdogTimeout = timeout;
                g_watchdogServicePhase = g_phase == ReportPhase::Service;
                g_watchdogDeadline = timeout.count() > 0 ?
                    std::chrono::steady_clock::now() + timeout :
                    NoDeadline;

                ++g_watchdogGeneration;
            }

            g_watchdogWake.notify_all();
        }

        // Runs on the watchdog thread while another thread is stuck in a call that may never
        // return, so it finishes the report itself and ends the process.
        void ReportTimeoutAndExit(
            _In_ std::wstring const& sectionName,
            _In_ std::chrono::milliseconds const timeout,
            _In_ bool const servicePhase) noexcept
        {
            try
            {
                std::unique_lock output{ g_outputLock, std::defer_lock };

                if (output.try_lock_for(std::chrono::seconds{ 2 }))
                {
                    auto const seconds = static_cast<uint32_t>(
                        std::chrono::duration_cast<std::chrono::seconds>(timeout).count());

                    WriteField(MIDIDIAG_FIELD_LABEL_SECTION_TIMED_OUT, sectionName);

                    if (servicePhase)
                    {
                        AddFinding(L"service_not_responding",
                            FormatResourceString(IDS_FINDING_SERVICE_NOT_RESPONDING, seconds));
                    }
                    else
                    {
                        AddFinding(L"section_timed_out",
                            FormatResourceString(IDS_FINDING_SECTION_TIMED_OUT, sectionName, seconds));
                    }

                    WriteFindingsSection();
                    WriteSectionTimingSection();

                    WriteSectionHeader(MIDIDIAG_SECTION_LABEL_ABORTED_RUN);
                    WriteError(internal::ResourceGetWString(IDS_ERROR_ABORTING_RUN));
                    WriteSectionHeader(MIDIDIAG_SECTION_LABEL_END_OF_FILE);
                }
            }
            catch (...)
            {
            }

            fflush(stdout);

            // a normal exit runs DLL detach, which can wait on the lock the stuck thread holds
            ::TerminateProcess(::GetCurrentProcess(),
                servicePhase ? RETURN_SERVICE_NOT_RESPONDING : RETURN_REPORT_SECTION_TIMED_OUT);
        }

        void WatchdogThread(_In_ std::stop_token const stop) noexcept
        {
            try
            {
                std::unique_lock lock{ g_watchdogLock };

                while (!stop.stop_requested())
                {
                    auto const generation = g_watchdogGeneration;
                    auto const deadline = g_watchdogDeadline;
                    auto const rearmed = [generation]() { return g_watchdogGeneration != generation; };

                    bool const changed = deadline == NoDeadline ?
                        g_watchdogWake.wait(lock, stop, rearmed) :
                        g_watchdogWake.wait_until(lock, stop, deadline, rearmed);

                    if (changed || stop.stop_requested())
                    {
                        continue;
                    }

                    auto const section = g_watchdogSection;
                    auto const timeout = g_watchdogTimeout;
                    auto const servicePhase = g_watchdogServicePhase;

                    lock.unlock();

                    ReportTimeoutAndExit(section, timeout, servicePhase);

                    return;
                }
            }
            catch (...)
            {
            }
        }
    }

    ReportContext& Context() noexcept
    {
        static ReportContext context{};

        return context;
    }

    KeyValueText& KeyValueText::Add(std::wstring_view const key, std::wstring_view const value)
    {
        if (!m_text.empty())
        {
            m_text += L' ';
        }

        m_text += key;
        m_text += L'=';
        m_text += QuoteIfNeeded(RemoveUserNames(CleanText(value)));

        return *this;
    }

    KeyValueText& KeyValueText::AddIfNotEmpty(std::wstring_view const key, std::wstring_view const value)
    {
        if (!value.empty())
        {
            Add(key, value);
        }

        return *this;
    }

    KeyValueText& KeyValueText::AddBool(std::wstring_view const key, bool const value)
    {
        return Add(key, value ? L"true" : L"false");
    }

    KeyValueText& KeyValueText::AddNumber(std::wstring_view const key, uint64_t const value)
    {
        return Add(key, std::to_wstring(value));
    }

    KeyValueText& KeyValueText::AddSignedNumber(std::wstring_view const key, int64_t const value)
    {
        return Add(key, std::to_wstring(value));
    }

    KeyValueText& KeyValueText::AddHex(std::wstring_view const key, uint32_t const value, uint32_t const digits)
    {
        return Add(key, std::format(L"0x{:0{}X}", value, digits));
    }

    void WriteSection(std::wstring_view const sectionName)
    {
        std::scoped_lock lock{ g_outputLock };

        EndCurrentSectionTiming();
        WriteSectionHeader(sectionName);

        g_currentSection = sectionName;
        g_currentSectionStart = std::chrono::steady_clock::now();

        std::chrono::milliseconds timeout{ 0 };

        {
            std::scoped_lock watchdogLock{ g_watchdogLock };
            timeout = g_phaseTimeout;
        }

        ArmWatchdog(sectionName, timeout);
    }

    void WriteClosingSection(std::wstring_view const sectionName)
    {
        std::scoped_lock lock{ g_outputLock };

        EndCurrentSectionTiming();
        WriteSectionHeader(sectionName);
    }

    void WriteRecordBreak()
    {
        std::scoped_lock lock{ g_outputLock };

        if (g_anyOutput && !g_lastLineBlank)
        {
            fmt::println(L"");
            g_lastLineBlank = true;
        }
    }

    void WriteLine(std::wstring_view const text)
    {
        std::scoped_lock lock{ g_outputLock };

        fmt::println(L"{}", Styled(RemoveUserNames(CleanText(text)), normalTextStyle));

        g_anyOutput = true;
        g_lastLineBlank = false;
    }

    void WriteField(std::wstring_view const label, std::wstring_view const value)
    {
        WriteFieldText(label, RemoveUserNames(CleanText(value)), fieldValueTextStyle);
    }

    void WriteStyledField(std::wstring_view const label, std::wstring_view const value, fmt::text_style const& valueStyle)
    {
        WriteFieldText(label, RemoveUserNames(CleanText(value)), valueStyle);
    }

    void WriteField(std::wstring_view const label, KeyValueText const& values)
    {
        // each value was cleaned as it was added
        WriteFieldText(label, values.Text(), fieldValueTextStyle);
    }

    void WriteBoolField(std::wstring_view const label, bool const value)
    {
        WriteFieldText(label, value ? L"true" : L"false", fieldValueTextStyle);
    }

    void WriteNumberField(std::wstring_view const label, uint64_t const value)
    {
        WriteFieldText(label, std::to_wstring(value), fieldValueTextStyle);
    }

    void WriteError(std::wstring_view const message)
    {
        WriteFieldText(MIDIDIAG_FIELD_LABEL_ERROR, RemoveUserNames(CleanText(message)), errorTextStyle);
    }

    std::wstring RemoveUserNames(std::wstring_view const text)
    {
        std::wstring result{ text };

        if (result.empty())
        {
            return result;
        }

        auto const& folders = GetProfileFolders();

        ReplaceCurrentUserProfile(result, folders.UserProfileLower);
        ReplaceOtherProfiles(result, folders.ProfilesRootLower);

        return result;
    }

    std::wstring MaskIpAddress(std::wstring_view const address)
    {
        if (address.empty())
        {
            return {};
        }

        // IPv4: four parts of up to three digits, each no more than 255
        {
            std::vector<std::wstring_view> parts{};
            size_t start{ 0 };

            for (;;)
            {
                auto const dot = address.find(L'.', start);
                parts.push_back(address.substr(start, dot == std::wstring_view::npos ? std::wstring_view::npos : dot - start));

                if (dot == std::wstring_view::npos)
                {
                    break;
                }

                start = dot + 1;
            }

            auto const isOctet = [](std::wstring_view const part)
                {
                    if (part.empty() || part.size() > 3)
                    {
                        return false;
                    }

                    uint32_t value{ 0 };

                    for (auto const ch : part)
                    {
                        if (ch < L'0' || ch > L'9')
                        {
                            return false;
                        }

                        value = value * 10 + static_cast<uint32_t>(ch - L'0');
                    }

                    return value <= 255;
                };

            if (parts.size() == 4 && std::all_of(parts.begin(), parts.end(), isOctet))
            {
                return std::wstring{ L"x.x.x." } + std::wstring{ parts[3] };
            }
        }

        // IPv6, possibly with a zone and an IPv4 tail
        if (address.find(L':') != std::wstring_view::npos)
        {
            auto const withoutZone = address.substr(0, address.find(L'%'));

            bool const looksLikeAddress = std::all_of(withoutZone.begin(), withoutZone.end(), [](wchar_t const ch)
                {
                    return ::iswxdigit(ch) || ch == L':' || ch == L'.';
                });

            if (looksLikeAddress)
            {
                auto tail = withoutZone.substr(withoutZone.rfind(L':') + 1);
                auto const lastDot = tail.rfind(L'.');

                if (lastDot != std::wstring_view::npos)
                {
                    tail = tail.substr(lastDot + 1);
                }

                return std::wstring{ L"x::" } + std::wstring{ tail };
            }
        }

        return std::wstring{ address };
    }

    std::wstring FormatLocalTime(FILETIME const& utcTime)
    {
        if (utcTime.dwHighDateTime == 0 && utcTime.dwLowDateTime == 0)
        {
            return {};
        }

        SYSTEMTIME utcSystemTime{};
        SYSTEMTIME localSystemTime{};

        if (!::FileTimeToSystemTime(&utcTime, &utcSystemTime) ||
            !::SystemTimeToTzSpecificLocalTime(nullptr, &utcSystemTime, &localSystemTime))
        {
            return {};
        }

        return std::format(L"{:04}-{:02}-{:02} {:02}:{:02}:{:02}",
            localSystemTime.wYear, localSystemTime.wMonth, localSystemTime.wDay,
            localSystemTime.wHour, localSystemTime.wMinute, localSystemTime.wSecond);
    }

    std::wstring FormatLocalTime(foundation::DateTime const& time)
    {
        // The SDK hands back the Unix epoch for a time the service did not send, and 1601 for an unset one
        if (time.time_since_epoch().count() == 0 ||
            time == winrt::clock::from_sys(std::chrono::system_clock::time_point{}))
        {
            return {};
        }

        auto const fileTimeValue = winrt::clock::to_file_time(time).value;

        FILETIME utcFileTime{};
        utcFileTime.dwLowDateTime = static_cast<DWORD>(fileTimeValue & 0xFFFFFFFF);
        utcFileTime.dwHighDateTime = static_cast<DWORD>(fileTimeValue >> 32);

        return FormatLocalTime(utcFileTime);
    }

    std::wstring FormatDuration(std::chrono::seconds const duration)
    {
        auto total = duration.count() < 0 ? 0 : duration.count();

        auto const days = total / 86400;
        total %= 86400;

        return std::format(L"{}.{:02}:{:02}:{:02}", days, total / 3600, (total % 3600) / 60, total % 60);
    }

    std::wstring FormatHResult(HRESULT const hr)
    {
        return std::format(L"0x{:08X}", static_cast<uint32_t>(hr));
    }

    void AddFinding(std::wstring_view const id, std::wstring const& text)
    {
        std::scoped_lock lock{ g_findingsLock };

        g_findings.emplace_back(std::wstring{ id }, text);
    }

    void WriteFindingsSection()
    {
        std::scoped_lock lock{ g_outputLock };

        EndCurrentSectionTiming();
        WriteSectionHeader(MIDIDIAG_SECTION_LABEL_FINDINGS);

        std::vector<std::pair<std::wstring, std::wstring>> findings{};

        {
            std::scoped_lock findingsLock{ g_findingsLock };
            findings = g_findings;
        }

        WriteNumberField(MIDIDIAG_FIELD_LABEL_FINDING_COUNT, findings.size());

        for (auto const& finding : findings)
        {
            WriteField(MIDIDIAG_FIELD_LABEL_FINDING, KeyValueText{}.Add(L"id", finding.first).Add(L"text", finding.second));
        }
    }

    void WriteSectionTimingSection()
    {
        std::scoped_lock lock{ g_outputLock };

        EndCurrentSectionTiming();
        WriteSectionHeader(MIDIDIAG_SECTION_LABEL_SECTION_TIMING);

        for (auto const& timing : g_sectionTimings)
        {
            WriteField(MIDIDIAG_FIELD_LABEL_SECTION_TIMING,
                KeyValueText{}.Add(L"name", timing.Name).AddNumber(L"elapsed_ms", timing.ElapsedMilliseconds));
        }

        WriteNumberField(MIDIDIAG_FIELD_LABEL_TOTAL_ELAPSED_MS, MillisecondsSince(g_reportStart));
    }

    void StartWatchdog()
    {
        g_watchdogThread = std::jthread{ [](std::stop_token stop) { WatchdogThread(stop); } };
    }

    void StopWatchdog()
    {
        if (g_watchdogThread.joinable())
        {
            g_watchdogThread.request_stop();
            g_watchdogThread.join();
        }
    }

    void SetReportPhase(ReportPhase const phase, std::chrono::milliseconds const sectionTimeout)
    {
        std::scoped_lock lock{ g_watchdogLock };

        g_phase = phase;
        g_phaseTimeout = sectionTimeout;
    }

    void SetCurrentSectionTimeout(std::chrono::milliseconds const timeout)
    {
        std::wstring section{};

        {
            std::scoped_lock lock{ g_outputLock };
            section = g_currentSection;
        }

        ArmWatchdog(section, timeout);
    }
}
