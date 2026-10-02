// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#include "pch.h"
#include "MainWindow.xaml.h"
#include "ReportViewerWindow.xaml.h"

#include "BackgroundWork.h"
#include "ProcessRunner.h"
#include "ReportFile.h"
#include "StringResources.h"
#include "ToolPaths.h"

namespace native = ::miditroubleshooter;
namespace res = ::miditroubleshooter::resources;

namespace winrt::miditroubleshooter::implementation
{
    namespace
    {
        // These reports walk every endpoint and every kernel streaming filter on the PC, so a
        // couple of minutes is normal on a machine with a lot of hardware.
        constexpr std::chrono::seconds ReportTimeout{ 300 };

        // often enough to look live, seldom enough that replacing a long report's text stays cheap
        constexpr std::chrono::milliseconds LiveOutputInterval{ 250 };

        // The reading thread adds to Arrived; the UI thread moves it into Shown and the box.
        struct LiveReportOutput
        {
            std::mutex Lock{};
            std::wstring Arrived{};
            bool Finished{ false };

            std::wstring Shown{};
        };

        // New text puts the caret at the start and the view follows the caret, so it goes at the end.
        void ScrollToEnd(_In_ controls::TextBox const& box) noexcept
        {
            try
            {
                box.Select(static_cast<int32_t>(box.Text().size()), 0);
            }
            MIDI_TSHOOT_CATCH_AND_LOG(L"Unable to scroll a report to the end.")
        }

        void ShowArrivedOutput(_Inout_ LiveReportOutput& live, _In_ controls::TextBox const& box) noexcept
        {
            try
            {
                std::wstring arrived{};

                {
                    std::scoped_lock lock{ live.Lock };

                    // the complete report has already replaced the live view
                    if (live.Finished)
                    {
                        return;
                    }

                    arrived.swap(live.Arrived);
                }

                if (arrived.empty())
                {
                    return;
                }

                live.Shown += arrived;

                box.Text(winrt::hstring{ live.Shown });

                ScrollToEnd(box);
            }
            MIDI_TSHOOT_CATCH_AND_LOG(L"Unable to show a report as it arrives.")
        }

        // Windows ends a crashed program with its exception status, and those all have the top bit set.
        bool EndedByException(_In_ DWORD const exitCode) noexcept
        {
            return (exitCode & 0x80000000) != 0;
        }

