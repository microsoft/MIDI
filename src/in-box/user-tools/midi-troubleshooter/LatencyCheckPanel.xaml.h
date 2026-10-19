// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

#include "LatencyCheckPanel.g.h"

#include "ReportFile.h"
#include "SystemInfo.h"

#include "LatencyCheck.h"

namespace winrt::miditroubleshooter::implementation
{
    struct LatencyCheckPanel : LatencyCheckPanelT<LatencyCheckPanel>
    {
        LatencyCheckPanel();

        // What the page needs from the window it's in.
        struct Host
        {
            bool Elevated{ false };
            ::miditroubleshooter::SystemInformation System{};

            // false when the app doesn't have administrator rights; it has told the customer
            std::function<bool()> RequireElevation{};

            std::function<void(::miditroubleshooter::LoadedReport)> ShowReport{};
            std::function<HWND()> WindowHandle{};
        };

        void Initialize(_In_ Host host) noexcept;

        // the page has just been shown
        void OnShown() noexcept;

        // The window is closing. A check that's still recording is stopped, because a trace
        // session left running would keep recording until Windows restarts.
        void Shutdown() noexcept;

        // reads a saved trace, as Open a trace file does, for a path from the command line
        void OpenTraceFile(_In_ std::wstring const& path) noexcept;

        winrt::fire_and_forget OnStartClick(_In_ foundation::IInspectable const& sender, _In_ xaml::RoutedEventArgs const& args);
        winrt::fire_and_forget OnStopClick(_In_ foundation::IInspectable const& sender, _In_ xaml::RoutedEventArgs const& args);
        winrt::fire_and_forget OnOpenTraceClick(_In_ foundation::IInspectable const& sender, _In_ xaml::RoutedEventArgs const& args);
        void OnCancelReadClick(_In_ foundation::IInspectable const& sender, _In_ xaml::RoutedEventArgs const& args);

        void OnViewReportClick(_In_ foundation::IInspectable const& sender, _In_ xaml::RoutedEventArgs const& args);
        winrt::fire_and_forget OnSaveReportClick(_In_ foundation::IInspectable const& sender, _In_ xaml::RoutedEventArgs const& args);
        void OnCopyReportClick(_In_ foundation::IInspectable const& sender, _In_ xaml::RoutedEventArgs const& args);
        void OnShowTraceClick(_In_ foundation::IInspectable const& sender, _In_ xaml::RoutedEventArgs const& args);

    private:
        // Idle: nothing is happening. Starting and Finishing: the session is being set up or
        // the results worked out, and nothing may be touched. Running: recording, and Stop works.
        // Reading: a saved trace is being read, and Cancel works.
        enum class State
        {
            Idle,
            Starting,
            Running,
            Finishing,
            Reading
        };

        void SetState(_In_ State const state) noexcept;

        void StartTimer() noexcept;
        void StopTimer() noexcept;
        void OnTimerTick() noexcept;
        void ShowLiveStatus() noexcept;

        winrt::fire_and_forget FinishAsync() noexcept;

        // the path is taken by value, because the caller's copy may be gone by the time this resumes
        winrt::fire_and_forget AnalyzeFileAsync(_In_ std::wstring const path) noexcept;

        // in LatencyCheckPanelResults.cpp
        void ClearResults() noexcept;
        void ShowOutcome(_In_ std::shared_ptr<::miditroubleshooter::latency::CheckOutcome const> const& outcome) noexcept;
        std::wstring BuildReport(_In_ ::miditroubleshooter::latency::CheckOutcome const& outcome) const;

        // the Win32 dialogs, because the WinRT pickers never finish in an elevated process
        std::wstring ShowSaveDialog(
            _In_ std::wstring const& fileTypeLabel,
            _In_ std::wstring const& extension,
            _In_ std::wstring const& suggestedFileName) noexcept;
        std::wstring ShowOpenTraceDialog() noexcept;

        HWND WindowHandle() const noexcept;

        Host m_host{};
        State m_state{ State::Idle };

        std::shared_ptr<::miditroubleshooter::latency::LiveCheck> m_check{};

        // A check that's still starting, or working out its results, on a background thread.
        // Kept here so a closing window can stop its trace session.
        std::shared_ptr<::miditroubleshooter::latency::LiveCheck> m_startingCheck{};
        std::shared_ptr<::miditroubleshooter::latency::LiveCheck> m_finishingCheck{};

        // reading a saved trace
        std::shared_ptr<std::atomic<bool>> m_cancelRead{};
        std::shared_ptr<std::atomic<uint64_t>> m_eventsRead{};

        Microsoft::UI::Dispatching::DispatcherQueueTimer m_timer{ nullptr };
        std::chrono::steady_clock::time_point m_startedAt{};

        // how long the running check lasts; zero until the customer stops it
        std::chrono::seconds m_duration{ 0 };

        // where the running check saves its trace, and where the last one was saved
        std::wstring m_tracePath{};
        std::wstring m_savedTracePath{};

        // the last report, for viewing, copying and saving
        std::wstring m_report{};

        bool m_initialized{ false };
        bool m_closing{ false };
    };
}

namespace winrt::miditroubleshooter::factory_implementation
{
    struct LatencyCheckPanel : LatencyCheckPanelT<LatencyCheckPanel, implementation::LatencyCheckPanel>
    {
    };
}
