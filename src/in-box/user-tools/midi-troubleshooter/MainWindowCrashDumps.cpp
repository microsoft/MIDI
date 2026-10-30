// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#include "pch.h"
#include "MainWindow.xaml.h"
#include "CrashDumpsPanel.xaml.h"

#include "App.xaml.h"
#include "DocumentHandoff.h"
#include "Elevation.h"

#include <commctrl.h>
#pragma comment(lib, "comctl32.lib")

namespace native = ::miditroubleshooter;

namespace winrt::miditroubleshooter::implementation
{
    namespace
    {
        constexpr UINT_PTR CrashHandoffSubclassId = 1;

        // The arguments to put on an elevated copy's command line. Each is quoted, and none can
        // hold a quote: the exe name was checked when it was read.
        std::wstring QuotedArguments(_In_ std::vector<std::wstring> const& arguments)
        {
            std::wstring text{};

            for (auto const& argument : arguments)
            {
                text += L"\"" + argument + L"\" ";
            }

            return text;
        }
    }

    void MainWindow::InitializeCrashDumpsPage() noexcept
    {
        try
        {
            CrashDumpsPanel::Host host{};
            host.Elevated = m_elevated;

            host.RelaunchElevatedForServiceDump = [weak = get_weak()]()
                {
                    auto strong = weak.get();
                    return strong != nullptr && strong->RelaunchElevatedForServiceDump();
                };

            host.WindowHandle = [weak = get_weak()]() -> HWND
                {
                    auto strong = weak.get();
                    return strong != nullptr ? strong->WindowHandle() : nullptr;
                };

            winrt::get_self<CrashDumpsPanel>(CrashDumpsPagePanel())->Initialize(std::move(host));

            auto const window = m_chrome.WindowHandle();

            // A crash in another tool, passed on by the copy the crash handler started. The
            // window procedure gets a weak reference of its own, freed when the window goes.
            auto weak = std::make_unique<winrt::weak_ref<MainWindow>>(get_weak());

            if (::SetWindowSubclass(window, &MainWindow::CrashHandoffSubclassProcedure, CrashHandoffSubclassId,
                reinterpret_cast<DWORD_PTR>(weak.get())))
            {
                weak.release();
            }

            // Windows stops a message from an app without administrator rights reaching one that
            // has them, and the crashed tool, so the copy telling this one, normally has none.
            // The message is checked as hostile before anything is done with it.
            if (m_elevated)
            {
                LOG_IF_WIN32_BOOL_FALSE(::ChangeWindowMessageFilterEx(window, WM_COPYDATA, MSGFLT_ALLOW, nullptr));
            }
        }
        MIDI_TSHOOT_CATCH_AND_LOG(L"Unable to set up the crash dumps page.")
    }

    void MainWindow::ApplyCrashStartupOptions() noexcept
    {
        try
        {
            auto const& options = App::StartupOptions();

            if (options.HasCrashReport())
            {
                ShowCrashReport(options);
            }

            // The copy "Save a MIDI service dump" started with administrator rights.
            if (options.CaptureServiceDump && m_elevated)
            {
                winrt::get_self<CrashDumpsPanel>(CrashDumpsPagePanel())->SaveServiceDumpNow();
            }
        }
        MIDI_TSHOOT_CATCH_AND_LOG(L"Unable to apply the crash options.")
    }

    _Use_decl_annotations_
    void MainWindow::ShowCrashReport(::miditroubleshooter::CommandLineOptions const& options) noexcept
    {
        try
        {
            if (!options.HasCrashReport() || m_closing)
            {
                return;
            }

            m_crashReport = options;

            auto const pageIndex = native::AppSettings::PageIndexCrashDumps;

            ShowPage(pageIndex);

            if (auto const item = NavigationItemForPage(pageIndex))
            {
                MainNavigation().SelectedItem(item);
            }

            winrt::get_self<CrashDumpsPanel>(CrashDumpsPagePanel())->ShowCrash(options);
        }
        MIDI_TSHOOT_CATCH_AND_LOG(L"Unable to show a crash report.")
    }

    bool MainWindow::RelaunchElevatedForServiceDump() noexcept
    {
        try
        {
            // The crash goes along, so the elevated copy names the dump after it. Not WerFault's
            // id: that has finished, and the id could belong to something else by then. The
            // original command line goes first, so a zero here replaces any id it carried.
            auto crash = m_crashReport;
            crash.WerFaultProcessId = 0;

            auto const arguments = std::format(L"--relaunched --capture-service-dump --page crashdumps {}--werfault-pid 0",
                QuotedArguments(crash.CrashArguments()));

            // ownership goes to the elevated copy before it starts, or it finds this window
            // still up, raises it, and exits just as this one closes
            ::midiapp::SingleInstance::Release();

            if (native::TryRelaunchElevated(arguments))
            {
                Close();
                return true;
            }

            ::midiapp::SingleInstance::Reacquire();
        }
        MIDI_TSHOOT_CATCH_AND_LOG(L"Unable to relaunch with administrator rights for a service dump.")

        return false;
    }

    _Use_decl_annotations_
    LRESULT CALLBACK MainWindow::CrashHandoffSubclassProcedure(
        HWND window,
        UINT message,
        WPARAM wParam,
        LPARAM lParam,
        UINT_PTR subclassId,
        DWORD_PTR referenceData) noexcept
    {
        auto const weak = reinterpret_cast<winrt::weak_ref<MainWindow>*>(referenceData);

        if (message == WM_COPYDATA && weak != nullptr)
        {
            try
            {
                auto arguments = ::midiapp::ReadDocumentsFromCopyData(reinterpret_cast<COPYDATASTRUCT const*>(lParam));

                if (!arguments.empty() && arguments.front() == native::CrashReportHandoffMarker)
                {
                    arguments.erase(arguments.begin());

                    // Parsed the same way as a command line, which checks every value. Nothing
                    // else the sender put in is acted on.
                    auto const parsed = native::CommandLineOptions::Parse(arguments);

                    native::CommandLineOptions options{};
                    options.CrashedProcessId = parsed.CrashedProcessId;
                    options.CrashedExecutable = parsed.CrashedExecutable;
                    options.WerFaultProcessId = parsed.WerFaultProcessId;

                    // Shown after the sender has been answered, so it never waits on this window.
                    if (options.HasCrashReport())
                    {
                        if (auto const queue = winrt::Microsoft::UI::Dispatching::DispatcherQueue::GetForCurrentThread())
                        {
                            queue.TryEnqueue([options, target = *weak]()
                                {
                                    try
                                    {
                                        if (auto const strong = target.get())
                                        {
                                            strong->ShowCrashReport(options);
                                        }
                                    }
                                    MIDI_TSHOOT_CATCH_AND_LOG(L"Unable to show a crash passed on by another copy.")
                                });
                        }
                    }

                    return TRUE;
                }
            }
            catch (...)
            {
            }
        }

        if (message == WM_NCDESTROY)
        {
            ::RemoveWindowSubclass(window, &MainWindow::CrashHandoffSubclassProcedure, subclassId);
            delete weak;
        }

        return ::DefSubclassProc(window, message, wParam, lParam);
    }
}
