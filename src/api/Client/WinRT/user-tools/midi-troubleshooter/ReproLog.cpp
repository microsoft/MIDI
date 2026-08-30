// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#include "pch.h"
#include "ReproLog.h"
#include "ProcessRunner.h"
#include "ServiceControl.h"
#include "StringResources.h"
#include "SystemInfo.h"
#include "ToolPaths.h"

namespace miditroubleshooter
{
    namespace
    {
        namespace res
        {
            inline std::wstring GetString(std::wstring_view resourceKey) noexcept
            {
                return std::wstring{ ::miditroubleshooter::resources::GetString(resourceKey) };
            }

            template <typename... TArgs>
            inline std::wstring FormatString(std::wstring_view resourceKey, TArgs&&... args) noexcept
            {
                return std::wstring{ ::miditroubleshooter::resources::FormatString(
                    resourceKey, std::forward<TArgs>(args)...) };
            }
        }

        // The profile name inside providers.wprp
        constexpr wchar_t ReproProfileName[] = L"midi";

        std::wstring Timestamp() noexcept
        {
            SYSTEMTIME now{};
            ::GetLocalTime(&now);

            return std::format(L"{:04}{:02}{:02}-{:02}{:02}{:02}",
                now.wYear, now.wMonth, now.wDay, now.wHour, now.wMinute, now.wSecond);
        }

        std::wstring TempFolder() noexcept
        {
            wchar_t buffer[MAX_PATH + 1]{};

            if (::GetTempPathW(ARRAYSIZE(buffer), buffer) > 0)
            {
                std::wstring path{ buffer };

                while (!path.empty() && path.back() == L'\\')
                {
                    path.pop_back();
                }

                return path;
            }

            return {};
        }

        std::wstring Combine(_In_ std::wstring const& folder, _In_ std::wstring const& leaf) noexcept
        {
            if (folder.empty())
            {
                return leaf;
            }

            auto combined = folder;

            if (combined.back() != L'\\')
            {
                combined += L'\\';
            }

            return combined + leaf;
        }

        bool WriteTextFile(_In_ std::wstring const& path, _In_ std::wstring const& contents) noexcept
        {
            try
            {
                // UTF-8 with a byte order mark, so Notepad and the web both read it correctly
                auto const required = ::WideCharToMultiByte(
                    CP_UTF8, 0, contents.c_str(), static_cast<int>(contents.size()), nullptr, 0, nullptr, nullptr);

                std::string utf8{};

                if (required > 0)
                {
                    utf8.resize(static_cast<size_t>(required));

                    ::WideCharToMultiByte(
                        CP_UTF8, 0, contents.c_str(), static_cast<int>(contents.size()),
                        utf8.data(), required, nullptr, nullptr);
                }

                std::ofstream stream{ path, std::ios::binary | std::ios::trunc };

                if (!stream.is_open())
                {
                    return false;
                }

                constexpr char byteOrderMark[]{ '\xEF', '\xBB', '\xBF' };

                stream.write(byteOrderMark, sizeof(byteOrderMark));
                stream.write(utf8.data(), static_cast<std::streamsize>(utf8.size()));

                return stream.good();
            }
            MIDI_TSHOOT_CATCH_AND_LOG(L"Unable to write a text file into the capture folder.")

            return false;
        }

