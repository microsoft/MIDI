// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#include "pch.h"
#include "LatencyCheckPanel.xaml.h"
#if __has_include("LatencyCheckPanel.g.cpp")
#include "LatencyCheckPanel.g.cpp"
#endif

#include "BackgroundWork.h"
#include "LatencyCheckText.h"
#include "StringResources.h"

namespace native = ::miditroubleshooter;
namespace latency = ::miditroubleshooter::latency;
namespace res = ::miditroubleshooter::resources;

namespace winrt::miditroubleshooter::implementation
{
    namespace
    {
        // how often the page shows how the check is going
        constexpr auto TimerInterval{ std::chrono::milliseconds(500) };

        // the choices in the duration box, in order; zero is "until I stop it"
        constexpr std::array<std::chrono::seconds, 5> Durations
        {
            std::chrono::seconds(60),
            std::chrono::seconds(120),
            std::chrono::seconds(300),
            std::chrono::seconds(600),
            std::chrono::seconds(0)
        };

        // How long the check runs before results can mean anything. Stopped sooner, it still
        // shows what it has.
        constexpr auto MinimumUsefulDuration{ std::chrono::seconds(10) };

        std::wstring ElapsedText(_In_ std::chrono::seconds const elapsed)
        {
            auto const seconds = elapsed.count();

            return std::format(L"{}:{:02}", seconds / 60, seconds % 60);
        }

        bool IsChecked(_In_ controls::CheckBox const& box)
        {
            auto const value = box.IsChecked();

            return value != nullptr && value.Value();
        }

        winrt::fire_and_forget StopLeftoverSessionAsync() noexcept
        {
            try
            {
                co_await native::RunOnBackgroundAsync([]()
                    {
                        latency::KernelSession::StopLeftoverSession();
                    });
            }
            MIDI_TSHOOT_CATCH_AND_LOG(L"Unable to stop a DPC latency check session left running.")
        }

        std::wstring SuggestedFileName(_In_ std::wstring_view const extension)
        {
            SYSTEMTIME now{};
            ::GetLocalTime(&now);

            return std::format(L"dpc-latency-{:04}{:02}{:02}-{:02}{:02}{:02}.{}",
                now.wYear, now.wMonth, now.wDay, now.wHour, now.wMinute, now.wSecond, extension);
        }

        winrt::hstring ErrorText(_In_ uint32_t const error)
        {
            if (error == ERROR_ACCESS_DENIED)
            {
                return res::GetString(L"LatencyErrorAccessDenied");
            }

            if (error == ERROR_NO_SYSTEM_RESOURCES)
            {
                return res::GetString(L"LatencyErrorTooManySessions");
            }

            wchar_t* buffer{ nullptr };

            auto const length = ::FormatMessageW(
                FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
                nullptr, error, 0, reinterpret_cast<wchar_t*>(&buffer), 0, nullptr);

            std::wstring text{};

            if (length > 0 && buffer != nullptr)
            {
                text.assign(buffer, length);

                while (!text.empty() && (text.back() == L'\r' || text.back() == L'\n' || text.back() == L' '))
                {
                    text.pop_back();
                }
            }

            if (buffer != nullptr)
            {
                ::LocalFree(buffer);
            }

            return text.empty() ?
                res::FormatString(L"LatencyErrorCodeFormat", std::format(L"0x{:08X}", error)) :
                winrt::hstring{ text };
        }
    }

    LatencyCheckPanel::LatencyCheckPanel()
    {
        InitializeComponent();
    }

    _Use_decl_annotations_
    void LatencyCheckPanel::Initialize(Host host) noexcept
    {
        try
        {
            m_host = std::move(host);

            midiapp::MakeLiveStatusRegion(StatusText());

            m_initialized = true;

            SetState(State::Idle);
        }
        MIDI_TSHOOT_CATCH_AND_LOG(L"Unable to set up the DPC latency check page.")
    }

