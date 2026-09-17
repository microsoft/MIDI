// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#include "pch.h"
#include "MainWindow.xaml.h"
#if __has_include("MainWindow.g.cpp")
#include "MainWindow.g.cpp"
#endif

#include "App.xaml.h"
#include "BackgroundWork.h"
#include "InstanceHandoff.h"
#include "StringResources.h"
#include "resource.h"

#include <commctrl.h>

#pragma comment(lib, "comctl32.lib")

namespace native = ::midiplayer;
namespace res = ::midiplayer::resources;

namespace winrt::midiplayer::implementation
{
    winrt::weak_ref<winrt::midiplayer::MainWindow> MainWindow::s_instance{ nullptr };

    namespace
    {
        constexpr int32_t DefaultWindowWidth = 720;
        constexpr int32_t DefaultWindowHeight = 720;

        constexpr UINT_PTR SubclassId = 1;

        // Which output a first run should pick. Once the customer has played something the saved
        // choice wins, so this only decides what happens before that.
        //
        // The built-in synthesizer knows its own endpoint, so that is asked for first. The name
        // match is only there for a PC old enough to still be relying on the legacy wavetable
        // synth, where there is nothing else to go on.
        bool IsPreferredFirstRunEndpoint(midi2enum::MidiEndpointDeviceInformation const& device) noexcept
        {
            try
            {
                if (midi2synth::MidiSynthManager::IsTransportAvailable())
                {
                    auto const synthEndpointId = midi2synth::MidiSynthManager::EndpointDeviceId();

                    if (!synthEndpointId.empty())
                    {
                        return midiapp::EndpointIdsMatch(synthEndpointId, device.EndpointDeviceId());
                    }
                }

                std::wstring const name{ device.Name() };

                return name.find(L"General MIDI") != std::wstring::npos
                    || name.find(L"GS Wavetable") != std::wstring::npos;
            }
            catch (...)
            {
                return false;
            }
        }
    }

    MainWindow::MainWindow()
    {
        InitializeComponent();

        m_dispatcherQueue = winrt::Microsoft::UI::Dispatching::DispatcherQueue::GetForCurrentThread();
    }
    void MainWindow::RestoreWindowPlacement() noexcept
    {
        // static, because this runs before the chrome is initialized
        midiapp::WindowChrome::RestorePlacement(
            *this, native::AppSettings::Current(), DefaultWindowWidth, DefaultWindowHeight);
    }

    _Use_decl_annotations_
    void MainWindow::OnRootLoaded(foundation::IInspectable const&, xaml::RoutedEventArgs const&)
    {
        try
        {
            InitializeStaticText();
            InitializeWindowChrome();

            m_items = winrt::single_threaded_observable_vector<winrt::midiplayer::QueueItem>();
            m_tracks = winrt::single_threaded_observable_vector<winrt::midiplayer::TrackItem>();
            m_endpoints = winrt::single_threaded_observable_vector<appshared::EndpointChoice>();
            m_groups = winrt::single_threaded_observable_vector<appshared::NamedChoice>();

            QueueList().ItemsSource(m_items);
            TrackList().ItemsSource(m_tracks);
            EndpointComboBox().ItemsSource(m_endpoints);
            GroupComboBox().ItemsSource(m_groups);

            m_noteRoll.Initialize(NoteRollHost());
            m_keyboardRoll.Initialize(KeyboardRollHost());

            RepeatToggle().IsChecked(native::AppSettings::Current().RepeatQueue());
            QueueToggle().IsChecked(native::AppSettings::Current().ShowQueue());

            m_keyboardView = native::AppSettings::Current().KeyboardView();
            KeyboardViewToggle().IsChecked(m_keyboardView);
            ApplyViewMode();

            m_engine.SetCompletionHandler([weak = get_weak(), queue = m_dispatcherQueue]() noexcept
                {
                    if (queue == nullptr)
                    {
                        return;
                    }

                    queue.TryEnqueue([weak]()
                        {
                            if (auto strong = weak.get())
                            {
                                strong->HandlePlaybackCompleted();
                            }
                        });
                });

            SingleInstancePublish();

            Closed([weak = get_weak()](auto&&, auto&&)
                {
                    if (auto strong = weak.get())
                    {
                        strong->StopPositionTimer();
                        strong->StopEndpointWatcher();

                        // Synchronous on purpose: the note offs have to reach the instrument
                        // before this process ends, or a note is left sounding with nothing
                        // left to turn it off.
                        strong->m_engine.Stop();
                        strong->m_engine.Close();

                        if (strong->m_session != nullptr)
                        {
                            strong->m_session.Close();
                            strong->m_session = nullptr;
                        }

                        strong->m_noteRoll.Shutdown();
                        strong->m_keyboardRoll.Shutdown();

                        strong->m_chrome.SavePlacement();
                        strong->m_chrome.Shutdown();
                    }
                });

            m_initialized = true;

            UpdateQueueVisibility();
            UpdateNowPlayingText();
            UpdateTransportState();
            RebuildQueueList();
            RebuildTrackList();

            StartEndpointWatcher();
        }
        MIDI_PLAYER_CATCH_AND_LOG(L"Unable to finish loading the window.")
    }

