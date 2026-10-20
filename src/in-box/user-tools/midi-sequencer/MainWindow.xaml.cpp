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
#include "StringResources.h"
#include "resource.h"

namespace res = ::midisequencer::resources;

namespace winrt::midisequencer::implementation
{
    namespace
    {
        constexpr int32_t DefaultWindowWidth = 1400;
        constexpr int32_t DefaultWindowHeight = 920;

        // How often the playhead, the displays and the launcher are redrawn while playing.
        constexpr std::chrono::milliseconds FrameInterval{ 30 };

        bool FocusIsInText(xaml::XamlRoot const& root) noexcept
        {
            try
            {
                if (root == nullptr)
                {
                    return false;
                }

                auto const focused = input::FocusManager::GetFocusedElement(root);

                return focused.try_as<controls::TextBox>() != nullptr ||
                    focused.try_as<controls::NumberBox>() != nullptr ||
                    focused.try_as<controls::ComboBox>() != nullptr;
            }
            catch (...)
            {
                return false;
            }
        }
    }

    MainWindow::MainWindow()
    {
        // XAML objects must not call InitializeComponent during construction; winrt::make does it.
    }

    void MainWindow::RestoreWindowPlacement() noexcept
    {
        // static, because this runs before the chrome is initialized
        midiapp::WindowChrome::RestorePlacement(*this, seq::AppSettings::Current(), DefaultWindowWidth, DefaultWindowHeight);
    }