    void LatencyCheckPanel::OnShown() noexcept
    {
        try
        {
            // A session left behind by a copy of this app that didn't close cleanly is still
            // recording. Only an administrator can stop it, and nothing else uses its name.
            if (m_host.Elevated && m_state == State::Idle)
            {
                StopLeftoverSessionAsync();
            }
        }
        MIDI_TSHOOT_CATCH_AND_LOG(L"Unable to prepare the DPC latency check page.")
    }

    void LatencyCheckPanel::Shutdown() noexcept
    {
        m_closing = true;

        StopTimer();

        if (m_cancelRead != nullptr)
        {
            m_cancelRead->store(true);
        }

        // The app ends as soon as this window has closed, along with any background work still
        // running, so every trace session is stopped here rather than left to that work.
        try
        {
            // waits for a check that's still starting, then stops it
            if (auto const starting = std::exchange(m_startingCheck, nullptr))
            {
                starting->Abandon();
            }

            if (m_finishingCheck != nullptr)
            {
                m_finishingCheck->StopSession();
            }

            if (auto const running = std::exchange(m_check, nullptr))
            {
                running->Abandon();
            }
        }
        MIDI_TSHOOT_CATCH_AND_LOG(L"Unable to stop the DPC latency check.")
    }

    _Use_decl_annotations_
    void LatencyCheckPanel::SetState(State const state) noexcept
    {
        try
        {
            m_state = state;

            auto const idle = state == State::Idle;

            StartButton().IsEnabled(idle);
            OpenTraceButton().IsEnabled(idle);
            DurationBox().IsEnabled(idle);
            ProbeCheck().IsEnabled(idle);
            SaveTraceCheck().IsEnabled(idle);

            StopButton().IsEnabled(state == State::Running);
            CancelReadButton().Visibility(state == State::Reading ? xaml::Visibility::Visible : xaml::Visibility::Collapsed);

            BusyRing().IsActive(state == State::Starting || state == State::Finishing || state == State::Reading);

            LiveCard().Visibility(state == State::Running || state == State::Reading ? xaml::Visibility::Visible : xaml::Visibility::Collapsed);

            // the buttons that act on a report only work once there is one
            auto const haveReport = idle && !m_report.empty();

            ViewReportButton().IsEnabled(haveReport);
            SaveReportButton().IsEnabled(haveReport);
            CopyReportButton().IsEnabled(haveReport);
        }
        MIDI_TSHOOT_CATCH_AND_LOG(L"Unable to update the DPC latency check page.")
    }

    void LatencyCheckPanel::StartTimer() noexcept
    {
        try
        {
            if (m_timer == nullptr)
            {
                m_timer = DispatcherQueue().CreateTimer();
                m_timer.Interval(TimerInterval);
                m_timer.IsRepeating(true);

                m_timer.Tick([weak = get_weak()](auto&&, auto&&)
                    {
                        if (auto strong = weak.get())
                        {
                            strong->OnTimerTick();
                        }
                    });
            }

            m_timer.Start();
        }
        MIDI_TSHOOT_CATCH_AND_LOG(L"Unable to start the DPC latency check timer.")
    }

    void LatencyCheckPanel::StopTimer() noexcept
    {
        try
        {
            if (m_timer != nullptr)
            {
                m_timer.Stop();
            }
        }
        MIDI_TSHOOT_CATCH_AND_LOG(L"Unable to stop the DPC latency check timer.")
    }

    void LatencyCheckPanel::OnTimerTick() noexcept
    {
        try
        {
            if (m_closing)
            {
                return;
            }

            if (m_state == State::Reading)
            {
                auto const events = m_eventsRead != nullptr ? m_eventsRead->load() : 0;

                StatusText().Text(res::FormatString(L"LatencyStatusReadingFormat", latency::text::Count(events)));
                return;
            }

            if (m_state != State::Running || m_check == nullptr)
            {
                return;
            }

            ShowLiveStatus();

            auto const elapsed = std::chrono::duration_cast<std::chrono::seconds>(std::chrono::steady_clock::now() - m_startedAt);

            if (m_check->RecordingEnded() || (m_duration.count() > 0 && elapsed >= m_duration))
            {
                FinishAsync();
            }
        }
        MIDI_TSHOOT_CATCH_AND_LOG(L"Unable to show how the DPC latency check is going.")
    }

