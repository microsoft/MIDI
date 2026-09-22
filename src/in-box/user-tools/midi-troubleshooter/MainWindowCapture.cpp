// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#include "pch.h"
#include "MainWindow.xaml.h"

#include "BackgroundWork.h"
#include "StringResources.h"
#include "ToolPaths.h"

namespace native = ::miditroubleshooter;
namespace res = ::miditroubleshooter::resources;

namespace winrt::miditroubleshooter::implementation
{
    _Use_decl_annotations_
    void MainWindow::AppendCaptureLog(std::vector<std::wstring> const& lines) noexcept
    {
        try
        {
            if (lines.empty())
            {
                return;
            }

            std::wstring text{ CaptureLogBox().Text() };

            // A WinUI TextBox stores line breaks as a bare carriage return, so whatever break
            // is already there is dropped and one is supplied here instead.
            while (!text.empty() && (text.back() == L'\r' || text.back() == L'\n'))
            {
                text.pop_back();
            }

            for (auto const& line : lines)
            {
                if (!text.empty())
                {
                    text += L"\r\n";
                }

                text += line;
            }

            CaptureLogBox().Text(winrt::hstring{ text });

            // the newest line is the interesting one
            CaptureLogBox().Select(static_cast<int32_t>(text.size()), 0);
        }
        MIDI_TSHOOT_CATCH_AND_LOG(L"Unable to append to the capture log.")
    }

    _Use_decl_annotations_
    void MainWindow::SetCaptureUiState(CaptureUiState const state) noexcept
    {
        try
        {
            auto const idle = state == CaptureUiState::Idle;
            auto const running = state == CaptureUiState::Running;

            StartCaptureButton().IsEnabled(idle);

            // Only offered once tracing is actually live. Offering them while the capture is
            // still starting looks like the app is ready when it is not, and a click in that
            // window finds nothing to stop and silently does nothing at all.
            StopCaptureButton().IsEnabled(running);
            CancelCaptureButton().IsEnabled(running);

            CaptureSystemInfoCheck().IsEnabled(idle);
            CaptureMidiDiagCheck().IsEnabled(idle);
            CaptureMidiKsInfoCheck().IsEnabled(idle);

            CaptureTimeTravelCheck().IsEnabled(
                idle && !native::GetToolLocations().TimeTravelTracer.empty());

            CaptureProgressRing().IsActive(state == CaptureUiState::Working);
        }
        MIDI_TSHOOT_CATCH_AND_LOG(L"Unable to update the capture buttons.")
    }

    _Use_decl_annotations_
    void MainWindow::ShowTimeTravelNoticeIfNeeded(native::CaptureStepResult const& result) noexcept
    {
        try
        {
            // Shown whether or not the capture succeeded: the attach happened either way, and
            // the overhead it leaves behind is the same.
            if (result.TimeTravelTraceRecorded)
            {
                CaptureTimeTravelInfoBar().IsOpen(true);
            }
        }
        MIDI_TSHOOT_CATCH_AND_LOG(L"Unable to show the time travel notice.")
    }

    _Use_decl_annotations_
    winrt::fire_and_forget MainWindow::OnStartCaptureClick(foundation::IInspectable const&, xaml::RoutedEventArgs const&)
    {
        auto lifetime = get_strong();

        if (m_captureBusy)
        {
            co_return;
        }

        try
        {
            // wpr.exe cannot start a trace session without administrator rights
            if (!RequireElevation())
            {
                co_return;
            }

            native::CaptureOptions options{};

            options.IncludeSystemInformation = CaptureSystemInfoCheck().IsChecked().Value();
            options.IncludeMidiDiag = CaptureMidiDiagCheck().IsChecked().Value();
            options.IncludeMidiKsInfo = CaptureMidiKsInfoCheck().IsChecked().Value();
            options.IncludeTimeTravelTrace = CaptureTimeTravelCheck().IsChecked().Value();

            m_captureBusy = true;

            SetCaptureUiState(CaptureUiState::Working);
            CaptureStatusText().Text(res::GetString(L"CaptureStarting"));
            CaptureTimeTravelInfoBar().IsOpen(false);

            // Runs on the closing and the exception paths too, so the ring can never be left
            // spinning over a page whose buttons all say no.
            auto const restoreUi = wil::scope_exit([this]() noexcept
                {
                    m_captureBusy = false;

                    if (!m_closing)
                    {
                        SetCaptureUiState(m_capture.IsRunning() ?
                            CaptureUiState::Running : CaptureUiState::Idle);
                    }
                });

            native::CaptureStepResult result{};

            co_await native::RunOnBackgroundAsync([this, &result, &options]()
                {
                    result = m_capture.Start(options);
                });

            if (m_closing)
            {
                co_return;
            }

            AppendCaptureLog(result.Log);

            CaptureStatusText().Text(result.Succeeded ?
                res::GetString(L"CaptureReproduceNow") :
                (result.ErrorMessage.empty() ?
                    res::GetString(L"CaptureFailed") : winrt::hstring{ result.ErrorMessage }));
        }
        MIDI_TSHOOT_CATCH_AND_LOG(L"Unable to start the capture.")
    }