    _Use_decl_annotations_
    void MainWindow::OnRootLoaded(foundation::IInspectable const&, xaml::RoutedEventArgs const&)
    {
        if (m_loaded)
        {
            return;
        }

        m_loaded = true;

        try
        {
            midiapp::SetEndpointErrorHandler([](std::wstring_view message)
            {
                MIDI_SEQUENCER_LOG_INFO(std::wstring{ message }.c_str());
            });

            midiapp::SetMidiCiErrorHandler([](std::wstring_view message, winrt::hresult)
            {
                MIDI_SEQUENCER_LOG_INFO(std::wstring{ message }.c_str());
            });

            InitializeStaticText();
            InitializeWindowChrome();
            ApplyPalette();

            auto& settings = seq::AppSettings::Current();
            m_barWidth = settings.BarWidth();
            m_directory->ApplyOffsets(settings.ApplyDeviceOffsets());

            CreateCanvases();
            InitializeKeyboard();

            // Launch quantize, snap and value display choices.
            {
                std::vector<std::pair<int64_t, std::wstring_view>> const launchChoices{
                    { 0, L"LaunchImmediately" }, { 240, L"GridSixteenth" }, { 480, L"GridEighth" }, { 960, L"GridQuarter" },
                    { 1920, L"GridHalf" }, { 3840, L"GridOneBar" }, { 7680, L"GridTwoBars" }, { 15360, L"GridFourBars" } };

                int32_t selected{ 5 };

                for (size_t i = 0; i < launchChoices.size(); ++i)
                {
                    LaunchCombo().Items().Append(winrt::box_value(res::GetString(launchChoices[i].second)));

                    if (static_cast<uint32_t>(launchChoices[i].first) == settings.LaunchQuantizeTicks())
                    {
                        selected = static_cast<int32_t>(i);
                    }
                }

                LaunchCombo().SelectedIndex(selected);

                std::vector<std::pair<int64_t, std::wstring_view>> const snapChoices{
                    { 0, L"GridOff" }, { 120, L"GridThirtySecond" }, { 240, L"GridSixteenth" }, { 480, L"GridEighth" },
                    { 960, L"GridQuarter" }, { 1920, L"GridHalf" }, { 3840, L"GridOneBar" } };

                selected = 2;

                for (size_t i = 0; i < snapChoices.size(); ++i)
                {
                    SnapCombo().Items().Append(winrt::box_value(res::GetString(snapChoices[i].second)));

                    if (static_cast<uint32_t>(snapChoices[i].first) == settings.SnapTicks())
                    {
                        selected = static_cast<int32_t>(i);
                    }
                }

                SnapCombo().SelectedIndex(selected);

                ValuesAsCombo().Items().Append(winrt::box_value(res::GetString(L"ValuesMidi2")));
                ValuesAsCombo().Items().Append(winrt::box_value(res::GetString(L"ValuesMidi1")));
                ValuesAsCombo().Items().Append(winrt::box_value(res::GetString(L"ValuesPercent")));
                ValuesAsCombo().SelectedIndex(static_cast<int32_t>(settings.ValuesAs()));
            }

            LauncherToggle().IsChecked(settings.ShowLauncher());
            m_headerWidth = settings.HeaderWidth();
            m_launcherWidth = settings.LauncherWidth();
            InitializeArrangeSplitters();
            ApplyArrangeColumns();
            EditorPanel().Height(settings.EditorHeight());

            m_roll.SetPalette(&m_palette);
            m_roll.SetSnap(settings.SnapTicks());
            m_roll.SetValuesAs(settings.ValuesAs());

            seq::RollStrings strings{};
            strings.Velocity = res::GetString(L"LaneVelocity");
            strings.Midi2Velocity = res::GetString(L"LaneVelocity16");
            strings.Midi1Velocity = res::GetString(L"LaneVelocity7");
            strings.PercentVelocity = res::GetString(L"LaneVelocityPercent");
            strings.EmptyClip = res::GetString(L"EditorNoClip");
            m_roll.SetStrings(strings);

            InitializePatternEditor();
            InitializeGeneratorEditor();

            if (settings.ShowMixer())
            {
                ShowMixer(true);
            }

            m_frameTimer = xaml::DispatcherTimer{};
            m_frameTimer.Interval(FrameInterval);
            m_frameTimer.Tick([weak = get_weak()](auto&&, auto&&)
            {
                if (auto strong = weak.get())
                {
                    strong->OnFrame();
                }
            });

            m_serviceTimer = xaml::DispatcherTimer{};
            m_serviceTimer.Interval(std::chrono::seconds{ 3 });
            m_serviceTimer.Tick([weak = get_weak()](auto&&, auto&&)
            {
                if (auto strong = weak.get())
                {
                    strong->OnServiceTimer();
                }
            });
            m_serviceTimer.Start();

            m_autosaveTimer = xaml::DispatcherTimer{};
            m_autosaveTimer.Interval(std::chrono::seconds{ 2 });
            m_autosaveTimer.Tick([weak = get_weak()](auto&&, auto&&)
            {
                if (auto strong = weak.get())
                {
                    strong->m_autosaveTimer.Stop();
                    strong->AutosaveAsync();
                }
            });

            if (auto const appWindow = AppWindow(); appWindow != nullptr)
            {
                m_closingToken = appWindow.Closing([weak = get_weak()](auto&&, winrt::Microsoft::UI::Windowing::AppWindowClosingEventArgs const& args)
                {
                    if (auto strong = weak.get())
                    {
                        strong->OnWindowClosing(args);
                    }
                });
            }

            Closed([weak = get_weak()](auto&&, auto&&)
            {
                if (auto strong = weak.get())
                {
                    strong->Shutdown();
                }
            });

            auto const& startup = App::StartupOptions();

            if (!startup.File.empty())
            {
                NewDocument();
                OpenFileAsync(startup.File);
            }
            else if (startup.Sample)
            {
                OpenSample();
            }
            else
            {
                NewDocument();
            }

            midiapp::EndpointCatalog::Current().SetChangedHandler([weak = get_weak(), queue = DispatcherQueue()]()
            {
                if (queue == nullptr)
                {
                    return;
                }

                queue.TryEnqueue([weak]()
                {
                    if (auto strong = weak.get())
                    {
                        strong->RefreshEndpoints();
                    }
                });
            });

            StartMidiAsync();
            UpdateStatusBar();

            // The first thing to have focus is the timeline, not the title bar's first button.
            if (m_scrollLaneCanvas != nullptr)
            {
                m_scrollLaneCanvas.IsTabStop(true);
                m_scrollLaneCanvas.UseSystemFocusVisuals(false);
                m_scrollLaneCanvas.Focus(xaml::FocusState::Programmatic);
            }
        }
        MIDI_SEQUENCER_CATCH_AND_LOG(L"Unable to finish loading the window.")
    }