    void LatencyCheckPanel::ShowLiveStatus() noexcept
    {
        try
        {
            auto const status = m_check->Status();
            auto const elapsed = std::chrono::duration_cast<std::chrono::seconds>(std::chrono::steady_clock::now() - m_startedAt);

            if (m_duration.count() > 0)
            {
                LiveProgress().IsIndeterminate(false);
                LiveProgress().Value((std::min)(100.0, 100.0 * static_cast<double>(elapsed.count()) / static_cast<double>(m_duration.count())));

                StatusText().Text(res::FormatString(L"LatencyStatusRunningFormat", ElapsedText(elapsed), ElapsedText(m_duration)));
            }
            else
            {
                LiveProgress().IsIndeterminate(true);

                StatusText().Text(res::FormatString(L"LatencyStatusRunningOpenFormat", ElapsedText(elapsed)));
            }

            LiveDpcValue().Text(status.LongestDpcMicroseconds == 0 ?
                res::GetString(L"LatencyLiveNothingYet") :
                res::FormatString(L"LatencyLiveValueFormat", latency::text::Milliseconds(status.LongestDpcMicroseconds), status.LongestDpcModule));

            LiveIsrValue().Text(status.LongestIsrMicroseconds == 0 ?
                res::GetString(L"LatencyLiveNothingYet") :
                res::FormatString(L"LatencyLiveValueFormat", latency::text::Milliseconds(status.LongestIsrMicroseconds), status.LongestIsrModule));

            LiveHoldUpValue().Text(status.LateMoments == 0 ?
                res::FormatString(L"LatencyLiveNoHoldUpsFormat", latency::text::Milliseconds(status.LongestWaitMicroseconds)) :
                res::FormatString(L"LatencyLiveHoldUpsFormat", latency::text::Count(status.LateMoments), latency::text::Milliseconds(status.LongestWaitMicroseconds)));
        }
        MIDI_TSHOOT_CATCH_AND_LOG(L"Unable to show the DPC latency check's progress.")
    }

    _Use_decl_annotations_
    winrt::fire_and_forget LatencyCheckPanel::OnStartClick(foundation::IInspectable const&, xaml::RoutedEventArgs const&)
    {
        auto lifetime = get_strong();

        try
        {
            if (m_state != State::Idle)
            {
                co_return;
            }

            // recording kernel events needs administrator rights; reading a saved trace doesn't
            if (!m_host.Elevated)
            {
                if (m_host.RequireElevation)
                {
                    m_host.RequireElevation();
                }

                StatusText().Text(res::GetString(L"LatencyNeedsAdministrator"));
                co_return;
            }

            latency::CheckOptions options{};
            options.UseProbe = IsChecked(ProbeCheck());

            // the customer picks where the trace goes before anything is recorded
            if (IsChecked(SaveTraceCheck()))
            {
                options.TraceFilePath = ShowSaveDialog(
                    std::wstring{ res::GetString(L"LatencyTraceFileType") }, L"etl", SuggestedFileName(L"etl"));

                if (options.TraceFilePath.empty() || m_closing)
                {
                    co_return;
                }
            }

            auto const selected = DurationBox().SelectedIndex();
            m_duration = selected >= 0 && static_cast<size_t>(selected) < Durations.size() ?
                Durations[static_cast<size_t>(selected)] : Durations[1];

            m_tracePath = options.TraceFilePath;

            ClearResults();
            SetState(State::Starting);
            StatusText().Text(res::GetString(L"LatencyStatusStarting"));

            auto check = std::make_shared<latency::LiveCheck>();
            uint32_t status{ ERROR_SUCCESS };

            m_startingCheck = check;

            co_await native::RunOnBackgroundAsync([&check, &options, &status]()
                {
                    status = check->Start(options);
                });

            m_startingCheck.reset();

            if (m_closing)
            {
                // the check stops everything it started when it goes away
                co_return;
            }

            if (status != ERROR_SUCCESS)
            {
                check.reset();
                SetState(State::Idle);
                StatusText().Text(res::FormatString(L"LatencyStatusStartFailedFormat", ErrorText(status)));
                co_return;
            }

            m_check = std::move(check);
            m_startedAt = std::chrono::steady_clock::now();

            LiveDpcValue().Text(res::GetString(L"LatencyLiveNothingYet"));
            LiveIsrValue().Text(res::GetString(L"LatencyLiveNothingYet"));
            LiveHoldUpValue().Text(res::GetString(L"LatencyLiveNothingYet"));
            LiveProgress().Value(0);

            SetState(State::Running);
            ShowLiveStatus();
            StartTimer();
        }
        MIDI_TSHOOT_CATCH_AND_LOG(L"Unable to start the DPC latency check.")
    }

