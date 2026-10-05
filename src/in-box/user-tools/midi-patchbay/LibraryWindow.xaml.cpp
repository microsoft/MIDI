// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#include "pch.h"
#include "LibraryWindow.xaml.h"
#if __has_include("LibraryWindow.g.cpp")
#include "LibraryWindow.g.cpp"
#endif

#include "App.xaml.h"
#include "BackgroundWork.h"
#include "DocumentHandoff.h"
#include "PatchCanvas.h"
#include "PatchLayout.h"
#include "PatchStore.h"
#include "StringResources.h"
#include "resource.h"

using namespace winrt::Microsoft::UI::Xaml;

namespace patchbay = ::midipatchbay;
namespace resources = ::midipatchbay::resources;

namespace winrt::midipatchbay::implementation
{
    namespace
    {
        constexpr int32_t DefaultWindowWidth = 1280;
        constexpr int32_t DefaultWindowHeight = 820;

        constexpr int32_t RefreshIntervalMilliseconds = 500;

        constexpr double TileWidth = 300.0;
        constexpr double TileHeight = 214.0;
        constexpr double MiniMapWidth = 270.0;
        constexpr double MiniMapHeight = 74.0;

        // A map is a sketch of the patch, so its nodes are never drawn larger than this, however
        // few there are.
        constexpr double MiniMapMaximumScale = 0.3;

        bool SameText(_In_ std::wstring const& left, _In_ std::wstring const& right) noexcept
        {
            return ::CompareStringOrdinal(left.c_str(), -1, right.c_str(), -1, TRUE) == CSTR_EQUAL;
        }

        // Case-insensitive, the way a person expects a search box to work.
        bool ContainsText(_In_ std::wstring_view text, _In_ std::wstring_view search) noexcept
        {
            if (search.empty())
            {
                return true;
            }

            if (text.empty())
            {
                return false;
            }

            return ::FindNLSStringEx(
                LOCALE_NAME_USER_DEFAULT,
                FIND_FROMSTART | LINGUISTIC_IGNORECASE,
                text.data(), static_cast<int>(text.size()),
                search.data(), static_cast<int>(search.size()),
                nullptr, nullptr, nullptr, 0) >= 0;
        }

        media::Brush Brush(_In_ std::wstring_view key) noexcept
        {
            return patchbay::ThemeBrushes::Current().Get(key);
        }
    }

    LibraryWindow::LibraryWindow()
    {
        // XAML objects must not call InitializeComponent during construction; winrt::make does it
    }

    winrt::weak_ref<LibraryWindow> LibraryWindow::s_instance{};

