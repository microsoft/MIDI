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
#include "BackgroundWork.h"
#include "StringResources.h"
#include "resource.h"

namespace native = ::midikeyboard;
namespace res = ::midikeyboard::resources;

namespace winrt::midikeyboard::implementation
{
    namespace
    {
        using winrt::Windows::UI::Color;

        constexpr int64_t TicksPerMillisecond = 10000;

        // fast enough to feel like a key rebounding, slow enough to see
        constexpr int64_t KeyFadeMilliseconds = 220;

        constexpr double HeldKeyGlowOpacity = 1.0;

        // a key the player is holding while the arpeggiator runs is only part of the chord,
        // so it sits back and lets the note actually sounding read as the bright one
        constexpr double ChordKeyGlowOpacity = 0.32;

        // dragging up half a key height is full pressure
        constexpr double PressureTravelFactor = 0.5;

        constexpr double RibbonLightHeight = 20.0;
        constexpr double RibbonLightInset = 4.0;

        constexpr float RibbonGlowBlurRadius = 26.0f;
        constexpr float RibbonGlowOpacity = 0.95f;

        constexpr uint32_t PitchBendCenter = 0x80000000;
        constexpr uint8_t ModulationController = 1;

        constexpr double MaximumUnsignedValue = 4294967295.0;

        Color MakeColor(uint8_t r, uint8_t g, uint8_t b, uint8_t a = 255) noexcept
        {
            return Color{ a, r, g, b };
        }

        media::SolidColorBrush MakeSolidBrush(Color const& color) noexcept
        {
            media::SolidColorBrush brush{};
            brush.Color(color);
            return brush;
        }

        media::LinearGradientBrush MakeVerticalGradient(Color const& top, Color const& bottom) noexcept
        {
            media::LinearGradientBrush brush{};

            brush.StartPoint(foundation::Point{ 0.0f, 0.0f });
            brush.EndPoint(foundation::Point{ 0.0f, 1.0f });

            media::GradientStop first{};
            first.Color(top);
            first.Offset(0.0);

            media::GradientStop second{};
            second.Color(bottom);
            second.Offset(1.0);

            brush.GradientStops().Append(first);
            brush.GradientStops().Append(second);

            return brush;
        }

        media::Brush LookupBrush(std::wstring_view key) noexcept
        {
            try
            {
                // Lookup, not HasKey: the theme brushes live in a merged dictionary
                return xaml::Application::Current().Resources()
                    .Lookup(winrt::box_value(winrt::hstring{ key })).try_as<media::Brush>();
            }
            catch (...)
            {
            }

            return nullptr;
        }

        uint32_t NormalizedToUnsigned(double normalized) noexcept
        {
            if (normalized <= 0.0)
            {
                return 0;
            }

            if (normalized >= 1.0)
            {
                return 0xFFFFFFFF;
            }

            return static_cast<uint32_t>(std::llround(normalized * MaximumUnsignedValue));
        }

        void AppendChoice(collections::IVector<foundation::IInspectable> const& items, std::wstring_view key) noexcept
        {
            items.Append(winrt::box_value(res::GetString(key)));
        }

        bool IsIndexValid(int32_t index, size_t count) noexcept
        {
            return index >= 0 && static_cast<size_t>(index) < count;
        }

        // the strip trims to one line, so anything longer than a few words needs a tooltip
        void SetStripText(
            controls::TextBlock const& block,
            winrt::hstring const& text,
            winrt::hstring const& toolTip = {}) noexcept
        {
            try
            {
                block.Text(text);

                auto const& tip = toolTip.empty() ? text : toolTip;

                controls::ToolTipService::SetToolTip(block,
                    tip.empty() ? nullptr : winrt::box_value(tip));
            }
            MIDI_KEYBOARD_CATCH_AND_LOG(L"Unable to set the status text.")
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
        midiapp::WindowChrome::RestorePlacement(*this, native::AppSettings::Current(), 1320, 560);
    }

    _Use_decl_annotations_
    void MainWindow::OnRootLoaded(foundation::IInspectable const&, xaml::RoutedEventArgs const&)
    {
        try
        {
            InitializeBrushes();
            InitializeWindowChrome();
            InitializeChoiceLists();
            InitializeControlsFromSettings();

            m_arpeggiator.Initialize(
                m_dispatcherQueue,
                [this](int32_t note, uint16_t velocity)
                {
                    m_arpeggiatorSoundingNote = note;
                    SendNoteOnNow(note, velocity);
                    RefreshKeyGlow(note);
                },
                [this](int32_t note)
                {
                    if (m_arpeggiatorSoundingNote == note)
                    {
                        m_arpeggiatorSoundingNote = -1;
                    }

                    SendNoteOffNow(note);
                    RefreshKeyGlow(note);
                });

            Closed([weak = get_weak()](auto&&, auto&&)
                {
                    if (auto strong = weak.get())
                    {
                        strong->EndAllNotes();
                        strong->m_arpeggiator.Shutdown();
                        strong->StopEndpointWatcher();
                        strong->m_chrome.SavePlacement();
                        strong->m_chrome.Shutdown();
                        strong->ShutdownAsync();
                    }
                });

            m_initialized = true;

            RebuildKeyboard();
            UpdateOctaveDisplay();
            UpdateRibbonsFromValues();

            StartEndpointWatcher();

            ReconnectAsync();

            // the keyboard itself takes the focus, so the computer keyboard plays notes
            // without the player having to click anything first
            KeyboardCanvas().Focus(xaml::FocusState::Programmatic);
        }
        MIDI_KEYBOARD_CATCH_AND_LOG(L"Unable to finish loading the window.")
    }

    void MainWindow::InitializeBrushes() noexcept
    {
        try
        {
            m_whiteKeyBrush = MakeVerticalGradient(MakeColor(252, 252, 252), MakeColor(222, 222, 224));
            m_blackKeyBrush = MakeVerticalGradient(MakeColor(58, 58, 62), MakeColor(16, 16, 18));
            m_keyBorderBrush = MakeSolidBrush(MakeColor(24, 24, 26, 170));
            m_whiteKeyTextBrush = MakeSolidBrush(MakeColor(96, 96, 100));
            m_blackKeyTextBrush = MakeSolidBrush(MakeColor(196, 196, 200));

            m_glowBrush = LookupBrush(L"AccentFillColorDefaultBrush");

            if (m_glowBrush == nullptr)
            {
                m_glowBrush = MakeSolidBrush(MakeColor(0, 153, 204));
            }

            m_glowColor = MakeColor(0, 153, 204);

            if (auto const solid = m_glowBrush.try_as<media::SolidColorBrush>())
            {
                m_glowColor = solid.Color();
            }

            KeyboardFrame().Background(MakeVerticalGradient(MakeColor(26, 26, 30), MakeColor(12, 12, 14)));
            KeyboardFrame().BorderBrush(MakeSolidBrush(MakeColor(0, 0, 0, 140)));

            auto const trackBrush = MakeVerticalGradient(MakeColor(20, 20, 24), MakeColor(38, 38, 44));
            auto const trackBorder = MakeSolidBrush(MakeColor(0, 0, 0, 140));

            PitchRibbonTrack().Background(trackBrush);
            PitchRibbonTrack().BorderBrush(trackBorder);

            ModRibbonTrack().Background(MakeVerticalGradient(MakeColor(20, 20, 24), MakeColor(38, 38, 44)));
            ModRibbonTrack().BorderBrush(MakeSolidBrush(MakeColor(0, 0, 0, 140)));

            PitchRibbonCenterMark().Fill(MakeSolidBrush(MakeColor(150, 150, 158)));
            PitchRibbonLight().Fill(m_glowBrush);
            ModRibbonLight().Fill(m_glowBrush);

            m_pitchRibbon.Track = PitchRibbonCanvas();
            m_pitchRibbon.Light = PitchRibbonLight();
            m_pitchRibbon.GlowHost = PitchRibbonGlowHost();

            m_modRibbon.Track = ModRibbonCanvas();
            m_modRibbon.Light = ModRibbonLight();
            m_modRibbon.GlowHost = ModRibbonGlowHost();
        }
        MIDI_KEYBOARD_CATCH_AND_LOG(L"Unable to create the keyboard brushes.")
    }

    void MainWindow::InitializeWindowChrome() noexcept
    {
        try
        {
            Title(res::GetString(L"AppDisplayName"));
            AppTitleTextBlock().Text(res::GetString(L"AppDisplayName"));

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
        }
        MIDI_KEYBOARD_CATCH_AND_LOG(L"Unable to set up the window chrome.")
    }