    void MainWindow::InitializeStaticText() noexcept
    {
        try
        {
            Title(res::GetString(L"AppDisplayName"));
            AppTitleTextBlock().Text(res::GetString(L"AppDisplayName"));
        }
        MIDI_SEQUENCER_CATCH_AND_LOG(L"Unable to set the window text.")
    }

    void MainWindow::InitializeWindowChrome() noexcept
    {
        try
        {
            midiapp::ApplyPreviewBadgeVisibility(PreviewChiclet());
            midiapp::MakeLiveStatusRegion(MessageStatusText());

            midiapp::WindowChromeElements elements{};
            elements.Window = *this;
            elements.Root = RootGrid();
            elements.Fill = WindowFill();
            elements.Tint = WindowTint();
            elements.TitleBar = AppTitleBar();
            elements.LeftInset = TitleBarLeftInsetColumn();
            elements.RightInset = TitleBarRightInsetColumn();

            m_chrome.Initialize(elements, seq::AppSettings::Current());
            m_chrome.SetWindowIconFromResource(IDI_APPICON);

            // The style picked in Appearance: its brushes, the colors the drawn parts use, and
            // the plates behind the controls.
            SetUpPlates();
            LoadStyles();
            ApplyStyle();

            // Flyouts take the context menus' tinted backdrop, and lists and spin pop-ups the color
            // that backdrop shows over the window, so they match it. Tooltips take the window's color.
            midiapp::UseMenuBackdropForLists();
            midiapp::TintToolTips(seq::AppSettings::Current().UseCustomBackgroundColor(), seq::AppSettings::Current().BackgroundColorArgb());

            AppTitleBarIcon().Source(midiapp::WindowChrome::LoadIconImageSource(IDI_APPICON, 32));
            AlwaysOnTopToggle().IsChecked(seq::AppSettings::Current().AlwaysOnTop());

            RootGrid().ActualThemeChanged([weak = get_weak()](auto&&, auto&&)
            {
                if (auto strong = weak.get())
                {
                    strong->ApplyPalette();
                    strong->ColorDialogs();
                    strong->RebuildHeaders();
                    strong->RebuildSceneHeaders();
                    strong->InvalidateArrange();
                    strong->InvalidateEditor();

                    // The strips take their colors from the theme when they're built.
                    strong->RebuildMixer();
                }
            });
        }
        MIDI_SEQUENCER_CATCH_AND_LOG(L"Unable to set up the window chrome.")
    }