        bool WriteUtf8TextFile(_In_ std::wstring const& path, _In_ std::wstring const& contents) noexcept
        {
            try
            {
                // UTF-8 with a byte order mark, which is what the WinRT file writer produced
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
            catch (...)
            {
                return false;
            }
        }

        // what mididiag returns when it had to stop a report early
        constexpr DWORD MidiDiagServiceNotResponding{ 5 };
        constexpr DWORD MidiDiagSectionTimedOut{ 6 };

        // a memory dump of a busy service can be several hundred megabytes before compression
        constexpr std::chrono::seconds ZipTimeout{ 900 };

        // a folder of its own under the temp folder, so the zip holds only the one file
        std::wstring CreateWorkFolder(_In_ std::wstring_view const prefix) noexcept
        {
            try
            {
                wchar_t tempFolder[MAX_PATH + 1]{};

                if (::GetTempPathW(ARRAYSIZE(tempFolder), tempFolder) == 0)
                {
                    return {};
                }

                // the temp path already ends with a backslash
                auto const folder = std::format(L"{}{}-{}-{}", tempFolder, prefix, ::GetCurrentProcessId(), ::GetTickCount64());

                return ::CreateDirectoryW(folder.c_str(), nullptr) ? folder : std::wstring{};
            }
            catch (...)
            {
                return {};
            }
        }

        void DeleteWorkFolder(_In_ std::wstring const& folder, _In_ std::wstring_view const fileName) noexcept
        {
            try
            {
                if (!folder.empty())
                {
                    ::DeleteFileW(std::format(L"{}\\{}", folder, fileName).c_str());
                    ::RemoveDirectoryW(folder.c_str());
                }
            }
            catch (...)
            {
            }
        }

        // bsdtar has been in Windows since 1803 and is the only in-box way to write a zip. It
        // can't open a path with a character outside the system's ANSI code page, so it runs in
        // the work folder and is given plain names, and the zip is moved into place afterward.
        bool ZipOneFile(_In_ std::wstring const& folder, _In_ std::wstring_view const fileName, _In_ std::wstring const& zipPath) noexcept
        {
            try
            {
                constexpr std::wstring_view workZipName{ L"archive.zip" };

                auto const tarPath = native::GetNativeSystem32Folder() + L"\\tar.exe";

                if (!native::FileExists(tarPath))
                {
                    return false;
                }

                auto const workZipPath = std::format(L"{}\\{}", folder, workZipName);

                auto const removeWorkZip = wil::scope_exit([&workZipPath]() noexcept
                    {
                        ::DeleteFileW(workZipPath.c_str());
                    });

                auto const run = native::RunCaptureIn(tarPath,
                    std::format(L"-a -c -f \"{}\" \"{}\"", workZipName, fileName), folder, ZipTimeout);

                if (!run.Started || run.TimedOut || run.ExitCode != 0 || !native::FileExists(workZipPath))
                {
                    return false;
                }

                // the folder the customer picked can be on another drive
                return ::MoveFileExW(workZipPath.c_str(), zipPath.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_COPY_ALLOWED) != FALSE;
            }
            catch (...)
            {
                return false;
            }
        }

        // The dump can hold private information, so it never stays behind in the temp folder.
        native::ServiceOperationResult SaveServiceDumpAsZip(_In_ std::wstring const& zipPath) noexcept
        {
            native::ServiceOperationResult result{};

            try
            {
                constexpr std::wstring_view dumpName{ L"midisrv.dmp" };

                auto const folder = CreateWorkFolder(L"midisrv-dump");

                if (folder.empty())
                {
                    result.ErrorMessage = std::wstring{ res::GetString(L"ServiceDumpNoWorkFolder") };
                    return result;
                }

                auto const cleanup = wil::scope_exit([&folder, dumpName]() noexcept
                    {
                        DeleteWorkFolder(folder, dumpName);
                    });

                result = native::WriteMidiServiceDump(std::format(L"{}\\{}", folder, dumpName));

                if (result.Succeeded && !ZipOneFile(folder, dumpName, zipPath))
                {
                    result.Succeeded = false;
                    result.ErrorMessage = std::wstring{ res::GetString(L"ServiceDumpZipFailed") };
                }
            }
            catch (...)
            {
                result.Succeeded = false;
                result.ErrorMessage = std::wstring{ res::GetString(L"ServiceDumpZipFailed") };
            }

            return result;
        }
    }

    _Use_decl_annotations_
    winrt::fire_and_forget MainWindow::OnRunMidiDiagClick(foundation::IInspectable const&, xaml::RoutedEventArgs const&)
    {
        auto lifetime = get_strong();

        RunDiagnosticReportAsync(DiagnosticReport::MidiDiag);

        co_return;
    }

    _Use_decl_annotations_
    winrt::fire_and_forget MainWindow::OnRunMidiKsInfoClick(foundation::IInspectable const&, xaml::RoutedEventArgs const&)
    {
        auto lifetime = get_strong();

        RunDiagnosticReportAsync(DiagnosticReport::MidiKsInfo);

        co_return;
    }