    _Use_decl_annotations_
    winrt::fire_and_forget LatencyCheckPanel::OnStopClick(foundation::IInspectable const&, xaml::RoutedEventArgs const&)
    {
        if (m_state == State::Running)
        {
            FinishAsync();
        }

        co_return;
    }

    winrt::fire_and_forget LatencyCheckPanel::FinishAsync() noexcept
    {
        auto lifetime = get_strong();

        try
        {
            if (m_state != State::Running || m_check == nullptr)
            {
                co_return;
            }

            StopTimer();

            auto const elapsed = std::chrono::steady_clock::now() - m_startedAt;

            SetState(State::Finishing);
            StatusText().Text(res::GetString(L"LatencyStatusFinishing"));

            auto check = std::move(m_check);
            auto outcome = std::make_shared<latency::CheckOutcome>();

            m_finishingCheck = check;

            co_await native::RunOnBackgroundAsync([check, outcome]()
                {
                    *outcome = check->Finish();
                });

            m_finishingCheck.reset();

            if (m_closing)
            {
                co_return;
            }

            SetState(State::Idle);

            if (!outcome->Succeeded())
            {
                StatusText().Text(res::FormatString(L"LatencyStatusFinishFailedFormat", ErrorText(outcome->Error)));
                co_return;
            }

            m_savedTracePath = m_tracePath;

            ShowOutcome(outcome);

            StatusText().Text(elapsed < MinimumUsefulDuration ?
                res::GetString(L"LatencyStatusDoneShort") :
                res::GetString(L"LatencyStatusDone"));
        }
        MIDI_TSHOOT_CATCH_AND_LOG(L"Unable to finish the DPC latency check.")
    }

    _Use_decl_annotations_
    winrt::fire_and_forget LatencyCheckPanel::OnOpenTraceClick(foundation::IInspectable const&, xaml::RoutedEventArgs const&)
    {
        try
        {
            if (m_state != State::Idle)
            {
                co_return;
            }

            auto const path = ShowOpenTraceDialog();

            if (path.empty() || m_closing)
            {
                co_return;
            }

            AnalyzeFileAsync(path);
        }
        MIDI_TSHOOT_CATCH_AND_LOG(L"Unable to open a trace file.")
    }

    _Use_decl_annotations_
    void LatencyCheckPanel::OpenTraceFile(std::wstring const& path) noexcept
    {
        try
        {
            if (m_state == State::Idle && !path.empty())
            {
                AnalyzeFileAsync(path);
            }
        }
        MIDI_TSHOOT_CATCH_AND_LOG(L"Unable to open the trace file from the command line.")
    }

