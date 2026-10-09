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
#include "AppearanceFlyout.h"
#include "FileDialogs.h"
#include "LiveRegion.h"
#include "PreviewBuild.h"
#include "SingleInstance.h"
#include "StartupRegistration.h"
#include "StringResources.h"
#include "resource.h"

#include "AudioOutput.h"

namespace native = ::midisoundfontsynth;
namespace res = ::midisoundfontsynth::resources;
namespace sf = ::SoundFontSynth;

namespace winrt::midisoundfontsynth::implementation
{
    namespace
    {
        constexpr int32_t DefaultWindowWidth = 980;
        constexpr int32_t DefaultWindowHeight = 640;

        constexpr int32_t RefreshIntervalMilliseconds = 500;

        // Dragging a slider changes the volume dozens of times a second. The file is written
        // once the hand has stopped.
        constexpr uint64_t SaveDelayMilliseconds = 1000;

        constexpr double VolumeEpsilon = 0.01;

        constexpr uint32_t ExclusiveBufferChoices[]{ 3, 5, 10, 20, 40 };

        implementation::SynthItem* Impl(winrt::midisoundfontsynth::SynthItem const& item) noexcept
        {
            return item == nullptr ? nullptr : winrt::get_self<implementation::SynthItem>(item);
        }

        std::wstring TagOf(_In_ foundation::IInspectable const& sender)
        {
            if (auto const element = sender.try_as<xaml::FrameworkElement>())
            {
                return std::wstring{ winrt::unbox_value_or<winrt::hstring>(element.Tag(), winrt::hstring{}) };
            }

            return {};
        }

        std::wstring FileNameOf(_In_ std::wstring const& path)
        {
            return std::wstring{ ::PathFindFileNameW(path.c_str()) };
        }

        std::wstring StemOf(_In_ std::wstring const& path)
        {
            auto name = FileNameOf(path);
            auto const dot = name.find_last_of(L'.');

            if (dot != std::wstring::npos && dot > 0)
            {
                name.resize(dot);
            }

            return name;
        }

        // What a person typed, made safe to be an endpoint name.
        std::wstring CleanName(_In_ std::wstring name)
        {
            std::erase_if(name, [](wchar_t ch) { return ch < L' ' || ch == 0x7F; });

            auto const first = name.find_first_not_of(L' ');

            if (first == std::wstring::npos)
            {
                return {};
            }

            name.erase(0, first);
            name.erase(name.find_last_not_of(L' ') + 1);

            if (name.size() > native::SynthStore::MaximumNameLength)
            {
                name.resize(native::SynthStore::MaximumNameLength);
                name.erase(name.find_last_not_of(L' ') + 1);
            }

            return name;
        }

        media::Brush ThemeBrush(_In_ wchar_t const* key)
        {
            return xaml::Application::Current().Resources().Lookup(winrt::box_value(key)).as<media::Brush>();
        }

        winrt::hstring VolumeText(_In_ double decibels)
        {
            return res::FormatString(L"VolumeFormat", decibels);
        }
    }

    MainWindow::MainWindow()
    {
        InitializeComponent();

        m_dispatcherQueue = winrt::Microsoft::UI::Dispatching::DispatcherQueue::GetForCurrentThread();
    }

    void MainWindow::RestoreWindowPlacement() noexcept
    {
        midiapp::WindowChrome::RestorePlacement(
            *this, native::AppSettings::Current(), DefaultWindowWidth, DefaultWindowHeight);
    }

    HWND MainWindow::WindowHandle() const noexcept
    {
        if (auto const handle = m_chrome.WindowHandle())
        {
            return handle;
        }

        HWND handle{ nullptr };

        try
        {
            if (auto const window = this->try_as<::IWindowNative>())
            {
                LOG_IF_FAILED(window->get_WindowHandle(&handle));
            }
        }
        catch (...)
        {
        }

        return handle;
    }

    void MainWindow::MinimizeAtStartup() noexcept
    {
        try
        {
            if (auto const handle = WindowHandle())
            {
                ::ShowWindow(handle, SW_MINIMIZE);
            }
        }
        MIDI_SF2SYNTH_CATCH_AND_LOG(L"Unable to start minimized.")
    }

    _Use_decl_annotations_
    void MainWindow::OnRootLoaded(foundation::IInspectable const&, xaml::RoutedEventArgs const&)
    {
        try
        {
            if (m_loaded)
            {
                return;
            }

            m_loaded = true;

            InitializeWindowChrome();

            midiapp::MakeLiveStatusRegion(AudioStatusTextBlock());

            m_items = winrt::single_threaded_observable_vector<winrt::midisoundfontsynth::SynthItem>();
            SynthRepeater().ItemsSource(m_items);

            m_definitions = native::SynthStore::Load();

            for (auto const& definition : m_definitions)
            {
                AddItem(definition);
            }

            UpdateEmptyState();

            auto& host = native::SynthHost::Current();

            ApplyAudioSettings();

            // Raised on the host's threads, and often. One refresh at a time is queued.
            host.Start([weak = get_weak(), queued = m_refreshQueued, queue = m_dispatcherQueue]()
                {
                    if (queue == nullptr || queued->exchange(true))
                    {
                        return;
                    }

                    if (!queue.TryEnqueue([weak, queued]()
                        {
                            queued->store(false);

                            if (auto strong = weak.get())
                            {
                                strong->RefreshFromHost();
                            }
                        }))
                    {
                        queued->store(false);
                    }
                });

            for (auto const& definition : m_definitions)
            {
                if (definition.Enabled)
                {
                    host.Enable(definition);
                }
            }

            m_refreshTimer = xaml::DispatcherTimer{};
            m_refreshTimer.Interval(std::chrono::milliseconds{ RefreshIntervalMilliseconds });
            m_refreshTimer.Tick([weak = get_weak()](auto&&, auto&&)
                {
                    if (auto strong = weak.get())
                    {
                        strong->RefreshFromHost();
                    }
                });
            m_refreshTimer.Start();

            HookWindowEvents();
            InitializeNotificationArea();

            RefreshFromHost();

            // The title bar buttons are first in tab order, so focus would otherwise start there.
            AddSynthButton().Focus(xaml::FocusState::Programmatic);
        }
        MIDI_SF2SYNTH_CATCH_AND_LOG(L"Unable to finish loading the window.")
    }