    void MainWindow::InitializeChoiceLists() noexcept
    {
        try
        {
            m_endpoints = winrt::single_threaded_observable_vector<appshared::EndpointChoice>();
            m_groups = winrt::single_threaded_observable_vector<appshared::NamedChoice>();

            EndpointComboBox().ItemsSource(m_endpoints);
            GroupComboBox().ItemsSource(m_groups);

            // boxed IInspectable rather than IVector<hstring>: the stock item container renders
            // the former without needing a template
            auto channels = winrt::single_threaded_vector<foundation::IInspectable>();
            for (int32_t channel = 1; channel <= 16; channel++)
            {
                channels.Append(winrt::box_value(res::FormatString(L"ChannelChoiceFormat", channel)));
            }
            ChannelComboBox().ItemsSource(channels);

            auto arpModes = winrt::single_threaded_vector<foundation::IInspectable>();
            AppendChoice(arpModes, L"ArpModeOff");
            AppendChoice(arpModes, L"ArpModeUp");
            AppendChoice(arpModes, L"ArpModeUpDown");
            AppendChoice(arpModes, L"ArpModeUpDownRepeat");
            AppendChoice(arpModes, L"ArpModeDown");
            AppendChoice(arpModes, L"ArpModeRandom");
            AppendChoice(arpModes, L"ArpModeAsPlayed");
            ArpModeComboBox().ItemsSource(arpModes);

            auto arpRates = winrt::single_threaded_vector<foundation::IInspectable>();
            AppendChoice(arpRates, L"ArpRateQuarter");
            AppendChoice(arpRates, L"ArpRateEighth");
            AppendChoice(arpRates, L"ArpRateEighthTriplet");
            AppendChoice(arpRates, L"ArpRateSixteenth");
            AppendChoice(arpRates, L"ArpRateSixteenthTriplet");
            AppendChoice(arpRates, L"ArpRateThirtySecond");
            ArpRateComboBox().ItemsSource(arpRates);

            auto ribbons = winrt::single_threaded_vector<foundation::IInspectable>();
            AppendChoice(ribbons, L"RibbonPositionLeft");
            AppendChoice(ribbons, L"RibbonPositionRight");
            AppendChoice(ribbons, L"RibbonPositionDisabled");
            RibbonPositionComboBox().ItemsSource(ribbons);

            auto velocityModes = winrt::single_threaded_vector<foundation::IInspectable>();
            AppendChoice(velocityModes, L"VelocityModeKeyLocation");
            AppendChoice(velocityModes, L"VelocityModeFixed");
            AppendChoice(velocityModes, L"VelocityModeOff");
            VelocityModeComboBox().ItemsSource(velocityModes);

            auto pressureModes = winrt::single_threaded_vector<foundation::IInspectable>();
            AppendChoice(pressureModes, L"KeyPressureOff");
            AppendChoice(pressureModes, L"KeyPressurePerNote");
            AppendChoice(pressureModes, L"KeyPressureChannel");
            AppendChoice(pressureModes, L"KeyPressurePoly");
            KeyPressureComboBox().ItemsSource(pressureModes);
        }
        MIDI_KEYBOARD_CATCH_AND_LOG(L"Unable to build the choice lists.")
    }

    void MainWindow::InitializeControlsFromSettings() noexcept
    {
        try
        {
            auto const& settings = native::AppSettings::Current();

            m_suppressArpHandlers = true;

            // only the controls that are visible from the start; the settings panel is
            // populated the first time it is opened
            ArpModeComboBox().SelectedIndex(static_cast<int32_t>(settings.Arpeggiator()));
            ArpRateComboBox().SelectedIndex(static_cast<int32_t>(settings.ArpeggiatorRate()));
            ArpBpmBox().Value(settings.ArpeggiatorBpm());

            AlwaysOnTopToggle().IsChecked(settings.AlwaysOnTop());

            ReleaseFlagWhenIdle(&MainWindow::m_suppressArpHandlers);

            m_arpeggiator.Rate(settings.ArpeggiatorBpm(), settings.ArpeggiatorRate());
            m_arpeggiator.Mode(settings.Arpeggiator());

            UpdateConnectionModeLayout();
            UpdateVelocityLayout();
            UpdateRibbonLayout();
        }
        MIDI_KEYBOARD_CATCH_AND_LOG(L"Unable to apply the saved settings.")
    }

    void MainWindow::InitializeSettingsPanelControls() noexcept
    {
        try
        {
            if (m_settingsControlsInitialized)
            {
                return;
            }

            auto const& settings = native::AppSettings::Current();

            m_suppressSettingHandlers = true;

            ConnectionModeRadios().SelectedIndex(static_cast<int32_t>(settings.Connection()));
            ChannelComboBox().SelectedIndex(static_cast<int32_t>(settings.TransmitChannelNumber()) - 1);

            BaseOctaveBox().Value(settings.BaseOctave());
            OctaveCountBox().Value(settings.OctaveCount());
            TransposeBox().Value(settings.Transpose());

            ShowNoteNamesCheckBox().IsChecked(settings.ShowNoteNames());
            ShowComputerKeysCheckBox().IsChecked(settings.ShowComputerKeys());

            RibbonPositionComboBox().SelectedIndex(static_cast<int32_t>(settings.Ribbons()));
            VelocityModeComboBox().SelectedIndex(static_cast<int32_t>(settings.Velocity()));
            VelocityMinimumBox().Value(settings.VelocityMinimum());
            VelocityMaximumBox().Value(settings.VelocityMaximum());
            FixedVelocityBox().Value(settings.FixedVelocity());

            KeyPressureComboBox().SelectedIndex(static_cast<int32_t>(settings.KeyPressure()));
            PerNoteControllerBox().Value(settings.PerNoteControllerIndex());

            m_settingsControlsInitialized = true;

            ReleaseFlagWhenIdle(&MainWindow::m_suppressSettingHandlers);
        }
        MIDI_KEYBOARD_CATCH_AND_LOG(L"Unable to fill in the settings panel.")
    }

    // Templates are applied during the layout pass that follows, and the controls raise their
    // change events from there, so suppression has to outlive this call.
    _Use_decl_annotations_
    void MainWindow::ReleaseFlagWhenIdle(bool MainWindow::* flag) noexcept
    {
        auto released = false;

        try
        {
            if (m_dispatcherQueue != nullptr)
            {
                released = m_dispatcherQueue.TryEnqueue(
                    winrt::Microsoft::UI::Dispatching::DispatcherQueuePriority::Low,
                    [weak = get_weak(), flag]()
                    {
                        if (auto strong = weak.get())
                        {
                            strong.get()->*flag = false;
                        }
                    });
            }
        }
        MIDI_KEYBOARD_CATCH_AND_LOG(L"Unable to queue the settings handler release.")

        if (!released)
        {
            this->*flag = false;
        }
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

            m_watcher.Start();

            RefreshEndpointList();
        }
        MIDI_KEYBOARD_CATCH_AND_LOG(L"Unable to start the endpoint device watcher.")
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