    _Use_decl_annotations_
    winrt::fire_and_forget LatencyCheckPanel::AnalyzeFileAsync(std::wstring const path) noexcept
    {
        auto lifetime = get_strong();

        try
        {
            if (m_state != State::Idle || m_closing)
            {
                co_return;
            }

            ClearResults();

            m_cancelRead = std::make_shared<std::atomic<bool>>(false);
            m_eventsRead = std::make_shared<std::atomic<uint64_t>>(0);
            m_savedTracePath.clear();

            SetState(State::Reading);
            LiveProgress().IsIndeterminate(true);
            LiveDpcValue().Text(res::GetString(L"LatencyLiveNothingYet"));
            LiveIsrValue().Text(res::GetString(L"LatencyLiveNothingYet"));
            LiveHoldUpValue().Text(res::GetString(L"LatencyLiveNothingYet"));
            StatusText().Text(res::FormatString(L"LatencyStatusReadingFormat", latency::text::Count(0)));
            StartTimer();

            auto const cancel = m_cancelRead;
            auto const events = m_eventsRead;
            auto outcome = std::make_shared<latency::CheckOutcome>();

            co_await native::RunOnBackgroundAsync([path, cancel, events, outcome]()
                {
                    *outcome = latency::AnalyzeTraceFile(path, *cancel, *events);
                });

            StopTimer();

            if (m_closing)
            {
                co_return;
            }

            SetState(State::Idle);

            if (outcome->Canceled)
            {
                StatusText().Text(res::GetString(L"LatencyStatusCanceled"));
                co_return;
            }

            if (!outcome->Succeeded())
            {
                StatusText().Text(res::FormatString(L"LatencyStatusOpenFailedFormat", ErrorText(outcome->Error)));
                co_return;
            }

            ShowOutcome(outcome);
            StatusText().Text(res::GetString(L"LatencyStatusDone"));
        }
        MIDI_TSHOOT_CATCH_AND_LOG(L"Unable to read the trace file.")
    }

    _Use_decl_annotations_
    void LatencyCheckPanel::OnCancelReadClick(foundation::IInspectable const&, xaml::RoutedEventArgs const&)
    {
        if (m_cancelRead != nullptr)
        {
            m_cancelRead->store(true);
        }
    }

    _Use_decl_annotations_
    void LatencyCheckPanel::OnViewReportClick(foundation::IInspectable const&, xaml::RoutedEventArgs const&)
    {
        try
        {
            if (m_report.empty() || !m_host.ShowReport)
            {
                return;
            }

            auto loaded = native::LoadReportText(m_report);

            if (loaded.Error != native::ReportLoadError::None)
            {
                StatusText().Text(native::ReportLoadErrorMessage(loaded.Error));
                return;
            }

            m_host.ShowReport(std::move(loaded));
        }
        MIDI_TSHOOT_CATCH_AND_LOG(L"Unable to show the DPC latency report.")
    }

    _Use_decl_annotations_
    winrt::fire_and_forget LatencyCheckPanel::OnSaveReportClick(foundation::IInspectable const&, xaml::RoutedEventArgs const&)
    {
        auto lifetime = get_strong();

        try
        {
            if (m_report.empty())
            {
                co_return;
            }

            auto const path = ShowSaveDialog(std::wstring{ res::GetString(L"LatencyReportFileType") }, L"txt", SuggestedFileName(L"txt"));

            if (path.empty() || m_closing)
            {
                co_return;
            }

            auto const report = m_report;
            bool written{ false };

            co_await native::RunOnBackgroundAsync([&path, &report, &written]()
                {
                    written = latency::text::WriteUtf8File(path, report);
                });

            if (m_closing)
            {
                co_return;
            }

            StatusText().Text(written ?
                res::FormatString(L"LatencyReportSavedFormat", path) :
                res::FormatString(L"LatencyReportSaveFailedFormat", path));
        }
        MIDI_TSHOOT_CATCH_AND_LOG(L"Unable to save the DPC latency report.")
    }

    _Use_decl_annotations_
    void LatencyCheckPanel::OnCopyReportClick(foundation::IInspectable const&, xaml::RoutedEventArgs const&)
    {
        try
        {
            if (m_report.empty())
            {
                return;
            }

            winrt::Windows::ApplicationModel::DataTransfer::DataPackage package{};
            package.SetText(m_report);

            winrt::Windows::ApplicationModel::DataTransfer::Clipboard::SetContent(package);

            StatusText().Text(res::GetString(L"LatencyReportCopied"));
        }
        MIDI_TSHOOT_CATCH_AND_LOG(L"Unable to copy the DPC latency report.")
    }