    _Use_decl_annotations_
    winrt::fire_and_forget MainWindow::RunDiagnosticReportAsync(DiagnosticReport const report) noexcept
    {
        auto lifetime = get_strong();

        try
        {
            auto const isMidiDiag = report == DiagnosticReport::MidiDiag;

            auto const tools = native::GetToolLocations();
            auto const toolPath = isMidiDiag ? tools.MidiDiag : tools.MidiKsInfo;
            auto const toolName = winrt::hstring{ isMidiDiag ? L"mididiag.exe" : L"midiksinfo.exe" };

            auto const runButton = isMidiDiag ? RunMidiDiagButton() : RunMidiKsInfoButton();
            auto const copyButton = isMidiDiag ? CopyMidiDiagButton() : CopyMidiKsInfoButton();
            auto const saveButton = isMidiDiag ? SaveMidiDiagButton() : SaveMidiKsInfoButton();
            auto const zipButton = isMidiDiag ? SaveMidiDiagZipButton() : controls::Button{ nullptr };
            auto const statusText = isMidiDiag ? MidiDiagStatusText() : MidiKsInfoStatusText();
            auto const outputBox = isMidiDiag ? MidiDiagOutputBox() : MidiKsInfoOutputBox();

            auto& output = isMidiDiag ? m_midiDiagOutput : m_midiKsInfoOutput;
            auto& running = isMidiDiag ? m_midiDiagRunning : m_midiKsInfoRunning;

            if (toolPath.empty())
            {
                statusText.Text(res::FormatString(L"DiagnosticsToolMissingFormat", toolName));
                co_return;
            }

            // Runs on the closing and the exception paths too, so a report that goes wrong
            // cannot leave the buttons dead or the pointer busy for the rest of the session.
            auto const restoreUi = wil::scope_exit([this, &output, &running, runButton, copyButton, saveButton, zipButton]() noexcept
                {
                    try
                    {
                        running = false;

                        if (!m_closing)
                        {
                            runButton.IsEnabled(true);
                            copyButton.IsEnabled(!output.empty());
                            saveButton.IsEnabled(!output.empty());

                            if (zipButton)
                            {
                                zipButton.IsEnabled(!output.empty());
                            }

                            UpdateDiagnosticsCursor();
                        }
                    }
                    catch (...)
                    {
                    }
                });

            // nothing from an earlier run may be copied or saved as if it came from this one
            output = winrt::hstring{};
            outputBox.Text(winrt::hstring{});

            runButton.IsEnabled(false);
            copyButton.IsEnabled(false);
            saveButton.IsEnabled(false);

            if (zipButton)
            {
                zipButton.IsEnabled(false);
            }

            if (isMidiDiag)
            {
                MidiServiceStuckInfoBar().IsOpen(false);
            }

            statusText.Text(res::GetString(L"DiagnosticsRunning"));

            running = true;
            UpdateDiagnosticsCursor();

            auto const live = std::make_shared<LiveReportOutput>();
            auto const liveTimer = DispatcherQueue().CreateTimer();

            liveTimer.Interval(LiveOutputInterval);
            liveTimer.Tick([weak = get_weak(), live, outputBox](auto&&, auto&&)
                {
                    if (auto const strong = weak.get())
                    {
                        if (!strong->m_closing)
                        {
                            ShowArrivedOutput(*live, outputBox);
                        }
                    }
                });

            // reset below, before the complete report replaces the live view
            auto stopLiveOutput = wil::scope_exit([live, liveTimer]() noexcept
                {
                    try
                    {
                        liveTimer.Stop();

                        std::scoped_lock lock{ live->Lock };
                        live->Finished = true;
                    }
                    catch (...)
                    {
                    }
                });

            liveTimer.Start();

            native::ProcessResult result{};

            co_await native::RunOnBackgroundAsync([&result, &toolPath, live]()
                {
                    result = native::RunCapture(toolPath, L"", ReportTimeout, [live](std::wstring_view const text)
                        {
                            std::scoped_lock lock{ live->Lock };
                            live->Arrived.append(text);
                        });
                });

            stopLiveOutput.reset();

            if (m_closing)
            {
                co_return;
            }

            if (!result.Started)
            {
                statusText.Text(result.ErrorMessage.empty() ?
                    res::GetString(L"DiagnosticsFailed") : winrt::hstring{ result.ErrorMessage });

                co_return;
            }

            auto const stoppedUnexpectedly = !result.TimedOut && EndedByException(result.ExitCode);
            auto const exitCode = std::format(L"0x{:08X}", result.ExitCode);

            auto text = result.Output;

            // in the text itself, so a copied or saved report still says it is incomplete
            if (stoppedUnexpectedly)
            {
                if (!text.empty())
                {
                    text += text.back() == L'\n' ? L"\r\n" : L"\r\n\r\n";
                }

                text += res::FormatString(L"DiagnosticsStoppedUnexpectedlyNoteFormat", toolName, exitCode);
                text += L"\r\n";
            }

            output = winrt::hstring{ text };

            outputBox.Text(output);
            ScrollToEnd(outputBox);

            if (result.TimedOut)
            {
                statusText.Text(res::GetString(L"DiagnosticsTimedOut"));
            }
            else if (stoppedUnexpectedly)
            {
                statusText.Text(res::FormatString(L"DiagnosticsStoppedUnexpectedlyFormat", exitCode));
            }
            else if (isMidiDiag && result.ExitCode == MidiDiagServiceNotResponding)
            {
                statusText.Text(res::GetString(L"DiagnosticsServiceNotResponding"));

                auto const banner = MidiServiceStuckInfoBar();

                banner.Severity(controls::InfoBarSeverity::Warning);
                banner.Title(res::GetString(L"ServiceStuckTitle"));
                banner.Message(res::GetString(L"ServiceStuckMessage"));
                SaveServiceDumpButton().IsEnabled(true);
                banner.IsOpen(true);
            }
            else if (isMidiDiag && result.ExitCode == MidiDiagSectionTimedOut)
            {
                statusText.Text(res::GetString(L"DiagnosticsSectionTimedOut"));
            }
            else
            {
                statusText.Text(res::GetString(L"DiagnosticsComplete"));
            }
        }
        MIDI_TSHOOT_CATCH_AND_LOG(L"Unable to run a diagnostics report.")
    }