    void MainWindow::ApplyPalette() noexcept
    {
        try
        {
            auto const light = RootGrid().ActualTheme() == xaml::ElementTheme::Light;
            auto const& style = light ? m_styleLight : m_styleDark;

            m_palette = style != nullptr ? seq::MakePalette(style) : seq::MakePalette(light);
            m_renderer.InvalidateCaches();

            // The scroll bar's look and height come from the theme too.
            m_timelineBar.Palette(&m_palette);
            PlaceTimelineBar();
            ApplyStrips();
            ApplyPlates();
        }
        MIDI_SEQUENCER_CATCH_AND_LOG(L"Unable to apply the theme colors.")
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

    void MainWindow::InitializeKeyboard() noexcept
    {
        try
        {
            using winrt::Windows::System::VirtualKey;
            using winrt::Windows::System::VirtualKeyModifiers;

            auto const add = [this](VirtualKey key, VirtualKeyModifiers modifiers)
            {
                input::KeyboardAccelerator accelerator{};
                accelerator.Key(key);
                accelerator.Modifiers(modifiers);
                accelerator.Invoked({ this, &MainWindow::OnKeyboardAccelerator });
                RootGrid().KeyboardAccelerators().Append(accelerator);
            };

            add(VirtualKey::Space, VirtualKeyModifiers::None);
            add(VirtualKey::Z, VirtualKeyModifiers::Control);
            add(VirtualKey::Y, VirtualKeyModifiers::Control);
            add(VirtualKey::Delete, VirtualKeyModifiers::None);
            add(VirtualKey::D, VirtualKeyModifiers::Control);
            add(VirtualKey::Home, VirtualKeyModifiers::None);
            add(VirtualKey::L, VirtualKeyModifiers::None);
            add(VirtualKey::R, VirtualKeyModifiers::None);
            add(VirtualKey::T, VirtualKeyModifiers::Control);
            add(VirtualKey::V, VirtualKeyModifiers::Control);

            // The keyboard accelerators' own tooltips would repeat what the buttons already say.
            RootGrid().KeyboardAcceleratorPlacementMode(input::KeyboardAcceleratorPlacementMode::Hidden);
        }
        MIDI_SEQUENCER_CATCH_AND_LOG(L"Unable to set up the keyboard shortcuts.")
    }

    _Use_decl_annotations_
    void MainWindow::OnKeyboardAccelerator(input::KeyboardAccelerator const& sender, input::KeyboardAcceleratorInvokedEventArgs const& args)
    {
        try
        {
            using winrt::Windows::System::VirtualKey;
            using winrt::Windows::System::VirtualKeyModifiers;

            // Typing in a box is typing, not a command.
            if (FocusIsInText(Content().XamlRoot()))
            {
                args.Handled(false);
                return;
            }

            auto const control = sender.Modifiers() == VirtualKeyModifiers::Control;
            args.Handled(true);

            switch (sender.Key())
            {
            case VirtualKey::Space:
                if (m_engine != nullptr && m_engine->IsPlaying())
                {
                    Stop();
                }
                else
                {
                    Play(m_position);
                }
                break;

            case VirtualKey::Z:
                Undo();
                break;

            case VirtualKey::V:
                PasteMessagesAsync();
                break;

            case VirtualKey::Y:
                Redo();
                break;

            case VirtualKey::Delete:
                if (m_selectedLanePoint != SIZE_MAX || m_selectedTempoPoint != SIZE_MAX)
                {
                    DeleteSelectedAutomationPoint();
                }
                else if (!m_roll.Selection().empty() && !m_editorClipId.empty())
                {
                    ApplyRollEdit(m_roll.DeleteSelection());
                }
                else
                {
                    DeleteSelectedPlacement();
                }
                break;

            case VirtualKey::D:
                if (control)
                {
                    DuplicateSelectedPlacement();
                }
                break;

            case VirtualKey::Home:
                SetPosition(0);
                break;

            case VirtualKey::L:
                LoopToggle().IsChecked(!LoopToggle().IsChecked().GetBoolean());
                ApplyEngineSettings();
                InvalidateArrange();
                break;

            case VirtualKey::R:
                RecordButton().IsChecked(!RecordButton().IsChecked().GetBoolean());
                OnRecordClick(nullptr, nullptr);
                break;

            case VirtualKey::T:
                if (control)
                {
                    AddTrack(false);
                }
                break;

            default:
                args.Handled(false);
                break;
            }
        }
        MIDI_SEQUENCER_CATCH_AND_LOG(L"Unable to run a keyboard shortcut.")
    }

    _Use_decl_annotations_
    void MainWindow::OnSettingsClick(foundation::IInspectable const& sender, xaml::RoutedEventArgs const&)
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
                seq::AppSettings::Current(),
                strings,
                [weak = get_weak()]() noexcept
                {
                    if (auto strong = weak.get())
                    {
                        auto const& settings = seq::AppSettings::Current();

                        strong->m_chrome.ApplyTheme();
                        midiapp::TintToolTips(settings.UseCustomBackgroundColor(), settings.BackgroundColorArgb());
                        strong->ColorDialogs();

                        // A style made of a material takes the window color into its own colors.
                        std::optional<uint32_t> const tint = settings.UseCustomBackgroundColor()
                            ? std::optional<uint32_t>{ settings.BackgroundColorArgb() | 0xFF000000u } : std::nullopt;

                        if (strong->m_style != nullptr && strong->m_style->WindowColor == themeengine::WindowColorUse::Material && tint != strong->m_styleTint)
                        {
                            strong->RefreshStyle();
                        }
                    }
                },
                nullptr,
                MakeStylePicker());
        }
        MIDI_SEQUENCER_CATCH_AND_LOG(L"Unable to show the appearance settings.")
    }

    _Use_decl_annotations_
    void MainWindow::OnAlwaysOnTopToggled(foundation::IInspectable const&, xaml::RoutedEventArgs const&)
    {
        try
        {
            seq::AppSettings::Current().AlwaysOnTop(AlwaysOnTopToggle().IsChecked().GetBoolean());
            m_chrome.ApplyAlwaysOnTop();
        }
        MIDI_SEQUENCER_CATCH_AND_LOG(L"Unable to change the always on top setting.")
    }

    void MainWindow::UpdateTitle() noexcept
    {
        try
        {
            auto name = m_doc.Name.empty() ? std::wstring{ res::GetString(L"UntitledSequence") } : m_doc.Name;

            if (m_dirty)
            {
                name = L"\u2022 " + name;
            }

            auto const title = res::FormatString(L"WindowTitleFormat", name);
            Title(title);
            AppTitleTextBlock().Text(title);
        }
        MIDI_SEQUENCER_CATCH_AND_LOG(L"Unable to update the window title.")
    }

    _Use_decl_annotations_
    void MainWindow::ShowMessage(winrt::hstring const& text) noexcept
    {
        try
        {
            MessageStatusText().Text(text);
        }
        catch (...)
        {
        }
    }

    void MainWindow::UpdateStatusBar() noexcept
    {
        try
        {
            m_serviceRunning = midiapp::IsMidiServiceRunning();

            ServiceStatusText().Text(res::GetString(m_serviceRunning ? L"ServiceRunning" : L"ServiceStopped"));
            ServiceStatusIcon().Glyph(m_serviceRunning ? L"\uE73E" : L"\uE7BA");

            auto const brush = ThemeBrush(m_serviceRunning ? L"SeqPlayBrush" : L"SeqWarnTextBrush");

            if (brush != nullptr)
            {
                ServiceStatusText().Foreground(brush);
                ServiceStatusIcon().Foreground(brush);
            }

            UpdateClockStatus();

            // How many devices the sequence uses, and how many of those aren't here.
            std::set<std::wstring> used{};
            size_t missing{ 0 };

            seq::ForEachTrack(m_doc, [&](seq::Track const& track, size_t)
            {
                if (!track.IsFolder && !track.Destination.Endpoint.IsEmpty() && used.insert(track.Destination.Endpoint.Name).second)
                {
                    if (!m_directory->Resolve(track.Destination.Endpoint).has_value())
                    {
                        ++missing;
                    }
                }

                return true;
            });

            if (used.empty())
            {
                DevicesStatusText().Text(L"");
            }
            else if (missing == 0)
            {
                DevicesStatusText().Text(res::FormatString(used.size() == 1 ? L"DevicesInUseOneFormat" : L"DevicesInUseFormat", used.size()));
            }
            else
            {
                DevicesStatusText().Text(res::FormatString(used.size() == 1 ? L"DevicesInUseOneMissingFormat" : L"DevicesInUseMissingFormat", used.size(), missing));
            }

            // It opens Studio setup, which lists those devices, so it's only there when there are some.
            DevicesStatusButton().Visibility(used.empty() ? xaml::Visibility::Collapsed : xaml::Visibility::Visible);

            ClockOutStatusText().Text(L"");
        }
        MIDI_SEQUENCER_CATCH_AND_LOG(L"Unable to update the status bar.")
    }

    // Where the tempo comes from and where the clock goes.
    void MainWindow::UpdateClockStatus() noexcept
    {
        try
        {
            auto const& clockSettings = seq::AppSettings::Current();

            // Clock and time code to the same device is still one device.
            std::set<std::wstring> followerDevices{};

            for (auto const& output : clockSettings.ClockOutputs())
            {
                followerDevices.insert(output.EndpointId.empty() ? output.EndpointName : output.EndpointId);
            }

            if (clockSettings.TimeCode().IsOn())
            {
                auto const& timeCode = clockSettings.TimeCode();
                followerDevices.insert(timeCode.EndpointId.empty() ? timeCode.EndpointName : timeCode.EndpointId);
            }

            auto const followers = followerDevices.size();

            if (auto const& follow = clockSettings.FollowClock(); follow.IsOn())
            {
                auto const name = follow.EndpointName.empty() ? follow.EndpointId : follow.EndpointName;
                auto const followed = m_engine != nullptr ? m_engine->FollowedBeatsPerMinute() : 0.0;

                if (followed <= 0)
                {
                    ClockStatusText().Text(res::FormatString(L"ClockStatusWaitingFormat", name));
                    return;
                }

                auto const followedText = std::format(L"{:.1f}", followed);

                ClockStatusText().Text(
                    followers == 0 ? res::FormatString(L"ClockStatusFollowingFormat", name, followedText) :
                    followers == 1 ? res::FormatString(L"ClockStatusFollowingSendingOneFormat", name, followedText) :
                    res::FormatString(L"ClockStatusFollowingSendingFormat", name, followedText, followers));
                return;
            }

            auto const bpm = m_doc.Tempo.empty() ? 120.0 : seq::TempoMap{ m_doc.Tempo }.BeatsPerMinuteAtTick(m_position);
            auto const bpmText = std::format(L"{:g}", std::round(bpm * 100.0) / 100.0);

            ClockStatusText().Text(
                followers == 0 ? res::FormatString(L"ClockStatusInternalFormat", bpmText) :
                followers == 1 ? res::FormatString(L"ClockStatusSendingOneFormat", bpmText) :
                res::FormatString(L"ClockStatusSendingFormat", bpmText, followers));
        }
        MIDI_SEQUENCER_CATCH_AND_LOG(L"Unable to update the clock status.")
    }

    void MainWindow::OnServiceTimer() noexcept
    {
        UpdateStatusBar();
    }

    void MainWindow::OnFrame() noexcept
    {
        try
        {
            if (m_engine == nullptr)
            {
                return;
            }

            auto const playing = m_engine->IsPlaying();
            auto const tick = m_engine->PositionTick();

            if (playing)
            {
                m_position = tick;
            }

            // Follow the playhead when it runs off the right edge.
            if (playing)
            {
                auto const x = static_cast<double>(m_position) * (m_barWidth / 3840.0) - m_scrollX;
                auto const width = LaneWidth();

                if (x > width - 24.0 || x < 0)
                {
                    m_scrollX = std::max(0.0, static_cast<double>(m_position) * (m_barWidth / 3840.0) - width * 0.1);
                    UpdateScrollBars();
                    InvalidateArrange();
                }
            }

            UpdatePlayhead();
            UpdateDisplays(m_position);
            RecordArpeggiatedNotes();
            RecordPlayedNotes();
            UpdateSlotRecording();

            // A followed clock's tempo, twice a second.
            if (auto const now = std::chrono::steady_clock::now(); now - m_clockStatusAt > std::chrono::milliseconds{ 500 } && m_engine->IsFollowing())
            {
                m_clockStatusAt = now;
                UpdateClockStatus();
            }

            m_launchViews.clear();

            for (auto& view : m_engine->LaunchState())
            {
                m_launchViews.emplace(view.TrackId, std::move(view));
            }

            InvalidateLaunchers();

            if (m_recording)
            {
                {
                    std::scoped_lock guard{ m_inputLock };

                    if (auto take = m_takes.find(m_recordingPreviewTrackId); take != m_takes.end())
                    {
                        m_recordingPreview = take->second.NotesSoFar(m_position);
                    }
                }

                RecordingStatusText().Text(res::FormatString(L"RecordingStatusFormat", m_recordingPreview.size()));
                InvalidateArrange();
            }

            if (!m_editorClipId.empty())
            {
                InvalidateEditor();
            }

            if (!playing && !m_recording && !m_slotTake.has_value())
            {
                m_frameTimer.Stop();
                UpdateTransport();
                UpdateClockStatus();
                InvalidateArrange();
            }
        }
        MIDI_SEQUENCER_CATCH_AND_LOG(L"Unable to update the playback display.")
    }

    _Use_decl_annotations_
    void MainWindow::OnWindowClosing(winrt::Microsoft::UI::Windowing::AppWindowClosingEventArgs const& args)
    {
        try
        {
            if (!ReadyToClose())
            {
                args.Cancel(true);
            }
        }
        MIDI_SEQUENCER_CATCH_AND_LOG(L"Unable to handle closing the window.")
    }

    bool MainWindow::ReadyToClose() noexcept
    {
        try
        {
            // A save still writing finishes first. One saving for the close closes the window then;
            // any other closes it after, as asked.
            if (m_saving)
            {
                if (!m_closeConfirmed)
                {
                    m_closeRequested = true;
                }

                return false;
            }

            if (m_closeConfirmed)
            {
                return true;
            }

            // What's being recorded goes into the sequence, so closing can ask about it.
            if (m_recording)
            {
                StopRecording();
            }

            if (m_slotTake.has_value())
            {
                FinishSlotRecording(SlotTakeEnd::Stopped);
            }

            // A mixer change still waiting, or words still being typed, are changes to save like
            // any other.
            CommitMixerEdit();
            FlushGeneratorText();

            if (!m_dirty)
            {
                return true;
            }

            // A sequence with a file saves itself as it goes; this is the one still waiting. A file
            // from a newer version, or one that wouldn't save, asks instead.
            if (!m_path.empty() && !m_readOnly && !m_closeSaveFailed)
            {
                m_closeConfirmed = true;
                SaveAsync(false);
                return false;
            }

            AskAboutUnsavedChanges();
            return false;
        }
        catch (...)
        {
            LOG_CAUGHT_EXCEPTION();
            return true;
        }
    }

    void MainWindow::CloseIfAsked() noexcept
    {
        try
        {
            // Another save started since keeps the request until it's done too.
            if (!m_saving && std::exchange(m_closeRequested, false) && ReadyToClose())
            {
                Close();
            }
        }
        MIDI_SEQUENCER_CATCH_AND_LOG(L"Unable to close the window after saving.")
    }

    winrt::fire_and_forget MainWindow::AskAboutUnsavedChanges()
    {
        auto strong = get_strong();

        try
        {
            controls::ContentDialog dialog{};
            dialog.XamlRoot(Content().XamlRoot());
            dialog.Title(winrt::box_value(res::GetString(L"SaveChangesTitle")));
            dialog.Content(winrt::box_value(res::GetString(L"SaveChangesBody")));
            dialog.PrimaryButtonText(res::GetString(L"SaveChangesSave"));
            dialog.SecondaryButtonText(res::GetString(L"SaveChangesDiscard"));
            dialog.CloseButtonText(res::GetString(L"SaveChangesCancel"));
            dialog.DefaultButton(controls::ContentDialogButton::Primary);

            auto const result = co_await dialog.ShowAsync();

            if (result == controls::ContentDialogResult::Primary)
            {
                // Under a name of its own: a new sequence, a file from a newer version, or one that
                // wouldn't save where it was.
                m_closeConfirmed = true;
                SaveAsync(true);
            }
            else if (result == controls::ContentDialogResult::Secondary)
            {
                m_closeConfirmed = true;
                Close();
            }
        }
        MIDI_SEQUENCER_CATCH_AND_LOG(L"Unable to ask about unsaved changes.")
    }
    void MainWindow::Shutdown() noexcept
    {
        if (m_closing)
        {
            return;
        }

        m_closing = true;

        try
        {
            if (m_frameTimer != nullptr) { m_frameTimer.Stop(); }
            if (m_serviceTimer != nullptr) { m_serviceTimer.Stop(); }
            if (m_autosaveTimer != nullptr) { m_autosaveTimer.Stop(); }

            midiapp::EndpointCatalog::Current().SetChangedHandler(nullptr);

            if (m_sources != nullptr)
            {
                m_sources->SetHandler(nullptr);
            }

            // Synchronous on purpose: the note offs have to reach the instrument before this
            // process ends, or a note is left sounding with nothing left to turn it off.
            if (m_engine != nullptr)
            {
                m_engine->Stop();
                m_engine->StopThread();
            }

            StopRemoteControl();
            CloseCiLinks();

            if (m_sources != nullptr)
            {
                m_sources->CloseAll();
            }

            if (m_output != nullptr)
            {
                m_output->CloseAll();
            }

            if (m_session != nullptr)
            {
                m_session.Close();
                m_session = nullptr;
            }

            midiapp::EndpointCatalog::Current().Stop();

            m_chrome.SavePlacement();
            m_chrome.Shutdown();
        }
        MIDI_SEQUENCER_CATCH_AND_LOG(L"Unable to shut down cleanly.")
    }

    _Use_decl_annotations_
    void MainWindow::OnRootDragOver(foundation::IInspectable const&, xaml::DragEventArgs const& args)
    {
        try
        {
            if (args.DataView().Contains(winrt::Windows::ApplicationModel::DataTransfer::StandardDataFormats::StorageItems()))
            {
                args.AcceptedOperation(winrt::Windows::ApplicationModel::DataTransfer::DataPackageOperation::Copy);
            }
        }
        MIDI_SEQUENCER_CATCH_AND_LOG(L"Unable to accept a drag.")
    }

    _Use_decl_annotations_
    winrt::fire_and_forget MainWindow::OnRootDrop(foundation::IInspectable, xaml::DragEventArgs args)
    {
        auto strong = get_strong();

        try
        {
            auto const view = args.DataView();

            if (!view.Contains(winrt::Windows::ApplicationModel::DataTransfer::StandardDataFormats::StorageItems()))
            {
                co_return;
            }

            auto const items = co_await view.GetStorageItemsAsync();

            for (auto const& item : items)
            {
                auto const file = item.try_as<winrt::Windows::Storage::StorageFile>();

                if (file != nullptr)
                {
                    std::wstring const path{ file.Path() };
                    auto const extension = std::filesystem::path{ path }.extension().wstring();

                    if (_wcsicmp(extension.c_str(), L".midisequence") == 0)
                    {
                        if (CanReplaceDocument())
                        {
                            OpenFileAsync(path);
                        }
                        else
                        {
                            OpenInNewWindow(path);
                        }
                    }
                    else
                    {
                        ImportFile(path);
                    }

                    break;
                }
            }
        }
        MIDI_SEQUENCER_CATCH_AND_LOG(L"Unable to open a dropped file.")
    }

    _Use_decl_annotations_
    void MainWindow::OnCloseWindowClick(foundation::IInspectable const&, xaml::RoutedEventArgs const&)
    {
        try
        {
            // Window.Close doesn't raise AppWindow.Closing, so this asks about unsaved changes itself.
            if (ReadyToClose())
            {
                m_closeConfirmed = true;
                Close();
            }
        }
        MIDI_SEQUENCER_CATCH_AND_LOG(L"Unable to close the window.")
    }
}