    void MainWindow::SingleInstancePublish() noexcept
    {
        try
        {
            auto const handle = WindowHandle();

            if (handle == nullptr)
            {
                return;
            }

            s_instance = *this;

            midiapp::SingleInstance::PublishMainWindow(handle);

            ::SetWindowSubclass(handle, &MainWindow::SubclassProcedure, SubclassId, 0);
        }
        MIDI_PLAYER_CATCH_AND_LOG(L"Unable to publish the window handle.")
    }

    _Use_decl_annotations_
    LRESULT CALLBACK MainWindow::SubclassProcedure(
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
                auto const paths = native::ReadFilesFromCopyData(
                    reinterpret_cast<COPYDATASTRUCT const*>(lParam));

                if (!paths.empty())
                {
                    if (auto strong = s_instance.get())
                    {
                        winrt::get_self<MainWindow>(strong)->HandleFilesFromOtherInstance(paths);
                    }
                }

                return TRUE;
            }
            catch (...)
            {
                return TRUE;
            }
        }

        if (message == WM_NCDESTROY)
        {
            ::RemoveWindowSubclass(window, &MainWindow::SubclassProcedure, subclassId);
        }

        return ::DefSubclassProc(window, message, wParam, lParam);
    }

    _Use_decl_annotations_
    void MainWindow::HandleFilesFromOtherInstance(std::vector<std::wstring> const& paths) noexcept
    {
        try
        {
            std::vector<std::wstring> playable{};

            for (auto const& path : paths)
            {
                if (midifile::IsStandardMidiFileExtension(path))
                {
                    playable.push_back(path);
                }
            }

            if (playable.empty())
            {
                return;
            }

            // Opening a file from Explorer means play it, whether or not something else is
            // already going.
            AddFilesAsync(playable, true);
        }
        MIDI_PLAYER_CATCH_AND_LOG(L"Unable to open the files handed over by another copy of the player.")
    }

    HWND MainWindow::WindowHandle() noexcept
    {
        try
        {
            HWND handle{};

            if (auto const native = try_as<::IWindowNative>())
            {
                native->get_WindowHandle(&handle);
            }

            return handle;
        }
        catch (...)
        {
            return nullptr;
        }
    }

    void MainWindow::InitializeStaticText() noexcept
    {
        try
        {
            Title(res::GetString(L"AppDisplayName"));
            AppTitleTextBlock().Text(res::GetString(L"AppDisplayName"));
        }
        MIDI_PLAYER_CATCH_AND_LOG(L"Unable to set the window text.")
    }

    void MainWindow::InitializeWindowChrome() noexcept
    {
        try
        {
            midiapp::WindowChromeElements elements{};

            elements.Window = *this;
            elements.Root = RootGrid();
            elements.Fill = WindowFill();
            elements.Tint = WindowTint();
            elements.TitleBar = AppTitleBar();
            elements.LeftInset = TitleBarLeftInsetColumn();
            elements.RightInset = TitleBarRightInsetColumn();

            m_chrome.Initialize(elements, native::AppSettings::Current());
            m_chrome.SetWindowIconFromResource(IDI_APPICON);

            // 32px source for a 16px slot, so it stays crisp on a high DPI display
            AppTitleBarIcon().Source(midiapp::WindowChrome::LoadIconImageSource(IDI_APPICON, 32));

            AlwaysOnTopToggle().IsChecked(native::AppSettings::Current().AlwaysOnTop());
        }
        MIDI_PLAYER_CATCH_AND_LOG(L"Unable to set up the window chrome.")
    }

    _Use_decl_annotations_
    void MainWindow::OnAlwaysOnTopToggled(foundation::IInspectable const&, xaml::RoutedEventArgs const&)
    {
        try
        {
            native::AppSettings::Current().AlwaysOnTop(AlwaysOnTopToggle().IsChecked().GetBoolean());
            m_chrome.ApplyAlwaysOnTop();
        }
        MIDI_PLAYER_CATCH_AND_LOG(L"Unable to change the always on top setting.")
    }

    _Use_decl_annotations_
    void MainWindow::OnSettingsToggleClick(foundation::IInspectable const& sender, xaml::RoutedEventArgs const&)
    {
        try
        {
            auto const anchor = sender.try_as<xaml::FrameworkElement>();

            if (anchor == nullptr)
            {
                return;
            }

            midiapp::AppearanceStrings strings{};

            strings.Title = res::GetString(L"AppearanceTitle");
            strings.ThemeLabel = res::GetString(L"AppearanceTheme");
            strings.ThemeSystem = res::GetString(L"AppearanceThemeSystem");
            strings.ThemeLight = res::GetString(L"AppearanceThemeLight");
            strings.ThemeDark = res::GetString(L"AppearanceThemeDark");
            strings.BackdropLabel = res::GetString(L"AppearanceBackdrop");
            strings.BackdropSolid = res::GetString(L"AppearanceBackdropSolid");
            strings.BackdropMica = res::GetString(L"AppearanceBackdropMica");
            strings.BackdropAcrylic = res::GetString(L"AppearanceBackdropAcrylic");
            strings.CustomColorCheckBox = res::GetString(L"AppearanceCustomColor");
            strings.ColorPickerName = res::GetString(L"AppearanceColorPickerName");

            midiapp::ShowAppearanceFlyout(
                anchor,
                native::AppSettings::Current(),
                strings,
                [weak = get_weak()]() noexcept
                {
                    if (auto strong = weak.get())
                    {
                        strong->m_chrome.ApplyTheme();
                    }
                });
        }
        MIDI_PLAYER_CATCH_AND_LOG(L"Unable to show the appearance settings.")
    }

    // ------------------------------------------------------------------------------------
    // Endpoints
    // ------------------------------------------------------------------------------------

    void MainWindow::StartEndpointWatcher() noexcept
    {
        try
        {
            m_watcher = midi2enum::MidiEndpointDeviceWatcher::Create(
                midi2enum::MidiEndpointDeviceInformationFilters::AllStandardEndpoints);

            if (m_watcher == nullptr)
            {
                return;
            }

            auto const refresh = [weak = get_weak(), queue = m_dispatcherQueue]()
                {
                    if (queue == nullptr)
                    {
                        return;
                    }

                    queue.TryEnqueue([weak]()
                        {
                            if (auto strong = weak.get())
                            {
                                strong->RefreshEndpointList();
                            }
                        });
                };

            m_watcherAddedToken = m_watcher.Added([refresh](auto&&, auto&&) { refresh(); });
            m_watcherRemovedToken = m_watcher.Removed([refresh](auto&&, auto&&) { refresh(); });
            m_watcherUpdatedToken = m_watcher.Updated([refresh](auto&&, auto&&) { refresh(); });

            m_watcherEnumerationCompletedToken = m_watcher.EnumerationCompleted(
                [weak = get_weak(), queue = m_dispatcherQueue, refresh](auto&&, auto&&)
                {
                    if (queue == nullptr)
                    {
                        return;
                    }

                    queue.TryEnqueue([weak]()
                        {
                            if (auto strong = weak.get())
                            {
                                strong->m_enumerationCompleted = true;
                                strong->RefreshEndpointList();
                            }
                        });
                });

            m_watcher.Start();

            RefreshEndpointList();
        }
        MIDI_PLAYER_CATCH_AND_LOG(L"Unable to start the endpoint device watcher.")
    }

    void MainWindow::StopEndpointWatcher() noexcept
    {
        try
        {
            if (m_watcher == nullptr)
            {
                return;
            }

            m_watcher.Added(m_watcherAddedToken);
            m_watcher.Removed(m_watcherRemovedToken);
            m_watcher.Updated(m_watcherUpdatedToken);
            m_watcher.EnumerationCompleted(m_watcherEnumerationCompletedToken);

            m_watcher.Stop();
            m_watcher = nullptr;
        }
        MIDI_PLAYER_CATCH_AND_LOG(L"Unable to stop the endpoint device watcher.")
    }

    void MainWindow::RefreshEndpointList() noexcept
    {
        try
        {
            if (m_watcher == nullptr || m_endpoints == nullptr)
            {
                return;
            }

            auto const devices = midiapp::SortedEndpoints(m_watcher);

            // The watcher fires repeatedly, so whatever is picked has to survive a rebuild.
            auto const previous = SelectedEndpointDeviceId();

            m_suppressEndpointHandlers = true;

            m_endpoints.Clear();
            m_endpointDevices.clear();

            for (auto const& device : devices)
            {
                winrt::hstring imagePath{};

                if (auto const userInfo = device.GetUserSuppliedInfo())
                {
                    imagePath = midiapp::ResolveEndpointImagePath(userInfo.ImageFileName());
                }

                m_endpoints.Append(winrt::make<appshared::implementation::EndpointChoice>(
                    device.Name(), device.EndpointDeviceId(), imagePath));

                m_endpointDevices.push_back(device);
            }

            auto const& settings = native::AppSettings::Current();

            auto const wanted = previous.empty() ? settings.LastEndpointDeviceId() : previous;

            int32_t selected = -1;

            for (size_t index = 0; index < m_endpointDevices.size(); ++index)
            {
                if (!wanted.empty() &&
                    midiapp::EndpointIdsMatch(m_endpointDevices[index].EndpointDeviceId(), winrt::hstring{ wanted }))
                {
                    selected = static_cast<int32_t>(index);
                    break;
                }
            }

            if (selected < 0 && wanted.empty())
            {
                // First run. Prefer the synth that comes with Windows, so a file opened from
                // Explorer makes a sound without anything being set up.
                for (size_t index = 0; index < m_endpointDevices.size(); ++index)
                {
                    if (IsPreferredFirstRunEndpoint(m_endpointDevices[index]))
                    {
                        selected = static_cast<int32_t>(index);
                        break;
                    }
                }
            }

            // A saved endpoint which is not here is NOT quietly replaced: sending a file to
            // whatever happens to be plugged in today is worse than asking. Judged only once the
            // watcher says it has seen everything, or a half enumerated list raises a false alarm.
            m_savedEndpointMissing = m_enumerationCompleted && selected < 0 && !wanted.empty();

            EndpointComboBox().SelectedIndex(selected);

            m_suppressEndpointHandlers = false;

            RefreshGroupList(settings.GroupIndex());

            if (m_savedEndpointMissing)
            {
                ShowStatus(res::GetString(L"StatusSavedEndpointMissing"), controls::InfoBarSeverity::Warning);
            }
            else if (selected >= 0)
            {
                ClearStatus();
            }

            ApplyStartupOptions();

            // A file opened from Explorer before any endpoint had turned up is played now that
            // one has.
            if (selected >= 0 && m_pendingAutoPlay)
            {
                m_pendingAutoPlay = false;
                StartCurrentAsync(true);
            }

            UpdateTransportState();
        }
        MIDI_PLAYER_CATCH_AND_LOG(L"Unable to refresh the endpoint list.")
    }

    void MainWindow::RefreshGroupList(int32_t desiredGroupIndex) noexcept
    {
        try
        {
            if (m_groups == nullptr)
            {
                return;
            }

            m_suppressGroupHandlers = true;

            m_groups.Clear();

            auto const endpoint = SelectedEndpoint();

            int32_t selected = -1;

            if (endpoint.has_value())
            {
                auto const declared = midiapp::DeclaredGroups(endpoint.value());

                for (uint8_t index = 0; index < 16; ++index)
                {
                    if (!declared[index])
                    {
                        continue;
                    }

                    auto const blockName = midiapp::DescribeGroup(endpoint.value(), index);

                    auto const label = blockName.empty()
                        ? res::FormatString(L"GroupNumberFormat", index + 1)
                        : res::FormatString(L"GroupNumberAndNameFormat", index + 1, std::wstring{ blockName });

                    if (index == desiredGroupIndex)
                    {
                        selected = static_cast<int32_t>(m_groups.Size());
                    }

                    m_groups.Append(winrt::make<appshared::implementation::NamedChoice>(label, index));
                }
            }

            if (selected < 0 && m_groups.Size() > 0)
            {
                selected = 0;
            }

            GroupComboBox().SelectedIndex(selected);
            GroupComboBox().IsEnabled(m_groups.Size() > 1);

            m_suppressGroupHandlers = false;
        }
        MIDI_PLAYER_CATCH_AND_LOG(L"Unable to refresh the group list.")
    }

    std::optional<midi2enum::MidiEndpointDeviceInformation> MainWindow::SelectedEndpoint() noexcept
    {
        try
        {
            auto const index = EndpointComboBox().SelectedIndex();

            if (index < 0 || static_cast<size_t>(index) >= m_endpointDevices.size())
            {
                return std::nullopt;
            }

            return m_endpointDevices[static_cast<size_t>(index)];
        }
        catch (...)
        {
            return std::nullopt;
        }
    }

    std::wstring MainWindow::SelectedEndpointDeviceId() noexcept
    {
        auto const endpoint = SelectedEndpoint();

        if (!endpoint.has_value())
        {
            return {};
        }

        try
        {
            return std::wstring{ endpoint.value().EndpointDeviceId() };
        }
        catch (...)
        {
            return {};
        }
    }

    _Use_decl_annotations_
    void MainWindow::OnEndpointSelectionChanged(
        foundation::IInspectable const&,
        controls::SelectionChangedEventArgs const&)
    {
        try
        {
            if (m_suppressEndpointHandlers || !m_initialized)
            {
                return;
            }

            auto const id = SelectedEndpointDeviceId();

            if (id.empty())
            {
                return;
            }

            m_savedEndpointMissing = false;
            ClearStatus();

            native::AppSettings::Current().LastEndpointDeviceId(id);

            RefreshGroupList(native::AppSettings::Current().GroupIndex());

            // Changing where the music goes while it is playing would leave notes sounding on
            // the old instrument, so playback restarts from where it is.
            StartCurrentAsync(m_engine.State() == native::PlaybackState::Playing);

            UpdateTransportState();
        }
        MIDI_PLAYER_CATCH_AND_LOG(L"Unable to change the endpoint.")
    }

    _Use_decl_annotations_
    void MainWindow::OnGroupSelectionChanged(
        foundation::IInspectable const&,
        controls::SelectionChangedEventArgs const&)
    {
        try
        {
            if (m_suppressGroupHandlers || !m_initialized)
            {
                return;
            }

            auto const index = GroupComboBox().SelectedIndex();

            if (index < 0 || static_cast<uint32_t>(index) >= m_groups.Size())
            {
                return;
            }

            auto const choice = m_groups.GetAt(static_cast<uint32_t>(index));

            if (choice == nullptr)
            {
                return;
            }

            auto const groupIndex = static_cast<uint8_t>(std::clamp(choice.Value(), 0, 15));

            if (groupIndex == native::AppSettings::Current().GroupIndex())
            {
                return;
            }

            native::AppSettings::Current().GroupIndex(groupIndex);

            StartCurrentAsync(m_engine.State() == native::PlaybackState::Playing);
        }
        MIDI_PLAYER_CATCH_AND_LOG(L"Unable to change the group.")
    }

    void MainWindow::ApplyStartupOptions() noexcept
    {
        try
        {
            if (m_startupOptionsApplied || !m_initialized)
            {
                return;
            }

            // The watcher's first pass enumerates nothing, so waiting for a real list means a
            // command line endpoint is not thrown away.
            if (m_endpointDevices.empty())
            {
                return;
            }

            m_startupOptionsApplied = true;

            auto const& options = App::StartupOptions();

            if (!options.EndpointDeviceId.empty())
            {
                for (size_t index = 0; index < m_endpointDevices.size(); ++index)
                {
                    if (midiapp::EndpointIdsMatch(
                        m_endpointDevices[index].EndpointDeviceId(),
                        winrt::hstring{ options.EndpointDeviceId }))
                    {
                        EndpointComboBox().SelectedIndex(static_cast<int32_t>(index));
                        break;
                    }
                }
            }

            if (options.GroupNumber.has_value())
            {
                native::AppSettings::Current().GroupIndex(options.GroupNumber.value());
                RefreshGroupList(options.GroupNumber.value());
            }

            if (!options.Files.empty())
            {
                AddFilesAsync(options.Files, options.AutoPlay);
            }
        }
        MIDI_PLAYER_CATCH_AND_LOG(L"Unable to apply the startup options.")
    }
}