    void MainWindow::UpdateDiagnosticsCursor() noexcept
    {
        try
        {
            // ProtectedCursor is only projected for derived classes, but every element implements its interface.
            auto const panel = DiagnosticsPanel().as<xaml::IUIElementProtected>();

            if (m_midiDiagRunning || m_midiKsInfoRunning)
            {
                panel.ProtectedCursor(winrt::Microsoft::UI::Input::InputSystemCursor::Create(
                    winrt::Microsoft::UI::Input::InputSystemCursorShape::Wait));
            }
            else
            {
                panel.ProtectedCursor(nullptr);
            }

            // A text box shows its own I-beam, so a box that is still filling ignores the mouse and
            // the busy pointer shows over it too. It jumps to each new line anyway, so scrolling or
            // selecting in it would not stay put.
            MidiDiagOutputBox().IsHitTestVisible(!m_midiDiagRunning);
            MidiKsInfoOutputBox().IsHitTestVisible(!m_midiKsInfoRunning);
        }
        MIDI_TSHOOT_CATCH_AND_LOG(L"Unable to update the pointer on the diagnostics page.")
    }

    _Use_decl_annotations_
    void MainWindow::OnCopyMidiDiagClick(foundation::IInspectable const&, xaml::RoutedEventArgs const&)
    {
        CopyToClipboard(m_midiDiagOutput);

        try
        {
            MidiDiagStatusText().Text(res::GetString(L"DiagnosticsCopied"));
        }
        MIDI_TSHOOT_CATCH_AND_LOG(L"Unable to report the copy.")
    }

    _Use_decl_annotations_
    void MainWindow::OnCopyMidiKsInfoClick(foundation::IInspectable const&, xaml::RoutedEventArgs const&)
    {
        CopyToClipboard(m_midiKsInfoOutput);

        try
        {
            MidiKsInfoStatusText().Text(res::GetString(L"DiagnosticsCopied"));
        }
        MIDI_TSHOOT_CATCH_AND_LOG(L"Unable to report the copy.")
    }