    void MainWindow::InitializeWindowChrome() noexcept
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

            m_chrome.Initialize(elements, native::AppSettings::Current());

            // Now there is a window, a second launch has something to bring forward.
            ::midiapp::SingleInstance::PublishMainWindow(m_chrome.WindowHandle());

            m_chrome.SetWindowIconFromResource(IDI_APPICON);

            Title(res::GetString(L"AppDisplayName"));
            AppTitleTextBlock().Text(res::GetString(L"AppDisplayName"));

            // 32px source for a 16px slot, so it stays crisp on a high DPI display
            if (auto const icon = midiapp::WindowChrome::LoadIconImageSource(IDI_APPICON, 32))
            {
                AppTitleBarIcon().Source(icon);
            }

            AlwaysOnTopToggle().IsChecked(native::AppSettings::Current().AlwaysOnTop());

            RenameDialog().PrimaryButtonText(res::GetString(L"RenameDialogSave"));
            RenameDialog().CloseButtonText(res::GetString(L"DialogCancel"));
            RemoveDialog().PrimaryButtonText(res::GetString(L"RemoveDialogRemove"));
            RemoveDialog().CloseButtonText(res::GetString(L"DialogCancel"));
            ExitDialog().PrimaryButtonText(res::GetString(L"ExitDialogClose"));
            ExitDialog().CloseButtonText(res::GetString(L"DialogCancel"));
        }
        MIDI_SF2SYNTH_CATCH_AND_LOG(L"Unable to set up the window chrome.")
    }

    void MainWindow::HookWindowEvents() noexcept
    {
        try
        {
            auto weak = get_weak();

            // Closing becomes a hide when the customer asked for the app to keep running in the
            // notification area. Otherwise it asks first if an app is using a synth.
            m_closingToken = AppWindow().Closing(
                [weak](auto&&, windowing::AppWindowClosingEventArgs const& args)
                {
                    auto strong = weak.get();

                    if (strong == nullptr || strong->m_exiting)
                    {
                        return;
                    }

                    args.Cancel(true);

                    if (strong->TryHideToNotificationArea())
                    {
                        return;
                    }

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
                            if (presenter.State() == windowing::OverlappedPresenterState::Minimized &&
                                ::IsIconic(strong->WindowHandle()))
                            {
                                strong->TryHideToNotificationArea();
                            }
                        }
                    }
                });

            Closed([weak](auto&&, auto&&)
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

                    // A window hidden in the notification area has no placement worth keeping,
                    // and the last good one is already saved.
                    if (::IsWindowVisible(strong->WindowHandle()))
                    {
                        strong->m_chrome.SavePlacement();
                    }

                    strong->SaveNow();
                    strong->m_tray.Hide();
                    strong->m_chrome.Shutdown();
                });
        }
        MIDI_SF2SYNTH_CATCH_AND_LOG(L"Unable to hook the window events.")
    }

    // ------------------------------------------------------------------------------- title bar

    _Use_decl_annotations_
    void MainWindow::OnAlwaysOnTopToggled(foundation::IInspectable const&, xaml::RoutedEventArgs const&)
    {
        try
        {
            auto const isChecked = AlwaysOnTopToggle().IsChecked();

            native::AppSettings::Current().AlwaysOnTop(isChecked && isChecked.Value());
            m_chrome.ApplyAlwaysOnTop();
        }
        MIDI_SF2SYNTH_CATCH_AND_LOG(L"Unable to change the always on top setting.")
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

            // A button that opens a flyout, not a switch that stays on.
            SettingsToggle().IsChecked(false);

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
                [weak = get_weak()]()
                {
                    if (auto strong = weak.get())
                    {
                        strong->m_chrome.ApplyTheme();
                    }
                },
                BuildSettingsPanel());
        }
        MIDI_SF2SYNTH_CATCH_AND_LOG(L"Unable to show the settings.")
    }

    xaml::UIElement MainWindow::BuildSettingsPanel() noexcept
    {
        try
        {
            auto& settings = native::AppSettings::Current();
            auto weak = get_weak();

            controls::StackPanel panel{};
            panel.Spacing(10);
            panel.Margin(xaml::ThicknessHelper::FromLengths(0, 12, 0, 0));

            auto const addHeading = [&panel](winrt::hstring const& text)
                {
                    controls::TextBlock heading{};
                    heading.Text(text);
                    heading.FontSize(12);
                    heading.Margin(xaml::ThicknessHelper::FromLengths(0, 6, 0, 0));
                    heading.Foreground(ThemeBrush(L"TextFillColorTertiaryBrush"));
                    panel.Children().Append(heading);
                };

            auto const addHint = [&panel](winrt::hstring const& text)
                {
                    controls::TextBlock hint{};
                    hint.Text(text);
                    hint.FontSize(11);
                    hint.TextWrapping(xaml::TextWrapping::Wrap);
                    hint.Margin(xaml::ThicknessHelper::FromLengths(0, -6, 0, 0));
                    hint.Foreground(ThemeBrush(L"TextFillColorTertiaryBrush"));
                    panel.Children().Append(hint);
                };

            // ------------------------------------------------------------------ audio output

            addHeading(res::GetString(L"SettingsAudioHeading"));

            auto const devices = std::make_shared<std::vector<sf::AudioDeviceInfo>>(sf::AudioOutput::EnumerateDevices());

            controls::ComboBox deviceBox{};
            deviceBox.Header(winrt::box_value(res::GetString(L"SettingsAudioDevice")));
            deviceBox.HorizontalAlignment(xaml::HorizontalAlignment::Stretch);
            deviceBox.Items().Append(winrt::box_value(res::GetString(L"SettingsAudioDeviceDefault")));

            int32_t selectedDevice{ 0 };

            for (size_t i = 0; i < devices->size(); i++)
            {
                deviceBox.Items().Append(winrt::box_value(winrt::hstring{ (*devices)[i].Name }));

                if ((*devices)[i].Id == settings.AudioDeviceId())
                {
                    selectedDevice = static_cast<int32_t>(i + 1);
                }
            }

            // A chosen device that is unplugged right now is still the customer's choice, so it
            // is shown rather than quietly replaced.
            if (selectedDevice == 0 && !settings.AudioDeviceId().empty())
            {
                deviceBox.Items().Append(winrt::box_value(res::GetString(L"SettingsAudioDeviceMissing")));
                selectedDevice = static_cast<int32_t>(deviceBox.Items().Size() - 1);
            }

            deviceBox.SelectedIndex(selectedDevice);
            deviceBox.SelectionChanged([weak, devices](foundation::IInspectable const& sender, auto&&)
                {
                    try
                    {
                        auto const index = sender.as<controls::ComboBox>().SelectedIndex();

                        if (index < 0)
                        {
                            return;
                        }

                        auto& current = native::AppSettings::Current();

                        if (index == 0)
                        {
                            current.AudioDeviceId({});
                        }
                        else if (static_cast<size_t>(index) <= devices->size())
                        {
                            current.AudioDeviceId((*devices)[static_cast<size_t>(index) - 1].Id);
                        }
                        else
                        {
                            return;
                        }

                        if (auto strong = weak.get())
                        {
                            strong->ApplyAudioSettings();
                        }
                    }
                    MIDI_SF2SYNTH_CATCH_AND_LOG(L"Unable to change the audio device.")
                });

            panel.Children().Append(deviceBox);

            controls::ComboBox modeBox{};
            modeBox.Header(winrt::box_value(res::GetString(L"SettingsAudioMode")));
            modeBox.HorizontalAlignment(xaml::HorizontalAlignment::Stretch);
            modeBox.Items().Append(winrt::box_value(res::GetString(L"AudioModeShared")));
            modeBox.Items().Append(winrt::box_value(res::GetString(L"AudioModeExclusive")));
            modeBox.SelectedIndex(settings.ExclusiveMode() ? 1 : 0);

            controls::ComboBox bufferBox{};
            bufferBox.Header(winrt::box_value(res::GetString(L"SettingsAudioBuffer")));
            bufferBox.HorizontalAlignment(xaml::HorizontalAlignment::Stretch);

            int32_t selectedBuffer{ -1 };

            for (size_t i = 0; i < std::size(ExclusiveBufferChoices); i++)
            {
                bufferBox.Items().Append(winrt::box_value(res::FormatString(L"MillisecondsFormat", ExclusiveBufferChoices[i])));

                if (ExclusiveBufferChoices[i] == settings.ExclusiveBufferMilliseconds())
                {
                    selectedBuffer = static_cast<int32_t>(i);
                }
            }

            bufferBox.SelectedIndex(selectedBuffer < 0 ? 2 : selectedBuffer);
            bufferBox.IsEnabled(settings.ExclusiveMode());

            modeBox.SelectionChanged([weak, bufferBox](foundation::IInspectable const& sender, auto&&)
                {
                    try
                    {
                        auto const index = sender.as<controls::ComboBox>().SelectedIndex();

                        if (index < 0)
                        {
                            return;
                        }

                        native::AppSettings::Current().ExclusiveMode(index == 1);
                        bufferBox.IsEnabled(index == 1);

                        if (auto strong = weak.get())
                        {
                            strong->ApplyAudioSettings();
                        }
                    }
                    MIDI_SF2SYNTH_CATCH_AND_LOG(L"Unable to change the audio mode.")
                });

            bufferBox.SelectionChanged([weak](foundation::IInspectable const& sender, auto&&)
                {
                    try
                    {
                        auto const index = sender.as<controls::ComboBox>().SelectedIndex();

                        if (index < 0 || static_cast<size_t>(index) >= std::size(ExclusiveBufferChoices))
                        {
                            return;
                        }

                        native::AppSettings::Current().ExclusiveBufferMilliseconds(ExclusiveBufferChoices[index]);

                        if (auto strong = weak.get())
                        {
                            strong->ApplyAudioSettings();
                        }
                    }
                    MIDI_SF2SYNTH_CATCH_AND_LOG(L"Unable to change the audio buffer.")
                });

            panel.Children().Append(modeBox);
            addHint(res::GetString(L"SettingsAudioModeHint"));
            panel.Children().Append(bufferBox);

            // ------------------------------------------------------------------------- app

            addHeading(res::GetString(L"SettingsAppHeading"));

            auto const addToggle = [&panel](winrt::hstring const& header, bool isOn, std::function<void(controls::ToggleSwitch const&, bool)> onChanged)
                {
                    controls::ToggleSwitch toggle{};

                    toggle.Header(winrt::box_value(header));
                    toggle.IsOn(isOn);
                    toggle.OnContent(winrt::box_value(winrt::hstring{}));
                    toggle.OffContent(winrt::box_value(winrt::hstring{}));

                    toggle.Toggled([onChanged](foundation::IInspectable const& sender, xaml::RoutedEventArgs const&)
                        {
                            if (auto const control = sender.try_as<controls::ToggleSwitch>())
                            {
                                onChanged(control, control.IsOn());
                            }
                        });

                    panel.Children().Append(toggle);

                    return toggle;
                };

            // The real state is asked for asynchronously, so the switch is set without reacting
            // to its own change.
            auto startupGuard = std::make_shared<bool>(true);

            auto startupToggle = addToggle(res::GetString(L"SettingsStartWithWindows"), false,
                [weak, startupGuard](controls::ToggleSwitch const& toggle, bool value) -> void
                {
                    if (*startupGuard)
                    {
                        return;
                    }

                    [](winrt::weak_ref<MainWindow> weakWindow, controls::ToggleSwitch control, bool enable, std::shared_ptr<bool> guard) -> winrt::fire_and_forget
                        {
                            try
                            {
                                auto const state = static_cast<native::StartupState>(
                                    co_await native::StartupRegistration::SetEnabledAsync(enable));

                                *guard = true;
                                control.IsOn(state == native::StartupState::On);
                                *guard = false;

                                if (enable && state != native::StartupState::On)
                                {
                                    if (auto strong = weakWindow.get())
                                    {
                                        strong->ShowNotice(
                                            res::GetString(state == native::StartupState::OffByUser
                                                ? L"StartupDisabledByUser"
                                                : L"StartupNotChanged"),
                                            controls::InfoBarSeverity::Warning);
                                    }
                                }
                            }
                            MIDI_SF2SYNTH_CATCH_AND_LOG(L"Unable to change whether the app starts with Windows.")
                        }(weak, toggle, value, startupGuard);
                });

            addHint(res::GetString(L"SettingsStartWithWindowsHint"));

            [](controls::ToggleSwitch control, std::shared_ptr<bool> guard) -> winrt::fire_and_forget
                {
                    try
                    {
                        auto const state = static_cast<native::StartupState>(
                            co_await native::StartupRegistration::GetStateAsync());

                        control.IsOn(state == native::StartupState::On);
                        control.IsEnabled(state != native::StartupState::ControlledByPolicy);
                    }
                    MIDI_SF2SYNTH_CATCH_AND_LOG(L"Unable to read whether the app starts with Windows.")

                    *guard = false;
                }(startupToggle, startupGuard);

            addToggle(res::GetString(L"SettingsNotificationArea"), settings.MinimizeToNotificationArea(),
                [weak](controls::ToggleSwitch const&, bool value)
                {
                    native::AppSettings::Current().MinimizeToNotificationArea(value);

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

            addHint(res::GetString(L"SettingsNotificationAreaHint"));

            addToggle(res::GetString(L"SettingsStartMinimized"), settings.StartMinimized(),
                [](controls::ToggleSwitch const&, bool value)
                {
                    native::AppSettings::Current().StartMinimized(value);
                });

            return panel;
        }
        MIDI_SF2SYNTH_CATCH_AND_LOG(L"Unable to build the settings panel.")

        return nullptr;
    }

    void MainWindow::ApplyAudioSettings() noexcept
    {
        try
        {
            auto const& settings = native::AppSettings::Current();

            sf::AudioOutputSettings audio{};
            audio.DeviceId = settings.AudioDeviceId();
            audio.ShareMode = settings.ExclusiveMode() ? sf::AudioShareMode::Exclusive : sf::AudioShareMode::Shared;
            audio.ExclusiveBufferMilliseconds = settings.ExclusiveBufferMilliseconds();

            native::SynthHost::Current().ApplyAudioSettings(audio);
        }
        MIDI_SF2SYNTH_CATCH_AND_LOG(L"Unable to apply the audio settings.")
    }

    // ---------------------------------------------------------------------------- the synths

    _Use_decl_annotations_
    native::SynthDefinition* MainWindow::FindDefinition(std::wstring const& id) noexcept
    {
        for (auto& definition : m_definitions)
        {
            if (definition.Id == id)
            {
                return &definition;
            }
        }

        return nullptr;
    }

    _Use_decl_annotations_
    winrt::midisoundfontsynth::SynthItem MainWindow::FindItem(std::wstring const& id) noexcept
    {
        try
        {
            if (m_items == nullptr)
            {
                return nullptr;
            }

            for (auto const& item : m_items)
            {
                if (item.Id() == id)
                {
                    return item;
                }
            }
        }
        catch (...)
        {
        }

        return nullptr;
    }

    _Use_decl_annotations_
    void MainWindow::AddItem(native::SynthDefinition const& definition)
    {
        auto item = winrt::make<implementation::SynthItem>();

        Impl(item)->SetId(winrt::hstring{ definition.Id });
        m_items.Append(item);

        ApplyDefinitionToItem(definition);
    }

    _Use_decl_annotations_
    void MainWindow::ApplyDefinitionToItem(native::SynthDefinition const& definition) noexcept
    {
        try
        {
            auto* const item = Impl(FindItem(definition.Id));

            if (item == nullptr)
            {
                return;
            }

            winrt::hstring const name{ definition.Name };

            item->SetName(name);
            item->SetFile(winrt::hstring{ definition.SoundFontPath }, winrt::hstring{ FileNameOf(definition.SoundFontPath) });
            item->SetVolume(definition.VolumeDb, VolumeText(definition.VolumeDb));
            item->SetIsOn(definition.Enabled);
        }
        MIDI_SF2SYNTH_CATCH_AND_LOG(L"Unable to show a synth.")
    }

    void MainWindow::UpdateEmptyState() noexcept
    {
        try
        {
            auto const empty = m_definitions.empty();

            EmptyStatePanel().Visibility(empty ? xaml::Visibility::Visible : xaml::Visibility::Collapsed);
            SynthScroller().Visibility(empty ? xaml::Visibility::Collapsed : xaml::Visibility::Visible);
        }
        MIDI_SF2SYNTH_CATCH_AND_LOG(L"Unable to update the empty state.")
    }

    void MainWindow::RequestSave() noexcept
    {
        m_saveRequested = true;
        m_saveDueTick = ::GetTickCount64() + SaveDelayMilliseconds;
    }

    void MainWindow::SaveNow() noexcept
    {
        m_saveRequested = false;

        if (!native::SynthStore::Save(m_definitions))
        {
            ShowNotice(res::GetString(L"ErrorSaving"), controls::InfoBarSeverity::Error);
        }
    }

    _Use_decl_annotations_
    winrt::hstring MainWindow::StatusText(native::SynthStatus const& status) const
    {
        switch (status.State)
        {
        case native::SynthState::Loading:
            return res::GetString(L"StatusLoading");

        case native::SynthState::Running:
            if (status.InUse)
            {
                return res::GetString(status.PartlyLoaded ? L"StatusInUsePartly" : L"StatusInUse");
            }

            return res::GetString(status.PartlyLoaded ? L"StatusReadyPartly" : L"StatusReady");

        case native::SynthState::Failed:
            switch (status.Failure)
            {
            case native::SynthFailure::FileNotFound: return res::GetString(L"FailureFileNotFound");
            case native::SynthFailure::FileUnreadable: return res::GetString(L"FailureFileUnreadable");
            case native::SynthFailure::FileTooLarge: return res::GetString(L"FailureFileTooLarge");
            case native::SynthFailure::NothingPlayable: return res::GetString(L"FailureNothingPlayable");
            case native::SynthFailure::CompressedSamples: return res::GetString(L"FailureCompressedSamples");
            case native::SynthFailure::OutOfMemory: return res::GetString(L"FailureOutOfMemory");
            case native::SynthFailure::ServiceUnavailable: return res::GetString(L"FailureServiceUnavailable");
            case native::SynthFailure::EndpointFailed: return res::GetString(L"FailureEndpoint");
            default: return res::GetString(L"FailureNotASoundFont");
            }

        default:
            return res::GetString(L"StatusOff");
        }
    }

    _Use_decl_annotations_
    winrt::hstring MainWindow::AudioStatusText(sf::AudioEngineStatus const& status, bool anyRunning) const
    {
        if (status.Running)
        {
            auto text = std::wstring{ res::FormatString(
                L"AudioStatusRunningFormat",
                status.DeviceName,
                std::wstring{ res::GetString(status.ShareMode == sf::AudioShareMode::Exclusive ? L"AudioModeExclusive" : L"AudioModeShared") },
                status.SampleRate,
                status.PeriodMilliseconds) };

            if (status.UsingFallbackDevice)
            {
                text += L" ";
                text += res::GetString(L"AudioStatusFallback");
            }

            return winrt::hstring{ text };
        }

        if (status.Unavailable)
        {
            switch (status.LastOpenStatus)
            {
            case sf::AudioOutputStatus::NoDevice: return res::GetString(L"AudioNoDevice");
            case sf::AudioOutputStatus::DeviceInUse: return res::GetString(L"AudioDeviceInUse");
            case sf::AudioOutputStatus::ExclusiveNotAllowed: return res::GetString(L"AudioExclusiveNotAllowed");
            case sf::AudioOutputStatus::FormatNotSupported: return res::GetString(L"AudioFormatNotSupported");
            default: return res::GetString(L"AudioOpenFailed");
            }
        }

        return res::GetString(anyRunning ? L"AudioStatusIdle" : L"AudioStatusNoSynths");
    }

    void MainWindow::RefreshFromHost() noexcept
    {
        try
        {
            if (!m_loaded || m_closing || m_items == nullptr)
            {
                return;
            }

            auto& host = native::SynthHost::Current();

            for (auto& status : host.Snapshot())
            {
                m_statuses[status.Id] = std::move(status);
            }

            bool anyRunning{ false };
            bool anyUnavailable{ false };

            for (auto const& definition : m_definitions)
            {
                auto* const item = Impl(FindItem(definition.Id));

                if (item == nullptr)
                {
                    continue;
                }

                native::SynthStatus status{};

                if (auto const found = m_statuses.find(definition.Id); found != m_statuses.end())
                {
                    status = found->second;
                }

                auto const running = status.State == native::SynthState::Running;
                anyRunning = anyRunning || running;

                item->SetStatusText(StatusText(status));
                item->SetIsBusy(status.State == native::SynthState::Loading);
                item->SetWarning(status.State == native::SynthState::Failed || (running && status.PartlyLoaded));
                item->SetInUse(running && status.InUse);
                item->SetVoicesText(running ? res::FormatString(L"VoicesFormat", status.ActiveVoices) : winrt::hstring{});

                if (status.MelodicPresetCount > 0 || status.DrumKitCount > 0)
                {
                    auto detail = std::wstring{ res::FormatString(L"DetailFormat", status.MelodicPresetCount, status.DrumKitCount) };

                    if (!status.BankName.empty())
                    {
                        detail += L" \u00B7 ";
                        detail += status.BankName;
                    }

                    item->SetDetailText(winrt::hstring{ detail });
                }

                item->SetAccessibleNames(
                    res::FormatString(L"CardAccessibleNameFormat", definition.Name, std::wstring{ StatusText(status) }),
                    res::FormatString(L"ToggleAccessibleNameFormat", definition.Name),
                    res::FormatString(L"VolumeAccessibleNameFormat", definition.Name),
                    res::FormatString(L"MoreAccessibleNameFormat", definition.Name));

                anyUnavailable = anyUnavailable || (status.State == native::SynthState::Failed);
            }

            auto const audio = host.AudioStatus();

            AudioStatusTextBlock().Text(AudioStatusText(audio, anyRunning));
            AudioStatusIcon().Foreground(ThemeBrush(audio.Unavailable ? L"SystemFillColorCautionBrush" : L"TextFillColorSecondaryBrush"));

            UpdateTray();

            if (m_saveRequested && ::GetTickCount64() >= m_saveDueTick)
            {
                SaveNow();
            }
        }
        MIDI_SF2SYNTH_CATCH_AND_LOG(L"Unable to refresh the synths.")
    }

    _Use_decl_annotations_
    void MainWindow::SetSynthEnabled(std::wstring const& id, bool enabled) noexcept
    {
        try
        {
            auto* const definition = FindDefinition(id);

            if (definition == nullptr || definition->Enabled == enabled)
            {
                return;
            }

            definition->Enabled = enabled;

            if (auto* const item = Impl(FindItem(id)))
            {
                item->SetIsOn(enabled);
            }

            SaveNow();

            if (enabled)
            {
                native::SynthHost::Current().Enable(*definition);
            }
            else
            {
                native::SynthHost::Current().Disable(id);
            }

            UpdateTray();
        }
        MIDI_SF2SYNTH_CATCH_AND_LOG(L"Unable to turn a synth on or off.")
    }

    _Use_decl_annotations_
    void MainWindow::OnSynthToggled(foundation::IInspectable const& sender, xaml::RoutedEventArgs const&)
    {
        try
        {
            auto const toggle = sender.try_as<controls::ToggleSwitch>();

            if (toggle == nullptr)
            {
                return;
            }

            // Setting IsOn from the item raises this too. Only a change the definition does not
            // already have is the customer's.
            SetSynthEnabled(TagOf(sender), toggle.IsOn());
        }
        MIDI_SF2SYNTH_CATCH_AND_LOG(L"Unable to turn a synth on or off.")
    }

    _Use_decl_annotations_
    void MainWindow::OnVolumeChanged(foundation::IInspectable const& sender, controls::Primitives::RangeBaseValueChangedEventArgs const& args)
    {
        try
        {
            auto const id = TagOf(sender);
            auto* const definition = FindDefinition(id);

            if (definition == nullptr)
            {
                return;
            }

            auto const value = (std::clamp)(args.NewValue(), sf::Synthesizer::MinimumUserVolumeDb, sf::Synthesizer::MaximumUserVolumeDb);

            if (std::fabs(value - definition->VolumeDb) < VolumeEpsilon)
            {
                return;
            }

            definition->VolumeDb = value;

            native::SynthHost::Current().SetVolume(id, value);

            if (auto* const item = Impl(FindItem(id)))
            {
                item->SetVolume(value, VolumeText(value));
            }

            RequestSave();
        }
        MIDI_SF2SYNTH_CATCH_AND_LOG(L"Unable to change a synth's volume.")
    }

    _Use_decl_annotations_
    void MainWindow::OnSilenceClick(foundation::IInspectable const&, xaml::RoutedEventArgs const&)
    {
        native::SynthHost::Current().SilenceAll();
    }

    _Use_decl_annotations_
    void MainWindow::OnAddSynthClick(foundation::IInspectable const&, xaml::RoutedEventArgs const&)
    {
        AddSynthAsync();
    }

    winrt::fire_and_forget MainWindow::AddSynthAsync()
    {
        auto strong = get_strong();

        try
        {
            if (m_definitions.size() >= native::SynthStore::MaximumSynths)
            {
                ShowNotice(res::GetString(L"ErrorTooManySynths"), controls::InfoBarSeverity::Warning);
                co_return;
            }

            auto& settings = native::AppSettings::Current();

            auto const path = native::PickSoundFontFile(
                WindowHandle(),
                settings.LastSoundFontFolder(),
                std::wstring{ res::GetString(L"OpenDialogTitle") },
                std::wstring{ res::GetString(L"OpenDialogFilter") },
                std::wstring{ res::GetString(L"OpenDialogAllFiles") });

            if (path.empty())
            {
                co_return;
            }

            {
                std::wstring folder{ path };

                if (SUCCEEDED(::PathCchRemoveFileSpec(folder.data(), folder.size() + 1)))
                {
                    folder.resize(wcslen(folder.c_str()));
                    settings.LastSoundFontFolder(folder);
                }
            }

            // Two synths may share a bank, but not a name, or no one could tell them apart in
            // an app's device list.
            auto const baseName = CleanName(StemOf(path));
            auto const root = baseName.empty() ? std::wstring{ res::GetString(L"DefaultSynthName") } : baseName;

            auto const nameTaken = [this](std::wstring const& candidate)
                {
                    return std::any_of(m_definitions.begin(), m_definitions.end(),
                        [&candidate](native::SynthDefinition const& other)
                        {
                            return ::CompareStringOrdinal(other.Name.c_str(), -1, candidate.c_str(), -1, TRUE) == CSTR_EQUAL;
                        });
                };

            auto name = root;

            for (uint32_t suffix = 2; suffix < 100 && nameTaken(name); suffix++)
            {
                auto const ending = L" " + std::to_wstring(suffix);
                auto trimmed = root;

                if (trimmed.size() + ending.size() > native::SynthStore::MaximumNameLength)
                {
                    trimmed.resize(native::SynthStore::MaximumNameLength - ending.size());
                }

                name = trimmed + ending;
            }

            native::SynthDefinition definition{};
            definition.Id = native::SynthStore::NewId();
            definition.Name = name;
            definition.SoundFontPath = path;
            definition.ProductInstanceId = native::SynthStore::NewProductInstanceId();
            definition.VolumeDb = 0.0;
            definition.Enabled = true;

            m_definitions.push_back(definition);
            AddItem(definition);
            UpdateEmptyState();
            SaveNow();

            native::SynthHost::Current().Enable(definition);

            RefreshFromHost();
        }
        MIDI_SF2SYNTH_CATCH_AND_LOG(L"Unable to add a synth.")
    }

    _Use_decl_annotations_
    void MainWindow::OnMoreClick(foundation::IInspectable const& sender, xaml::RoutedEventArgs const&)
    {
        try
        {
            auto const anchor = sender.try_as<xaml::FrameworkElement>();
            auto const id = TagOf(sender);

            if (anchor == nullptr || FindDefinition(id) == nullptr)
            {
                return;
            }

            auto weak = get_weak();

            controls::MenuFlyout menu{};

            auto const addItem = [&menu](winrt::hstring const& text, wchar_t const* glyph, std::function<void()> action)
                {
                    controls::MenuFlyoutItem item{};
                    item.Text(text);

                    controls::FontIcon icon{};
                    icon.Glyph(glyph);
                    item.Icon(icon);

                    item.Click([action](auto&&, auto&&)
                        {
                            try
                            {
                                action();
                            }
                            MIDI_SF2SYNTH_CATCH_AND_LOG(L"A synth menu command failed.")
                        });

                    menu.Items().Append(item);
                };

            addItem(res::GetString(L"MenuRename"), L"\xE8AC", [weak, id]()
                {
                    if (auto strong = weak.get())
                    {
                        strong->RenameSynthAsync(id);
                    }
                });

            addItem(res::GetString(L"MenuShowInFolder"), L"\xE838", [weak, id]()
                {
                    if (auto strong = weak.get())
                    {
                        strong->ShowInFolder(id);
                    }
                });

            menu.Items().Append(controls::MenuFlyoutSeparator{});

            addItem(res::GetString(L"MenuRemove"), L"\xE74D", [weak, id]()
                {
                    if (auto strong = weak.get())
                    {
                        strong->RemoveSynthAsync(id);
                    }
                });

            menu.ShowAt(anchor);
        }
        MIDI_SF2SYNTH_CATCH_AND_LOG(L"Unable to show the synth menu.")
    }

    _Use_decl_annotations_
    void MainWindow::OnRenameTextChanged(foundation::IInspectable const&, controls::TextChangedEventArgs const&)
    {
        try
        {
            RenameDialog().IsPrimaryButtonEnabled(!CleanName(std::wstring{ RenameTextBox().Text() }).empty());
        }
        MIDI_SF2SYNTH_CATCH_AND_LOG(L"Unable to check the new name.")
    }

    winrt::fire_and_forget MainWindow::RenameSynthAsync(std::wstring id)
    {
        auto strong = get_strong();

        try
        {
            auto const* definition = FindDefinition(id);

            if (definition == nullptr || m_dialogOpen)
            {
                co_return;
            }

            m_dialogOpen = true;
            auto const reset = wil::scope_exit([this]() { m_dialogOpen = false; });

            RenameTextBox().Text(winrt::hstring{ definition->Name });
            RenameTextBox().SelectAll();
            RenameDialog().XamlRoot(Content().XamlRoot());

            auto const result = co_await RenameDialog().ShowAsync();

            if (result != controls::ContentDialogResult::Primary)
            {
                co_return;
            }

            auto const name = CleanName(std::wstring{ RenameTextBox().Text() });

            // Looked up again: the list may have changed while the dialog was open.
            auto* const current = FindDefinition(id);

            if (name.empty() || current == nullptr || current->Name == name)
            {
                co_return;
            }

            current->Name = name;

            ApplyDefinitionToItem(*current);
            SaveNow();

            native::SynthHost::Current().Rename(id, name);

            RefreshFromHost();
        }
        MIDI_SF2SYNTH_CATCH_AND_LOG(L"Unable to rename a synth.")
    }

    winrt::fire_and_forget MainWindow::RemoveSynthAsync(std::wstring id)
    {
        auto strong = get_strong();

        try
        {
            auto const* definition = FindDefinition(id);

            if (definition == nullptr || m_dialogOpen)
            {
                co_return;
            }

            m_dialogOpen = true;
            auto const reset = wil::scope_exit([this]() { m_dialogOpen = false; });

            auto const inUse = m_statuses.contains(id) && m_statuses[id].InUse &&
                m_statuses[id].State == native::SynthState::Running;

            RemoveDialogText().Text(res::FormatString(
                inUse ? L"RemoveDialogInUseFormat" : L"RemoveDialogFormat", definition->Name));

            RemoveDialog().XamlRoot(Content().XamlRoot());

            auto const result = co_await RemoveDialog().ShowAsync();

            if (result != controls::ContentDialogResult::Primary || FindDefinition(id) == nullptr)
            {
                co_return;
            }

            native::SynthHost::Current().Forget(id);

            std::erase_if(m_definitions, [&id](native::SynthDefinition const& other) { return other.Id == id; });
            m_statuses.erase(id);

            for (uint32_t i = 0; i < m_items.Size(); i++)
            {
                if (m_items.GetAt(i).Id() == id)
                {
                    m_items.RemoveAt(i);
                    break;
                }
            }

            UpdateEmptyState();
            SaveNow();
            UpdateTray();
        }
        MIDI_SF2SYNTH_CATCH_AND_LOG(L"Unable to remove a synth.")
    }

    _Use_decl_annotations_
    void MainWindow::ShowInFolder(std::wstring const& id) noexcept
    {
        try
        {
            auto const* definition = FindDefinition(id);

            if (definition == nullptr)
            {
                return;
            }

            wil::unique_any<PIDLIST_ABSOLUTE, decltype(&::ILFree), ::ILFree> item{ ::ILCreateFromPathW(definition->SoundFontPath.c_str()) };

            if (!item || FAILED(::SHOpenFolderAndSelectItems(item.get(), 0, nullptr, 0)))
            {
                ShowNotice(res::GetString(L"FailureFileNotFound"), controls::InfoBarSeverity::Warning);
            }
        }
        MIDI_SF2SYNTH_CATCH_AND_LOG(L"Unable to show the file in its folder.")
    }

    _Use_decl_annotations_
    void MainWindow::ShowNotice(winrt::hstring const& message, controls::InfoBarSeverity severity) noexcept
    {
        try
        {
            NoticeBar().Severity(severity);
            NoticeBar().Message(message);
            NoticeBar().IsOpen(true);
        }
        MIDI_SF2SYNTH_CATCH_AND_LOG(L"Unable to show a notice.")
    }

    // --------------------------------------------------------------------- notification area

    void MainWindow::InitializeNotificationArea() noexcept
    {
        try
        {
            auto weak = get_weak();

            // Wired up whether or not the icon shows, because the setting can be turned on later
            // and an icon whose menu does nothing is worse than no icon.
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
                        strong->RestoreFromNotificationArea();
                        strong->ConfirmExitAsync();
                    }
                });

            m_tray.SetSilenceHandler([]()
                {
                    native::SynthHost::Current().SilenceAll();
                });

            m_tray.SetToggleSynthHandler([weak](std::wstring const& id)
                {
                    if (auto strong = weak.get())
                    {
                        if (auto const* definition = strong->FindDefinition(id))
                        {
                            strong->SetSynthEnabled(id, !definition->Enabled);
                        }
                    }
                });

            if (native::AppSettings::Current().MinimizeToNotificationArea())
            {
                m_tray.Show();
                UpdateTray();

                // A start minimized launch happened before there was an icon to hide behind.
                if (auto const handle = WindowHandle(); handle != nullptr && ::IsIconic(handle))
                {
                    TryHideToNotificationArea();
                }
            }
        }
        MIDI_SF2SYNTH_CATCH_AND_LOG(L"Unable to set up the notification area.")
    }

    bool MainWindow::TryHideToNotificationArea() noexcept
    {
        try
        {
            if (m_exiting || m_closing || m_restoringFromNotificationArea)
            {
                return false;
            }

            // The icon, not the setting, so a PC where the icon could not be added never hides
            // the only way back to the app.
            if (!m_tray.IsVisible())
            {
                return false;
            }

            auto const handle = WindowHandle();

            if (handle == nullptr || !::IsWindowVisible(handle))
            {
                return false;
            }

            if (!::IsIconic(handle))
            {
                m_chrome.SavePlacement();
            }

            ::ShowWindow(handle, SW_HIDE);

            return true;
        }
        MIDI_SF2SYNTH_CATCH_AND_LOG(L"Unable to hide the window to the notification area.")

        return false;
    }

    void MainWindow::RestoreFromNotificationArea() noexcept
    {
        try
        {
            auto const handle = WindowHandle();

            if (handle == nullptr)
            {
                return;
            }

            m_restoringFromNotificationArea = true;
            auto const reset = wil::scope_exit([this]() { m_restoringFromNotificationArea = false; });

            // A window hidden while minimized needs both steps, and the flag above keeps the
            // brief minimized-but-visible moment from hiding it again.
            ::ShowWindow(handle, SW_SHOW);

            if (::IsIconic(handle))
            {
                ::ShowWindow(handle, SW_RESTORE);
            }

            ::SetForegroundWindow(handle);
        }
        MIDI_SF2SYNTH_CATCH_AND_LOG(L"Unable to show the window from the notification area.")
    }

    void MainWindow::UpdateTray() noexcept
    {
        try
        {
            if (!m_tray.IsVisible())
            {
                return;
            }

            std::vector<native::TraySynthItem> items{};
            uint32_t onCount{ 0 };

            for (auto const& definition : m_definitions)
            {
                native::TraySynthItem item{};

                item.Id = definition.Id;
                item.Name = definition.Name;
                item.IsOn = definition.Enabled;

                onCount += definition.Enabled ? 1u : 0u;

                if (auto const found = m_statuses.find(definition.Id); found != m_statuses.end())
                {
                    if (found->second.State == native::SynthState::Running && found->second.InUse)
                    {
                        item.Detail = std::wstring{ res::GetString(L"TrayInUse") };
                    }
                    else if (found->second.State == native::SynthState::Failed)
                    {
                        item.Detail = std::wstring{ res::GetString(L"TrayFailed") };
                    }
                }

                items.push_back(std::move(item));
            }

            m_tray.Update(
                std::wstring{ res::FormatString(L"TrayTooltipFormat", onCount, m_definitions.size()) },
                std::move(items));
        }
        MIDI_SF2SYNTH_CATCH_AND_LOG(L"Unable to update the notification area.")
    }

    winrt::fire_and_forget MainWindow::ConfirmExitAsync()
    {
        auto strong = get_strong();

        try
        {
            uint32_t inUse{ 0 };

            for (auto const& [id, status] : m_statuses)
            {
                if (status.State == native::SynthState::Running && status.InUse)
                {
                    inUse++;
                }
            }

            if (inUse > 0)
            {
                if (m_dialogOpen)
                {
                    co_return;
                }

                m_dialogOpen = true;
                auto const reset = wil::scope_exit([this]() { m_dialogOpen = false; });

                ExitDialogText().Text(inUse == 1
                    ? res::GetString(L"ExitDialogMessageOne")
                    : res::FormatString(L"ExitDialogMessageFormat", inUse));

                ExitDialog().XamlRoot(Content().XamlRoot());

                auto const result = co_await ExitDialog().ShowAsync();

                if (result != controls::ContentDialogResult::Primary)
                {
                    co_return;
                }
            }

            ExitApp();
        }
        MIDI_SF2SYNTH_CATCH_AND_LOG(L"Unable to close the app.")
    }

    void MainWindow::ExitApp() noexcept
    {
        try
        {
            m_exiting = true;
            Close();
        }
        MIDI_SF2SYNTH_CATCH_AND_LOG(L"Unable to close the app.")
    }
}
