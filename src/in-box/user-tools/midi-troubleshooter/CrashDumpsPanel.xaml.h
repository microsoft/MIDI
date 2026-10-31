// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

#include "CrashDumpsPanel.g.h"

#include "CommandLineOptions.h"
#include "CrashDumps.h"

namespace winrt::miditroubleshooter::implementation
{
    struct CrashDumpsPanel : CrashDumpsPanelT<CrashDumpsPanel>
    {
        CrashDumpsPanel();

        // What the page needs from the window it's in.
        struct Host
        {
            bool Elevated{ false };

            // Starts an elevated copy of the app that saves a MIDI service dump as it opens, and
            // closes this window. False when the customer declined the prompt.
            std::function<bool()> RelaunchElevatedForServiceDump{};

            std::function<HWND()> WindowHandle{};
        };

        void Initialize(_In_ Host host) noexcept;

        // the page has just been shown
        void OnShown() noexcept;

        // Another MIDI tool crashed and the WER crash handler opened this app, or told the copy
        // that was already open. Shows which tool and waits for Windows to finish its dump.
        void ShowCrash(_In_ ::miditroubleshooter::CommandLineOptions const& options) noexcept;

        // The elevated copy started by "Save a MIDI service dump" saves it straight away.
        void SaveServiceDumpNow() noexcept;

        // the window is closing
        void Shutdown() noexcept;

        winrt::fire_and_forget OnSaveServiceDumpClick(_In_ foundation::IInspectable const& sender, _In_ xaml::RoutedEventArgs const& args);
        void OnShowCrashDumpClick(_In_ foundation::IInspectable const& sender, _In_ xaml::RoutedEventArgs const& args);
        void OnRefreshClick(_In_ foundation::IInspectable const& sender, _In_ xaml::RoutedEventArgs const& args);
        void OnOpenFolderClick(_In_ foundation::IInspectable const& sender, _In_ xaml::RoutedEventArgs const& args);
        winrt::fire_and_forget OnZipClick(_In_ foundation::IInspectable const& sender, _In_ xaml::RoutedEventArgs const& args);
        winrt::fire_and_forget OnDeleteClick(_In_ foundation::IInspectable const& sender, _In_ xaml::RoutedEventArgs const& args);
        void OnDumpsSelectionChanged(_In_ foundation::IInspectable const& sender, _In_ controls::SelectionChangedEventArgs const& args);

    private:
        winrt::fire_and_forget RefreshAsync() noexcept;
        winrt::fire_and_forget WaitForCrashDumpAsync() noexcept;
        winrt::fire_and_forget SaveServiceDumpAsync() noexcept;

        void ShowCrashState() noexcept;
        void UpdateListButtons() noexcept;
        void SetListBusy(_In_ bool busy) noexcept;
        void ShowListStatus(_In_ winrt::hstring const& text) noexcept;

        // "MIDI Glass" for midiglass.exe, or the exe name for a tool this version doesn't know
        winrt::hstring ToolDisplayName(_In_ std::wstring const& folderOrExecutable) const noexcept;

        std::vector<::miditroubleshooter::crashdumps::DumpFile> SelectedDumps();

        std::wstring ShowSaveZipDialog(_In_ std::wstring const& suggestedFileName) noexcept;
        HWND WindowHandle() const noexcept;

        Host m_host{};

        // the dumps the list shows, by path, so the rows can stay plain strings
        std::vector<::miditroubleshooter::crashdumps::DumpFile> m_dumps{};
        collections::IObservableVector<miditroubleshooter::CrashDumpItem> m_items{
            winrt::single_threaded_observable_vector<miditroubleshooter::CrashDumpItem>() };

        // the crash this page was opened for, if any
        enum class CrashState
        {
            None,
            Waiting,
            Saved,
            NotSaved
        };

        CrashState m_crashState{ CrashState::None };
        DWORD m_crashedProcessId{ 0 };
        std::wstring m_crashedExecutable{};
        DWORD m_werFaultProcessId{ 0 };
        std::wstring m_crashDumpPath{};

        // Shared with the background wait, which can outlive this page.
        std::shared_ptr<std::atomic<bool>> m_cancelWait{};

        bool m_listBusy{ false };
        bool m_serviceDumpBusy{ false };
        bool m_initialized{ false };
    };
}

namespace winrt::miditroubleshooter::factory_implementation
{
    struct CrashDumpsPanel : CrashDumpsPanelT<CrashDumpsPanel, implementation::CrashDumpsPanel>
    {
    };
}
