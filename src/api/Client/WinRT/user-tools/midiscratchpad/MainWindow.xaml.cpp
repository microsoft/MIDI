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
#include "StringResources.h"
#include "resource.h"

namespace native = ::midiscratchpad;
namespace res = ::midiscratchpad::resources;

namespace winrt::midiscratchpad::implementation
{
    namespace
    {
        constexpr wchar_t NoteNames[][8]
        {
            L"C", L"C#/Db", L"D", L"D#/Eb", L"E", L"F",
            L"F#/Gb", L"G", L"G#/Ab", L"A", L"A#/Bb", L"B"
        };

        // middle C, note 60, is C3 here, matching the rest of the Windows MIDI Services tools
        std::wstring NoteDisplayName(uint8_t noteIndex) noexcept
        {
            auto const octave = static_cast<int32_t>(noteIndex / 12) - 2;

            return std::format(L"{} ({}{})", noteIndex, NoteNames[noteIndex % 12], octave);
        }

        std::wstring ControllerDisplayName(uint8_t index) noexcept
        {
            wchar_t const* name{ nullptr };

            switch (index)
            {
            case 0: name = L"Bank select"; break;
            case 1: name = L"Mod wheel"; break;
            case 2: name = L"Breath"; break;
            case 4: name = L"Foot pedal"; break;
            case 5: name = L"Portamento time"; break;
            case 6: name = L"Data entry MSB"; break;
            case 7: name = L"Volume"; break;
            case 8: name = L"Balance"; break;
            case 10: name = L"Pan"; break;
            case 11: name = L"Expression"; break;
            case 32: name = L"Bank select LSB"; break;
            case 64: name = L"Sustain pedal"; break;
            case 65: name = L"Portamento"; break;
            case 66: name = L"Sostenuto pedal"; break;
            case 67: name = L"Soft pedal"; break;
            case 68: name = L"Legato footswitch"; break;
            case 69: name = L"Hold 2"; break;
            case 74: name = L"Brightness"; break;
            case 84: name = L"Portamento amount"; break;
            case 88: name = L"High resolution velocity"; break;
            case 96: name = L"Data increment"; break;
            case 97: name = L"Data decrement"; break;
            case 98: name = L"NRPN LSB"; break;
            case 99: name = L"NRPN MSB"; break;
            case 100: name = L"RPN LSB"; break;
            case 101: name = L"RPN MSB"; break;
            case 120: name = L"All sound off"; break;
            case 121: name = L"Reset all controllers"; break;
            case 122: name = L"Local control"; break;
            case 123: name = L"All notes off"; break;
            case 124: name = L"Omni mode off"; break;
            case 125: name = L"Omni mode on"; break;
            case 126: name = L"Mono mode on"; break;
            case 127: name = L"Poly mode on"; break;
            default: break;
            }

            return name == nullptr
                ? std::format(L"{}", index)
                : std::format(L"{} ({})", index, name);
        }

        int32_t SelectedIndexOrZero(controls::ComboBox const& box) noexcept
        {
            auto const index = box.SelectedIndex();
            return index < 0 ? 0 : index;
        }

        bool IsToggleOn(controls::ToggleSwitch const& toggle) noexcept
        {
            return toggle != nullptr && toggle.IsOn();
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
        midiapp::WindowChrome::RestorePlacement(*this, native::AppSettings::Current(), 1100, 780);
    }

    _Use_decl_annotations_
    void MainWindow::OnRootLoaded(foundation::IInspectable const&, xaml::RoutedEventArgs const&)
    {
        try
        {
            InitializeWindowChrome();
            InitializeCollections();
            InitializeControlsFromSettings();

            Closed([weak = get_weak()](auto&&, auto&&)
                {
                    if (auto strong = weak.get())
                    {
                        strong->m_chrome.SavePlacement();
                        strong->StopEndpointWatcher();
                        strong->m_chrome.Shutdown();
                    }
                });

            StartEndpointWatcher();

            m_initialized = true;

            UpdateParseStatus();
            UpdateCommandStates();

            // programmatic rather than keyboard focus, so the device picker is where typing and
            // tabbing begin without drawing a focus rectangle on launch
            EndpointComboBox().Focus(xaml::FocusState::Programmatic);
        }
        MIDI_SCRATCHPAD_CATCH_AND_LOG(L"Unable to finish loading the window.")
    }

