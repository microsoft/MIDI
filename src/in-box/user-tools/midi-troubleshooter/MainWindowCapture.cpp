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
    void MainWindow::SetCaptureButtonsForState(bool const running) noexcept
    {
        try
        {
            StartCaptureButton().IsEnabled(!running);
            StopCaptureButton().IsEnabled(running);
            CancelCaptureButton().IsEnabled(running);

            CaptureSystemInfoCheck().IsEnabled(!running);
            CaptureMidiDiagCheck().IsEnabled(!running);
            CaptureMidiKsInfoCheck().IsEnabled(!running);

            CaptureTimeTravelCheck().IsEnabled(
                !running && !native::GetToolLocations().TimeTravelTracer.empty());
        }
        MIDI_TSHOOT_CATCH_AND_LOG(L"Unable to update the capture buttons.")
    }

    _Use_decl_annotations_
    winrt::fire_and_forget MainWindow::OnStartCaptureClick(foundation::IInspectable const&, xaml::RoutedEventArgs const&)
    {
        auto lifetime = get_strong();

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

            SetCaptureButtonsForState(true);
            CaptureProgressRing().IsActive(true);
            CaptureStatusText().Text(res::GetString(L"CaptureStarting"));

            native::CaptureStepResult result{};

            co_await native::RunOnBackgroundAsync([this, &result, &options]()
                {
                    result = m_capture.Start(options);
                });

            if (m_closing)
            {
                co_return;
            }

            CaptureProgressRing().IsActive(false);

            AppendCaptureLog(result.Log);

            if (!result.Succeeded)
            {
                SetCaptureButtonsForState(false);

                CaptureStatusText().Text(result.ErrorMessage.empty() ?
                    res::GetString(L"CaptureFailed") : winrt::hstring{ result.ErrorMessage });

                co_return;
            }

            CaptureStatusText().Text(res::GetString(L"CaptureReproduceNow"));
        }
        MIDI_TSHOOT_CATCH_AND_LOG(L"Unable to start the capture.")
    }

    _Use_decl_annotations_
    winrt::fire_and_forget MainWindow::OnStopCaptureClick(foundation::IInspectable const&, xaml::RoutedEventArgs const&)
    {
        auto lifetime = get_strong();

        try
        {
            if (!m_capture.IsRunning())
            {
                co_return;
            }

            // The destination is chosen before anything is collected, so a canceled dialog
            // does not throw away a trace that has already been stopped.
            winrt::Windows::Storage::Pickers::FileSavePicker picker{};

            picker.as<::IInitializeWithWindow>()->Initialize(WindowHandle());

            auto extensions = winrt::single_threaded_vector<winrt::hstring>({ L".zip" });

            picker.FileTypeChoices().Insert(res::GetString(L"SaveZipFileType"), extensions);
            picker.SuggestedFileName(winrt::hstring{ native::ReproCapture::SuggestedFileName() });

            auto const file = co_await picker.PickSaveFileAsync();

            if (file == nullptr)
            {
                co_return;
            }

            auto const outputPath = std::wstring{ file.Path() };

            StopCaptureButton().IsEnabled(false);
            CancelCaptureButton().IsEnabled(false);
            CaptureProgressRing().IsActive(true);
            CaptureStatusText().Text(res::GetString(L"CaptureCollecting"));

            native::CaptureStepResult result{};

            co_await native::RunOnBackgroundAsync([this, &result, &outputPath]()
                {
                    result = m_capture.Finish(outputPath);
                });

            if (m_closing)
            {
                co_return;
            }

            CaptureProgressRing().IsActive(false);
            SetCaptureButtonsForState(false);

            AppendCaptureLog(result.Log);

            if (!result.Succeeded)
            {
                CaptureStatusText().Text(result.ErrorMessage.empty() ?
                    res::GetString(L"CaptureFailed") : winrt::hstring{ result.ErrorMessage });

                co_return;
            }

            CaptureStatusText().Text(res::FormatString(L"CaptureSavedFormat", file.Path()));

            // Shows the package selected in Explorer, which is what the customer needs next.
            auto const folder = co_await file.GetParentAsync();

            if (folder != nullptr)
            {
                winrt::Windows::System::FolderLauncherOptions launcherOptions{};

                launcherOptions.ItemsToSelect().Append(file);

                co_await winrt::Windows::System::Launcher::LaunchFolderAsync(folder, launcherOptions);
            }
        }
        MIDI_TSHOOT_CATCH_AND_LOG(L"Unable to finish the capture.")
    }

    _Use_decl_annotations_
    winrt::fire_and_forget MainWindow::OnCancelCaptureClick(foundation::IInspectable const&, xaml::RoutedEventArgs const&)
    {
        auto lifetime = get_strong();

        try
        {
            if (!m_capture.IsRunning())
            {
                co_return;
            }

            auto const confirmed = co_await ConfirmAsync(
                res::GetString(L"CaptureCancelTitle"),
                res::GetString(L"CaptureCancelMessage"));

            if (!confirmed)
            {
                co_return;
            }

            CaptureProgressRing().IsActive(true);
            CaptureStatusText().Text(res::GetString(L"CaptureCanceling"));

            native::CaptureStepResult result{};

            co_await native::RunOnBackgroundAsync([this, &result]()
                {
                    result = m_capture.Cancel();
                });

            if (m_closing)
            {
                co_return;
            }

            CaptureProgressRing().IsActive(false);
            SetCaptureButtonsForState(false);

            AppendCaptureLog(result.Log);

            CaptureStatusText().Text(res::GetString(L"CaptureCanceled"));
        }
        MIDI_TSHOOT_CATCH_AND_LOG(L"Unable to cancel the capture.")
    }
}