    _Use_decl_annotations_
    winrt::fire_and_forget MainWindow::OnSaveMidiDiagClick(foundation::IInspectable const&, xaml::RoutedEventArgs const&)
    {
        auto lifetime = get_strong();

        SaveTextAsync(L"mididiag.txt", m_midiDiagOutput);

        co_return;
    }

    _Use_decl_annotations_
    winrt::fire_and_forget MainWindow::OnSaveMidiKsInfoClick(foundation::IInspectable const&, xaml::RoutedEventArgs const&)
    {
        auto lifetime = get_strong();

        SaveTextAsync(L"midiksinfo.txt", m_midiKsInfoOutput);

        co_return;
    }

    // The report is often larger than a GitHub issue can hold, and a zip can be attached instead.
    _Use_decl_annotations_
    winrt::fire_and_forget MainWindow::OnSaveMidiDiagZipClick(foundation::IInspectable const&, xaml::RoutedEventArgs const&)
    {
        auto lifetime = get_strong();

        auto const contents = std::wstring{ m_midiDiagOutput };

        try
        {
            auto const path = ShowSaveFileDialog(
                std::wstring{ res::GetString(L"SaveZipFileType") }, L"zip", L"mididiag.zip");

            if (path.empty())
            {
                co_return;
            }

            bool written{ false };

            co_await native::RunOnBackgroundAsync([&written, &path, &contents]()
                {
                    constexpr std::wstring_view reportName{ L"mididiag.txt" };

                    auto const folder = CreateWorkFolder(L"mididiag");

                    if (folder.empty())
                    {
                        return;
                    }

                    auto const cleanup = wil::scope_exit([&folder, reportName]() noexcept
                        {
                            DeleteWorkFolder(folder, reportName);
                        });

                    written = WriteUtf8TextFile(std::format(L"{}\\{}", folder, reportName), contents) &&
                        ZipOneFile(folder, reportName, path);
                });

            if (m_closing || written)
            {
                co_return;
            }

            MidiDiagStatusText().Text(res::GetString(L"DiagnosticsZipFailed"));
        }
        MIDI_TSHOOT_CATCH_AND_LOG(L"Unable to save the report as a zip file.")
    }

    // The report this page just ran, when there is one. Otherwise a report file the customer
    // picks, such as the zip somebody attached to an issue.
    _Use_decl_annotations_
    winrt::fire_and_forget MainWindow::OnViewMidiDiagReportClick(foundation::IInspectable const&, xaml::RoutedEventArgs const&)
    {
        auto lifetime = get_strong();

        try
        {
            if (!m_midiDiagOutput.empty())
            {
                auto loaded = native::LoadReportText(m_midiDiagOutput);

                // mididiag can fail before it writes a report, and then a file is the only choice
                if (loaded.Error == native::ReportLoadError::None)
                {
                    ShowReportViewer(std::move(loaded));
                    co_return;
                }
            }

            auto const path = native::ShowOpenReportDialog(WindowHandle());

            if (path.empty() || m_closing)
            {
                co_return;
            }

            native::LoadedReport loaded{};

            co_await native::RunOnBackgroundAsync([&loaded, &path]()
                {
                    loaded = native::LoadReportFile(path);
                });

            if (m_closing)
            {
                co_return;
            }

            if (loaded.Error != native::ReportLoadError::None)
            {
                MidiDiagStatusText().Text(native::ReportLoadErrorMessage(loaded.Error));
                co_return;
            }

            ShowReportViewer(std::move(loaded));
        }
        MIDI_TSHOOT_CATCH_AND_LOG(L"Unable to show a report in the viewer.")
    }