        void DeleteFolderRecursive(_In_ std::wstring const& folder) noexcept
        {
            try
            {
                if (folder.empty())
                {
                    return;
                }

                auto const pattern = Combine(folder, L"*");

                WIN32_FIND_DATAW findData{};

                wil::unique_hfind findHandle{ ::FindFirstFileW(pattern.c_str(), &findData) };

                if (findHandle)
                {
                    do
                    {
                        std::wstring const name{ findData.cFileName };

                        if (name == L"." || name == L"..")
                        {
                            continue;
                        }

                        auto const child = Combine(folder, name);

                        if ((findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0)
                        {
                            DeleteFolderRecursive(child);
                        }
                        else
                        {
                            ::SetFileAttributesW(child.c_str(), FILE_ATTRIBUTE_NORMAL);
                            ::DeleteFileW(child.c_str());
                        }
                    }
                    while (::FindNextFileW(findHandle.get(), &findData));
                }

                findHandle.reset();

                ::RemoveDirectoryW(folder.c_str());
            }
            MIDI_TSHOOT_CATCH_AND_LOG(L"Unable to remove the capture working folder.")
        }

        std::wstring DescribeServiceState(_In_ ServiceState const state) noexcept
        {
            switch (state)
            {
            case ServiceState::Running:         return L"Running";
            case ServiceState::Stopped:         return L"Stopped";
            case ServiceState::Paused:          return L"Paused";
            case ServiceState::StartPending:    return L"Start pending";
            case ServiceState::StopPending:     return L"Stop pending";
            case ServiceState::PausePending:    return L"Pause pending";
            case ServiceState::ContinuePending: return L"Continue pending";
            case ServiceState::NotInstalled:    return L"Not installed";
            default:                            return L"Unknown";
            }
        }

        // A short header the support engineer sees before opening anything else in the package.
        std::wstring BuildSummaryReport() noexcept
        {
            std::wstring report{};

            try
            {
                auto const information = GatherSystemInformation();
                auto const service = QueryMidiServiceStatus();

                report += L"Windows MIDI Services capture summary\r\n";
                report += L"=====================================\r\n\r\n";
                report += L"captured                 : " + Timestamp() + L"\r\n";
                report += L"computer                 : " + information.ComputerName + L"\r\n";
                report += L"windows_version          : " + information.WindowsVersion + L"\r\n";
                report += L"windows_edition          : " + information.WindowsEdition + L"\r\n";
                report += L"windows_architecture     : " + information.WindowsArchitecture + L"\r\n";
                report += L"culture                  : " + information.CultureName + L"\r\n";
                report += L"windows_app_sdk_version  : " + information.WindowsAppSdkVersion + L"\r\n";
                report += L"midi_sdk_version         : " + information.MidiSdkVersion + L"\r\n";
                report += L"troubleshooter_version   : " + information.ToolVersion + L"\r\n";
                report += std::wstring{ L"developer_mode           : " } +
                    (information.DeveloperModeEnabled ? L"true" : L"false") + L"\r\n";
                report += L"\r\n";
                report += L"service_state            : " + DescribeServiceState(service.State) + L"\r\n";
                report += L"service_account          : " + service.AccountName + L"\r\n";
                report += L"service_image            : " + service.ImagePath + L"\r\n";
                report += L"service_image_version    : " + service.ImageVersion + L"\r\n";
            }
            MIDI_TSHOOT_CATCH_AND_LOG(L"Unable to build the capture summary.")

            return report;
        }
    }

    std::wstring ReproCapture::SuggestedFileName() noexcept
    {
        wchar_t computerName[MAX_COMPUTERNAME_LENGTH + 1]{};
        DWORD computerNameLength{ ARRAYSIZE(computerName) };

        if (!::GetComputerNameW(computerName, &computerNameLength))
        {
            wcscpy_s(computerName, L"PC");
        }

        return std::format(L"MIDI Capture {}_{}.zip", computerName, Timestamp());
    }

    _Use_decl_annotations_
    CaptureStepResult ReproCapture::Start(CaptureOptions const& options) noexcept
    {
        CaptureStepResult result{};

        try
        {
            if (m_running)
            {
                result.ErrorMessage = res::GetString(L"CaptureErrorAlreadyRunning");
                return result;
            }

            m_options = options;
            m_timeTravelTracingStarted = false;

            auto const tools = GetToolLocations();

            if (tools.WindowsPerformanceRecorder.empty())
            {
                result.ErrorMessage = res::GetString(L"CaptureErrorNoWpr");
                return result;
            }

            if (tools.ReproProfile.empty())
            {
                result.ErrorMessage = res::GetString(L"CaptureErrorNoProfile");
                return result;
            }

            wchar_t computerName[MAX_COMPUTERNAME_LENGTH + 1]{};
            DWORD computerNameLength{ ARRAYSIZE(computerName) };

            if (!::GetComputerNameW(computerName, &computerNameLength))
            {
                wcscpy_s(computerName, L"PC");
            }

            m_workingFolder = Combine(TempFolder(), std::format(L"{}_{}", computerName, Timestamp()));

            if (!::CreateDirectoryW(m_workingFolder.c_str(), nullptr))
            {
                result.ErrorMessage = res::FormatString(
                    L"CaptureErrorFolderFormat", winrt::hstring{ m_workingFolder });

                m_workingFolder.clear();
                return result;
            }

            result.Log.push_back(res::FormatString(L"CaptureLogWorkingFolderFormat", winrt::hstring{ m_workingFolder }));

            // Started before the trace, because attaching can restart the service and none of
            // that noise belongs in the ETL.
            if (options.IncludeTimeTravelTrace && !tools.TimeTravelTracer.empty())
            {
                auto const service = QueryMidiServiceStatus();

                if (service.ProcessId != 0)
                {
                    auto const arguments = std::format(
                        L"-ring -out \"{}\" -attach {}", m_workingFolder, service.ProcessId);

                    auto const run = RunCapture(tools.TimeTravelTracer, arguments, std::chrono::seconds{ 60 });

                    WriteTextFile(Combine(m_workingFolder, L"tttracer-start.log"), run.Output);

                    if (run.Started && run.ExitCode == 0)
                    {
                        m_timeTravelTracingStarted = true;
                        result.Log.push_back(res::GetString(L"CaptureLogTimeTravelStarted"));
                    }
                    else
                    {
                        // Usually the shadow stack mitigation refusing the attach. Worth saying
                        // so rather than silently producing a package with no trace in it.
                        result.Log.push_back(res::GetString(L"CaptureLogTimeTravelFailed"));
                    }
                }
                else
                {
                    result.Log.push_back(res::GetString(L"CaptureLogTimeTravelNoService"));
                }
            }

            auto const startArguments = std::format(
                L"-start \"{}!{}\"", tools.ReproProfile, ReproProfileName);

            auto const startRun = RunCapture(
                tools.WindowsPerformanceRecorder, startArguments, std::chrono::seconds{ 120 });

            if (!startRun.Started || startRun.ExitCode != 0)
            {
                result.ErrorMessage = startRun.Output.empty() ?
                    res::GetString(L"CaptureErrorWprStartFailed") : startRun.Output;

                StopTracing(result.Log);
                RemoveWorkingFolder();

                return result;
            }

            result.Log.push_back(res::GetString(L"CaptureLogTracingStarted"));

            m_running = true;
            result.Succeeded = true;
        }
        catch (winrt::hresult_error const& ex)
        {
            result.ErrorMessage = std::wstring{ ex.message() };
            MIDI_TSHOOT_LOG_HRESULT_EXCEPTION(ex, L"Unable to start the repro capture.");
        }
        catch (...)
        {
            result.ErrorMessage = res::GetString(L"CaptureErrorUnexpected");
            MIDI_TSHOOT_LOG_GENERAL_EXCEPTION(L"Unable to start the repro capture.");
        }

        return result;
    }