    void MainWindow::InitializeWindowChrome() noexcept
    {
        try
        {
            UpdateWindowTitle();

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
        }
        MIDI_SCRATCHPAD_CATCH_AND_LOG(L"Unable to set up the window chrome.")
    }

    void MainWindow::InitializeCollections() noexcept
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
            ChannelComboBox().SelectedIndex(0);

            auto notes = winrt::single_threaded_vector<foundation::IInspectable>();
            auto velocities = winrt::single_threaded_vector<foundation::IInspectable>();
            auto controllers = winrt::single_threaded_vector<foundation::IInspectable>();
            auto controllerValues = winrt::single_threaded_vector<foundation::IInspectable>();

            for (uint8_t i = 0; i < 128; i++)
            {
                notes.Append(winrt::box_value(winrt::hstring{ NoteDisplayName(i) }));
                velocities.Append(winrt::box_value(winrt::hstring{ std::format(L"{}", i) }));
                controllers.Append(winrt::box_value(winrt::hstring{ ControllerDisplayName(i) }));
                controllerValues.Append(winrt::box_value(winrt::hstring{ std::format(L"{}", i) }));
            }

            NoteComboBox().ItemsSource(notes);
            NoteComboBox().SelectedIndex(60);

            VelocityComboBox().ItemsSource(velocities);
            VelocityComboBox().SelectedIndex(96);

            ControllerComboBox().ItemsSource(controllers);
            ControllerComboBox().SelectedIndex(7);