    _Use_decl_annotations_
    void LatencyCheckPanel::OnShowTraceClick(foundation::IInspectable const&, xaml::RoutedEventArgs const&)
    {
        try
        {
            if (m_savedTracePath.empty())
            {
                return;
            }

            // File Explorer, with the trace selected
            if (auto const item = ::ILCreateFromPathW(m_savedTracePath.c_str()))
            {
                LOG_IF_FAILED(::SHOpenFolderAndSelectItems(item, 0, nullptr, 0));
                ::ILFree(item);
            }
        }
        MIDI_TSHOOT_CATCH_AND_LOG(L"Unable to show the trace file.")
    }

    HWND LatencyCheckPanel::WindowHandle() const noexcept
    {
        try
        {
            return m_host.WindowHandle ? m_host.WindowHandle() : nullptr;
        }
        catch (...)
        {
            return nullptr;
        }
    }

    _Use_decl_annotations_
    std::wstring LatencyCheckPanel::ShowSaveDialog(
        std::wstring const& fileTypeLabel,
        std::wstring const& extension,
        std::wstring const& suggestedFileName) noexcept
    {
        try
        {
            winrt::com_ptr<IFileSaveDialog> dialog{};

            if (FAILED(::CoCreateInstance(CLSID_FileSaveDialog, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(dialog.put()))))
            {
                return {};
            }

            auto const pattern = L"*." + extension;

            COMDLG_FILTERSPEC const filters[]
            {
                { fileTypeLabel.c_str(), pattern.c_str() }
            };

            LOG_IF_FAILED(dialog->SetFileTypes(ARRAYSIZE(filters), filters));
            LOG_IF_FAILED(dialog->SetDefaultExtension(extension.c_str()));
            LOG_IF_FAILED(dialog->SetOptions(FOS_OVERWRITEPROMPT | FOS_PATHMUSTEXIST | FOS_FORCEFILESYSTEM));
            LOG_IF_FAILED(dialog->SetFileName(suggestedFileName.c_str()));

            // canceling is reported as a failure, so it isn't logged
            if (FAILED(dialog->Show(WindowHandle())))
            {
                return {};
            }

            winrt::com_ptr<IShellItem> item{};

            if (FAILED(dialog->GetResult(item.put())) || item == nullptr)
            {
                return {};
            }

            wil::unique_cotaskmem_string path{};

            if (FAILED(item->GetDisplayName(SIGDN_FILESYSPATH, path.put())))
            {
                return {};
            }

            return std::wstring{ path.get() };
        }
        MIDI_TSHOOT_CATCH_AND_LOG(L"Unable to show the save dialog.")

        return {};
    }

    std::wstring LatencyCheckPanel::ShowOpenTraceDialog() noexcept
    {
        try
        {
            winrt::com_ptr<IFileOpenDialog> dialog{};

            if (FAILED(::CoCreateInstance(CLSID_FileOpenDialog, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(dialog.put()))))
            {
                return {};
            }

            auto const tracesLabel = res::GetString(L"LatencyTraceFileType");
            auto const allFilesLabel = res::GetString(L"OpenAllFilesType");
            auto const title = res::GetString(L"LatencyOpenTraceDialogTitle");

            COMDLG_FILTERSPEC const filters[]
            {
                { tracesLabel.c_str(), L"*.etl" },
                { allFilesLabel.c_str(), L"*.*" }
            };

            LOG_IF_FAILED(dialog->SetFileTypes(ARRAYSIZE(filters), filters));
            LOG_IF_FAILED(dialog->SetTitle(title.c_str()));
            LOG_IF_FAILED(dialog->SetOptions(FOS_FILEMUSTEXIST | FOS_PATHMUSTEXIST | FOS_FORCEFILESYSTEM));

            if (FAILED(dialog->Show(WindowHandle())))
            {
                return {};
            }

            winrt::com_ptr<IShellItem> item{};

            if (FAILED(dialog->GetResult(item.put())) || item == nullptr)
            {
                return {};
            }

            wil::unique_cotaskmem_string path{};

            if (FAILED(item->GetDisplayName(SIGDN_FILESYSPATH, path.put())))
            {
                return {};
            }

            return std::wstring{ path.get() };
        }
        MIDI_TSHOOT_CATCH_AND_LOG(L"Unable to show the open dialog.")

        return {};
    }
}