    _Use_decl_annotations_
    void MainWindow::ShowReportViewer(native::LoadedReport report) noexcept
    {
        try
        {
            if (m_reportViewer == nullptr)
            {
                auto viewer = winrt::make_self<ReportViewerWindow>();

                // sized and placed before the first paint, so it doesn't visibly jump
                viewer->RestoreWindowPlacement();
                viewer->ShowReport(std::move(report));

                m_reportViewer = viewer.as<miditroubleshooter::ReportViewerWindow>();

                m_reportViewer.Closed([weak = get_weak()](auto&&, auto&&)
                    {
                        if (auto strong = weak.get())
                        {
                            strong->m_reportViewer = nullptr;
                        }
                    });

                m_reportViewer.Activate();

                return;
            }

            auto* const viewer = winrt::get_self<ReportViewerWindow>(m_reportViewer);

            viewer->ShowReport(std::move(report));
            viewer->BringToFront();
        }
        MIDI_TSHOOT_CATCH_AND_LOG(L"Unable to open the report viewer.")
    }

    // Offered when mididiag found the service stuck. A dump taken now shows the developers where
    // it is stuck, and restarting the PC, which is what clears it, would lose that.
    _Use_decl_annotations_
    winrt::fire_and_forget MainWindow::OnSaveServiceDumpClick(foundation::IInspectable const&, xaml::RoutedEventArgs const&)
    {
        auto lifetime = get_strong();

        try
        {
            if (!RequireElevation())
            {
                co_return;
            }

            SYSTEMTIME now{};
            ::GetLocalTime(&now);

            auto const suggestedName = std::format(L"midisrv-dump-{:04}{:02}{:02}-{:02}{:02}{:02}.zip",
                now.wYear, now.wMonth, now.wDay, now.wHour, now.wMinute, now.wSecond);

            auto const path = ShowSaveFileDialog(
                std::wstring{ res::GetString(L"SaveZipFileType") }, L"zip", suggestedName);

            if (path.empty())
            {
                co_return;
            }

            auto const banner = MidiServiceStuckInfoBar();
            auto const button = SaveServiceDumpButton();

            button.IsEnabled(false);
            banner.Message(res::GetString(L"ServiceDumpSaving"));

            native::ServiceOperationResult result{};

            co_await native::RunOnBackgroundAsync([&result, &path]()
                {
                    result = SaveServiceDumpAsZip(path);
                });

            if (m_closing)
            {
                co_return;
            }

            if (result.Succeeded)
            {
                banner.Severity(controls::InfoBarSeverity::Success);
                banner.Title(res::GetString(L"ServiceDumpSavedTitle"));
                banner.Message(res::FormatString(L"ServiceDumpSavedFormat", winrt::hstring{ path }));
            }
            else
            {
                // the service may still be stuck, so another try is allowed
                banner.Severity(controls::InfoBarSeverity::Error);
                banner.Title(res::GetString(L"ServiceDumpFailedTitle"));
                banner.Message(winrt::hstring{ result.ErrorMessage });
                button.IsEnabled(true);
            }

            banner.IsOpen(true);
        }
        MIDI_TSHOOT_CATCH_AND_LOG(L"Unable to save a memory dump of the MIDI service.")
    }

    _Use_decl_annotations_
    winrt::fire_and_forget MainWindow::SaveTextAsync(winrt::hstring const& suggestedName, winrt::hstring const& text) noexcept
    {
        auto lifetime = get_strong();

        auto const name = std::wstring{ suggestedName };
        auto const contents = std::wstring{ text };

        try
        {
            auto const path = ShowSaveFileDialog(
                std::wstring{ res::GetString(L"SaveTextFileType") }, L"txt", name);

            if (path.empty())
            {
                co_return;
            }

            bool written{ false };

            co_await native::RunOnBackgroundAsync([&written, &path, &contents]()
                {
                    written = WriteUtf8TextFile(path, contents);
                });

            if (m_closing || written)
            {
                co_return;
            }

            MIDI_TSHOOT_LOG_WARNING(L"Unable to write the report file.");
        }
        MIDI_TSHOOT_CATCH_AND_LOG(L"Unable to save the report.")
    }
}