    _Use_decl_annotations_
    LRESULT CALLBACK LibraryWindow::HandoffSubclassProcedure(
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

                // Imported after the sender has been answered, so a slow disk never holds the
                // other process up.
                if (!paths.empty())
                {
                    if (auto const queue = winrt::Microsoft::UI::Dispatching::DispatcherQueue::GetForCurrentThread())
                    {
                        queue.TryEnqueue([paths = std::move(paths)]()
                            {
                                if (auto strong = s_instance.get())
                                {
                                    strong->ImportPatchFiles(paths);
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
            ::RemoveWindowSubclass(window, &LibraryWindow::HandoffSubclassProcedure, subclassId);
        }

        return ::DefSubclassProc(window, message, wParam, lParam);
    }

    void LibraryWindow::RestoreWindowPlacement() noexcept
    {
        midiapp::WindowChrome::RestorePlacement(
            *this, patchbay::AppSettings::Current(), DefaultWindowWidth, DefaultWindowHeight);
    }

    void LibraryWindow::MinimizeAtStartup() noexcept
    {
        try
        {
            // From the window itself: this runs before the root has loaded, and the chrome only
            // knows the window once it has, so asking the chrome gave no handle and no minimize.
            HWND handle{ nullptr };

            if (auto const native = xaml::Window{ *this }.try_as<::IWindowNative>())
            {
                LOG_IF_FAILED(native->get_WindowHandle(&handle));
            }

            if (handle != nullptr)
            {
                ::ShowWindow(handle, SW_MINIMIZE);
            }
        }
        MIDI_PATCHBAY_CATCH_AND_LOG(L"Unable to start minimized.")
    }

    void LibraryWindow::BringForward() noexcept
    {
        try
        {
            auto const handle = m_chrome.WindowHandle();

            if (handle != nullptr && (!::IsWindowVisible(handle) || ::IsIconic(handle)))
            {
                RestoreFromNotificationArea();
                return;
            }

            Activate();
        }
        MIDI_PATCHBAY_CATCH_AND_LOG(L"Unable to bring the library forward.")
    }

    _Use_decl_annotations_
    void LibraryWindow::OnRootLoaded(foundation::IInspectable const& sender, xaml::RoutedEventArgs const& args)
    {
        UNREFERENCED_PARAMETER(sender);
        UNREFERENCED_PARAMETER(args);

        try
        {
            if (m_loaded)
            {
                return;
            }

            m_loaded = true;

            // Before anything builds a visual in code, because that is where its brushes come
            // from. Every editor uses these too: this window lives as long as the app.
            patchbay::ThemeBrushes::Current().Initialize(ThemeBrushSource());

            InitializeWindowChrome();
            ApplyAssistantVisibility();

            auto weak = get_weak();
            auto queue = DispatcherQueue();

            // Tiles built in code cannot re-theme themselves, and changing the Windows theme
            // raises no settings change, so this is the one event that catches both.
            RootGrid().ActualThemeChanged([weak](auto&&, auto&&)
                {
                    if (auto strong = weak.get())
                    {
                        strong->RebuildTiles();
                    }
                });

            m_libraryToken = patchbay::PatchLibrary::Current().Subscribe(
                [weak](patchbay::LibraryChange change, std::wstring const& key)
                {
                    if (auto strong = weak.get())
                    {
                        strong->OnLibraryChanged(change, key);
                    }
                });

            // The catalog raises its change on a watcher thread, so the library hears about it
            // back on this one.
            patchbay::EndpointCatalog::Current().SetChangedHandler([queue]()
                {
                    if (queue != nullptr)
                    {
                        queue.TryEnqueue([]()
                            {
                                patchbay::PatchLibrary::Current().EndpointsChanged();
                            });
                    }
                });

            auto& library = patchbay::PatchLibrary::Current();

            library.Load();

            if (auto const error = library.LastErrorMessage(); !error.empty())
            {
                ShowStatus(error, controls::InfoBarSeverity::Error);
            }

            RebuildTiles();
            UpdateStatusStrip();

            auto const& options = App::StartupOptions();

            // A patch double-clicked in Explorer, which is what started the app.
            if (!options.FilesToImport.empty())
            {
                ImportPatchFiles(options.FilesToImport);
            }
            else if (!options.PatchName.empty())
            {
                for (auto const* patch : library.Patches())
                {
                    if (SameText(patch->Name, options.PatchName))
                    {
                        OpenPatch(patch->SessionKey);
                        break;
                    }
                }
            }

            m_refreshTimer = xaml::DispatcherTimer{};
            m_refreshTimer.Interval(std::chrono::milliseconds{ RefreshIntervalMilliseconds });
            m_refreshTimer.Tick([weak](auto&&, auto&&)
                {
                    if (auto strong = weak.get())
                    {
                        strong->OnRefreshTimerTick();
                    }
                });
            m_refreshTimer.Start();

            // Closing is turned into a hide when the customer has asked the app to keep running.
            // Otherwise closing the library closes the app, which asks first if that stops a route.
            m_closingToken = AppWindow().Closing(
                [weak](auto&&, windowing::AppWindowClosingEventArgs const& args)
                {
                    auto strong = weak.get();

                    if (strong == nullptr)
                    {
                        return;
                    }

                    if (strong->TryHideToNotificationArea())
                    {
                        args.Cancel(true);
                        return;
                    }

                    if (strong->m_exiting)
                    {
                        return;
                    }

                    args.Cancel(true);
                    strong->ConfirmExitAsync();
                });

            // Minimizing goes the same way, so the app is in one place rather than two.
            m_windowChangedToken = AppWindow().Changed(
                [weak](windowing::AppWindow const& sender, windowing::AppWindowChangedEventArgs const& args)
                {
                    if (!args.DidPresenterChange() && !args.DidVisibilityChange())
                    {
                        return;
                    }

                    if (auto strong = weak.get())
                    {
                        if (auto const presenter = sender.Presenter().try_as<windowing::OverlappedPresenter>())
                        {
                            if (presenter.State() == windowing::OverlappedPresenterState::Minimized)
                            {
                                // the live state as well, so a change raised on the way back out
                                // of the notification area cannot hide the window again
                                if (::IsIconic(strong->m_chrome.WindowHandle()))
                                {
                                    strong->TryHideToNotificationArea();
                                }
                            }
                        }
                    }
                });

            m_closedToken = this->Closed([weak](auto&&, auto&&)
                {
                    auto strong = weak.get();

                    if (strong == nullptr)
                    {
                        return;
                    }

                    strong->m_closing = true;

                    if (strong->m_refreshTimer != nullptr)
                    {
                        strong->m_refreshTimer.Stop();
                    }

                    // A window hidden in the notification area reports no useful placement, and
                    // the one from just before it was hidden is already saved.
                    if (::IsWindowVisible(strong->m_chrome.WindowHandle()))
                    {
                        strong->m_chrome.SavePlacement();
                    }

                    strong->m_chrome.Shutdown();

                    auto& closingLibrary = patchbay::PatchLibrary::Current();

                    closingLibrary.Unsubscribe(strong->m_libraryToken);
                    strong->m_libraryToken = 0;

                    // Changes still waiting for the quiet moment are written now, because that
                    // moment is not coming.
                    for (auto const* patch : closingLibrary.Patches())
                    {
                        if (!patch->FilePath.empty() && closingLibrary.IsUnsaved(patch->SessionKey))
                        {
                            closingLibrary.Save(patch->SessionKey);
                        }
                    }

                    patchbay::EndpointCatalog::Current().SetChangedHandler(nullptr);
                    patchbay::EndpointCatalog::Current().Stop();

                    // The engine holds service connections, so it is torn down off the UI thread
                    // and the window waits for nothing.
                    std::thread([]() { patchbay::RouteEngine::Current().Shutdown(); }).detach();
                });

            // The catalog is shared with the other MIDI tools, so it cannot reach this app's
            // telemetry by itself. Give it the same sink everything else here logs to.
            midiapp::SetEndpointErrorHandler([](std::wstring_view message)
                {
                    MIDI_PATCHBAY_LOG_GENERAL_EXCEPTION(std::wstring{ message }.c_str());
                });

            // Starting the watcher blocks on the service, so it never happens on this thread.
            patchbay::RunOnBackgroundAsync([]()
                {
                    patchbay::EndpointCatalog::Current().Start();
                });

            InitializeNotificationArea();
        }
        MIDI_PATCHBAY_CATCH_AND_LOG(L"Unable to finish loading the library.")
    }

    void LibraryWindow::InitializeWindowChrome() noexcept
    {
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

            m_chrome.Initialize(elements, patchbay::AppSettings::Current());

            // Now that there is a window, a later launch has something to bring forward.
            ::midiapp::SingleInstance::PublishMainWindow(m_chrome.WindowHandle());

            // And something to hand a double-clicked patch to.
            s_instance = get_weak();
            ::SetWindowSubclass(m_chrome.WindowHandle(), &LibraryWindow::HandoffSubclassProcedure, 1, 0);

            m_chrome.SetWindowIconFromResource(IDI_APPICON);

            Title(resources::GetString(L"AppDisplayName"));
            AppTitleTextBlock().Text(resources::GetString(L"AppDisplayName"));

            // 32px source for a 16px slot, so it stays crisp on a high DPI display
            if (auto const icon = midiapp::WindowChrome::LoadIconImageSource(IDI_APPICON, 32))
            {
                AppTitleBarIcon().Source(icon);
            }

            AlwaysOnTopToggle().IsChecked(patchbay::AppSettings::Current().AlwaysOnTop());
        }
        MIDI_PATCHBAY_CATCH_AND_LOG(L"Unable to set up the window chrome.")
    }

    void LibraryWindow::ApplyAssistantVisibility() noexcept
    {
        try
        {
            AssistantButton().Visibility(patchbay::AppSettings::Current().ShowAssistant()
                ? xaml::Visibility::Visible
                : xaml::Visibility::Collapsed);
        }
        MIDI_PATCHBAY_CATCH_AND_LOG(L"Unable to show or hide Ask an AI assistant.")
    }

    _Use_decl_annotations_
    void LibraryWindow::OnAppearanceButtonClick(foundation::IInspectable const& sender, xaml::RoutedEventArgs const& args)
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

            controls::InfoBar note{};
            note.Severity(controls::InfoBarSeverity::Informational);
            note.IsOpen(true);
            note.IsClosable(false);
            note.Message(resources::GetString(L"AppearanceRoutingNote"));

            auto weak = get_weak();

            midiapp::ShowAppearanceFlyout(
                AppearanceButton(),
                patchbay::AppSettings::Current(),
                strings,
                [weak]()
                {
                    if (auto strong = weak.get())
                    {
                        strong->m_chrome.ApplyTheme();
                        strong->RebuildTiles();

                        App::ApplyAppearanceToEditors();
                    }
                },
                BuildAppSettingsPanel(),
                note);
        }
        MIDI_PATCHBAY_CATCH_AND_LOG(L"Unable to show the appearance flyout.")
    }

    xaml::UIElement LibraryWindow::BuildAppSettingsPanel() noexcept
    {
        try
        {
            controls::StackPanel panel{};
            panel.Spacing(10);
            panel.Margin(xaml::ThicknessHelper::FromLengths(0, 12, 0, 0));

            controls::TextBlock heading{};
            heading.Text(resources::GetString(L"SettingsSectionHeading"));
            heading.FontSize(12);
            heading.Foreground(Brush(L"TextFillColorTertiaryBrush"));
            panel.Children().Append(heading);

            auto weak = get_weak();

            auto const addToggle = [&panel](winrt::hstring const& header, winrt::hstring const& description,
                bool isOn, std::function<void(bool)> onChanged)
                {
                    controls::ToggleSwitch toggle{};

                    toggle.Header(winrt::box_value(header));
                    toggle.IsOn(isOn);
                    toggle.OnContent(winrt::box_value(winrt::hstring{ L"" }));
                    toggle.OffContent(winrt::box_value(winrt::hstring{ L"" }));

                    toggle.Toggled([onChanged](foundation::IInspectable const& s, xaml::RoutedEventArgs const&)
                        {
                            if (auto const control = s.try_as<controls::ToggleSwitch>())
                            {
                                onChanged(control.IsOn());
                            }
                        });

                    panel.Children().Append(toggle);

                    if (!description.empty())
                    {
                        controls::TextBlock hint{};
                        hint.Text(description);
                        hint.FontSize(11);
                        hint.TextWrapping(xaml::TextWrapping::Wrap);
                        hint.Margin(xaml::ThicknessHelper::FromLengths(0, -6, 0, 0));
                        hint.Foreground(Brush(L"TextFillColorTertiaryBrush"));
                        panel.Children().Append(hint);
                    }
                };

            addToggle(
                resources::GetString(L"SettingStartWithWindows"),
                resources::GetString(L"SettingStartWithWindowsHint"),
                patchbay::AppSettings::StartsWithWindows(),
                [weak](bool value)
                {
                    if (!patchbay::AppSettings::TrySetStartsWithWindows(value))
                    {
                        if (auto strong = weak.get())
                        {
                            strong->ShowStatus(
                                resources::GetString(L"ErrorStartupEntry"),
                                controls::InfoBarSeverity::Warning);
                        }
                    }
                });

            addToggle(
                resources::GetString(L"SettingNotificationArea"),
                resources::GetString(L"SettingNotificationAreaHint"),
                patchbay::AppSettings::Current().MinimizeToNotificationArea(),
                [weak](bool value)
                {
                    patchbay::AppSettings::Current().MinimizeToNotificationArea(value);

                    if (auto strong = weak.get())
                    {
                        if (value)
                        {
                            strong->m_tray.Show();
                            strong->UpdateTray();
                        }
                        else
                        {
                            strong->m_tray.Hide();
                        }
                    }
                });

            addToggle(
                resources::GetString(L"SettingStartMinimized"),
                {},
                patchbay::AppSettings::Current().StartMinimized(),
                [](bool value) { patchbay::AppSettings::Current().StartMinimized(value); });

            addToggle(
                resources::GetString(L"SettingActivateAtStartup"),
                {},
                patchbay::AppSettings::Current().ActivateSavedPatchesAtStartup(),
                [](bool value) { patchbay::AppSettings::Current().ActivateSavedPatchesAtStartup(value); });

            addToggle(
                resources::GetString(L"SettingWarnAboutLoops"),
                {},
                patchbay::AppSettings::Current().WarnAboutLoops(),
                [](bool value) { patchbay::AppSettings::Current().WarnAboutLoops(value); });

            addToggle(
                resources::GetString(L"SettingAssistant"),
                resources::GetString(L"SettingAssistantHint"),
                patchbay::AppSettings::Current().ShowAssistant(),
                [weak](bool value)
                {
                    patchbay::AppSettings::Current().ShowAssistant(value);

                    if (auto strong = weak.get())
                    {
                        strong->ApplyAssistantVisibility();
                    }
                });

            return panel;
        }
        MIDI_PATCHBAY_CATCH_AND_LOG(L"Unable to build the app settings panel.")

        return nullptr;
    }

    _Use_decl_annotations_
    void LibraryWindow::OnAlwaysOnTopToggled(foundation::IInspectable const& sender, xaml::RoutedEventArgs const& args)
    {
        UNREFERENCED_PARAMETER(sender);
        UNREFERENCED_PARAMETER(args);

        try
        {
            auto const isChecked = AlwaysOnTopToggle().IsChecked();

            patchbay::AppSettings::Current().AlwaysOnTop(isChecked && isChecked.Value());
            m_chrome.ApplyAlwaysOnTop();

            App::ApplyAppearanceToEditors();
        }
        MIDI_PATCHBAY_CATCH_AND_LOG(L"Unable to change the always on top setting.")
    }

    // ------------------------------------------------------- notification area

    void LibraryWindow::InitializeNotificationArea() noexcept
    {
        try
        {
            auto weak = get_weak();

            // The handlers are wired up whether or not the icon is showing, because the setting
            // can be turned on later and an icon whose menu does nothing is worse than no icon.
            m_tray.SetOpenHandler([weak]()
                {
                    if (auto strong = weak.get())
                    {
                        strong->RestoreFromNotificationArea();
                    }
                });

            m_tray.SetExitHandler([weak]()
                {
                    if (auto strong = weak.get())
                    {
                        strong->ExitApp();
                    }
                });

            m_tray.SetStopAllHandler([]()
                {
                    patchbay::PatchLibrary::Current().StopAllRouting();
                });

            m_tray.SetTogglePatchHandler([](std::wstring const& key)
                {
                    auto& library = patchbay::PatchLibrary::Current();
                    library.SetRouting(key, !library.IsRouting(key));
                });

            if (patchbay::AppSettings::Current().MinimizeToNotificationArea())
            {
                m_tray.Show();
                UpdateTray();

                // a start minimized launch happened before there was an icon to hide behind
                if (auto const handle = m_chrome.WindowHandle())
                {
                    if (::IsIconic(handle))
                    {
                        TryHideToNotificationArea();
                    }
                }
            }
        }
        MIDI_PATCHBAY_CATCH_AND_LOG(L"Unable to set up the notification area.")
    }

    bool LibraryWindow::TryHideToNotificationArea() noexcept
    {
        try
        {
            if (m_exiting || m_closing || m_restoringFromNotificationArea)
            {
                return false;
            }

            // IsVisible rather than the setting, so a machine where the icon could not be added
            // never hides the only way back to the app.
            if (!m_tray.IsVisible())
            {
                return false;
            }

            auto const handle = m_chrome.WindowHandle();

            if (handle == nullptr)
            {
                return false;
            }

            auto const minimized = ::IsIconic(handle) != FALSE;

            // already hidden, so there is nothing to do and nothing to cancel for
            if (!::IsWindowVisible(handle))
            {
                return false;
            }

            // a minimized window has no placement worth keeping, and the last good one is saved
            if (!minimized)
            {
                m_chrome.SavePlacement();
            }

            ::ShowWindow(handle, SW_HIDE);

            return true;
        }
        MIDI_PATCHBAY_CATCH_AND_LOG(L"Unable to hide the window to the notification area.")

        return false;
    }

    void LibraryWindow::RestoreFromNotificationArea() noexcept
    {
        try
        {
            auto const handle = m_chrome.WindowHandle();

            if (handle == nullptr)
            {
                return;
            }

            m_restoringFromNotificationArea = true;

            auto const reset = wil::scope_exit([this]() { m_restoringFromNotificationArea = false; });

            // A window hidden while minimized needs both steps, and the flag above is what makes
            // that safe: it is briefly visible and still minimized in between. Showing a window
            // hidden while maximized keeps it maximized, so there is no restore in that case.
            ::ShowWindow(handle, SW_SHOW);

            if (::IsIconic(handle))
            {
                ::ShowWindow(handle, SW_RESTORE);
            }

            ::SetForegroundWindow(handle);
        }
        MIDI_PATCHBAY_CATCH_AND_LOG(L"Unable to show the window from the notification area.")
    }

    void LibraryWindow::UpdateTray() noexcept
    {
        try
        {
            if (!m_tray.IsVisible())
            {
                return;
            }

            auto& library = patchbay::PatchLibrary::Current();

            std::vector<patchbay::TrayPatchItem> items{};

            for (auto const* patch : library.Patches())
            {
                patchbay::TrayPatchItem item{};

                item.PatchId = patch->SessionKey;
                item.Name = patch->Name;
                item.IsRouting = library.IsRouting(patch->SessionKey);
                item.HasWarning = library.HasMissingEndpoint(patch->SessionKey);

                if (item.HasWarning)
                {
                    item.Detail = std::wstring{ resources::GetString(L"TrayWaiting") };
                }
                else if (!item.IsRouting)
                {
                    item.Detail = std::wstring{ resources::GetString(L"TrayOff") };
                }

                items.push_back(std::move(item));
            }

            m_tray.Update(
                std::wstring{ resources::FormatString(L"TrayTooltipFormat", library.RoutingCount()) },
                std::move(items));
        }
        MIDI_PATCHBAY_CATCH_AND_LOG(L"Unable to update the notification area.")
    }

    winrt::fire_and_forget LibraryWindow::ConfirmExitAsync()
    {
        auto strong = get_strong();

        try
        {
            auto const routing = patchbay::PatchLibrary::Current().RoutingCount();

            if (routing > 0)
            {
                ConfirmExitText().Text(routing == 1
                    ? resources::GetString(L"ConfirmExitMessageOne")
                    : resources::FormatString(L"ConfirmExitMessageFormat", routing));

                ConfirmExitDialog().XamlRoot(Content().XamlRoot());

                auto const result = co_await ConfirmExitDialog().ShowAsync();

                if (result != controls::ContentDialogResult::Primary)
                {
                    co_return;
                }
            }

            ExitApp();
        }
        MIDI_PATCHBAY_CATCH_AND_LOG(L"Unable to close the app.")
    }

    void LibraryWindow::ExitApp() noexcept
    {
        try
        {
            m_exiting = true;

            App::CloseAllEditors();

            Close();
        }
        MIDI_PATCHBAY_CATCH_AND_LOG(L"Unable to close the app.")
    }

    // ---------------------------------------------------------------- importing

    _Use_decl_annotations_
    void LibraryWindow::OnImportPatchClick(foundation::IInspectable const& sender, xaml::RoutedEventArgs const& args)
    {
        UNREFERENCED_PARAMETER(sender);
        UNREFERENCED_PARAMETER(args);

        try
        {
            // The Win32 common item dialog, never Windows.Storage.Pickers, which needs a package
            // identity this unpackaged app does not have.
            auto dialog = wil::CoCreateInstance<IFileOpenDialog>(CLSID_FileOpenDialog);

            auto const filterName = resources::GetString(L"ImportPatchFilter");

            COMDLG_FILTERSPEC const filters[]
            {
                { filterName.c_str(), L"*.midipatch" },
            };

            dialog->SetFileTypes(ARRAYSIZE(filters), filters);
            dialog->SetTitle(resources::GetString(L"ImportPatchTitle").c_str());

            if (FAILED(dialog->Show(m_chrome.WindowHandle())))
            {
                return;
            }

            winrt::com_ptr<IShellItem> item{};

            if (FAILED(dialog->GetResult(item.put())) || item == nullptr)
            {
                return;
            }

            wil::unique_cotaskmem_string path{};

            if (FAILED(item->GetDisplayName(SIGDN_FILESYSPATH, &path)) || path.get() == nullptr)
            {
                return;
            }

            ImportPatchFiles({ std::wstring{ path.get() } });
        }
        MIDI_PATCHBAY_CATCH_AND_LOG(L"Unable to import a patch.")
    }

    _Use_decl_annotations_
    bool LibraryWindow::ImportPatchFiles(std::vector<std::wstring> const& paths) noexcept
    {
        try
        {
            auto& store = patchbay::PatchStore::Current();
            auto& library = patchbay::PatchLibrary::Current();

            std::wstring openKey{};
            std::wstring openName{};

            for (auto const& path : paths)
            {
                auto imported = store.Import(path);

                if (!imported.has_value())
                {
                    ShowStatus(store.LastErrorMessage(), controls::InfoBarSeverity::Error);
                    continue;
                }

                // A file that was in the folder all along is here already.
                patchbay::PatchDocument const* existing{ nullptr };

                for (auto const* patch : library.Patches())
                {
                    if (!patch->FilePath.empty() && SameText(patch->FilePath, imported->FilePath))
                    {
                        existing = patch;
                        break;
                    }
                }

                if (existing != nullptr)
                {
                    openKey = existing->SessionKey;
                    openName = existing->Name;
                    continue;
                }

                // Never routed on the way in. Somebody else wrote this file, and the customer
                // turns it on once they have looked at it.
                auto const* added = library.Add(std::move(imported.value()), false);

                if (added == nullptr)
                {
                    ShowStatus(library.LastErrorMessage(), controls::InfoBarSeverity::Error);
                    continue;
                }

                openKey = added->SessionKey;
                openName = added->Name;
            }

            if (openKey.empty())
            {
                return false;
            }

            OpenPatch(openKey);

            ShowStatus(resources::FormatString(L"StatusPatchImportedFormat", openName),
                controls::InfoBarSeverity::Success);

            return true;
        }
        MIDI_PATCHBAY_CATCH_AND_LOG(L"Unable to import the patches.")

        return false;
    }

    // -------------------------------------------------------------------- tiles

    _Use_decl_annotations_
    void LibraryWindow::OnLibraryChanged(patchbay::LibraryChange change, std::wstring const& key) noexcept
    {
        UNREFERENCED_PARAMETER(key);

        try
        {
            if (m_closing)
            {
                return;
            }

            switch (change)
            {
            case patchbay::LibraryChange::Routing:
            {
                // Only a filter by routing changes which tiles show.
                auto const filter = LibraryFilterBar().SelectedItem();

                if (filter == FilterRoutingItem() || filter == FilterStoppedItem())
                {
                    RebuildTiles();
                }
                else
                {
                    RefreshTileStates();
                }

                UpdateTray();
                UpdateStatusStrip();
                break;
            }

            case patchbay::LibraryChange::Activity:
                break;

            default:
                RebuildTiles();
                UpdateTray();
                UpdateStatusStrip();
                break;
            }
        }
        MIDI_PATCHBAY_CATCH_AND_LOG(L"Unable to catch up with a change to the patches.")
    }

    _Use_decl_annotations_
    bool LibraryWindow::NeedsAttention(patchbay::PatchDocument const& patch) noexcept
    {
        auto& library = patchbay::PatchLibrary::Current();

        return library.HasMissingEndpoint(patch.SessionKey) ||
            library.Problem(patch.SessionKey).has_value() ||
            library.Analysis(patch.SessionKey).HasCertainLoop();
    }

    _Use_decl_annotations_
    winrt::hstring LibraryWindow::TileStateText(patchbay::PatchDocument const& patch) noexcept
    {
        try
        {
            auto& library = patchbay::PatchLibrary::Current();

            winrt::hstring state{};

            if (library.Problem(patch.SessionKey).has_value())
            {
                state = resources::GetString(L"TileStateProblem");
            }
            else if (library.HasMissingEndpoint(patch.SessionKey))
            {
                state = resources::GetString(L"TileStateWaiting");
            }
            else
            {
                state = library.IsRouting(patch.SessionKey)
                    ? resources::GetString(L"ChipRouting")
                    : resources::GetString(L"ChipNotRouting");
            }

            if (patch.FilePath.empty())
            {
                state = resources::FormatString(L"TileStateTemporaryFormat", state);
            }

            return state;
        }
        MIDI_PATCHBAY_CATCH_AND_LOG(L"Unable to describe a patch.")

        return {};
    }

    void LibraryWindow::RebuildTiles() noexcept
    {
        try
        {
            if (!m_loaded || m_closing)
            {
                return;
            }

            auto& library = patchbay::PatchLibrary::Current();
            auto const patches = library.Patches();

            auto const search = std::wstring{ SearchBox().Text() };
            auto const filter = LibraryFilterBar().SelectedItem();

            std::vector<patchbay::PatchDocument const*> shown{};

            for (auto const* patch : patches)
            {
                auto const routing = library.IsRouting(patch->SessionKey);

                if ((filter == FilterRoutingItem() && !routing) ||
                    (filter == FilterStoppedItem() && routing) ||
                    (filter == FilterAttentionItem() && !NeedsAttention(*patch)))
                {
                    continue;
                }

                if (!search.empty())
                {
                    auto matches = ContainsText(patch->Name, search) || ContainsText(patch->Description, search);

                    for (auto const& endpoint : patch->Endpoints)
                    {
                        matches = matches || ContainsText(endpoint.DisplayName, search);
                    }

                    if (!matches)
                    {
                        continue;
                    }
                }

                shown.push_back(patch);
            }

            if (patchbay::AppSettings::Current().SortOrder() == patchbay::PatchSortOrder::Name)
            {
                std::sort(shown.begin(), shown.end(),
                    [](patchbay::PatchDocument const* a, patchbay::PatchDocument const* b)
                    {
                        return ::CompareStringOrdinal(a->Name.c_str(), -1, b->Name.c_str(), -1, TRUE) == CSTR_LESS_THAN;
                    });
            }
            else
            {
                // A patch that was never saved has no time yet, so it sorts first, where the
                // customer just made it.
                std::stable_sort(shown.begin(), shown.end(),
                    [](patchbay::PatchDocument const* a, patchbay::PatchDocument const* b)
                    {
                        if (a->FilePath.empty() != b->FilePath.empty())
                        {
                            return a->FilePath.empty();
                        }

                        return a->ModifiedTimestamp > b->ModifiedTimestamp;
                    });
            }

            m_updatingTiles = true;
            auto const reset = wil::scope_exit([this]() { m_updatingTiles = false; });

            m_tiles.clear();
            PatchGrid().Items().Clear();

            for (auto const* patch : shown)
            {
                if (auto const tile = BuildTile(*patch))
                {
                    PatchGrid().Items().Append(tile);
                }
            }

            EmptyPanel().Visibility(patches.empty() ? xaml::Visibility::Visible : xaml::Visibility::Collapsed);
            NoMatchesText().Visibility(!patches.empty() && shown.empty()
                ? xaml::Visibility::Visible
                : xaml::Visibility::Collapsed);
        }
        MIDI_PATCHBAY_CATCH_AND_LOG(L"Unable to show the patches.")
    }

    void LibraryWindow::RefreshTileStates() noexcept
    {
        try
        {
            auto& library = patchbay::PatchLibrary::Current();

            m_updatingTiles = true;
            auto const reset = wil::scope_exit([this]() { m_updatingTiles = false; });

            for (auto const& tile : m_tiles)
            {
                auto const* patch = library.Find(tile.Key);

                if (patch == nullptr)
                {
                    continue;
                }

                if (tile.Routing != nullptr)
                {
                    tile.Routing.IsOn(library.IsRouting(tile.Key));
                }

                if (tile.State != nullptr)
                {
                    tile.State.Text(TileStateText(*patch));
                }
            }
        }
        MIDI_PATCHBAY_CATCH_AND_LOG(L"Unable to refresh the patch tiles.")
    }

    _Use_decl_annotations_
    xaml::UIElement LibraryWindow::BuildTile(patchbay::PatchDocument const& patch) noexcept
    {
        try
        {
            auto& library = patchbay::PatchLibrary::Current();
            auto const key = patch.SessionKey;
            auto const name = winrt::hstring{ patch.Name };
            auto weak = get_weak();

            controls::Grid tile{};

            tile.Width(TileWidth);
            tile.Height(TileHeight);
            tile.Padding(xaml::ThicknessHelper::FromLengths(14, 12, 14, 12));
            tile.RowSpacing(6);
            tile.CornerRadius(xaml::CornerRadiusHelper::FromUniformRadius(8));
            tile.BorderThickness(xaml::ThicknessHelper::FromUniformLength(1));
            tile.BorderBrush(Brush(L"CardStrokeColorDefaultBrush"));
            tile.Background(Brush(L"CardBackgroundFillColorDefaultBrush"));
            tile.Tag(winrt::box_value(winrt::hstring{ key }));

            for (auto const height : { xaml::GridLength{ 0, xaml::GridUnitType::Auto },
                                       xaml::GridLength{ 0, xaml::GridUnitType::Auto },
                                       xaml::GridLength{ 1, xaml::GridUnitType::Star },
                                       xaml::GridLength{ 0, xaml::GridUnitType::Auto },
                                       xaml::GridLength{ 0, xaml::GridUnitType::Auto } })
            {
                controls::RowDefinition row{};
                row.Height(height);
                tile.RowDefinitions().Append(row);
            }

            // ---- name and the routing switch
            controls::Grid header{};
            header.ColumnSpacing(8);

            for (auto const width : { xaml::GridLength{ 1, xaml::GridUnitType::Star },
                                      xaml::GridLength{ 0, xaml::GridUnitType::Auto } })
            {
                controls::ColumnDefinition column{};
                column.Width(width);
                header.ColumnDefinitions().Append(column);
            }

            controls::TextBlock title{};
            title.Text(name);
            title.FontSize(15);
            title.FontWeight(winrt::Microsoft::UI::Text::FontWeights::SemiBold());
            title.TextTrimming(xaml::TextTrimming::CharacterEllipsis);
            title.VerticalAlignment(xaml::VerticalAlignment::Center);
            header.Children().Append(title);

            controls::ToggleSwitch routing{};
            routing.IsOn(library.IsRouting(key));
            routing.MinWidth(0);
            routing.OnContent(winrt::box_value(winrt::hstring{}));
            routing.OffContent(winrt::box_value(winrt::hstring{}));
            routing.VerticalAlignment(xaml::VerticalAlignment::Center);

            xaml::Automation::AutomationProperties::SetName(routing,
                resources::FormatString(L"TileRoutingFormat", patch.Name));

            routing.Toggled([weak, key](foundation::IInspectable const& sender, auto&&)
                {
                    auto strong = weak.get();
                    auto const toggle = sender.try_as<controls::ToggleSwitch>();

                    if (strong == nullptr || toggle == nullptr || strong->m_updatingTiles)
                    {
                        return;
                    }

                    patchbay::PatchLibrary::Current().SetRouting(key, toggle.IsOn());
                });

            controls::Grid::SetColumn(routing, 1);
            header.Children().Append(routing);

            tile.Children().Append(header);

            // ---- description
            controls::TextBlock description{};
            description.Text(winrt::hstring{ patch.Description.empty()
                ? std::wstring{ resources::GetString(L"TileNoDescription") }
                : patch.Description });
            description.FontSize(12);
            description.TextTrimming(xaml::TextTrimming::CharacterEllipsis);
            description.Foreground(Brush(L"TextFillColorTertiaryBrush"));
            controls::Grid::SetRow(description, 1);
            tile.Children().Append(description);

            // ---- a sketch of the canvas
            controls::Border map{};
            map.CornerRadius(xaml::CornerRadiusHelper::FromUniformRadius(6));
            map.Background(Brush(L"SolidBackgroundFillColorTertiaryBrush"));
            map.Child(BuildMiniMap(patch));
            controls::Grid::SetRow(map, 2);
            tile.Children().Append(map);

            // ---- what it connects
            controls::StackPanel devices{};
            devices.Orientation(controls::Orientation::Horizontal);
            devices.Spacing(6);

            constexpr size_t chipCount = 2;
            size_t chips{ 0 };

            for (auto const& endpoint : patch.Endpoints)
            {
                if (chips == chipCount)
                {
                    break;
                }

                auto const present = patchbay::ResolveEndpoint(endpoint).has_value();

                controls::Border chip{};
                chip.CornerRadius(xaml::CornerRadiusHelper::FromUniformRadius(9));
                chip.Padding(xaml::ThicknessHelper::FromLengths(8, 1, 8, 2));
                chip.BorderThickness(xaml::ThicknessHelper::FromUniformLength(1));
                chip.BorderBrush(Brush(present ? L"CardStrokeColorDefaultBrush" : L"SystemFillColorCautionBrush"));
                chip.MaxWidth(116);

                controls::TextBlock text{};
                text.Text(winrt::hstring{ endpoint.DisplayName });
                text.FontSize(11);
                text.TextTrimming(xaml::TextTrimming::CharacterEllipsis);
                text.Foreground(Brush(present ? L"TextFillColorSecondaryBrush" : L"SystemFillColorCautionBrush"));
                chip.Child(text);

                devices.Children().Append(chip);
                chips++;
            }

            if (patch.Endpoints.size() > chips)
            {
                controls::TextBlock more{};
                more.Text(resources::FormatString(L"TileMoreEndpointsFormat", patch.Endpoints.size() - chips));
                more.FontSize(11);
                more.VerticalAlignment(xaml::VerticalAlignment::Center);
                more.Foreground(Brush(L"TextFillColorTertiaryBrush"));
                devices.Children().Append(more);
            }

            controls::Grid::SetRow(devices, 3);
            tile.Children().Append(devices);

            // ---- state and size
            controls::Grid footer{};

            for (auto const width : { xaml::GridLength{ 1, xaml::GridUnitType::Star },
                                      xaml::GridLength{ 0, xaml::GridUnitType::Auto } })
            {
                controls::ColumnDefinition column{};
                column.Width(width);
                footer.ColumnDefinitions().Append(column);
            }

            controls::TextBlock state{};
            state.Text(TileStateText(patch));
            state.FontSize(12);
            state.TextTrimming(xaml::TextTrimming::CharacterEllipsis);
            state.Foreground(Brush(NeedsAttention(patch) ? L"SystemFillColorCautionBrush" : L"TextFillColorSecondaryBrush"));
            footer.Children().Append(state);

            controls::TextBlock size{};
            size.Text(resources::FormatString(L"TileSizeFormat", patch.StepCount(), patch.Connections.size()));
            size.FontSize(12);
            size.Foreground(Brush(L"TextFillColorTertiaryBrush"));
            controls::Grid::SetColumn(size, 1);
            footer.Children().Append(size);

            controls::Grid::SetRow(footer, 4);
            tile.Children().Append(footer);

            tile.ContextFlyout(BuildTileMenu(key));

            if (!patch.Description.empty())
            {
                controls::ToolTipService::SetToolTip(tile, winrt::box_value(winrt::hstring{ patch.Description }));
            }

            xaml::Automation::AutomationProperties::SetName(tile,
                resources::FormatString(L"TileAccessibleNameFormat", patch.Name, TileStateText(patch)));

            m_tiles.push_back(TileParts{ key, routing, state });

            return tile;
        }
        MIDI_PATCHBAY_CATCH_AND_LOG(L"Unable to build a patch tile.")

        return nullptr;
    }

    _Use_decl_annotations_
    xaml::UIElement LibraryWindow::BuildMiniMap(patchbay::PatchDocument const& patch) noexcept
    {
        controls::Canvas canvas{};

        try
        {
            canvas.Width(MiniMapWidth);
            canvas.Height(MiniMapHeight);
            canvas.IsHitTestVisible(false);

            if (patch.Endpoints.empty() && patch.Blocks.empty())
            {
                return canvas;
            }

            struct Box
            {
                std::wstring Id{};
                double X{ 0 };
                double Y{ 0 };
                double Width{ 0 };
                double Height{ 0 };
                std::optional<patchbay::BlockCategory> Category{};
            };

            std::vector<Box> boxes{};

            for (auto const& endpoint : patch.Endpoints)
            {
                auto const size = patchbay::EstimatedNodeSize(patch, endpoint.Id);
                boxes.push_back(Box{ endpoint.Id, endpoint.CanvasX, endpoint.CanvasY, size.Width, size.Height, std::nullopt });
            }

            for (auto const& block : patch.Blocks)
            {
                auto const size = patchbay::EstimatedNodeSize(patch, block.Id);
                boxes.push_back(Box{ block.Id, block.CanvasX, block.CanvasY, size.Width, size.Height, patchbay::CategoryOf(block.Kind) });
            }

            auto left = std::numeric_limits<double>::max();
            auto top = std::numeric_limits<double>::max();
            auto right = std::numeric_limits<double>::lowest();
            auto bottom = std::numeric_limits<double>::lowest();

            for (auto const& box : boxes)
            {
                left = (std::min)(left, box.X);
                top = (std::min)(top, box.Y);
                right = (std::max)(right, box.X + box.Width);
                bottom = (std::max)(bottom, box.Y + box.Height);
            }

            constexpr double inset = 6.0;

            auto const scale = (std::min)({
                (MiniMapWidth - 2 * inset) / (std::max)(right - left, 1.0),
                (MiniMapHeight - 2 * inset) / (std::max)(bottom - top, 1.0),
                MiniMapMaximumScale });

            auto const offsetX = (MiniMapWidth - (right - left) * scale) / 2 - left * scale;
            auto const offsetY = (MiniMapHeight - (bottom - top) * scale) / 2 - top * scale;

            auto const find = [&boxes](std::wstring const& id) -> Box const*
                {
                    auto const found = std::find_if(boxes.begin(), boxes.end(), [&id](Box const& b) { return b.Id == id; });
                    return found == boxes.end() ? nullptr : &(*found);
                };

            auto const lineBrush = Brush(L"TextFillColorTertiaryBrush");

            // Out on the right of the source, in on the left of the destination, like the canvas.
            for (auto const& link : patch.Connections)
            {
                auto const* source = find(link.SourceId);
                auto const* destination = find(link.DestinationId);

                if (source == nullptr || destination == nullptr)
                {
                    continue;
                }

                shapes::Line line{};

                line.X1((source->X + source->Width) * scale + offsetX);
                line.Y1((source->Y + source->Height / 2) * scale + offsetY);
                line.X2(destination->X * scale + offsetX);
                line.Y2((destination->Y + destination->Height / 2) * scale + offsetY);
                line.StrokeThickness(1);
                line.Stroke(lineBrush);
                line.Opacity(link.Muted ? 0.3 : 0.8);

                canvas.Children().Append(line);
            }

            for (auto const& box : boxes)
            {
                shapes::Rectangle node{};

                node.Width((std::max)(box.Width * scale, 3.0));
                node.Height((std::max)(box.Height * scale, 3.0));
                node.RadiusX(2);
                node.RadiusY(2);

                if (box.Category.has_value())
                {
                    node.Fill(patchbay::PatchCanvas::CategoryBrush(box.Category.value(), 0.85));
                }
                else
                {
                    node.Fill(Brush(L"ControlAltFillColorSecondaryBrush"));
                    node.Stroke(Brush(L"TextFillColorTertiaryBrush"));
                    node.StrokeThickness(1);
                }

                controls::Canvas::SetLeft(node, box.X * scale + offsetX);
                controls::Canvas::SetTop(node, box.Y * scale + offsetY);

                canvas.Children().Append(node);
            }
        }
        MIDI_PATCHBAY_CATCH_AND_LOG(L"Unable to draw a patch map.")

        return canvas;
    }

    _Use_decl_annotations_
    controls::MenuFlyout LibraryWindow::BuildTileMenu(std::wstring const& key) noexcept
    {
        controls::MenuFlyout menu{};

        try
        {
            auto weak = get_weak();

            auto const addItem = [&menu, weak](winrt::hstring const& text, std::function<void(LibraryWindow&)> action)
                {
                    controls::MenuFlyoutItem item{};

                    item.Text(text);
                    item.Click([weak, action](auto&&, auto&&)
                        {
                            if (auto strong = weak.get())
                            {
                                action(*strong);
                            }
                        });

                    menu.Items().Append(item);
                };

            addItem(resources::GetString(L"TileMenuOpen"), [key](LibraryWindow& window) { window.OpenPatch(key); });

            // Worked out when the menu opens, so it reads right whatever happened since the tile
            // was built.
            controls::ToggleMenuFlyoutItem routingItem{};
            routingItem.Text(resources::GetString(L"MenuRouteThisPatch"));
            routingItem.Click([key](foundation::IInspectable const& sender, auto&&)
                {
                    if (auto const item = sender.try_as<controls::ToggleMenuFlyoutItem>())
                    {
                        patchbay::PatchLibrary::Current().SetRouting(key, item.IsChecked());
                    }
                });

            menu.Items().Append(routingItem);

            // Weak, because the item is in the menu and the menu keeps this handler.
            menu.Opening([key, item = winrt::make_weak(routingItem)](auto&&, auto&&)
                {
                    if (auto const routing = item.get())
                    {
                        routing.IsChecked(patchbay::PatchLibrary::Current().IsRouting(key));
                    }
                });

            menu.Items().Append(controls::MenuFlyoutSeparator{});

            addItem(resources::GetString(L"TileMenuDuplicate"), [key](LibraryWindow& window) { window.DuplicatePatch(key); });

            if (auto const* patch = patchbay::PatchLibrary::Current().Find(key); patch != nullptr && !patch->FilePath.empty())
            {
                auto const path = patch->FilePath;
                addItem(resources::GetString(L"TileMenuShowInFolder"), [path](LibraryWindow& window) { window.ShowInFolder(path); });
            }

            menu.Items().Append(controls::MenuFlyoutSeparator{});

            addItem(resources::GetString(L"MenuDeletePatch"), [key](LibraryWindow& window) { window.DeletePatchAsync(key); });
        }
        MIDI_PATCHBAY_CATCH_AND_LOG(L"Unable to build a patch menu.")

        return menu;
    }

    _Use_decl_annotations_
    void LibraryWindow::OpenPatch(std::wstring const& key) noexcept
    {
        App::OpenEditorWindow(key);
    }

    _Use_decl_annotations_
    void LibraryWindow::DuplicatePatch(std::wstring const& key) noexcept
    {
        try
        {
            auto& library = patchbay::PatchLibrary::Current();
            auto const* original = library.Find(key);

            if (original == nullptr)
            {
                return;
            }

            auto copy = *original;

            copy.Name = library.UniqueName(std::wstring{ resources::FormatString(L"PatchCopyNameFormat", original->Name) });
            copy.FilePath.clear();
            copy.CreatedTimestamp = 0;
            copy.ModifiedTimestamp = 0;
            copy.LoadedFileVersion = patchbay::CurrentPatchFileVersion;
            copy.ConversionIssues.clear();
            copy.EarlierVersionPath.clear();

            // A copy of a saved patch is saved too, so it is still there next time.
            auto const keepOnDisk = !original->FilePath.empty();
            copy.IsTemporary = !keepOnDisk;

            // Never routing: it would send everything the original sends a second time.
            auto const* added = library.Add(std::move(copy), false);

            if (added == nullptr)
            {
                ShowStatus(library.LastErrorMessage(), controls::InfoBarSeverity::Error);
                return;
            }

            auto const addedKey = added->SessionKey;
            auto const addedName = added->Name;

            if (keepOnDisk && !library.Save(addedKey))
            {
                ShowStatus(library.LastErrorMessage(), controls::InfoBarSeverity::Error);
                return;
            }

            ShowStatus(resources::FormatString(L"StatusPatchDuplicatedFormat", addedName),
                controls::InfoBarSeverity::Success);
        }
        MIDI_PATCHBAY_CATCH_AND_LOG(L"Unable to duplicate the patch.")
    }

    winrt::fire_and_forget LibraryWindow::DeletePatchAsync(std::wstring key)
    {
        auto strong = get_strong();

        try
        {
            auto& library = patchbay::PatchLibrary::Current();
            auto const* patch = library.Find(key);

            if (patch == nullptr)
            {
                co_return;
            }

            ConfirmDeleteText().Text(resources::FormatString(L"DeletePatchMessageFormat", patch->Name));
            ConfirmDeleteDialog().XamlRoot(Content().XamlRoot());

            auto const result = co_await ConfirmDeleteDialog().ShowAsync();

            if (result != controls::ContentDialogResult::Primary)
            {
                co_return;
            }

            // An editor showing the patch closes when the library says it is gone.
            if (!patchbay::PatchLibrary::Current().Remove(key))
            {
                ShowStatus(patchbay::PatchLibrary::Current().LastErrorMessage(), controls::InfoBarSeverity::Error);
            }
        }
        MIDI_PATCHBAY_CATCH_AND_LOG(L"Unable to delete the patch.")
    }

    _Use_decl_annotations_
    void LibraryWindow::ShowInFolder(std::wstring const& path) noexcept
    {
        try
        {
            // Explorer opens on the folder with the file picked out, rather than opening the file.
            wil::unique_any<PIDLIST_ABSOLUTE, decltype(&::ILFree), ::ILFree> item{ ::ILCreateFromPathW(path.c_str()) };

            if (item)
            {
                LOG_IF_FAILED(::SHOpenFolderAndSelectItems(item.get(), 0, nullptr, 0));
            }
        }
        MIDI_PATCHBAY_CATCH_AND_LOG(L"Unable to show the patch in its folder.")
    }

    // ------------------------------------------------------------ toolbar

    _Use_decl_annotations_
    void LibraryWindow::OnNewPatchClick(foundation::IInspectable const& sender, xaml::RoutedEventArgs const& args)
    {
        UNREFERENCED_PARAMETER(sender);
        UNREFERENCED_PARAMETER(args);

        try
        {
            auto& library = patchbay::PatchLibrary::Current();

            if (auto const* patch = library.Create())
            {
                OpenPatch(patch->SessionKey);
            }
            else
            {
                ShowStatus(library.LastErrorMessage(), controls::InfoBarSeverity::Error);
            }
        }
        MIDI_PATCHBAY_CATCH_AND_LOG(L"Unable to create a patch.")
    }

    _Use_decl_annotations_
    void LibraryWindow::OnNewQuickPatchClick(foundation::IInspectable const& sender, xaml::RoutedEventArgs const& args)
    {
        UNREFERENCED_PARAMETER(sender);
        UNREFERENCED_PARAMETER(args);

        ShowQuickPatchDialogAsync();
    }

    _Use_decl_annotations_
    void LibraryWindow::OnAssistantClick(foundation::IInspectable const& sender, xaml::RoutedEventArgs const& args)
    {
        UNREFERENCED_PARAMETER(sender);
        UNREFERENCED_PARAMETER(args);

        ShowAssistantDialogAsync();
    }

    _Use_decl_annotations_
    void LibraryWindow::OnOpenFolderClick(foundation::IInspectable const& sender, xaml::RoutedEventArgs const& args)
    {
        UNREFERENCED_PARAMETER(sender);
        UNREFERENCED_PARAMETER(args);

        patchbay::PatchStore::Current().ShowFolder();
    }

    _Use_decl_annotations_
    void LibraryWindow::OnSortClick(foundation::IInspectable const& sender, xaml::RoutedEventArgs const& args)
    {
        UNREFERENCED_PARAMETER(args);

        try
        {
            auto const anchor = sender.try_as<xaml::FrameworkElement>();

            if (anchor == nullptr)
            {
                return;
            }

            controls::MenuFlyout menu{};
            auto weak = get_weak();

            auto const addItem = [&menu, weak](winrt::hstring const& text, patchbay::PatchSortOrder order)
                {
                    controls::ToggleMenuFlyoutItem item{};

                    item.Text(text);
                    item.IsChecked(patchbay::AppSettings::Current().SortOrder() == order);

                    item.Click([weak, order](auto&&, auto&&)
                        {
                            if (auto strong = weak.get())
                            {
                                patchbay::AppSettings::Current().SortOrder(order);
                                strong->RebuildTiles();
                            }
                        });

                    menu.Items().Append(item);
                };

            addItem(resources::GetString(L"NavSortNewest"), patchbay::PatchSortOrder::Newest);
            addItem(resources::GetString(L"NavSortName"), patchbay::PatchSortOrder::Name);

            menu.ShowAt(anchor);
        }
        MIDI_PATCHBAY_CATCH_AND_LOG(L"Unable to show the sort menu.")
    }

    _Use_decl_annotations_
    void LibraryWindow::OnSearchChanged(foundation::IInspectable const& sender, controls::TextChangedEventArgs const& args)
    {
        UNREFERENCED_PARAMETER(sender);
        UNREFERENCED_PARAMETER(args);

        RebuildTiles();
    }

    _Use_decl_annotations_
    void LibraryWindow::OnFilterChanged(
        controls::SelectorBar const& sender,
        controls::SelectorBarSelectionChangedEventArgs const& args)
    {
        UNREFERENCED_PARAMETER(sender);
        UNREFERENCED_PARAMETER(args);

        RebuildTiles();
    }

    _Use_decl_annotations_
    void LibraryWindow::OnPatchTileClick(foundation::IInspectable const& sender, controls::ItemClickEventArgs const& args)
    {
        UNREFERENCED_PARAMETER(sender);

        try
        {
            auto const element = args.ClickedItem().try_as<xaml::FrameworkElement>();

            if (element == nullptr)
            {
                return;
            }

            auto const key = std::wstring{ winrt::unbox_value_or<winrt::hstring>(element.Tag(), L"") };

            if (!key.empty())
            {
                OpenPatch(key);
            }
        }
        MIDI_PATCHBAY_CATCH_AND_LOG(L"Unable to open the patch.")
    }

    // ------------------------------------------------------------------ status

    void LibraryWindow::OnRefreshTimerTick() noexcept
    {
        try
        {
            if (m_closing)
            {
                return;
            }

            auto& library = patchbay::PatchLibrary::Current();

            // Every window hears the new counts from the library.
            library.RefreshActivity();
            library.SaveDueChanges();

            UpdateStatusStrip();
            UpdateTray();

            ServiceBar().IsOpen(!patchbay::EndpointCatalog::Current().IsServiceAvailable());

            auto const error = patchbay::RouteEngine::Current().LastErrorMessage();

            if (error.empty())
            {
                m_lastEngineError = {};
            }
            else if (error != m_lastEngineError && !ServiceBar().IsOpen())
            {
                m_lastEngineError = error;
                ShowStatus(error, controls::InfoBarSeverity::Error);
            }
        }
        MIDI_PATCHBAY_CATCH_AND_LOG(L"The refresh timer failed.")
    }

    void LibraryWindow::UpdateStatusStrip() noexcept
    {
        try
        {
            auto& library = patchbay::PatchLibrary::Current();

            auto const count = library.Patches().size();
            auto const routing = library.RoutingCount();

            StatusPatchesText().Text(count == 1
                ? resources::GetString(L"StatusPatchesOne")
                : resources::FormatString(L"StatusPatchesFormat", count));

            StatusRoutingText().Text(resources::FormatString(L"StatusRoutingFormat", routing));

            auto const delivered = library.TotalDelivered();
            auto const now = std::chrono::steady_clock::now();

            if (m_lastRateSample.time_since_epoch().count() != 0)
            {
                auto const elapsed = std::chrono::duration<double>(now - m_lastRateSample).count();

                if (elapsed > 0.05 && delivered >= m_lastDelivered)
                {
                    auto const rate = static_cast<uint64_t>((delivered - m_lastDelivered) / elapsed);

                    StatusRateText().Text(resources::FormatString(L"StatusRateFormat", rate));
                }
            }

            m_lastRateSample = now;
            m_lastDelivered = delivered;
        }
        MIDI_PATCHBAY_CATCH_AND_LOG(L"Unable to update the status strip.")
    }

    _Use_decl_annotations_
    void LibraryWindow::ShowStatus(winrt::hstring const& message, controls::InfoBarSeverity severity) noexcept
    {
        try
        {
            if (message.empty())
            {
                StatusBar().IsOpen(false);
                return;
            }

            StatusBar().Severity(severity);
            StatusBar().Message(message);
            StatusBar().IsOpen(true);
        }
        MIDI_PATCHBAY_CATCH_AND_LOG(L"Unable to show a status message.")
    }
}
