// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#include "pch.h"
#include "MainWindow.xaml.h"
#include "MainWindow.g.cpp"
#include "App.xaml.h"

#include "AppSettings.h"
#include "StringResources.h"
#include "AppearanceFlyout.h"
#include "DocumentHandoff.h"
#include "EndpointCatalog.h"
#include "LayoutStore.h"
#include "MidiServiceStatus.h"
#include "SingleInstance.h"
#include "PreviewBuild.h"
#include "resource.h"

#include <commctrl.h>

#include <filesystem>

#pragma comment(lib, "comctl32.lib")

namespace resources = ::midiglass::resources;

namespace winrt::midiglass::implementation
{
    namespace
    {
        constexpr int32_t DefaultWindowWidth = 1280;
        constexpr int32_t DefaultWindowHeight = 860;

        constexpr UINT_PTR HandoffSubclassId = 1;
    }

    _Use_decl_annotations_
    LRESULT CALLBACK MainWindow::HandoffSubclassProcedure(
        HWND window,
        UINT message,
        WPARAM wParam,
        LPARAM lParam,
        UINT_PTR subclassId,
        DWORD_PTR referenceData) noexcept
    {
        UNREFERENCED_PARAMETER(referenceData);

        if (message == WM_COPYDATA)
        {
            try
            {
                auto paths = ::midiapp::ReadDocumentsFromCopyData(
                    reinterpret_cast<COPYDATASTRUCT const*>(lParam));

                // Opened after the sender has been answered, so a window that takes a moment to
                // build never holds the other process up.
                if (!paths.empty())
                {
                    if (auto const queue = winrt::Microsoft::UI::Dispatching::DispatcherQueue::GetForCurrentThread())
                    {
                        queue.TryEnqueue([paths = std::move(paths)]()
                            {
                                for (auto const& path : paths)
                                {
                                    std::error_code ignored{};

                                    // Another process named it, so it has to be a layout that is there.
                                    if (glass::IsLayoutFileName(path) &&
                                        std::filesystem::is_regular_file(path, ignored))
                                    {
                                        App::OpenRuntimeWindow(path);
                                    }
                                }
                            });
                    }
                }
            }
            catch (...)
            {
            }

            return TRUE;
        }

        if (message == WM_NCDESTROY)
        {
            ::RemoveWindowSubclass(window, &MainWindow::HandoffSubclassProcedure, subclassId);
        }

        return ::DefSubclassProc(window, message, wParam, lParam);
    }

    void MainWindow::RestoreWindowPlacement()
    {
        midiapp::WindowChrome::RestorePlacement(
            *this, ::midiglass::AppSettings::Current(), DefaultWindowWidth, DefaultWindowHeight);
    }

    _Use_decl_annotations_
    void MainWindow::OnRootLoaded(foundation::IInspectable const& sender, xaml::RoutedEventArgs const& args)
    {
        UNREFERENCED_PARAMETER(sender);
        UNREFERENCED_PARAMETER(args);

        try
        {
            midiapp::ApplyPreviewBadgeVisibility(PreviewChiclet());

            midiapp::WindowChromeElements elements{};

            elements.Window = *this;
            elements.Root = RootGrid();
            elements.Fill = WindowFill();
            elements.Tint = WindowTint();
            elements.TitleBar = AppTitleBar();
            elements.LeftInset = TitleBarLeftInsetColumn();
            elements.RightInset = TitleBarRightInsetColumn();

            m_chrome.Initialize(elements, ::midiglass::AppSettings::Current());

            // Now that there is a window, a later launch has something to bring forward.
            ::midiapp::SingleInstance::PublishMainWindow(m_chrome.WindowHandle());
            ::SetWindowSubclass(m_chrome.WindowHandle(), &MainWindow::HandoffSubclassProcedure, HandoffSubclassId, 0);

            m_chrome.SetWindowIconFromResource(IDI_APPICON);

            Title(resources::GetString(L"AppDisplayName"));
            AppTitleTextBlock().Text(resources::GetString(L"AppDisplayName"));

            // 32px source for a 16px slot, so it stays crisp on a high DPI display
            if (auto const icon = midiapp::WindowChrome::LoadIconImageSource(IDI_APPICON, 32))
            {
                AppTitleBarIcon().Source(icon);
            }

            AlwaysOnTopToggle().IsChecked(::midiglass::AppSettings::Current().AlwaysOnTop());
            KeepAwakeMenuItem().IsChecked(
                ::midiglass::AppSettings::Current().KeepAwakeWhileRunning());

            m_dispatcher = DispatcherQueue();

            m_updatingChrome = true;

            SortSelector().Items().Append(box_value(resources::GetString(L"SortByLastUsed")));
            SortSelector().Items().Append(box_value(resources::GetString(L"SortByName")));
            SortSelector().Items().Append(box_value(resources::GetString(L"SortByLastChanged")));
            SortSelector().SelectedIndex(
                static_cast<int32_t>(::midiglass::AppSettings::Current().LibrarySortOrder()));

            m_updatingChrome = false;

            FavoritesGrid().ItemsSource(m_favorites);
            RecentGrid().ItemsSource(m_recent);

            ApplyViewMode();

            // A device arriving or leaving changes what every card says about itself, so the
            // library is rebuilt rather than left claiming a synth is still there. It is also
            // the moment to look at the service, because a service that stopped takes every
            // device with it and says nothing else about itself.
            auto weak = get_weak();

            // Resolved only on the UI thread. A window released on the watcher's thread is
            // destroyed there, and XAML objects must not be.
            m_endpointsChangedToken = midiapp::EndpointCatalog::Current().AddChangedHandler(
                [weak, queue = m_dispatcher]()
                {
                    if (queue == nullptr)
                    {
                        return;
                    }

                    queue.TryEnqueue([weak]()
                        {
                            try
                            {
                                if (auto inner = weak.get())
                                {
                                    inner->CheckServiceState();
                                    inner->UpdateStatusBar();
                                    inner->RefreshLibrary();
                                }
                            }
                            MIDI_GLASS_CATCH_AND_LOG(L"Unable to show the device change.")
                        });
                });

            // A device event is not the only way the service can go away: somebody can stop it
            // while nothing is plugged in, and then no watcher fires at all. Querying the
            // service control manager costs microseconds, so it is also polled.
            m_serviceRunning = midiapp::IsMidiServiceRunning();

            m_serviceTimer = xaml::DispatcherTimer();
            m_serviceTimer.Interval(std::chrono::seconds{ 3 });
            m_serviceTimer.Tick([weak](auto&&, auto&&)
                {
                    if (auto strong = weak.get())
                    {
                        if (strong->CheckServiceState())
                        {
                            strong->RefreshLibrary();
                        }
                    }
                });

            m_serviceTimer.Start();

            // The watcher blocks on the service, so it is started off the UI thread.
            std::thread([]()
                {
                    winrt::init_apartment(winrt::apartment_type::multi_threaded);

                    midiapp::EndpointCatalog::Current().Start();

                    winrt::uninit_apartment();
                }).detach();

            RefreshLibrary();

            Closed({ this, &MainWindow::OnWindowClosed });
        }
        MIDI_GLASS_CATCH_AND_LOG(L"Unable to set up the window chrome.")
    }