            m_watcher.Stop();
            m_watcher = nullptr;
        }
        MIDI_KEYBOARD_CATCH_AND_LOG(L"Unable to stop the endpoint device watcher.")
    }

    void MainWindow::RefreshEndpointList() noexcept
    {
        try
        {
            if (m_watcher == nullptr || m_endpoints == nullptr)
            {
                return;
            }

            auto const& settings = native::AppSettings::Current();
            auto const devices = midiapp::SortedEndpoints(m_watcher);

            auto const clientEndpointId = m_output.ClientEndpointDeviceId();
            auto const previousSelection = winrt::hstring{ settings.EndpointDeviceId() };

            auto const previousSuppress = m_suppressSettingHandlers;
            m_suppressSettingHandlers = true;

            m_endpoints.Clear();
            m_endpointDevices.clear();

            for (auto const& device : devices)
            {
                // playing our own virtual device from its client side would be a feedback loop
                if (!clientEndpointId.empty() &&
                    midiapp::EndpointIdsMatch(device.EndpointDeviceId(), clientEndpointId))
                {
                    continue;
                }

                winrt::hstring imagePath{};

                if (auto const userInfo = device.GetUserSuppliedInfo())
                {
                    imagePath = midiapp::ResolveEndpointImagePath(userInfo.ImageFileName());
                }

                m_endpoints.Append(winrt::make<appshared::implementation::EndpointChoice>(
                    device.Name(), device.EndpointDeviceId(), imagePath));

                m_endpointDevices.push_back(device);
            }

            auto desiredEndpointId = previousSelection;

            auto const& options = App::StartupOptions();

            if (!m_startupOptionsApplied && !options.HasError && !options.ShowHelp &&
                !options.EndpointDeviceId.empty())
            {
                desiredEndpointId = winrt::hstring{ options.EndpointDeviceId };
            }

            int32_t selectedIndex{ -1 };

            for (uint32_t i = 0; i < m_endpoints.Size(); i++)
            {
                if (midiapp::EndpointIdsMatch(m_endpoints.GetAt(i).EndpointDeviceId(), desiredEndpointId))
                {
                    selectedIndex = static_cast<int32_t>(i);
                    break;
                }
            }

            EndpointComboBox().SelectedIndex(selectedIndex);

            m_suppressSettingHandlers = previousSuppress;

            RefreshGroupList();

            if (!m_startupOptionsApplied && selectedIndex >= 0 && !options.EndpointDeviceId.empty())
            {
                m_startupOptionsApplied = true;

                native::AppSettings::Current().EndpointDeviceId(options.EndpointDeviceId);
                native::AppSettings::Current().Connection(native::ConnectionMode::ExistingEndpoint);

                if (options.GroupNumber.has_value())
                {
                    native::AppSettings::Current().TransmitGroupNumber(options.GroupNumber.value());
                }

                if (options.ChannelNumber.has_value())
                {
                    native::AppSettings::Current().TransmitChannelNumber(options.ChannelNumber.value());
                }

                InitializeControlsFromSettings();
                RefreshGroupList();
                ReconnectAsync();
            }
        }
        MIDI_KEYBOARD_CATCH_AND_LOG(L"Unable to refresh the endpoint list.")
    }

    void MainWindow::RefreshGroupList() noexcept
    {
        try
        {
            auto const endpointIndex = EndpointComboBox().SelectedIndex();
            auto const previousGroupNumber = native::AppSettings::Current().TransmitGroupNumber();

            auto const previousSuppress = m_suppressSettingHandlers;
            m_suppressSettingHandlers = true;

            m_groups.Clear();

            if (IsIndexValid(endpointIndex, m_endpointDevices.size()))
            {
                auto const& endpoint = m_endpointDevices[static_cast<size_t>(endpointIndex)];
                auto const declared = midiapp::DeclaredGroups(endpoint);

                for (uint8_t groupIndex = 0; groupIndex < 16; groupIndex++)
                {
                    if (!declared[groupIndex])
                    {
                        continue;
                    }

                    auto const description = midiapp::DescribeGroup(endpoint, groupIndex);
                    auto const groupNumber = static_cast<int32_t>(groupIndex) + 1;

                    auto const label = description.empty()
                        ? res::FormatString(L"GroupChoiceFormat", groupNumber)
                        : res::FormatString(L"GroupChoiceNamedFormat", groupNumber, description);

                    m_groups.Append(winrt::make<appshared::implementation::NamedChoice>(label, groupNumber));
                }
            }

            int32_t selectedIndex{ m_groups.Size() > 0 ? 0 : -1 };

            for (uint32_t i = 0; i < m_groups.Size(); i++)
            {
                if (m_groups.GetAt(i).Value() == static_cast<int32_t>(previousGroupNumber))
                {
                    selectedIndex = static_cast<int32_t>(i);
                    break;
                }
            }

            GroupComboBox().SelectedIndex(selectedIndex);

            m_suppressSettingHandlers = previousSuppress;
        }
        MIDI_KEYBOARD_CATCH_AND_LOG(L"Unable to refresh the group list.")
    }

    // ------------------------------------------------------------------------------------
    // Connection
    // ------------------------------------------------------------------------------------

    winrt::fire_and_forget MainWindow::ReconnectAsync()
    {
        auto strong = get_strong();

        try
        {
            if (m_reconnectInProgress)
            {
                m_reconnectRequested = true;
                co_return;
            }

            m_reconnectInProgress = true;

            native::ConnectResult result{ native::ConnectResult::Success };

            for (;;)
            {
                m_reconnectRequested = false;

                EndAllNotes();
                ShowConnectingState();

                auto const mode = native::AppSettings::Current().Connection();
                auto const endpointId = native::AppSettings::Current().EndpointDeviceId();

                co_await native::RunOnBackgroundAsync([this, mode, endpointId, &result]()
                    {
                        result = (mode == native::ConnectionMode::VirtualDevice)
                            ? m_output.ConnectVirtualDevice()
                            : m_output.ConnectEndpoint(endpointId);
                    });

                if (!m_reconnectRequested)
                {
                    break;
                }
            }

            m_reconnectInProgress = false;

            UpdateConnectionDisplay(result);

            // the virtual device's own client endpoint has to stay out of the destination list
            RefreshEndpointList();
        }
        catch (winrt::hresult_error const& ex)
        {
            m_reconnectInProgress = false;
            MIDI_KEYBOARD_LOG_HRESULT_EXCEPTION(ex, L"Unable to connect.");
        }
        catch (...)
        {
            m_reconnectInProgress = false;
            MIDI_KEYBOARD_LOG_GENERAL_EXCEPTION(L"Unable to connect.");
        }
    }

    winrt::fire_and_forget MainWindow::ShutdownAsync()
    {
        auto strong = get_strong();

        try
        {
            co_await native::RunOnBackgroundAsync([strong]()
                {
                    strong->m_output.Disconnect();
                });
        }
        MIDI_KEYBOARD_CATCH_AND_LOG(L"Unable to close the MIDI connection.")
    }

    void MainWindow::ShowConnectingState() noexcept
    {
        try
        {
            ConnectionStateDot().Fill(LookupBrush(L"SystemFillColorCautionBrush"));
            SetStripText(ConnectionNameText(), res::GetString(L"ConnectionConnecting"));
            SetStripText(ConnectionDetailText(), L"");
        }
        MIDI_KEYBOARD_CATCH_AND_LOG(L"Unable to show the connecting state.")
    }

    _Use_decl_annotations_
    void MainWindow::UpdateConnectionDisplay(native::ConnectResult result) noexcept
    {
        try
        {
            auto const& settings = native::AppSettings::Current();
            auto const connected = result == native::ConnectResult::Success;

            ConnectionStateDot().Fill(LookupBrush(
                connected ? L"SystemFillColorSuccessBrush" : L"SystemFillColorCriticalBrush"));

            if (!connected)
            {
                wchar_t const* key{ L"ConnectionFailed" };

                switch (result)
                {
                case native::ConnectResult::ServiceUnavailable: key = L"ConnectionServiceUnavailable"; break;
                case native::ConnectResult::VirtualDeviceFailed: key = L"ConnectionVirtualDeviceFailed"; break;
                case native::ConnectResult::NoEndpointChosen: key = L"ConnectionNoEndpoint"; break;
                default: break;
                }

                SetStripText(ConnectionNameText(), res::GetString(key));
                SetStripText(ConnectionDetailText(), L"");
                return;
            }

            if (settings.Connection() == native::ConnectionMode::VirtualDevice)
            {
                SetStripText(ConnectionNameText(), res::GetString(L"ConnectionVirtualDeviceName"));
                SetStripText(ConnectionDetailText(),
                    res::GetString(L"ConnectionVirtualDeviceDetail"),
                    res::GetString(L"ConnectionVirtualDeviceDetailToolTip"));
                return;
            }

            winrt::hstring endpointName{ settings.EndpointDeviceId() };

            if (auto const choice = EndpointComboBox().SelectedItem().try_as<appshared::EndpointChoice>())
            {
                endpointName = choice.DisplayName();
            }

            SetStripText(ConnectionNameText(), endpointName);
            SetStripText(ConnectionDetailText(), res::FormatString(L"ConnectionEndpointDetailFormat",
                static_cast<int32_t>(settings.TransmitGroupNumber()),
                static_cast<int32_t>(settings.TransmitChannelNumber())));
        }
        MIDI_KEYBOARD_CATCH_AND_LOG(L"Unable to show the connection state.")
    }

    void MainWindow::UpdateConnectionModeLayout() noexcept
    {
        try
        {
            auto const isEndpoint =
                native::AppSettings::Current().Connection() == native::ConnectionMode::ExistingEndpoint;

            EndpointSettingsPanel().Visibility(
                isEndpoint ? xaml::Visibility::Visible : xaml::Visibility::Collapsed);

            VirtualDeviceHint().Visibility(
                isEndpoint ? xaml::Visibility::Collapsed : xaml::Visibility::Visible);
        }
        MIDI_KEYBOARD_CATCH_AND_LOG(L"Unable to update the connection layout.")
    }

    void MainWindow::UpdateVelocityLayout() noexcept
    {
        try
        {
            auto const& settings = native::AppSettings::Current();

            VelocityRangePanel().Visibility(settings.Velocity() == native::VelocityMode::KeyLocation
                ? xaml::Visibility::Visible : xaml::Visibility::Collapsed);

            FixedVelocityBox().Visibility(settings.Velocity() == native::VelocityMode::Fixed
                ? xaml::Visibility::Visible : xaml::Visibility::Collapsed);

            PerNoteControllerBox().Visibility(settings.KeyPressure() == native::KeyPressureMode::PerNoteController
                ? xaml::Visibility::Visible : xaml::Visibility::Collapsed);
        }
        MIDI_KEYBOARD_CATCH_AND_LOG(L"Unable to update the velocity layout.")
    }

    void MainWindow::UpdateRibbonLayout() noexcept
    {
        try
        {
            auto const position = native::AppSettings::Current().Ribbons();

            if (position == native::RibbonPosition::Disabled)
            {
                RibbonPanel().Visibility(xaml::Visibility::Collapsed);

                // anything the ribbons were holding has to be released, or a synth is left bent
                ApplyPitchValue(0.0, true);
                ApplyModValue(0.0, true);
                return;
            }

            RibbonPanel().Visibility(xaml::Visibility::Visible);

            if (position == native::RibbonPosition::Left)
            {
                controls::Grid::SetColumn(RibbonPanel(), 0);
                RibbonPanel().Margin(xaml::ThicknessHelper::FromLengths(12, 0, 4, 12));
            }
            else
            {
                controls::Grid::SetColumn(RibbonPanel(), 2);
                RibbonPanel().Margin(xaml::ThicknessHelper::FromLengths(4, 0, 12, 12));
            }
        }
        MIDI_KEYBOARD_CATCH_AND_LOG(L"Unable to position the ribbons.")
    }

    void MainWindow::UpdateOctaveDisplay() noexcept
    {
        try
        {
            auto const& settings = native::AppSettings::Current();

            auto const first = TransposedNote(FirstNoteNumber());
            auto const last = TransposedNote(FirstNoteNumber() + static_cast<int32_t>(settings.OctaveCount()) * 12);

            OctaveRangeText().Text(res::FormatString(L"OctaveRangeFormat",
                native::NoteName(first), native::NoteName(last)));

            OctaveDownButton().IsEnabled(settings.BaseOctave() > native::AppSettings::MinimumBaseOctave);
            OctaveUpButton().IsEnabled(settings.BaseOctave() < native::AppSettings::MaximumBaseOctave);
        }
        MIDI_KEYBOARD_CATCH_AND_LOG(L"Unable to update the octave display.")
    }

    // ------------------------------------------------------------------------------------
    // Keyboard drawing
    // ------------------------------------------------------------------------------------

    int32_t MainWindow::FirstNoteNumber() const noexcept
    {
        // note 60 is C3, so octave n starts at (n + 2) * 12
        return (native::AppSettings::Current().BaseOctave() + 2) * 12;
    }

    _Use_decl_annotations_
    int32_t MainWindow::TransposedNote(int32_t noteNumber) const noexcept
    {
        return noteNumber + native::AppSettings::Current().Transpose();
    }

    void MainWindow::RebuildKeyboard() noexcept
    {
        try
        {
            if (!m_initialized)
            {
                return;
            }

            EndAllNotes();

            auto const canvas = KeyboardCanvas();

            canvas.Children().Clear();
            m_keys.clear();
            m_keyGeometry.clear();

            auto const& settings = native::AppSettings::Current();

            auto const width = std::max(1.0, canvas.ActualWidth());
            auto const height = std::max(1.0, canvas.ActualHeight());

            m_keyGeometry = native::BuildKeyboard(FirstNoteNumber(), settings.OctaveCount(), width, height);

            m_keys.reserve(m_keyGeometry.size());

            for (size_t i = 0; i < m_keyGeometry.size(); i++)
            {
                KeyVisual key{};

                controls::Grid inner{};

                key.Glow = shapes::Rectangle{};
                key.Glow.Fill(m_glowBrush);
                key.Glow.Opacity(0.0);
                key.Glow.IsHitTestVisible(false);

                controls::StackPanel labels{};
                labels.HorizontalAlignment(xaml::HorizontalAlignment::Center);
                labels.VerticalAlignment(xaml::VerticalAlignment::Bottom);
                labels.IsHitTestVisible(false);

                key.ComputerKeyLabel = controls::TextBlock{};
                key.ComputerKeyLabel.TextAlignment(xaml::TextAlignment::Center);
                key.ComputerKeyLabel.HorizontalAlignment(xaml::HorizontalAlignment::Center);

                key.NoteLabel = controls::TextBlock{};
                key.NoteLabel.TextAlignment(xaml::TextAlignment::Center);
                key.NoteLabel.HorizontalAlignment(xaml::HorizontalAlignment::Center);

                labels.Children().Append(key.ComputerKeyLabel);
                labels.Children().Append(key.NoteLabel);

                inner.Children().Append(key.Glow);
                inner.Children().Append(labels);

                key.Body = controls::Border{};
                key.Body.Child(inner);
                key.Body.BorderThickness(xaml::ThicknessHelper::FromUniformLength(1));
                key.Body.BorderBrush(m_keyBorderBrush);

                canvas.Children().Append(key.Body);

                m_keys.push_back(key);
            }

            LayoutKeyboard();
        }
        MIDI_KEYBOARD_CATCH_AND_LOG(L"Unable to build the keyboard.")
    }

    void MainWindow::LayoutKeyboard() noexcept
    {
        try
        {
            if (m_keys.empty())
            {
                return;
            }

            auto const canvas = KeyboardCanvas();
            auto const& settings = native::AppSettings::Current();

            auto const width = std::max(1.0, canvas.ActualWidth());
            auto const height = std::max(1.0, canvas.ActualHeight());

            m_keyGeometry = native::BuildKeyboard(FirstNoteNumber(), settings.OctaveCount(), width, height);

            if (m_keyGeometry.size() != m_keys.size())
            {
                return;
            }

            auto const whiteWidth = width / static_cast<double>(native::WhiteKeyCount(settings.OctaveCount()));

            auto const noteFontSize = std::clamp(whiteWidth * 0.26, 6.0, 13.0);
            auto const computerFontSize = std::clamp(whiteWidth * 0.28, 6.0, 14.0);

            // below this the text is unreadable and just makes the keys look dirty
            auto const roomForLabels = whiteWidth >= 22.0 && height >= 70.0;

            auto const showNotes = settings.ShowNoteNames() && roomForLabels;
            auto const showComputerKeys = settings.ShowComputerKeys() && roomForLabels;

            auto const cornerRadius = std::clamp(whiteWidth * 0.16, 2.0, 6.0);

            for (size_t i = 0; i < m_keys.size(); i++)
            {
                auto const& geometry = m_keyGeometry[i];
                auto& key = m_keys[i];

                controls::Canvas::SetLeft(key.Body, geometry.Left);
                controls::Canvas::SetTop(key.Body, geometry.Top);

                key.Body.Width(geometry.Width);
                key.Body.Height(geometry.Height);
                key.Body.Background(geometry.IsBlack ? m_blackKeyBrush : m_whiteKeyBrush);
                key.Body.CornerRadius(xaml::CornerRadiusHelper::FromRadii(0, 0, cornerRadius, cornerRadius));

                auto const textBrush = geometry.IsBlack ? m_blackKeyTextBrush : m_whiteKeyTextBrush;

                auto const displayedNote = TransposedNote(geometry.NoteNumber);

                key.NoteLabel.Foreground(textBrush);
                key.NoteLabel.FontSize(noteFontSize);
                key.NoteLabel.Margin(xaml::ThicknessHelper::FromLengths(0, 0, 0, geometry.IsBlack ? 6.0 : 8.0));

                if (showNotes && displayedNote >= 0 && displayedNote <= 127)
                {
                    key.NoteLabel.Text(winrt::hstring{
                        std::format(L"{}\n{}", native::NoteName(displayedNote), displayedNote) });
                    key.NoteLabel.Visibility(xaml::Visibility::Visible);
                }
                else
                {
                    key.NoteLabel.Visibility(xaml::Visibility::Collapsed);
                }

                auto const computerKey = native::ComputerKeyLabel(geometry.NoteNumber - FirstNoteNumber());

                key.ComputerKeyLabel.Foreground(textBrush);
                key.ComputerKeyLabel.FontSize(computerFontSize);
                key.ComputerKeyLabel.FontWeight(winrt::Microsoft::UI::Text::FontWeights::SemiBold());
                key.ComputerKeyLabel.Margin(xaml::ThicknessHelper::FromLengths(0, 0, 0, 2));

                if (showComputerKeys && !computerKey.empty())
                {
                    key.ComputerKeyLabel.Text(winrt::hstring{ computerKey });
                    key.ComputerKeyLabel.Visibility(xaml::Visibility::Visible);
                }
                else
                {
                    key.ComputerKeyLabel.Visibility(xaml::Visibility::Collapsed);
                }
            }
        }
        MIDI_KEYBOARD_CATCH_AND_LOG(L"Unable to lay out the keyboard.")
    }

    _Use_decl_annotations_
    int32_t MainWindow::KeyIndexForNote(int32_t noteNumber) const noexcept
    {
        for (size_t i = 0; i < m_keyGeometry.size(); i++)
        {
            if (TransposedNote(m_keyGeometry[i].NoteNumber) == noteNumber)
            {
                return static_cast<int32_t>(i);
            }
        }

        return -1;
    }

    _Use_decl_annotations_
    void MainWindow::SetKeyGlow(KeyVisual& key, double opacity, bool fade) noexcept
    {
        try
        {
            if (key.Glow == nullptr)
            {
                return;
            }

            if (key.FadeOut == nullptr)
            {
                animation::DoubleAnimation fadeAnimation{};

                fadeAnimation.To(0.0);
                fadeAnimation.Duration(xaml::Duration{
                    foundation::TimeSpan{ KeyFadeMilliseconds * TicksPerMillisecond },
                    xaml::DurationType::TimeSpan });

                animation::Storyboard storyboard{};
                storyboard.Children().Append(fadeAnimation);

                animation::Storyboard::SetTarget(fadeAnimation, key.Glow);
                animation::Storyboard::SetTargetProperty(fadeAnimation, L"Opacity");

                key.FadeOut = storyboard;
            }

            // a storyboard holds its end value, so a direct opacity set is ignored until it stops
            key.FadeOut.Stop();

            if (fade)
            {
                key.FadeOut.Begin();
                return;
            }

            key.Glow.Opacity(opacity);
        }
        MIDI_KEYBOARD_CATCH_AND_LOG(L"Unable to light the key.")
    }

    _Use_decl_annotations_
    void MainWindow::RefreshKeyGlow(int32_t noteNumber) noexcept
    {
        try
        {
            auto const index = KeyIndexForNote(noteNumber);

            if (!IsIndexValid(index, m_keys.size()))
            {
                return;
            }

            auto& key = m_keys[static_cast<size_t>(index)];

            auto const held = noteNumber >= 0 && noteNumber <= 127 &&
                m_noteHoldCount[static_cast<size_t>(noteNumber)] > 0;

            if (m_arpeggiatorSoundingNote == noteNumber)
            {
                SetKeyGlow(key, HeldKeyGlowOpacity, false);
                return;
            }

            if (held)
            {
                SetKeyGlow(key,
                    m_arpeggiator.IsEnabled() ? ChordKeyGlowOpacity : HeldKeyGlowOpacity, false);
                return;
            }

            SetKeyGlow(key, 0.0, true);
        }
        MIDI_KEYBOARD_CATCH_AND_LOG(L"Unable to refresh the key.")
    }

    // ------------------------------------------------------------------------------------
    // Notes
    // ------------------------------------------------------------------------------------

    uint8_t MainWindow::TransmitGroupIndex() const noexcept
    {
        auto const& settings = native::AppSettings::Current();

        return settings.Connection() == native::ConnectionMode::VirtualDevice
            ? native::MidiOutput::VirtualDeviceGroupIndex
            : static_cast<uint8_t>(settings.TransmitGroupNumber() - 1);
    }

    uint8_t MainWindow::TransmitChannelIndex() const noexcept
    {
        auto const& settings = native::AppSettings::Current();

        return settings.Connection() == native::ConnectionMode::VirtualDevice
            ? uint8_t{ 0 }
            : static_cast<uint8_t>(settings.TransmitChannelNumber() - 1);
    }

    _Use_decl_annotations_
    uint16_t MainWindow::VelocityForKey(native::KeyGeometry const& key, double y) const noexcept
    {
        auto const& settings = native::AppSettings::Current();

        switch (settings.Velocity())
        {
        case native::VelocityMode::Off:
            return 0xFFFF;

        case native::VelocityMode::Fixed:
            return static_cast<uint16_t>(native::ScaleUpValue(settings.FixedVelocity(), 7, 16));

        default:
            break;
        }

        auto const travel = std::max(1.0, key.Height);
        auto const position = std::clamp((y - key.Top) / travel, 0.0, 1.0);

        auto const low = static_cast<double>(settings.VelocityMinimum());
        auto const high = static_cast<double>(settings.VelocityMaximum());

        // the closer to the player, the harder the note
        auto const velocity7 = static_cast<uint32_t>(std::lround(low + ((high - low) * position)));

        return static_cast<uint16_t>(native::ScaleUpValue(
            std::clamp(velocity7, native::AppSettings::MinimumVelocity, native::AppSettings::MaximumVelocity),
            7, 16));
    }

    _Use_decl_annotations_
    void MainWindow::BeginNote(int32_t noteNumber, uint16_t velocity) noexcept
    {
        try
        {
            if (noteNumber < 0 || noteNumber > 127)
            {
                return;
            }

            auto& count = m_noteHoldCount[static_cast<size_t>(noteNumber)];

            count++;

            if (count == 1)
            {
                if (m_arpeggiator.IsEnabled())
                {
                    m_arpeggiator.HoldNote(noteNumber, velocity);
                }
                else
                {
                    SendNoteOnNow(noteNumber, velocity);
                }
            }

            RefreshKeyGlow(noteNumber);
        }
        MIDI_KEYBOARD_CATCH_AND_LOG(L"Unable to start the note.")
    }

    _Use_decl_annotations_
    void MainWindow::EndNote(int32_t noteNumber) noexcept
    {
        try
        {
            if (noteNumber < 0 || noteNumber > 127)
            {
                return;
            }

            auto& count = m_noteHoldCount[static_cast<size_t>(noteNumber)];

            if (count <= 0)
            {
                return;
            }

            count--;

            if (count == 0)
            {
                if (m_arpeggiator.IsEnabled())
                {
                    m_arpeggiator.ReleaseNote(noteNumber);
                }
                else
                {
                    SendNoteOffNow(noteNumber);
                }

                if (native::AppSettings::Current().KeyPressure() == native::KeyPressureMode::ChannelPressure)
                {
                    auto const anyHeld = std::any_of(m_noteHoldCount.begin(), m_noteHoldCount.end(),
                        [](int32_t value) { return value > 0; });

                    if (!anyHeld)
                    {
                        m_output.SendChannelPressure(TransmitGroupIndex(), TransmitChannelIndex(), 0);
                    }
                }
            }

            RefreshKeyGlow(noteNumber);
        }
        MIDI_KEYBOARD_CATCH_AND_LOG(L"Unable to stop the note.")
    }

    void MainWindow::EndAllNotes() noexcept
    {
        try
        {
            m_activePointers.clear();
            m_computerKeyNotes.clear();

            for (size_t note = 0; note < m_noteHoldCount.size(); note++)
            {
                if (m_noteHoldCount[note] <= 0)
                {
                    continue;
                }

                m_noteHoldCount[note] = 0;

                SendNoteOffNow(static_cast<int32_t>(note));
                RefreshKeyGlow(static_cast<int32_t>(note));
            }

            m_arpeggiator.Reset();
            m_arpeggiatorSoundingNote = -1;

            m_output.SendAllNotesOff(TransmitGroupIndex(), TransmitChannelIndex());
        }
        MIDI_KEYBOARD_CATCH_AND_LOG(L"Unable to stop all notes.")
    }

    _Use_decl_annotations_
    void MainWindow::SendNoteOnNow(int32_t noteNumber, uint16_t velocity) noexcept
    {
        if (noteNumber < 0 || noteNumber > 127)
        {
            return;
        }

        m_output.SendNoteOn(TransmitGroupIndex(), TransmitChannelIndex(),
            static_cast<uint8_t>(noteNumber), velocity);
    }

    _Use_decl_annotations_
    void MainWindow::SendNoteOffNow(int32_t noteNumber) noexcept
    {
        if (noteNumber < 0 || noteNumber > 127)
        {
            return;
        }

        m_output.SendNoteOff(TransmitGroupIndex(), TransmitChannelIndex(),
            static_cast<uint8_t>(noteNumber));
    }

    _Use_decl_annotations_
    void MainWindow::SendKeyPressure(int32_t noteNumber, uint32_t pressure) noexcept
    {
        try
        {
            if (noteNumber < 0 || noteNumber > 127)
            {
                return;
            }

            auto const& settings = native::AppSettings::Current();
            auto const group = TransmitGroupIndex();
            auto const channel = TransmitChannelIndex();
            auto const note = static_cast<uint8_t>(noteNumber);

            switch (settings.KeyPressure())
            {
            case native::KeyPressureMode::PerNoteController:
                m_output.SendPerNoteController(group, channel, note,
                    static_cast<uint8_t>(settings.PerNoteControllerIndex()), pressure);
                break;

            case native::KeyPressureMode::ChannelPressure:
                m_output.SendChannelPressure(group, channel, pressure);
                break;

            case native::KeyPressureMode::PolyPressure:
                m_output.SendPolyPressure(group, channel, note, pressure);
                break;

            default:
                break;
            }
        }
        MIDI_KEYBOARD_CATCH_AND_LOG(L"Unable to send the key pressure.")
    }

    // ------------------------------------------------------------------------------------
    // Pointer input
    // ------------------------------------------------------------------------------------

    _Use_decl_annotations_
    void MainWindow::OnKeyboardSizeChanged(foundation::IInspectable const&, xaml::SizeChangedEventArgs const&)
    {
        if (m_keys.empty())
        {
            RebuildKeyboard();
            return;
        }

        LayoutKeyboard();
    }

    _Use_decl_annotations_
    void MainWindow::OnKeyboardPointerPressed(
        foundation::IInspectable const&,
        input::PointerRoutedEventArgs const& args)
    {
        try
        {
            auto const canvas = KeyboardCanvas();
            auto const point = args.GetCurrentPoint(canvas);
            auto const position = point.Position();

            auto const index = native::HitTestKey(m_keyGeometry, position.X, position.Y);

            if (!IsIndexValid(index, m_keyGeometry.size()))
            {
                return;
            }

            auto const& geometry = m_keyGeometry[static_cast<size_t>(index)];
            auto const noteNumber = TransposedNote(geometry.NoteNumber);

            canvas.CapturePointer(args.Pointer());

            ActivePointer active{};
            active.KeyIndex = index;
            active.NoteNumber = noteNumber;
            active.PressY = position.Y;
            active.Pressure = 0;

            m_activePointers[args.Pointer().PointerId()] = active;

            BeginNote(noteNumber, VelocityForKey(geometry, position.Y));

            args.Handled(true);
        }
        MIDI_KEYBOARD_CATCH_AND_LOG(L"Unable to handle the key press.")
    }

    _Use_decl_annotations_
    void MainWindow::OnKeyboardPointerMoved(
        foundation::IInspectable const&,
        input::PointerRoutedEventArgs const& args)
    {
        try
        {
            auto const pointerId = args.Pointer().PointerId();
            auto const entry = m_activePointers.find(pointerId);

            if (entry == m_activePointers.end())
            {
                return;
            }

            auto const canvas = KeyboardCanvas();
            auto const position = args.GetCurrentPoint(canvas).Position();

            auto const index = native::HitTestKey(m_keyGeometry, position.X, position.Y);

            // sliding sideways off one key and onto another plays the new one, the way a
            // finger dragged along a real keyboard does
            if (IsIndexValid(index, m_keyGeometry.size()) && index != entry->second.KeyIndex)
            {
                auto const& geometry = m_keyGeometry[static_cast<size_t>(index)];
                auto const noteNumber = TransposedNote(geometry.NoteNumber);

                EndNote(entry->second.NoteNumber);

                entry->second.KeyIndex = index;
                entry->second.NoteNumber = noteNumber;
                entry->second.PressY = position.Y;
                entry->second.Pressure = 0;

                BeginNote(noteNumber, VelocityForKey(geometry, position.Y));

                args.Handled(true);
                return;
            }

            if (!IsIndexValid(entry->second.KeyIndex, m_keyGeometry.size()))
            {
                return;
            }

            if (native::AppSettings::Current().KeyPressure() == native::KeyPressureMode::Off)
            {
                return;
            }

            auto const& geometry = m_keyGeometry[static_cast<size_t>(entry->second.KeyIndex)];
            auto const travel = std::max(1.0, geometry.Height * PressureTravelFactor);

            // dragging up the key, away from the player, presses harder
            auto const normalized = std::clamp((entry->second.PressY - position.Y) / travel, 0.0, 1.0);
            auto const pressure = NormalizedToUnsigned(normalized);

            if (pressure != entry->second.Pressure)
            {
                entry->second.Pressure = pressure;
                SendKeyPressure(entry->second.NoteNumber, pressure);
            }

            args.Handled(true);
        }
        MIDI_KEYBOARD_CATCH_AND_LOG(L"Unable to handle the key drag.")
    }

    _Use_decl_annotations_
    void MainWindow::OnKeyboardPointerReleased(
        foundation::IInspectable const&,
        input::PointerRoutedEventArgs const& args)
    {
        try
        {
            auto const pointerId = args.Pointer().PointerId();
            auto const entry = m_activePointers.find(pointerId);

            if (entry == m_activePointers.end())
            {
                return;
            }

            auto const noteNumber = entry->second.NoteNumber;

            m_activePointers.erase(entry);

            EndNote(noteNumber);

            KeyboardCanvas().ReleasePointerCapture(args.Pointer());

            args.Handled(true);
        }
        MIDI_KEYBOARD_CATCH_AND_LOG(L"Unable to handle the key release.")
    }

    // ------------------------------------------------------------------------------------
    // Computer keyboard input
    // ------------------------------------------------------------------------------------

    bool MainWindow::IsTextInputFocused() noexcept
    {
        try
        {
            auto const focused = xaml::Input::FocusManager::GetFocusedElement(RootGrid().XamlRoot());

            if (focused == nullptr)
            {
                return false;
            }

            return focused.try_as<controls::TextBox>() != nullptr ||
                focused.try_as<controls::NumberBox>() != nullptr ||
                focused.try_as<controls::ComboBox>() != nullptr ||
                focused.try_as<controls::AutoSuggestBox>() != nullptr;
        }
        MIDI_KEYBOARD_CATCH_AND_LOG(L"Unable to check the focused element.")

        return false;
    }

    _Use_decl_annotations_
    void MainWindow::OnRootPreviewKeyDown(
        foundation::IInspectable const&,
        input::KeyRoutedEventArgs const& args)
    {
        try
        {
            if (!m_initialized || IsTextInputFocused())
            {
                return;
            }

            auto const key = static_cast<uint32_t>(args.Key());

            if (key == VK_PRIOR || key == VK_NEXT)
            {
                auto& settings = native::AppSettings::Current();

                EndAllNotes();
                settings.BaseOctave(settings.BaseOctave() + (key == VK_PRIOR ? 1 : -1));

                if (m_settingsControlsInitialized)
                {
                    auto const previousSuppress = m_suppressSettingHandlers;
                    m_suppressSettingHandlers = true;
                    BaseOctaveBox().Value(settings.BaseOctave());
                    m_suppressSettingHandlers = previousSuppress;
                }

                RebuildKeyboard();
                UpdateOctaveDisplay();

                args.Handled(true);
                return;
            }

            // held keys repeat, and a repeat is not a new note
            if (args.KeyStatus().WasKeyDown)
            {
                return;
            }

            auto const semitones = native::ComputerKeyToSemitones(key);

            if (semitones < 0)
            {
                return;
            }

            if (m_computerKeyNotes.find(key) != m_computerKeyNotes.end())
            {
                return;
            }

            auto const noteNumber = TransposedNote(FirstNoteNumber() + semitones);

            m_computerKeyNotes[key] = noteNumber;

            auto const& settings = native::AppSettings::Current();

            // there is no strike position on a computer key, so the fixed velocity is used
            auto const velocity = settings.Velocity() == native::VelocityMode::Off
                ? uint16_t{ 0xFFFF }
                : static_cast<uint16_t>(native::ScaleUpValue(settings.FixedVelocity(), 7, 16));

            BeginNote(noteNumber, velocity);

            args.Handled(true);
        }
        MIDI_KEYBOARD_CATCH_AND_LOG(L"Unable to handle the computer key press.")
    }

    _Use_decl_annotations_
    void MainWindow::OnRootPreviewKeyUp(
        foundation::IInspectable const&,
        input::KeyRoutedEventArgs const& args)
    {
        try
        {
            auto const key = static_cast<uint32_t>(args.Key());
            auto const entry = m_computerKeyNotes.find(key);

            if (entry == m_computerKeyNotes.end())
            {
                return;
            }

            auto const noteNumber = entry->second;

            m_computerKeyNotes.erase(entry);

            EndNote(noteNumber);

            args.Handled(true);
        }
        MIDI_KEYBOARD_CATCH_AND_LOG(L"Unable to handle the computer key release.")
    }

    // ------------------------------------------------------------------------------------
    // Ribbons
    // ------------------------------------------------------------------------------------

    _Use_decl_annotations_
    void MainWindow::OnRibbonSizeChanged(foundation::IInspectable const&, xaml::SizeChangedEventArgs const&)
    {
        try
        {
            auto const canvas = PitchRibbonCanvas();

            PitchRibbonCenterMark().Width(std::max(0.0, canvas.ActualWidth() - (RibbonLightInset * 2)));
            controls::Canvas::SetLeft(PitchRibbonCenterMark(), RibbonLightInset);
            controls::Canvas::SetTop(PitchRibbonCenterMark(), (canvas.ActualHeight() / 2.0) - 1.0);

            UpdateRibbonsFromValues();
        }
        MIDI_KEYBOARD_CATCH_AND_LOG(L"Unable to lay out the ribbons.")
    }

    void MainWindow::UpdateRibbonsFromValues() noexcept
    {
        UpdateRibbonVisual(m_pitchRibbon, (m_pitchValue + 1.0) / 2.0);
        UpdateRibbonVisual(m_modRibbon, m_modValue);
    }

    // The mask is taken from the light itself, so the glow follows its rounded ends rather
    // than sitting behind it as a rectangle.
    _Use_decl_annotations_
    void MainWindow::EnsureRibbonGlow(RibbonVisuals& ribbon) noexcept
    {
        try
        {
            if (ribbon.GlowVisual != nullptr || ribbon.GlowHost == nullptr || ribbon.Light == nullptr)
            {
                return;
            }

            // an unmeasured shape has no alpha mask yet
            if (ribbon.Light.ActualWidth() <= 0.0 || ribbon.Light.ActualHeight() <= 0.0)
            {
                return;
            }

            namespace hosting = winrt::Microsoft::UI::Xaml::Hosting;

            auto const compositor =
                hosting::ElementCompositionPreview::GetElementVisual(ribbon.GlowHost).Compositor();

            auto shadow = compositor.CreateDropShadow();
            shadow.Color(m_glowColor);
            shadow.BlurRadius(RibbonGlowBlurRadius);
            shadow.Opacity(RibbonGlowOpacity);
            shadow.Mask(ribbon.Light.GetAlphaMask());

            auto sprite = compositor.CreateSpriteVisual();
            sprite.Shadow(shadow);

            hosting::ElementCompositionPreview::SetElementChildVisual(ribbon.GlowHost, sprite);

            ribbon.GlowVisual = sprite;
        }
        MIDI_KEYBOARD_CATCH_AND_LOG(L"Unable to create the ribbon glow.")
    }

    _Use_decl_annotations_
    void MainWindow::UpdateRibbonVisual(RibbonVisuals& ribbon, double normalized) noexcept
    {
        try
        {
            if (ribbon.Track == nullptr || ribbon.Light == nullptr)
            {
                return;
            }

            auto const width = std::max(0.0, ribbon.Track.ActualWidth() - (RibbonLightInset * 2));
            auto const travel = std::max(0.0, ribbon.Track.ActualHeight() - RibbonLightHeight);

            ribbon.Light.Width(width);
            ribbon.Light.Height(RibbonLightHeight);

            controls::Canvas::SetLeft(ribbon.Light, RibbonLightInset);

            // the top of the track is the high end of the value, the way a wheel reads
            auto const top = (1.0 - std::clamp(normalized, 0.0, 1.0)) * travel;

            controls::Canvas::SetTop(ribbon.Light, top);

            EnsureRibbonGlow(ribbon);

            if (ribbon.GlowHost != nullptr)
            {
                ribbon.GlowHost.Width(width);
                ribbon.GlowHost.Height(RibbonLightHeight);

                controls::Canvas::SetLeft(ribbon.GlowHost, RibbonLightInset);
                controls::Canvas::SetTop(ribbon.GlowHost, top);
            }

            if (ribbon.GlowVisual != nullptr)
            {
                ribbon.GlowVisual.Size({ static_cast<float>(width), static_cast<float>(RibbonLightHeight) });
            }
        }
        MIDI_KEYBOARD_CATCH_AND_LOG(L"Unable to move the ribbon light.")
    }

    _Use_decl_annotations_
    double MainWindow::RibbonValueFromPoint(controls::Canvas const& canvas, double y, bool bipolar) const noexcept
    {
        auto const travel = std::max(1.0, canvas.ActualHeight() - RibbonLightHeight);
        auto const clamped = std::clamp(y - (RibbonLightHeight / 2.0), 0.0, travel);
        auto const normalized = 1.0 - (clamped / travel);

        return bipolar ? (normalized * 2.0) - 1.0 : normalized;
    }

    _Use_decl_annotations_
    void MainWindow::ApplyPitchValue(double value, bool send) noexcept
    {
        m_pitchValue = std::clamp(value, -1.0, 1.0);

        UpdateRibbonVisual(m_pitchRibbon, (m_pitchValue + 1.0) / 2.0);

        if (!send)
        {
            return;
        }

        auto const scaled = (m_pitchValue == 0.0)
            ? PitchBendCenter
            : NormalizedToUnsigned((m_pitchValue + 1.0) / 2.0);

        m_output.SendPitchBend(TransmitGroupIndex(), TransmitChannelIndex(), scaled);
    }

    _Use_decl_annotations_
    void MainWindow::ApplyModValue(double value, bool send) noexcept
    {
        m_modValue = std::clamp(value, 0.0, 1.0);

        UpdateRibbonVisual(m_modRibbon, m_modValue);

        if (!send)
        {
            return;
        }

        m_output.SendControlChange(TransmitGroupIndex(), TransmitChannelIndex(),
            ModulationController, NormalizedToUnsigned(m_modValue));
    }

    _Use_decl_annotations_
    void MainWindow::OnPitchRibbonPointerPressed(
        foundation::IInspectable const&,
        input::PointerRoutedEventArgs const& args)
    {
        try
        {
            auto const position = args.GetCurrentPoint(PitchRibbonCanvas()).Position();

            m_pitchPointerId = args.Pointer().PointerId();
            m_pitchCaptured = PitchRibbonTrack().CapturePointer(args.Pointer());

            ApplyPitchValue(RibbonValueFromPoint(PitchRibbonCanvas(), position.Y, true), true);

            args.Handled(true);
        }
        MIDI_KEYBOARD_CATCH_AND_LOG(L"Unable to start the pitch bend.")
    }

    _Use_decl_annotations_
    void MainWindow::OnPitchRibbonPointerMoved(
        foundation::IInspectable const&,
        input::PointerRoutedEventArgs const& args)
    {
        try
        {
            if (!m_pitchCaptured || args.Pointer().PointerId() != m_pitchPointerId)
            {
                return;
            }

            auto const position = args.GetCurrentPoint(PitchRibbonCanvas()).Position();

            ApplyPitchValue(RibbonValueFromPoint(PitchRibbonCanvas(), position.Y, true), true);

            args.Handled(true);
        }
        MIDI_KEYBOARD_CATCH_AND_LOG(L"Unable to bend the pitch.")
    }

    _Use_decl_annotations_
    void MainWindow::OnPitchRibbonPointerReleased(
        foundation::IInspectable const&,
        input::PointerRoutedEventArgs const& args)
    {
        try
        {
            if (!m_pitchCaptured || args.Pointer().PointerId() != m_pitchPointerId)
            {
                return;
            }

            m_pitchCaptured = false;

            PitchRibbonTrack().ReleasePointerCapture(args.Pointer());

            // pitch springs back to center, the way a wheel does
            ApplyPitchValue(0.0, true);

            args.Handled(true);
        }
        MIDI_KEYBOARD_CATCH_AND_LOG(L"Unable to release the pitch bend.")
    }

    _Use_decl_annotations_
    void MainWindow::OnModRibbonPointerPressed(
        foundation::IInspectable const&,
        input::PointerRoutedEventArgs const& args)
    {
        try
        {
            auto const position = args.GetCurrentPoint(ModRibbonCanvas()).Position();

            m_modPointerId = args.Pointer().PointerId();
            m_modCaptured = ModRibbonTrack().CapturePointer(args.Pointer());

            ApplyModValue(RibbonValueFromPoint(ModRibbonCanvas(), position.Y, false), true);

            args.Handled(true);
        }
        MIDI_KEYBOARD_CATCH_AND_LOG(L"Unable to start the modulation.")
    }

    _Use_decl_annotations_
    void MainWindow::OnModRibbonPointerMoved(
        foundation::IInspectable const&,
        input::PointerRoutedEventArgs const& args)
    {
        try
        {
            if (!m_modCaptured || args.Pointer().PointerId() != m_modPointerId)
            {
                return;
            }

            auto const position = args.GetCurrentPoint(ModRibbonCanvas()).Position();

            ApplyModValue(RibbonValueFromPoint(ModRibbonCanvas(), position.Y, false), true);

            args.Handled(true);
        }
        MIDI_KEYBOARD_CATCH_AND_LOG(L"Unable to change the modulation.")
    }

    _Use_decl_annotations_
    void MainWindow::OnModRibbonPointerReleased(
        foundation::IInspectable const&,
        input::PointerRoutedEventArgs const& args)
    {
        try
        {
            if (!m_modCaptured || args.Pointer().PointerId() != m_modPointerId)
            {
                return;
            }

            m_modCaptured = false;

            ModRibbonTrack().ReleasePointerCapture(args.Pointer());

            // modulation stays where it was left, the way a wheel does
            args.Handled(true);
        }
        MIDI_KEYBOARD_CATCH_AND_LOG(L"Unable to release the modulation.")
    }

    // ------------------------------------------------------------------------------------
    // Command strip
    // ------------------------------------------------------------------------------------

    _Use_decl_annotations_
    void MainWindow::OnOctaveDownClick(foundation::IInspectable const&, xaml::RoutedEventArgs const&)
    {
        try
        {
            auto& settings = native::AppSettings::Current();

            EndAllNotes();
            settings.BaseOctave(settings.BaseOctave() - 1);

            if (m_settingsControlsInitialized)
            {
                auto const previousSuppress = m_suppressSettingHandlers;
                m_suppressSettingHandlers = true;
                BaseOctaveBox().Value(settings.BaseOctave());
                m_suppressSettingHandlers = previousSuppress;
            }

            RebuildKeyboard();
            UpdateOctaveDisplay();
        }
        MIDI_KEYBOARD_CATCH_AND_LOG(L"Unable to shift down an octave.")
    }

    _Use_decl_annotations_
    void MainWindow::OnOctaveUpClick(foundation::IInspectable const&, xaml::RoutedEventArgs const&)
    {
        try
        {
            auto& settings = native::AppSettings::Current();

            EndAllNotes();
            settings.BaseOctave(settings.BaseOctave() + 1);

            if (m_settingsControlsInitialized)
            {
                auto const previousSuppress = m_suppressSettingHandlers;
                m_suppressSettingHandlers = true;
                BaseOctaveBox().Value(settings.BaseOctave());
                m_suppressSettingHandlers = previousSuppress;
            }

            RebuildKeyboard();
            UpdateOctaveDisplay();
        }
        MIDI_KEYBOARD_CATCH_AND_LOG(L"Unable to shift up an octave.")
    }

    _Use_decl_annotations_
    void MainWindow::OnPanicClick(foundation::IInspectable const&, xaml::RoutedEventArgs const&)
    {
        EndAllNotes();
        ApplyPitchValue(0.0, true);
        ApplyModValue(0.0, true);
    }

    _Use_decl_annotations_
    void MainWindow::OnArpModeChanged(foundation::IInspectable const&, controls::SelectionChangedEventArgs const&)
    {
        try
        {
            if (m_suppressArpHandlers)
            {
                return;
            }

            auto const index = ArpModeComboBox().SelectedIndex();

            if (index < 0)
            {
                return;
            }

            auto const mode = static_cast<native::ArpeggiatorMode>(index);

            if (mode == native::AppSettings::Current().Arpeggiator())
            {
                return;
            }

            EndAllNotes();

            native::AppSettings::Current().Arpeggiator(mode);
            m_arpeggiator.Mode(mode);
        }
        MIDI_KEYBOARD_CATCH_AND_LOG(L"Unable to change the arpeggiator mode.")
    }

    _Use_decl_annotations_
    void MainWindow::OnArpRateChanged(foundation::IInspectable const&, controls::SelectionChangedEventArgs const&)
    {
        try
        {
            if (m_suppressArpHandlers)
            {
                return;
            }

            auto const index = ArpRateComboBox().SelectedIndex();

            if (index < 0)
            {
                return;
            }

            auto& settings = native::AppSettings::Current();
            auto const division = static_cast<native::ArpeggiatorDivision>(index);

            if (division == settings.ArpeggiatorRate())
            {
                return;
            }

            settings.ArpeggiatorRate(division);
            m_arpeggiator.Rate(settings.ArpeggiatorBpm(), settings.ArpeggiatorRate());
        }
        MIDI_KEYBOARD_CATCH_AND_LOG(L"Unable to change the arpeggiator rate.")
    }

    _Use_decl_annotations_
    void MainWindow::OnArpBpmChanged(
        controls::NumberBox const&,
        controls::NumberBoxValueChangedEventArgs const& args)
    {
        try
        {
            if (m_suppressArpHandlers || std::isnan(args.NewValue()))
            {
                return;
            }

            auto& settings = native::AppSettings::Current();
            auto const bpm = static_cast<uint32_t>(std::lround(args.NewValue()));

            if (bpm == settings.ArpeggiatorBpm())
            {
                return;
            }

            settings.ArpeggiatorBpm(bpm);
            m_arpeggiator.Rate(settings.ArpeggiatorBpm(), settings.ArpeggiatorRate());
        }
        MIDI_KEYBOARD_CATCH_AND_LOG(L"Unable to change the arpeggiator tempo.")
    }

    // ------------------------------------------------------------------------------------
    // Settings panel
    // ------------------------------------------------------------------------------------

    _Use_decl_annotations_
    void MainWindow::OnSettingsToggleClick(foundation::IInspectable const&, xaml::RoutedEventArgs const&)
    {
        try
        {
            auto const checked = SettingsToggle().IsChecked();
            auto const isOn = checked != nullptr && checked.Value();

            if (isOn)
            {
                InitializeSettingsPanelControls();
            }

            SettingsPanel().Visibility(isOn ? xaml::Visibility::Visible : xaml::Visibility::Collapsed);

            if (!isOn)
            {
                KeyboardCanvas().Focus(xaml::FocusState::Programmatic);
            }
        }
        MIDI_KEYBOARD_CATCH_AND_LOG(L"Unable to show the settings.")
    }

    _Use_decl_annotations_
    void MainWindow::OnConnectionModeChanged(
        foundation::IInspectable const&,
        controls::SelectionChangedEventArgs const&)
    {
        try
        {
            if (m_suppressSettingHandlers)
            {
                return;
            }

            auto const index = ConnectionModeRadios().SelectedIndex();

            if (index < 0)
            {
                return;
            }

            auto const mode = static_cast<native::ConnectionMode>(index);

            // a control can raise this long after its value was set, so only a real change acts
            if (mode == native::AppSettings::Current().Connection())
            {
                return;
            }

            native::AppSettings::Current().Connection(mode);

            UpdateConnectionModeLayout();
            ReconnectAsync();
        }
        MIDI_KEYBOARD_CATCH_AND_LOG(L"Unable to change the connection type.")
    }

    _Use_decl_annotations_
    void MainWindow::OnEndpointSelectionChanged(
        foundation::IInspectable const&,
        controls::SelectionChangedEventArgs const&)
    {
        try
        {
            if (m_suppressSettingHandlers)
            {
                return;
            }

            auto const choice = EndpointComboBox().SelectedItem().try_as<appshared::EndpointChoice>();

            std::wstring const endpointId{ choice == nullptr ? L"" : std::wstring{ choice.EndpointDeviceId() } };

            if (endpointId == native::AppSettings::Current().EndpointDeviceId())
            {
                return;
            }

            native::AppSettings::Current().EndpointDeviceId(endpointId);

            RefreshGroupList();
            ReconnectAsync();
        }
        MIDI_KEYBOARD_CATCH_AND_LOG(L"Unable to change the destination.")
    }

    _Use_decl_annotations_
    void MainWindow::OnGroupSelectionChanged(
        foundation::IInspectable const&,
        controls::SelectionChangedEventArgs const&)
    {
        try
        {
            if (m_suppressSettingHandlers)
            {
                return;
            }

            if (auto const choice = GroupComboBox().SelectedItem().try_as<appshared::NamedChoice>())
            {
                auto const groupNumber = static_cast<uint32_t>(choice.Value());

                if (groupNumber == native::AppSettings::Current().TransmitGroupNumber())
                {
                    return;
                }

                EndAllNotes();
                native::AppSettings::Current().TransmitGroupNumber(groupNumber);
                UpdateConnectionDisplay(native::ConnectResult::Success);
            }
        }
        MIDI_KEYBOARD_CATCH_AND_LOG(L"Unable to change the group.")
    }

    _Use_decl_annotations_
    void MainWindow::OnChannelSelectionChanged(
        foundation::IInspectable const&,
        controls::SelectionChangedEventArgs const&)
    {
        try
        {
            if (m_suppressSettingHandlers)
            {
                return;
            }

            auto const index = ChannelComboBox().SelectedIndex();

            if (index < 0)
            {
                return;
            }

            auto const channelNumber = static_cast<uint32_t>(index) + 1;

            if (channelNumber == native::AppSettings::Current().TransmitChannelNumber())
            {
                return;
            }

            EndAllNotes();
            native::AppSettings::Current().TransmitChannelNumber(channelNumber);
            UpdateConnectionDisplay(native::ConnectResult::Success);
        }
        MIDI_KEYBOARD_CATCH_AND_LOG(L"Unable to change the channel.")
    }

    _Use_decl_annotations_
    void MainWindow::OnBaseOctaveChanged(
        controls::NumberBox const&,
        controls::NumberBoxValueChangedEventArgs const& args)
    {
        try
        {
            if (m_suppressSettingHandlers || std::isnan(args.NewValue()))
            {
                return;
            }

            auto const octave = static_cast<int32_t>(std::lround(args.NewValue()));

            if (octave == native::AppSettings::Current().BaseOctave())
            {
                return;
            }

            EndAllNotes();
            native::AppSettings::Current().BaseOctave(octave);

            RebuildKeyboard();
            UpdateOctaveDisplay();
        }
        MIDI_KEYBOARD_CATCH_AND_LOG(L"Unable to change the base octave.")
    }

    _Use_decl_annotations_
    void MainWindow::OnOctaveCountChanged(
        controls::NumberBox const&,
        controls::NumberBoxValueChangedEventArgs const& args)
    {
        try
        {
            if (m_suppressSettingHandlers || std::isnan(args.NewValue()))
            {
                return;
            }

            auto const count = static_cast<uint32_t>(std::lround(args.NewValue()));

            if (count == native::AppSettings::Current().OctaveCount())
            {
                return;
            }

            EndAllNotes();
            native::AppSettings::Current().OctaveCount(count);

            RebuildKeyboard();
            UpdateOctaveDisplay();
        }
        MIDI_KEYBOARD_CATCH_AND_LOG(L"Unable to change the octave count.")
    }

    _Use_decl_annotations_
    void MainWindow::OnTransposeChanged(
        controls::NumberBox const&,
        controls::NumberBoxValueChangedEventArgs const& args)
    {
        try
        {
            if (m_suppressSettingHandlers || std::isnan(args.NewValue()))
            {
                return;
            }

            auto const transpose = static_cast<int32_t>(std::lround(args.NewValue()));

            if (transpose == native::AppSettings::Current().Transpose())
            {
                return;
            }

            EndAllNotes();
            native::AppSettings::Current().Transpose(transpose);

            LayoutKeyboard();
            UpdateOctaveDisplay();
        }
        MIDI_KEYBOARD_CATCH_AND_LOG(L"Unable to change the transposition.")
    }

    _Use_decl_annotations_
    void MainWindow::OnShowNoteNamesChanged(foundation::IInspectable const&, xaml::RoutedEventArgs const&)
    {
        try
        {
            if (m_suppressSettingHandlers)
            {
                return;
            }

            auto const checked = ShowNoteNamesCheckBox().IsChecked();
            auto const show = checked != nullptr && checked.Value();

            if (show == native::AppSettings::Current().ShowNoteNames())
            {
                return;
            }

            native::AppSettings::Current().ShowNoteNames(show);

            LayoutKeyboard();
        }
        MIDI_KEYBOARD_CATCH_AND_LOG(L"Unable to change the note name display.")
    }

    _Use_decl_annotations_
    void MainWindow::OnShowComputerKeysChanged(foundation::IInspectable const&, xaml::RoutedEventArgs const&)
    {
        try
        {
            if (m_suppressSettingHandlers)
            {
                return;
            }

            auto const checked = ShowComputerKeysCheckBox().IsChecked();
            auto const show = checked != nullptr && checked.Value();

            if (show == native::AppSettings::Current().ShowComputerKeys())
            {
                return;
            }

            native::AppSettings::Current().ShowComputerKeys(show);

            LayoutKeyboard();
        }
        MIDI_KEYBOARD_CATCH_AND_LOG(L"Unable to change the computer key display.")
    }

    _Use_decl_annotations_
    void MainWindow::OnRibbonPositionChanged(
        foundation::IInspectable const&,
        controls::SelectionChangedEventArgs const&)
    {
        try
        {
            if (m_suppressSettingHandlers)
            {
                return;
            }

            auto const index = RibbonPositionComboBox().SelectedIndex();

            if (index < 0)
            {
                return;
            }

            auto const position = static_cast<native::RibbonPosition>(index);

            if (position == native::AppSettings::Current().Ribbons())
            {
                return;
            }

            native::AppSettings::Current().Ribbons(position);

            UpdateRibbonLayout();
        }
        MIDI_KEYBOARD_CATCH_AND_LOG(L"Unable to move the ribbons.")
    }

    _Use_decl_annotations_
    void MainWindow::OnVelocityModeChanged(
        foundation::IInspectable const&,
        controls::SelectionChangedEventArgs const&)
    {
        try
        {
            if (m_suppressSettingHandlers)
            {
                return;
            }

            auto const index = VelocityModeComboBox().SelectedIndex();

            if (index < 0)
            {
                return;
            }

            auto const mode = static_cast<native::VelocityMode>(index);

            if (mode == native::AppSettings::Current().Velocity())
            {
                return;
            }

            native::AppSettings::Current().Velocity(mode);

            UpdateVelocityLayout();
        }
        MIDI_KEYBOARD_CATCH_AND_LOG(L"Unable to change the velocity mode.")
    }

    _Use_decl_annotations_
    void MainWindow::OnVelocityMinimumChanged(
        controls::NumberBox const&,
        controls::NumberBoxValueChangedEventArgs const& args)
    {
        try
        {
            if (m_suppressSettingHandlers || std::isnan(args.NewValue()))
            {
                return;
            }

            auto& settings = native::AppSettings::Current();

            settings.VelocityMinimum(static_cast<uint32_t>(std::lround(args.NewValue())));

            if (settings.VelocityMaximum() < settings.VelocityMinimum())
            {
                settings.VelocityMaximum(settings.VelocityMinimum());

                auto const previousSuppress = m_suppressSettingHandlers;
                m_suppressSettingHandlers = true;
                VelocityMaximumBox().Value(settings.VelocityMaximum());
                m_suppressSettingHandlers = previousSuppress;
            }
        }
        MIDI_KEYBOARD_CATCH_AND_LOG(L"Unable to change the softest velocity.")
    }

    _Use_decl_annotations_
    void MainWindow::OnVelocityMaximumChanged(
        controls::NumberBox const&,
        controls::NumberBoxValueChangedEventArgs const& args)
    {
        try
        {
            if (m_suppressSettingHandlers || std::isnan(args.NewValue()))
            {
                return;
            }

            auto& settings = native::AppSettings::Current();

            settings.VelocityMaximum(static_cast<uint32_t>(std::lround(args.NewValue())));

            if (settings.VelocityMaximum() < settings.VelocityMinimum())
            {
                settings.VelocityMinimum(settings.VelocityMaximum());

                auto const previousSuppress = m_suppressSettingHandlers;
                m_suppressSettingHandlers = true;
                VelocityMinimumBox().Value(settings.VelocityMinimum());
                m_suppressSettingHandlers = previousSuppress;
            }
        }
        MIDI_KEYBOARD_CATCH_AND_LOG(L"Unable to change the hardest velocity.")
    }

    _Use_decl_annotations_
    void MainWindow::OnFixedVelocityChanged(
        controls::NumberBox const&,
        controls::NumberBoxValueChangedEventArgs const& args)
    {
        try
        {
            if (m_suppressSettingHandlers || std::isnan(args.NewValue()))
            {
                return;
            }

            native::AppSettings::Current().FixedVelocity(static_cast<uint32_t>(std::lround(args.NewValue())));
        }
        MIDI_KEYBOARD_CATCH_AND_LOG(L"Unable to change the fixed velocity.")
    }

    _Use_decl_annotations_
    void MainWindow::OnKeyPressureChanged(
        foundation::IInspectable const&,
        controls::SelectionChangedEventArgs const&)
    {
        try
        {
            if (m_suppressSettingHandlers)
            {
                return;
            }

            auto const index = KeyPressureComboBox().SelectedIndex();

            if (index < 0)
            {
                return;
            }

            auto const mode = static_cast<native::KeyPressureMode>(index);

            if (mode == native::AppSettings::Current().KeyPressure())
            {
                return;
            }

            EndAllNotes();
            native::AppSettings::Current().KeyPressure(mode);

            UpdateVelocityLayout();
        }
        MIDI_KEYBOARD_CATCH_AND_LOG(L"Unable to change the key pressure mode.")
    }

    _Use_decl_annotations_
    void MainWindow::OnPerNoteControllerChanged(
        controls::NumberBox const&,
        controls::NumberBoxValueChangedEventArgs const& args)
    {
        try
        {
            if (m_suppressSettingHandlers || std::isnan(args.NewValue()))
            {
                return;
            }

            auto const controllerIndex = static_cast<uint32_t>(std::lround(args.NewValue()));

            if (controllerIndex == native::AppSettings::Current().PerNoteControllerIndex())
            {
                return;
            }

            native::AppSettings::Current().PerNoteControllerIndex(controllerIndex);
        }
        MIDI_KEYBOARD_CATCH_AND_LOG(L"Unable to change the per-note controller.")
    }

    // ------------------------------------------------------------------------------------
    // Chrome plumbing
    // ------------------------------------------------------------------------------------

    _Use_decl_annotations_
    void MainWindow::OnRootSizeChanged(foundation::IInspectable const&, xaml::SizeChangedEventArgs const&)
    {
        m_chrome.UpdateTitleBarInsets();
    }

    _Use_decl_annotations_
    void MainWindow::OnAlwaysOnTopToggled(foundation::IInspectable const&, xaml::RoutedEventArgs const&)
    {
        try
        {
            auto const checked = AlwaysOnTopToggle().IsChecked();

            native::AppSettings::Current().AlwaysOnTop(checked != nullptr && checked.Value());

            m_chrome.ApplyAlwaysOnTop();
        }
        MIDI_KEYBOARD_CATCH_AND_LOG(L"Unable to change the always on top setting.")
    }

    _Use_decl_annotations_
    void MainWindow::OnAppearanceClick(foundation::IInspectable const&, xaml::RoutedEventArgs const&)
    {
        try
        {
            midiapp::AppearanceStrings strings{};

            strings.Title = res::GetString(L"SettingsTitle");
            strings.ThemeLabel = res::GetString(L"SettingsThemeLabel");
            strings.ThemeSystem = res::GetString(L"SettingsThemeSystem");
            strings.ThemeLight = res::GetString(L"SettingsThemeLight");
            strings.ThemeDark = res::GetString(L"SettingsThemeDark");
            strings.BackdropLabel = res::GetString(L"SettingsBackdropLabel");
            strings.BackdropSolid = res::GetString(L"SettingsBackdropSolid");
            strings.BackdropMica = res::GetString(L"SettingsBackdropMica");
            strings.BackdropAcrylic = res::GetString(L"SettingsBackdropAcrylic");
            strings.CustomColorCheckBox = res::GetString(L"SettingsCustomColorCheckBox");
            strings.ColorPickerName = res::GetString(L"SettingsColorPickerName");

            midiapp::ShowAppearanceFlyout(
                AppearanceButton(),
                native::AppSettings::Current(),
                strings,
                [weak = get_weak()]()
                {
                    if (auto strong = weak.get())
                    {
                        strong->m_chrome.ApplyTheme();
                    }
                });
        }
        MIDI_KEYBOARD_CATCH_AND_LOG(L"Unable to open the appearance settings.")
    }
}
