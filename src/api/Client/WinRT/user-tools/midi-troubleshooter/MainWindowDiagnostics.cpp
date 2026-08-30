// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#include "pch.h"
#include "MainWindow.xaml.h"

#include "BackgroundWork.h"
#include "ProcessRunner.h"
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
    }

    _Use_decl_annotations_
    winrt::fire_and_forget MainWindow::OnRunMidiDiagClick(foundation::IInspectable const&, xaml::RoutedEventArgs const&)
    {
        auto lifetime = get_strong();

        try
        {
            auto const toolPath = native::GetToolLocations().MidiDiag;

            if (toolPath.empty())
            {
                MidiDiagStatusText().Text(res::FormatString(L"DiagnosticsToolMissingFormat", winrt::hstring{ L"mididiag.exe" }));
                co_return;
            }

            RunMidiDiagButton().IsEnabled(false);
            MidiDiagStatusText().Text(res::GetString(L"DiagnosticsRunning"));

            native::ProcessResult result{};

            co_await native::RunOnBackgroundAsync([&result, &toolPath]()
                {
                    result = native::RunCapture(toolPath, L"", ReportTimeout);
                });

            if (m_closing)
            {
                co_return;
            }

            RunMidiDiagButton().IsEnabled(true);

            if (!result.Started)
            {
                MidiDiagStatusText().Text(result.ErrorMessage.empty() ?
                    res::GetString(L"DiagnosticsFailed") : winrt::hstring{ result.ErrorMessage });

                co_return;
            }

            m_midiDiagOutput = winrt::hstring{ result.Output };

            MidiDiagOutputBox().Text(m_midiDiagOutput);

            CopyMidiDiagButton().IsEnabled(!m_midiDiagOutput.empty());
            SaveMidiDiagButton().IsEnabled(!m_midiDiagOutput.empty());

            MidiDiagStatusText().Text(result.TimedOut ?
                res::GetString(L"DiagnosticsTimedOut") : res::GetString(L"DiagnosticsComplete"));
        }
        MIDI_TSHOOT_CATCH_AND_LOG(L"Unable to run the MIDI diagnostics report.")
    }

    _Use_decl_annotations_
    winrt::fire_and_forget MainWindow::OnRunMidiKsInfoClick(foundation::IInspectable const&, xaml::RoutedEventArgs const&)
    {
        auto lifetime = get_strong();

        try
        {
            auto const toolPath = native::GetToolLocations().MidiKsInfo;

            if (toolPath.empty())
            {
                MidiKsInfoStatusText().Text(res::FormatString(L"DiagnosticsToolMissingFormat", winrt::hstring{ L"midiksinfo.exe" }));
                co_return;
            }

            RunMidiKsInfoButton().IsEnabled(false);
            MidiKsInfoStatusText().Text(res::GetString(L"DiagnosticsRunning"));

            native::ProcessResult result{};

            co_await native::RunOnBackgroundAsync([&result, &toolPath]()
                {
                    result = native::RunCapture(toolPath, L"", ReportTimeout);
                });

            if (m_closing)
            {
                co_return;
            }

            RunMidiKsInfoButton().IsEnabled(true);

            if (!result.Started)
            {
                MidiKsInfoStatusText().Text(result.ErrorMessage.empty() ?
                    res::GetString(L"DiagnosticsFailed") : winrt::hstring{ result.ErrorMessage });

                co_return;
            }

            m_midiKsInfoOutput = winrt::hstring{ result.Output };

            MidiKsInfoOutputBox().Text(m_midiKsInfoOutput);

            CopyMidiKsInfoButton().IsEnabled(!m_midiKsInfoOutput.empty());
            SaveMidiKsInfoButton().IsEnabled(!m_midiKsInfoOutput.empty());

            MidiKsInfoStatusText().Text(result.TimedOut ?
                res::GetString(L"DiagnosticsTimedOut") : res::GetString(L"DiagnosticsComplete"));
        }
        MIDI_TSHOOT_CATCH_AND_LOG(L"Unable to run the kernel streaming report.")
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

    _Use_decl_annotations_
    winrt::fire_and_forget MainWindow::SaveTextAsync(winrt::hstring const& suggestedName, winrt::hstring const& text) noexcept
    {
        auto lifetime = get_strong();

        auto const name = suggestedName;
        auto const contents = text;

        try
        {
            winrt::Windows::Storage::Pickers::FileSavePicker picker{};

            // a desktop app has no implicit window for the picker to parent to
            picker.as<::IInitializeWithWindow>()->Initialize(WindowHandle());

            auto extensions = winrt::single_threaded_vector<winrt::hstring>({ L".txt" });

            picker.FileTypeChoices().Insert(res::GetString(L"SaveTextFileType"), extensions);
            picker.SuggestedFileName(name);

            auto const file = co_await picker.PickSaveFileAsync();

            if (file == nullptr)
            {
                co_return;
            }

            co_await winrt::Windows::Storage::FileIO::WriteTextAsync(file, contents);
        }
        MIDI_TSHOOT_CATCH_AND_LOG(L"Unable to save the report.")
    }
}
