// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

#include "ReportViewerWindow.g.h"

#include "ReportFile.h"
#include "WindowChrome.h"

namespace miditroubleshooter
{
    // Builds the view of one report. Defined in ReportViewerWindow.xaml.cpp.
    class ReportPresenter;
}

namespace winrt::miditroubleshooter::implementation
{
    // Shows a mididiag report as sections that open and close, instead of as one long text.
    // One of these at a time, owned by the main window and closed along with it.
    struct ReportViewerWindow : ReportViewerWindowT<ReportViewerWindow>
    {
        ReportViewerWindow() = default;

        // runs before Activate, so it can't touch the chrome instance
        void RestoreWindowPlacement() noexcept;

        // Replaces whatever is showing. Before the window has loaded, it's kept until it has.
        void ShowReport(_In_ ::miditroubleshooter::LoadedReport report) noexcept;

        // Brings the window forward, and back from the taskbar when it was minimized.
        void BringToFront() noexcept;

        // The main window's appearance or always on top setting changed.
        void ApplyAppearance() noexcept;
        void ApplyAlwaysOnTop() noexcept;

        void OnRootLoaded(_In_ foundation::IInspectable const& sender, _In_ xaml::RoutedEventArgs const& args);
        void OnRootSizeChanged(_In_ foundation::IInspectable const& sender, _In_ xaml::SizeChangedEventArgs const& args);

        winrt::fire_and_forget OnOpenReportClick(_In_ foundation::IInspectable const& sender, _In_ xaml::RoutedEventArgs const& args);
        void OnExpandAllClick(_In_ foundation::IInspectable const& sender, _In_ xaml::RoutedEventArgs const& args);
        void OnCollapseAllClick(_In_ foundation::IInspectable const& sender, _In_ xaml::RoutedEventArgs const& args);

    private:
        void RenderReport() noexcept;
        void ShowLoadError(_In_ winrt::hstring const& message) noexcept;
        void SaveWindowPlacement() noexcept;

        HWND WindowHandle() noexcept;

        midiapp::WindowChrome m_chrome{};

        ::miditroubleshooter::LoadedReport m_report{};

        // Replaced with each report. The sections it hasn't built yet hold it only weakly.
        std::shared_ptr<::miditroubleshooter::ReportPresenter> m_presenter{};

        bool m_loaded{ false };
        bool m_closing{ false };

        // a file is being read, so another open waits for it
        bool m_opening{ false };
    };
}

namespace winrt::miditroubleshooter::factory_implementation
{
    struct ReportViewerWindow : ReportViewerWindowT<ReportViewerWindow, implementation::ReportViewerWindow>
    {
    };
}