    _Use_decl_annotations_
    winrt::fire_and_forget MainWindow::OnStopCaptureClick(foundation::IInspectable const&, xaml::RoutedEventArgs const&)
    {
        auto lifetime = get_strong();

        if (m_captureBusy || !m_capture.IsRunning())
        {
            co_return;
        }

        try
        {
            // The destination is chosen before anything is collected, so a canceled dialog
            // does not throw away a trace that has already been stopped.
            auto const outputPath = ShowSaveFileDialog(
                std::wstring{ res::GetString(L"SaveZipFileType") },
                L"zip",
                native::ReproCapture::SuggestedFileName());

            if (outputPath.empty() || m_closing)
            {
                co_return;
            }

            native::CaptureStepResult result{};

            // Scoped so the page is released before Explorer is brought up, which is not part
            // of the capture and should not be shown as one.
            {
                m_captureBusy = true;

                SetCaptureUiState(CaptureUiState::Working);
                CaptureStatusText().Text(res::GetString(L"CaptureCollecting"));

                auto const restoreUi = wil::scope_exit([this]() noexcept
                    {
                        m_captureBusy = false;

                        if (!m_closing)
                        {
                            SetCaptureUiState(m_capture.IsRunning() ?
                                CaptureUiState::Running : CaptureUiState::Idle);
                        }
                    });

                co_await native::RunOnBackgroundAsync([this, &result, &outputPath]()
                    {
                        result = m_capture.Finish(outputPath);
                    });

                if (m_closing)
                {
                    co_return;
                }

                AppendCaptureLog(result.Log);
            }

            ShowTimeTravelNoticeIfNeeded(result);

            if (!result.Succeeded)
            {
                CaptureStatusText().Text(result.ErrorMessage.empty() ?
                    res::GetString(L"CaptureFailed") : winrt::hstring{ result.ErrorMessage });

                co_return;
            }

            CaptureStatusText().Text(res::FormatString(L"CaptureSavedFormat", winrt::hstring{ outputPath }));

            // Shows the package selected in Explorer, which is what the customer needs next.
            if (auto const idList = ::ILCreateFromPathW(outputPath.c_str()))
            {
                LOG_IF_FAILED(::SHOpenFolderAndSelectItems(idList, 0, nullptr, 0));

                ::ILFree(idList);
            }
        }
        MIDI_TSHOOT_CATCH_AND_LOG(L"Unable to finish the capture.")
    }

    _Use_decl_annotations_
    winrt::fire_and_forget MainWindow::OnCancelCaptureClick(foundation::IInspectable const&, xaml::RoutedEventArgs const&)
    {
        auto lifetime = get_strong();

        if (m_captureBusy || !m_capture.IsRunning())
        {
            co_return;
        }

        try
        {
            auto const confirmed = co_await ConfirmAsync(
                res::GetString(L"CaptureCancelTitle"),
                res::GetString(L"CaptureCancelMessage"));

            if (!confirmed || m_closing)
            {
                co_return;
            }

            m_captureBusy = true;

            SetCaptureUiState(CaptureUiState::Working);
            CaptureStatusText().Text(res::GetString(L"CaptureCanceling"));

            auto const restoreUi = wil::scope_exit([this]() noexcept
                {
                    m_captureBusy = false;

                    if (!m_closing)
                    {
                        SetCaptureUiState(m_capture.IsRunning() ?
                            CaptureUiState::Running : CaptureUiState::Idle);
                    }
                });

            native::CaptureStepResult result{};

            co_await native::RunOnBackgroundAsync([this, &result]()
                {
                    result = m_capture.Cancel();
                });

            if (m_closing)
            {
                co_return;
            }

            AppendCaptureLog(result.Log);

            ShowTimeTravelNoticeIfNeeded(result);

            CaptureStatusText().Text(res::GetString(L"CaptureCanceled"));
        }
        MIDI_TSHOOT_CATCH_AND_LOG(L"Unable to cancel the capture.")
    }
}