    _Use_decl_annotations_
    void ReproCapture::StopTracing(std::vector<std::wstring>& log) noexcept
    {
        try
        {
            auto const tools = GetToolLocations();

            if (m_timeTravelTracingStarted && !tools.TimeTravelTracer.empty())
            {
                auto const run = RunCapture(tools.TimeTravelTracer, L"-stop all", std::chrono::seconds{ 300 });

                WriteTextFile(Combine(m_workingFolder, L"tttracer-stop.log"), run.Output);

                log.push_back(res::GetString(L"CaptureLogTimeTravelStopped"));

                m_timeTravelTracingStarted = false;
            }
        }
        MIDI_TSHOOT_CATCH_AND_LOG(L"Unable to stop time travel tracing.")
    }

    void ReproCapture::RemoveWorkingFolder() noexcept
    {
        if (!m_workingFolder.empty())
        {
            DeleteFolderRecursive(m_workingFolder);
            m_workingFolder.clear();
        }
    }

    _Use_decl_annotations_
    CaptureStepResult ReproCapture::Finish(std::wstring const& outputZipPath) noexcept
    {
        CaptureStepResult result{};

        try
        {
            if (!m_running)
            {
                result.ErrorMessage = res::GetString(L"CaptureErrorNotRunning");
                return result;
            }

            auto const tools = GetToolLocations();

            auto const stopArguments = std::format(
                L"-stop \"{}\"", Combine(m_workingFolder, L"Repro.etl"));

            auto const stopRun = RunCapture(
                tools.WindowsPerformanceRecorder, stopArguments, std::chrono::seconds{ 600 });

            if (stopRun.Started && stopRun.ExitCode == 0)
            {
                result.Log.push_back(res::GetString(L"CaptureLogTraceWritten"));
            }
            else
            {
                result.Log.push_back(res::GetString(L"CaptureLogTraceStopFailed"));

                if (!stopRun.Output.empty())
                {
                    result.Log.push_back(stopRun.Output);
                }
            }

            // after the trace, so any service restart it causes is not in the ETL
            StopTracing(result.Log);

            m_running = false;

            WriteTextFile(Combine(m_workingFolder, L"capture-summary.txt"), BuildSummaryReport());
            result.Log.push_back(res::GetString(L"CaptureLogSummaryWritten"));

            // The script never captured these two, and they are the first thing support asks
            // for after the trace itself.
            if (m_options.IncludeMidiDiag && !tools.MidiDiag.empty())
            {
                auto const run = RunCapture(tools.MidiDiag, L"", std::chrono::seconds{ 300 });

                WriteTextFile(Combine(m_workingFolder, L"mididiag.txt"), run.Output);
                result.Log.push_back(res::GetString(L"CaptureLogMidiDiagCaptured"));
            }

            if (m_options.IncludeMidiKsInfo && !tools.MidiKsInfo.empty())
            {
                auto const run = RunCapture(tools.MidiKsInfo, L"", std::chrono::seconds{ 300 });

                WriteTextFile(Combine(m_workingFolder, L"midiksinfo.txt"), run.Output);
                result.Log.push_back(res::GetString(L"CaptureLogMidiKsInfoCaptured"));
            }

            if (m_options.IncludeSystemInformation)
            {
                struct ExternalReport
                {
                    std::wstring Tool;
                    std::wstring Arguments;
                    std::wstring LogKey;
                };

                std::vector<ExternalReport> reports{
                    { tools.DdoDiag, std::format(L"-o \"{}\"", Combine(m_workingFolder, L"ddodiag.xml")), L"CaptureLogDdoDiag" },
                    { tools.DxDiag, std::format(L"/t \"{}\"", Combine(m_workingFolder, L"dxdiag.txt")), L"CaptureLogDxDiag" },
                    { tools.PnpUtil, std::format(L"/export-pnpstate \"{}\" /force", Combine(m_workingFolder, L"pnpstate.pnp")), L"CaptureLogPnpState" }
                };

                for (auto const& report : reports)
                {
                    if (report.Tool.empty())
                    {
                        continue;
                    }

                    auto const run = RunToCompletion(report.Tool, report.Arguments, std::chrono::seconds{ 120 });

                    result.Log.push_back(run.TimedOut ?
                        res::FormatString(L"CaptureLogToolTimedOutFormat", res::GetString(report.LogKey)) :
                        res::GetString(report.LogKey));
                }
            }

            WriteTextFile(Combine(m_workingFolder, L"capture-log.txt"),
                [&result]()
                {
                    std::wstring text{};

                    for (auto const& line : result.Log)
                    {
                        text += line + L"\r\n";
                    }

                    return text;
                }());

            // bsdtar has been in Windows since 1803 and is the only in-box way to write a zip
            auto const tarPath = Combine(GetNativeSystem32Folder(), L"tar.exe");

            if (!FileExists(tarPath))
            {
                result.ErrorMessage = res::FormatString(
                    L"CaptureErrorNoZipFormat", winrt::hstring{ m_workingFolder });

                return result;
            }

            ::DeleteFileW(outputZipPath.c_str());

            auto const zipArguments = std::format(
                L"-a -c -f \"{}\" -C \"{}\" .", outputZipPath, m_workingFolder);

            auto const zipRun = RunCapture(tarPath, zipArguments, std::chrono::seconds{ 900 });

            if (!zipRun.Started || zipRun.ExitCode != 0 || !FileExists(outputZipPath))
            {
                result.ErrorMessage = zipRun.Output.empty() ?
                    res::FormatString(L"CaptureErrorNoZipFormat", winrt::hstring{ m_workingFolder }) :
                    zipRun.Output;

                return result;
            }

            result.Log.push_back(res::FormatString(L"CaptureLogPackageWrittenFormat", winrt::hstring{ outputZipPath }));

            RemoveWorkingFolder();

            result.Succeeded = true;
        }
        catch (winrt::hresult_error const& ex)
        {
            result.ErrorMessage = std::wstring{ ex.message() };
            MIDI_TSHOOT_LOG_HRESULT_EXCEPTION(ex, L"Unable to finish the repro capture.");
        }
        catch (...)
        {
            result.ErrorMessage = res::GetString(L"CaptureErrorUnexpected");
            MIDI_TSHOOT_LOG_GENERAL_EXCEPTION(L"Unable to finish the repro capture.");
        }

        return result;
    }

    CaptureStepResult ReproCapture::Cancel() noexcept
    {
        CaptureStepResult result{};

        try
        {
            auto const tools = GetToolLocations();

            if (m_running && !tools.WindowsPerformanceRecorder.empty())
            {
                RunCapture(tools.WindowsPerformanceRecorder, L"-cancel", std::chrono::seconds{ 120 });

                result.Log.push_back(res::GetString(L"CaptureLogTracingCanceled"));
            }

            StopTracing(result.Log);

            m_running = false;

            RemoveWorkingFolder();

            result.Succeeded = true;
        }
        catch (winrt::hresult_error const& ex)
        {
            result.ErrorMessage = std::wstring{ ex.message() };
            MIDI_TSHOOT_LOG_HRESULT_EXCEPTION(ex, L"Unable to cancel the repro capture.");
        }
        catch (...)
        {
            result.ErrorMessage = res::GetString(L"CaptureErrorUnexpected");
            MIDI_TSHOOT_LOG_GENERAL_EXCEPTION(L"Unable to cancel the repro capture.");
        }

        return result;
    }
}