    _Use_decl_annotations_
    void MainWindow::OnWindowClosed(foundation::IInspectable const& sender, xaml::WindowEventArgs const& args)
    {
        UNREFERENCED_PARAMETER(sender);
        UNREFERENCED_PARAMETER(args);

        try
        {
            // A handler left pointing at a closed window is a use after free waiting for
            // somebody to plug something in.
            midiapp::EndpointCatalog::Current().RemoveChangedHandler(m_endpointsChangedToken);
            m_endpointsChangedToken = 0;

            if (m_serviceTimer != nullptr)
            {
                m_serviceTimer.Stop();
                m_serviceTimer = nullptr;
            }

            m_chrome.SavePlacement();
            m_chrome.Shutdown();
        }
        MIDI_GLASS_CATCH_AND_LOG(L"Unable to shut the window down cleanly.")
    }

    _Use_decl_annotations_
    void MainWindow::OnAppearanceButtonClick(foundation::IInspectable const& sender, xaml::RoutedEventArgs const& args)
    {
        UNREFERENCED_PARAMETER(sender);
        UNREFERENCED_PARAMETER(args);

        try
        {
            midiapp::AppearanceStrings strings{};

            strings.Title = resources::GetString(L"AppearanceTitle");
            strings.ThemeLabel = resources::GetString(L"AppearanceTheme");
            strings.ThemeSystem = resources::GetString(L"AppearanceThemeSystem");
            strings.ThemeLight = resources::GetString(L"AppearanceThemeLight");
            strings.ThemeDark = resources::GetString(L"AppearanceThemeDark");
            strings.BackdropLabel = resources::GetString(L"AppearanceBackdrop");
            strings.BackdropSolid = resources::GetString(L"AppearanceBackdropSolid");
            strings.BackdropMica = resources::GetString(L"AppearanceBackdropMica");
            strings.BackdropAcrylic = resources::GetString(L"AppearanceBackdropAcrylic");
            strings.CustomColorCheckBox = resources::GetString(L"AppearanceCustomColor");
            strings.ColorPickerName = resources::GetString(L"AppearanceColorPicker");

            auto weak = get_weak();

            midiapp::ShowAppearanceFlyout(
                AppearanceButton(),
                ::midiglass::AppSettings::Current(),
                strings,
                [weak]()
                {
                    if (auto strong = weak.get())
                    {
                        strong->m_chrome.ApplyTheme();
                    }
                });
        }
        MIDI_GLASS_CATCH_AND_LOG(L"Unable to show the appearance flyout.")
    }

    _Use_decl_annotations_
    void MainWindow::OnAlwaysOnTopToggled(foundation::IInspectable const& sender, xaml::RoutedEventArgs const& args)
    {
        UNREFERENCED_PARAMETER(sender);
        UNREFERENCED_PARAMETER(args);

        try
        {
            auto const checked = AlwaysOnTopToggle().IsChecked();

            ::midiglass::AppSettings::Current().AlwaysOnTop(checked && checked.Value());

            m_chrome.ApplyAlwaysOnTop();
        }
        MIDI_GLASS_CATCH_AND_LOG(L"Unable to change the always on top setting.")
    }

    _Use_decl_annotations_
    void MainWindow::OnNewLayoutClick(foundation::IInspectable const& sender, xaml::RoutedEventArgs const& args)
    {
        UNREFERENCED_PARAMETER(sender);
        UNREFERENCED_PARAMETER(args);

        ShowNewLayoutDialogAsync();
    }
}