            ControllerValueComboBox().ItemsSource(controllerValues);
            ControllerValueComboBox().SelectedIndex(64);
        }
        MIDI_SCRATCHPAD_CATCH_AND_LOG(L"Unable to build the pick lists.")
    }

    void MainWindow::InitializeControlsFromSettings() noexcept
    {
        try
        {
            auto const& settings = native::AppSettings::Current();
            auto const& options = App::StartupOptions();

            // an explicit mode on the command line wins over the remembered one, and sticks so
            // the next launch without arguments opens the same way
            auto const mode = options.Mode.value_or(settings.Mode());

            if (options.Mode.has_value() && options.Mode.value() != settings.Mode())
            {
                native::AppSettings::Current().Mode(mode);
            }

            m_suppressModeHandling = true;

            if (mode == native::ScratchPadMode::UmpWords)
            {
                UmpModeButton().IsChecked(true);
            }
            else
            {
                Midi1ModeButton().IsChecked(true);
            }

            m_suppressModeHandling = false;

            RunningStatusToggle().IsOn(settings.AllowRunningStatus());
            AlwaysOnTopToggle().IsChecked(settings.AlwaysOnTop());

            EditorTextBox().FontSize(
                native::AppSettings::BaseEditorFontSize * (settings.EditorFontPercent() / 100.0));

            ApplyModeToLayout();
        }
        MIDI_SCRATCHPAD_CATCH_AND_LOG(L"Unable to apply the saved settings.")
    }

    // ------------------------------------------------------------------------------------
    // Endpoints
    // ------------------------------------------------------------------------------------

    void MainWindow::StartEndpointWatcher() noexcept
    {
        try
        {
            if (!midi2::MidiApi::EnsureServiceAvailable())
            {
                ShowMessageAsync(
                    res::GetString(L"ServiceUnavailableTitle"),
                    res::GetString(L"ServiceUnavailableBody"));
                return;
            }

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
        MIDI_SCRATCHPAD_CATCH_AND_LOG(L"Unable to start the endpoint device watcher.")
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
        MIDI_SCRATCHPAD_CATCH_AND_LOG(L"Unable to stop the endpoint device watcher.")
    }

    void MainWindow::RefreshEndpointList() noexcept
    {
        try
        {
            if (m_watcher == nullptr || m_endpoints == nullptr)
            {
                return;
            }

            auto const previousSelection = SelectedEndpointDeviceId();
            auto const devices = midiapp::SortedEndpoints(m_watcher);

            m_suppressSelectionHandling = true;

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

            m_suppressSelectionHandling = false;

            auto const& options = App::StartupOptions();

            auto desiredEndpointId = previousSelection;

            if (desiredEndpointId.empty() && !options.HasError && !options.ShowHelp && !options.EndpointDeviceId.empty())
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
        }
        MIDI_SCRATCHPAD_CATCH_AND_LOG(L"Unable to refresh the endpoint list.")
    }

    void MainWindow::RefreshGroupList() noexcept
    {
        try
        {
            auto const endpointIndex = EndpointComboBox().SelectedIndex();
            auto const previousGroupNumber = SelectedGroupNumber();

            m_suppressSelectionHandling = true;

            m_groups.Clear();

            if (endpointIndex >= 0 && static_cast<size_t>(endpointIndex) < m_endpointDevices.size())
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

            m_suppressSelectionHandling = false;

            int32_t selectedIndex{ m_groups.Size() > 0 ? 0 : -1 };

            auto const& options = App::StartupOptions();

            if (!m_startupOptionsApplied && endpointIndex >= 0)
            {
                m_startupOptionsApplied = true;

                if (options.GroupNumber.has_value())
                {
                    for (uint32_t i = 0; i < m_groups.Size(); i++)
                    {
                        if (m_groups.GetAt(i).Value() == static_cast<int32_t>(options.GroupNumber.value()))
                        {
                            selectedIndex = static_cast<int32_t>(i);
                            break;
                        }
                    }
                }
            }
            else if (previousGroupNumber > 0)
            {
                for (uint32_t i = 0; i < m_groups.Size(); i++)
                {
                    if (m_groups.GetAt(i).Value() == static_cast<int32_t>(previousGroupNumber))
                    {
                        selectedIndex = static_cast<int32_t>(i);
                        break;
                    }
                }
            }

            GroupComboBox().SelectedIndex(selectedIndex);
        }
        MIDI_SCRATCHPAD_CATCH_AND_LOG(L"Unable to refresh the group list.")
    }

    winrt::hstring MainWindow::SelectedEndpointDeviceId() noexcept
    {
        try
        {
            if (auto const choice = EndpointComboBox().SelectedItem().try_as<appshared::EndpointChoice>())
            {
                return choice.EndpointDeviceId();
            }
        }
        MIDI_SCRATCHPAD_CATCH_AND_LOG(L"Unable to read the selected endpoint.")

        return {};
    }

    uint8_t MainWindow::SelectedGroupNumber() noexcept
    {
        try
        {
            if (auto const choice = GroupComboBox().SelectedItem().try_as<appshared::NamedChoice>())
            {
                return static_cast<uint8_t>(std::clamp(choice.Value(), 0, 16));
            }
        }
        MIDI_SCRATCHPAD_CATCH_AND_LOG(L"Unable to read the selected group.")

        return 0;
    }

    uint8_t MainWindow::SelectedChannelNumber() noexcept
    {
        return static_cast<uint8_t>(SelectedIndexOrZero(ChannelComboBox()) + 1);
    }

    _Use_decl_annotations_
    void MainWindow::OnEndpointSelectionChanged(foundation::IInspectable const&, controls::SelectionChangedEventArgs const&)
    {
        if (m_suppressSelectionHandling)
        {
            return;
        }

        RefreshGroupList();
        UpdateWindowTitle();
        UpdateCommandStates();
    }

    _Use_decl_annotations_
    void MainWindow::OnGroupSelectionChanged(foundation::IInspectable const&, controls::SelectionChangedEventArgs const&)
    {
        if (m_suppressSelectionHandling)
        {
            return;
        }

        UpdateWindowTitle();
        UpdateCommandStates();
    }

    // The image lives in the item template, which the closed ComboBox also uses for the
    // selection box. Remove it there so the picture only shows in the drop down list.
    _Use_decl_annotations_
    void MainWindow::OnEndpointChoiceLoaded(foundation::IInspectable const& sender, xaml::RoutedEventArgs const&)
    {
        try
        {
            auto const panel = sender.try_as<controls::Grid>();

            if (panel == nullptr)
            {
                return;
            }

            uint32_t imageIndex{ 0 };
            bool foundImage{ false };

            for (uint32_t i = 0; i < panel.Children().Size(); i++)
            {
                if (panel.Children().GetAt(i).try_as<controls::Image>() != nullptr)
                {
                    imageIndex = i;
                    foundImage = true;
                    break;
                }
            }

            if (!foundImage)
            {
                return;
            }

            xaml::DependencyObject current{ panel };

            while (current != nullptr)
            {
                if (current.try_as<controls::ComboBoxItem>() != nullptr)
                {
                    return;
                }

                current = media::VisualTreeHelper::GetParent(current);
            }

            // removal rather than collapse: the binding re-evaluates when the DataContext changes
            panel.Children().RemoveAt(imageIndex);
        }
        MIDI_SCRATCHPAD_CATCH_AND_LOG(L"Unable to adjust the endpoint item template.")
    }

    // ------------------------------------------------------------------------------------
    // Mode and editing
    // ------------------------------------------------------------------------------------

    ::midiscratchpad::ScratchPadMode MainWindow::CurrentMode() noexcept
    {
        try
        {
            auto const checked = UmpModeButton().IsChecked();

            if (checked != nullptr && checked.Value())
            {
                return native::ScratchPadMode::UmpWords;
            }
        }
        MIDI_SCRATCHPAD_CATCH_AND_LOG(L"Unable to read the current mode.")

        return native::ScratchPadMode::Midi1Bytes;
    }

    void MainWindow::ApplyModeToLayout() noexcept
    {
        try
        {
            auto const isMidi1 = CurrentMode() == native::ScratchPadMode::Midi1Bytes;

            // in UMP mode the group travels inside the message, so the picker would be a lie
            GroupPanel().Visibility(isMidi1 ? xaml::Visibility::Visible : xaml::Visibility::Collapsed);
            GroupColumn().Width(isMidi1
                ? xaml::GridLength{ 1.0, xaml::GridUnitType::Star }
                : xaml::GridLength{ 0.0, xaml::GridUnitType::Pixel });

            RunningStatusToggle().Visibility(isMidi1 ? xaml::Visibility::Visible : xaml::Visibility::Collapsed);

            // the insert helpers all emit MIDI 1.0 bytes
            InsertRail().Visibility(isMidi1 ? xaml::Visibility::Visible : xaml::Visibility::Collapsed);
            InsertRailColumn().Width(isMidi1
                ? xaml::GridLength{ 1.0, xaml::GridUnitType::Auto }
                : xaml::GridLength{ 0.0, xaml::GridUnitType::Pixel });

            EditorTextBox().PlaceholderText(res::GetString(
                isMidi1 ? L"EditorPlaceholderMidi1" : L"EditorPlaceholderUmp"));
        }
        MIDI_SCRATCHPAD_CATCH_AND_LOG(L"Unable to lay out for the current mode.")
    }

    _Use_decl_annotations_
    void MainWindow::OnModeChanged(foundation::IInspectable const&, xaml::RoutedEventArgs const&)
    {
        if (m_suppressModeHandling)
        {
            return;
        }

        try
        {
            native::AppSettings::Current().Mode(CurrentMode());

            ApplyModeToLayout();
            UpdateParseStatus();
            UpdateCommandStates();
            UpdateWindowTitle();
        }
        MIDI_SCRATCHPAD_CATCH_AND_LOG(L"Unable to change the mode.")
    }

    _Use_decl_annotations_
    void MainWindow::OnEditorTextChanged(foundation::IInspectable const&, controls::TextChangedEventArgs const&)
    {
        if (!m_initialized)
        {
            return;
        }

        UpdateParseStatus();
        UpdateCommandStates();
    }

    _Use_decl_annotations_
    void MainWindow::OnEditorSelectionChanged(foundation::IInspectable const&, xaml::RoutedEventArgs const&)
    {
        try
        {
            m_selectionText = EditorTextBox().SelectedText();
            UpdateCommandStates();
        }
        MIDI_SCRATCHPAD_CATCH_AND_LOG(L"Unable to track the editor selection.")
    }

    void MainWindow::UpdateParseStatus() noexcept
    {
        try
        {
            std::wstring const text{ EditorTextBox().Text() };

            auto const isMidi1 = CurrentMode() == native::ScratchPadMode::Midi1Bytes;

            m_lastParse = isMidi1
                ? native::ParseMidi1Bytes(text)
                : native::ParseUmpWords(text);

            if (m_lastParse.HasError)
            {
                StatusIcon().Glyph(L"\uEA39");
                StatusIcon().Foreground(xaml::Application::Current().Resources()
                    .Lookup(winrt::box_value(L"SystemFillColorCriticalBrush")).as<media::Brush>());

                auto const detail = m_lastParse.ErrorToken.empty()
                    ? res::GetString(m_lastParse.ErrorResourceKey.c_str())
                    : res::FormatString(m_lastParse.ErrorResourceKey.c_str(), m_lastParse.ErrorToken.c_str());

                StatusText().Text(m_lastParse.ErrorLine > 0
                    ? res::FormatString(L"StatusErrorWithLine", static_cast<int32_t>(m_lastParse.ErrorLine), detail.c_str())
                    : detail);

                return;
            }

            StatusIcon().Glyph(L"\uE73E");
            StatusIcon().Foreground(xaml::Application::Current().Resources()
                .Lookup(winrt::box_value(L"SystemFillColorSuccessBrush")).as<media::Brush>());

            if (m_lastParse.IsEmpty())
            {
                StatusText().Text(res::GetString(L"StatusEmpty"));
                return;
            }

            if (isMidi1)
            {
                StatusText().Text(res::FormatString(
                    L"StatusMidi1Format", static_cast<int32_t>(m_lastParse.Bytes.size())));
            }
            else
            {
                StatusText().Text(res::FormatString(
                    L"StatusUmpFormat",
                    static_cast<int32_t>(m_lastParse.Words.size()),
                    static_cast<int32_t>(m_lastParse.PacketCount)));
            }
        }
        MIDI_SCRATCHPAD_CATCH_AND_LOG(L"Unable to update the parse status.")
    }

    void MainWindow::UpdateCommandStates() noexcept
    {
        try
        {
            auto const hasEndpoint = !SelectedEndpointDeviceId().empty();
            auto const isMidi1 = CurrentMode() == native::ScratchPadMode::Midi1Bytes;
            auto const hasGroup = !isMidi1 || SelectedGroupNumber() > 0;

            auto const sendable = hasEndpoint && hasGroup && !m_lastParse.HasError && !m_lastParse.IsEmpty();

            SendAllButton().IsEnabled(sendable);
            SendSelectionButton().IsEnabled(sendable && !m_selectionText.empty());
            ClearButton().IsEnabled(!EditorTextBox().Text().empty());
        }
        MIDI_SCRATCHPAD_CATCH_AND_LOG(L"Unable to update the command states.")
    }

    void MainWindow::UpdateWindowTitle() noexcept
    {
        try
        {
            auto const appName = res::GetString(L"AppDisplayName");

            auto const endpointIndex = EndpointComboBox().SelectedIndex();

            if (endpointIndex < 0 || static_cast<size_t>(endpointIndex) >= m_endpointDevices.size())
            {
                Title(appName);
                AppTitleTextBlock().Text(appName);
                return;
            }

            auto const name = m_endpointDevices[static_cast<size_t>(endpointIndex)].Name();

            auto const title = res::FormatString(L"WindowTitleFormat", name.c_str(), appName.c_str());

            Title(title);
            AppTitleTextBlock().Text(title);
        }
        MIDI_SCRATCHPAD_CATCH_AND_LOG(L"Unable to update the window title.")
    }

    // ------------------------------------------------------------------------------------
    // Insert helpers
    // ------------------------------------------------------------------------------------

    void MainWindow::AppendText(std::wstring const& text) noexcept
    {
        try
        {
            std::wstring current{ EditorTextBox().Text() };

            // the TextBox normalizes line endings, so strip whatever it left and supply our own
            while (!current.empty() && (current.back() == L'\r' || current.back() == L'\n'))
            {
                current.pop_back();
            }

            if (!current.empty())
            {
                current += L"\r\n";
            }

            current += text;

            EditorTextBox().Text(winrt::hstring{ current });
            EditorTextBox().Select(static_cast<int32_t>(current.size()), 0);
            EditorTextBox().Focus(xaml::FocusState::Programmatic);
        }
        MIDI_SCRATCHPAD_CATCH_AND_LOG(L"Unable to insert the message.")
    }

    _Use_decl_annotations_
    void MainWindow::OnAddNoteOn(foundation::IInspectable const&, xaml::RoutedEventArgs const&)
    {
        AppendText(std::format(L"9{:X} {:02X} {:02X}",
            SelectedIndexOrZero(ChannelComboBox()),
            SelectedIndexOrZero(NoteComboBox()),
            SelectedIndexOrZero(VelocityComboBox())));
    }

    _Use_decl_annotations_
    void MainWindow::OnAddNoteOff(foundation::IInspectable const&, xaml::RoutedEventArgs const&)
    {
        AppendText(std::format(L"8{:X} {:02X} {:02X}",
            SelectedIndexOrZero(ChannelComboBox()),
            SelectedIndexOrZero(NoteComboBox()),
            0));
    }

    _Use_decl_annotations_
    void MainWindow::OnAddControlChange(foundation::IInspectable const&, xaml::RoutedEventArgs const&)
    {
        AppendText(std::format(L"B{:X} {:02X} {:02X}",
            SelectedIndexOrZero(ChannelComboBox()),
            SelectedIndexOrZero(ControllerComboBox()),
            SelectedIndexOrZero(ControllerValueComboBox())));
    }

    _Use_decl_annotations_
    void MainWindow::OnAddProgramChange(foundation::IInspectable const&, xaml::RoutedEventArgs const&)
    {
        AppendText(std::format(L"C{:X} 00", SelectedIndexOrZero(ChannelComboBox())));
    }

    _Use_decl_annotations_
    void MainWindow::OnAddPitchBend(foundation::IInspectable const&, xaml::RoutedEventArgs const&)
    {
        // 0x2000 is centre: LSB 0x00, MSB 0x40
        AppendText(std::format(L"E{:X} 00 40", SelectedIndexOrZero(ChannelComboBox())));
    }

    _Use_decl_annotations_
    void MainWindow::OnAddChannelPressure(foundation::IInspectable const&, xaml::RoutedEventArgs const&)
    {
        AppendText(std::format(L"D{:X} 40", SelectedIndexOrZero(ChannelComboBox())));
    }

    _Use_decl_annotations_
    void MainWindow::OnAddAllNotesOff(foundation::IInspectable const&, xaml::RoutedEventArgs const&)
    {
        AppendText(std::format(L"B{:X} 7B 00", SelectedIndexOrZero(ChannelComboBox())));
    }

    _Use_decl_annotations_
    void MainWindow::OnAddIdentityRequest(foundation::IInspectable const&, xaml::RoutedEventArgs const&)
    {
        AppendText(L"F0 7E 7F 06 01 F7");
    }

    _Use_decl_annotations_
    void MainWindow::OnAddEmptySysEx(foundation::IInspectable const&, xaml::RoutedEventArgs const&)
    {
        AppendText(L"F0 00 00 00 00 00 00 F7");
    }

    // ------------------------------------------------------------------------------------
    // Sending
    // ------------------------------------------------------------------------------------

    _Use_decl_annotations_
    void MainWindow::OnClearClick(foundation::IInspectable const&, xaml::RoutedEventArgs const&)
    {
        try
        {
            EditorTextBox().Text(L"");
            SendResultText().Text(L"");
            EditorTextBox().Focus(xaml::FocusState::Programmatic);
        }
        MIDI_SCRATCHPAD_CATCH_AND_LOG(L"Unable to clear the scratch pad.")
    }

    _Use_decl_annotations_
    void MainWindow::OnSendAllClick(foundation::IInspectable const&, xaml::RoutedEventArgs const&)
    {
        SendText(EditorTextBox().Text(), false);
    }

    _Use_decl_annotations_
    void MainWindow::OnSendSelectionClick(foundation::IInspectable const&, xaml::RoutedEventArgs const&)
    {
        SendText(EditorTextBox().SelectedText(), true);
    }

    void MainWindow::SendText(winrt::hstring const& text, bool isSelection) noexcept
    {
        try
        {
            auto const endpointId = SelectedEndpointDeviceId();

            if (endpointId.empty())
            {
                ShowSendResult(res::GetString(L"SendErrorNoEndpoint"), true);
                return;
            }

            auto const isMidi1 = CurrentMode() == native::ScratchPadMode::Midi1Bytes;

            auto const parsed = isMidi1
                ? native::ParseMidi1Bytes(std::wstring{ text })
                : native::ParseUmpWords(std::wstring{ text });

            if (parsed.HasError || parsed.IsEmpty())
            {
                ShowSendResult(res::GetString(
                    isSelection ? L"SendErrorSelectionInvalid" : L"SendErrorNothingToSend"), true);
                return;
            }

            std::vector<uint32_t> words{};

            if (isMidi1)
            {
                auto const groupNumber = SelectedGroupNumber();

                if (groupNumber == 0)
                {
                    ShowSendResult(res::GetString(L"SendErrorNoGroup"), true);
                    return;
                }

                midi2::MidiGroup const group{ static_cast<uint8_t>(groupNumber - 1) };

                // the SDK owns running status and SysEx7 packetisation, so the wire format here
                // matches everything else in Windows MIDI Services
                auto const converted = midi2msg::MidiMessageConverter::ConvertMidi1CompleteMessageBytesToUmpWords(
                    group, parsed.Bytes, RunningStatusToggle().IsOn());

                if (converted == nullptr || converted.Size() == 0)
                {
                    ShowSendResult(res::GetString(L"SendErrorConversionFailed"), true);
                    return;
                }

                for (auto const word : converted)
                {
                    words.push_back(word);
                }
            }
            else
            {
                words = parsed.Words;
            }

            if (m_session == nullptr)
            {
                m_session = midi2::MidiSession::Create(res::GetString(L"AppDisplayName"));
            }

            if (m_session == nullptr)
            {
                ShowSendResult(res::GetString(L"SendErrorNoSession"), true);
                return;
            }

            auto connection = m_session.CreateEndpointConnection(endpointId);

            if (connection == nullptr || !connection.Open())
            {
                ShowSendResult(res::GetString(L"SendErrorOpenFailed"), true);
                return;
            }

            auto const result = connection.SendMultipleMessagesWordList(
                midi2::MidiClock::TimestampConstantSendImmediately(), words);

            m_session.DisconnectEndpointConnection(connection.ConnectionId());

            if (!midi2::MidiEndpointConnection::SendMessageSucceeded(result))
            {
                ShowSendResult(res::GetString(L"SendErrorSendFailed"), true);
                return;
            }

            ShowSendResult(res::FormatString(L"SendSucceededFormat", static_cast<int32_t>(words.size())), false);
        }
        MIDI_SCRATCHPAD_CATCH_AND_LOG(L"Unable to send the messages.")
    }

    void MainWindow::ShowSendResult(winrt::hstring const& message, bool isError) noexcept
    {
        try
        {
            SendResultText().Text(message);

            SendResultText().Foreground(xaml::Application::Current().Resources()
                .Lookup(winrt::box_value(isError ? L"SystemFillColorCriticalBrush" : L"TextFillColorSecondaryBrush"))
                .as<media::Brush>());
        }
        MIDI_SCRATCHPAD_CATCH_AND_LOG(L"Unable to show the send result.")
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
    void MainWindow::OnRunningStatusToggled(foundation::IInspectable const&, xaml::RoutedEventArgs const&)
    {
        if (!m_initialized)
        {
            return;
        }

        native::AppSettings::Current().AllowRunningStatus(IsToggleOn(RunningStatusToggle()));
    }

    _Use_decl_annotations_
    void MainWindow::OnAlwaysOnTopToggled(foundation::IInspectable const&, xaml::RoutedEventArgs const&)
    {
        try
        {
            auto const checked = AlwaysOnTopToggle().IsChecked();
            auto const isOn = checked != nullptr && checked.Value();

            native::AppSettings::Current().AlwaysOnTop(isOn);

            m_chrome.ApplyAlwaysOnTop();
        }
        MIDI_SCRATCHPAD_CATCH_AND_LOG(L"Unable to change the always on top setting.")
    }

    _Use_decl_annotations_
    void MainWindow::OnSettingsClick(foundation::IInspectable const&, xaml::RoutedEventArgs const&)
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
                SettingsButton(),
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
        MIDI_SCRATCHPAD_CATCH_AND_LOG(L"Unable to open the settings.")
    }

    void MainWindow::ApplyStartupOptions() noexcept
    {
    }

    _Use_decl_annotations_
    void MainWindow::ShowMessageAsync(winrt::hstring title, winrt::hstring message)
    {
        try
        {
            controls::ContentDialog dialog{};

            dialog.XamlRoot(RootGrid().XamlRoot());
            dialog.Title(winrt::box_value(title));
            dialog.CloseButtonText(res::GetString(L"CommonClose"));

            controls::TextBlock body{};
            body.Text(message);
            body.TextWrapping(xaml::TextWrapping::Wrap);

            dialog.Content(body);
            dialog.ShowAsync();
        }
        MIDI_SCRATCHPAD_CATCH_AND_LOG(L"Unable to show the message.")
    }
}
