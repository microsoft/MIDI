// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#include "pch.h"
#include "MainWindow.xaml.h"
#include "MainWindow.g.cpp"
#include "SysExRow.h"

#include "App.xaml.h"
#include "StringResources.h"
#include "resource.h"

namespace native = ::midisysextool;
namespace res = ::midisysextool::resources;

namespace winrt::midisysextool::implementation
{
    namespace
    {
        media::Brush OutOfSequenceBrush() noexcept
        {
            try
            {
                return xaml::Application::Current().Resources()
                    .Lookup(winrt::box_value(L"SystemFillColorCriticalBrush")).as<media::Brush>();
            }
            catch (...)
            {
                LOG_CAUGHT_EXCEPTION();
                return nullptr;
            }
        }

        // The color of a token follows from the token itself, so a row only has to carry its
        // text rather than a parallel list of kinds for every byte in the dump.
        void FillByteInlines(
            controls::TextBlock const& block,
            midisysextool::SysExRow const& row,
            media::Brush const& framingBrush,
            media::Brush const& dataBrush) noexcept
        {
            try
            {
                block.Inlines().Clear();

                if (row == nullptr)
                {
                    return;
                }

                auto const data = row.IsOutOfSequence()
                    ? OutOfSequenceBrush()
                    : dataBrush;

                auto const append = [&block](winrt::hstring const& text, media::Brush const& brush)
                    {
                        if (text.empty())
                        {
                            return;
                        }

                        xaml::Documents::Run run{};
                        run.Text(text);

                        if (brush != nullptr)
                        {
                            run.Foreground(brush);
                        }

                        block.Inlines().Append(run);
                    };

                // the prefix is either F0 or the three spaces that line data up beneath it
                append(row.BytePrefix(), framingBrush);
                append(row.ByteData(), data);
                append(row.ByteSuffix(), framingBrush);
            }
            catch (...)
            {
                LOG_CAUGHT_EXCEPTION();
            }
        }

        bool IsChecked(controls::Primitives::ToggleButton const& button) noexcept
        {
            if (button == nullptr)
            {
                return false;
            }

            auto const checked = button.IsChecked();
            return checked != nullptr && checked.Value();
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
        midiapp::WindowChrome::RestorePlacement(*this, native::AppSettings::Current(), 1000, 760);
    }

    _Use_decl_annotations_
    void MainWindow::OnRootLoaded(foundation::IInspectable const&, xaml::RoutedEventArgs const&)
    {
        try
        {
            InitializeWindowChrome();
            InitializeCollections();
            InitializeControlsFromSettings();

            UpdateRowBrushes();

            native::AppSettings::Current().EnsureLibraryFolderExists();

            Closed([weak = get_weak()](auto&&, auto&&)
                {
                    if (auto strong = weak.get())
                    {
                        strong->m_chrome.SavePlacement();
                        strong->CancelSend();
                        strong->StopReceiving();
                        strong->CloseConnection();
                        strong->StopEndpointWatcher();
                        strong->m_chrome.Shutdown();
                    }
                });

            StartEndpointWatcher();

            m_initialized = true;

            UpdateCommandStates();
            UpdateStatus();

            // programmatic rather than keyboard focus, so the device picker is where typing and
            // tabbing begin without drawing a focus rectangle on launch
            EndpointComboBox().Focus(xaml::FocusState::Programmatic);
        }
        MIDI_SYSEXTOOL_CATCH_AND_LOG(L"Unable to finish loading the window.")
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

            // 32px source for a 16px slot, so it stays crisp on a high DPI display
            AppTitleBarIcon().Source(midiapp::WindowChrome::LoadIconImageSource(IDI_APPICON, 32));
        }
        MIDI_SYSEXTOOL_CATCH_AND_LOG(L"Unable to set up the window chrome.")
    }

    void MainWindow::InitializeCollections() noexcept
    {
        try
        {
            m_endpoints = winrt::single_threaded_observable_vector<appshared::EndpointChoice>();
            m_groups = winrt::single_threaded_observable_vector<appshared::NamedChoice>();
            m_rows = winrt::single_threaded_observable_vector<midisysextool::SysExRow>();

            EndpointComboBox().ItemsSource(m_endpoints);
            GroupComboBox().ItemsSource(m_groups);
            DumpListView().ItemsSource(m_rows);

            m_displayTimer = m_dispatcherQueue.CreateTimer();
            m_displayTimer.Interval(std::chrono::milliseconds{ DisplayRefreshMilliseconds });
            m_displayTimer.IsRepeating(false);

            m_displayTimer.Tick([weak = get_weak()](auto&&, auto&&)
                {
                    if (auto strong = weak.get())
                    {
                        strong->RefreshDisplay();
                    }
                });
        }
        MIDI_SYSEXTOOL_CATCH_AND_LOG(L"Unable to build the collections.")
    }

    void MainWindow::InitializeControlsFromSettings() noexcept
    {
        try
        {
            auto const& settings = native::AppSettings::Current();
            auto const& options = App::StartupOptions();

            m_suppressModeHandling = true;

            SendModeButton().IsChecked(true);

            m_suppressModeHandling = false;

            MessageCountBox().Value(static_cast<double>(settings.SingleTransferMessageCount()));
            SpacingBox().Value(static_cast<double>(settings.TransferSpacingMilliseconds()));

            AlwaysOnTopToggle().IsChecked(settings.AlwaysOnTop());

            if (!options.HasError && !options.ShowHelp && !options.FilePath.empty())
            {
                m_sendFilePath = options.FilePath;
                SendFilePathBox().Text(winrt::hstring{ m_sendFilePath });
            }

            ApplyTaskToLayout();
        }
        MIDI_SYSEXTOOL_CATCH_AND_LOG(L"Unable to apply the saved settings.")
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
        MIDI_SYSEXTOOL_CATCH_AND_LOG(L"Unable to start the endpoint device watcher.")
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
        MIDI_SYSEXTOOL_CATCH_AND_LOG(L"Unable to stop the endpoint device watcher.")
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
        MIDI_SYSEXTOOL_CATCH_AND_LOG(L"Unable to refresh the endpoint list.")
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
        MIDI_SYSEXTOOL_CATCH_AND_LOG(L"Unable to refresh the group list.")
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
        MIDI_SYSEXTOOL_CATCH_AND_LOG(L"Unable to read the selected endpoint.")

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
        MIDI_SYSEXTOOL_CATCH_AND_LOG(L"Unable to read the selected group.")

        return 0;
    }

    _Use_decl_annotations_
    void MainWindow::OnEndpointSelectionChanged(foundation::IInspectable const&, controls::SelectionChangedEventArgs const&)
    {
        if (m_suppressSelectionHandling)
        {
            return;
        }

        // the open connection belongs to the previous device
        StopReceiving();
        CloseConnection();

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

        // the receiver is built for one group, so a change has to restart it
        StopReceiving();

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
        MIDI_SYSEXTOOL_CATCH_AND_LOG(L"Unable to adjust the endpoint item template.")
    }

    // ------------------------------------------------------------------------------------
    // Task switching
    // ------------------------------------------------------------------------------------

    bool MainWindow::IsReceiveTask() noexcept
    {
        return IsChecked(ReceiveModeButton());
    }

    void MainWindow::ApplyTaskToLayout() noexcept
    {
        try
        {
            auto const receiving = IsReceiveTask();

            SendPanel().Visibility(receiving ? xaml::Visibility::Collapsed : xaml::Visibility::Visible);
            ReceivePanel().Visibility(receiving ? xaml::Visibility::Visible : xaml::Visibility::Collapsed);

            SaveButton().Visibility(receiving ? xaml::Visibility::Visible : xaml::Visibility::Collapsed);
            ClearButton().Visibility(receiving ? xaml::Visibility::Visible : xaml::Visibility::Collapsed);

            UpdateCommandStates();
            UpdateStatus();
        }
        MIDI_SYSEXTOOL_CATCH_AND_LOG(L"Unable to switch the task.")
    }

    _Use_decl_annotations_
    void MainWindow::OnTaskModeChanged(foundation::IInspectable const&, xaml::RoutedEventArgs const&)
    {
        if (m_suppressModeHandling || !m_initialized)
        {
            return;
        }

        // leaving the receive page tears down the receiver rather than leaving it running unseen
        if (!IsReceiveTask())
        {
            StopReceiving();
        }

        ApplyTaskToLayout();
    }

    _Use_decl_annotations_
    void MainWindow::OnTransferSettingChanged(controls::NumberBox const&, controls::NumberBoxValueChangedEventArgs const&)
    {
        if (!m_initialized)
        {
            return;
        }

        try
        {
            auto const messageCount = MessageCountBox().Value();
            auto const spacing = SpacingBox().Value();

            if (!std::isnan(messageCount))
            {
                native::AppSettings::Current().SingleTransferMessageCount(static_cast<uint32_t>(messageCount));
            }

            if (!std::isnan(spacing))
            {
                native::AppSettings::Current().TransferSpacingMilliseconds(static_cast<uint32_t>(spacing));
            }
        }
        MIDI_SYSEXTOOL_CATCH_AND_LOG(L"Unable to save the transfer settings.")
    }

    // ------------------------------------------------------------------------------------
    // Connection
    // ------------------------------------------------------------------------------------

    bool MainWindow::EnsureConnection() noexcept
    {
        try
        {
            auto const endpointId = SelectedEndpointDeviceId();

            if (endpointId.empty())
            {
                return false;
            }

            if (m_connection != nullptr && m_connectedEndpointId == endpointId)
            {
                return true;
            }

            CloseConnection();

            if (m_session == nullptr)
            {
                m_session = midi2::MidiSession::Create(res::GetString(L"AppDisplayName"));
            }

            if (m_session == nullptr)
            {
                return false;
            }

            auto connection = m_session.CreateEndpointConnection(endpointId);

            if (connection == nullptr || !connection.Open())
            {
                return false;
            }

            m_connection = connection;
            m_connectedEndpointId = endpointId;

            return true;
        }
        MIDI_SYSEXTOOL_CATCH_AND_LOG(L"Unable to open the endpoint connection.")

        return false;
    }

    void MainWindow::CloseConnection() noexcept
    {
        try
        {
            if (m_connection == nullptr)
            {
                return;
            }

            if (m_session != nullptr)
            {
                m_session.DisconnectEndpointConnection(m_connection.ConnectionId());
            }

            m_connection = nullptr;
            m_connectedEndpointId = {};
        }
        MIDI_SYSEXTOOL_CATCH_AND_LOG(L"Unable to close the endpoint connection.")
    }

    // ------------------------------------------------------------------------------------
    // Sending
    // ------------------------------------------------------------------------------------

    _Use_decl_annotations_
    void MainWindow::OnPrimaryClick(foundation::IInspectable const&, xaml::RoutedEventArgs const&)
    {
        if (IsReceiveTask())
        {
            if (m_receiver != nullptr)
            {
                StopReceiving();
            }
            else
            {
                StartReceiving();
            }

            return;
        }

        if (m_isSending)
        {
            CancelSend();
            return;
        }

        SendFileAsync();
    }

    winrt::fire_and_forget MainWindow::SendFileAsync()
    {
        auto lifetime = get_strong();

        try
        {
            if (m_sendFilePath.empty())
            {
                ShowSendProgress(0, 0);
                StatusText().Text(res::GetString(L"SendErrorNoFile"));
                co_return;
            }

            auto const groupNumber = SelectedGroupNumber();

            if (!EnsureConnection() || groupNumber == 0)
            {
                StatusText().Text(res::GetString(L"SendErrorNoDestination"));
                co_return;
            }

            midi2::MidiGroup const group{ static_cast<uint8_t>(groupNumber - 1) };

            auto const messageCount = native::AppSettings::Current().SingleTransferMessageCount();
            auto const spacing = static_cast<uint16_t>(native::AppSettings::Current().TransferSpacingMilliseconds());

            auto const path = m_sendFilePath;

            m_isSending = true;
            m_sendTotalBytes = 0;

            UpdateCommandStates();

            SendProgressBar().Value(0);
            StatusText().Text(res::GetString(L"SendStarting"));

            auto const file = co_await winrt::Windows::Storage::StorageFile::GetFileFromPathAsync(winrt::hstring{ path });

            if (file != nullptr)
            {
                auto const properties = co_await file.GetBasicPropertiesAsync();
                m_sendTotalBytes = properties.Size();
            }

            auto const stream = co_await file.OpenSequentialReadAsync();

            midi2msg::MidiBytestreamToUmpMessageConverterState converterState{};

            m_sendOperation = midi2sysex::MidiSystemExclusiveSender::SendBinarySysEx7ByteDataAsync(
                m_connection, group, stream, messageCount, spacing, converterState);

            m_sendOperation.Progress([weak = get_weak(), queue = m_dispatcherQueue](
                auto&&, midi2sysex::MidiSystemExclusiveSendProgress const& progress)
                {
                    if (queue == nullptr || progress == nullptr)
                    {
                        return;
                    }

                    auto const bytesRead = progress.CountBytesRead();
                    auto const messagesSent = progress.CountMessagesSent();

                    queue.TryEnqueue([weak, bytesRead, messagesSent]()
                        {
                            if (auto strong = weak.get())
                            {
                                strong->ShowSendProgress(bytesRead, messagesSent);
                            }
                        });
                });

            auto const succeeded = co_await m_sendOperation;

            m_sendOperation = nullptr;
            m_isSending = false;

            SendProgressBar().Value(succeeded ? 100 : 0);

            StatusText().Text(succeeded
                ? res::FormatString(L"SendSucceededFormat", static_cast<int64_t>(m_sendTotalBytes))
                : res::GetString(L"SendFailed"));

            UpdateCommandStates();
        }
        catch (winrt::hresult_canceled const&)
        {
            m_sendOperation = nullptr;
            m_isSending = false;

            StatusText().Text(res::GetString(L"SendCanceled"));
            UpdateCommandStates();
        }
        catch (...)
        {
            LOG_CAUGHT_EXCEPTION();

            m_sendOperation = nullptr;
            m_isSending = false;

            StatusText().Text(res::GetString(L"SendFailed"));
            UpdateCommandStates();
        }
    }

    void MainWindow::CancelSend() noexcept
    {
        try
        {
            if (m_sendOperation != nullptr)
            {
                m_sendOperation.Cancel();
            }
        }
        MIDI_SYSEXTOOL_CATCH_AND_LOG(L"Unable to cancel the transfer.")
    }

    _Use_decl_annotations_
    void MainWindow::ShowSendProgress(uint64_t bytesRead, uint64_t messagesSent) noexcept
    {
        try
        {
            if (m_sendTotalBytes > 0)
            {
                auto const percent = std::clamp(
                    (static_cast<double>(bytesRead) / static_cast<double>(m_sendTotalBytes)) * 100.0, 0.0, 100.0);

                SendProgressBar().IsIndeterminate(false);
                SendProgressBar().Value(percent);
            }
            else
            {
                SendProgressBar().IsIndeterminate(true);
            }

            // bytes read from the file, not bytes the device has acknowledged
            SendProgressText().Text(res::FormatString(L"SendProgressFormat",
                static_cast<int64_t>(bytesRead),
                static_cast<int64_t>(m_sendTotalBytes),
                static_cast<int64_t>(messagesSent)));
        }
        MIDI_SYSEXTOOL_CATCH_AND_LOG(L"Unable to show the transfer progress.")
    }

    _Use_decl_annotations_
    void MainWindow::OnBrowseSendFileClick(foundation::IInspectable const&, xaml::RoutedEventArgs const&)
    {
        try
        {
            auto const picker = winrt::create_instance<IFileOpenDialog>(CLSID_FileOpenDialog);

            COMDLG_FILTERSPEC const filters[]
            {
                { L"MIDI System Exclusive (*.syx)", L"*.syx" },
                { L"All files (*.*)", L"*.*" }
            };

            picker->SetFileTypes(ARRAYSIZE(filters), filters);
            picker->SetDefaultExtension(L"syx");

            auto const folder = native::AppSettings::Current().LibraryFolder();

            winrt::com_ptr<IShellItem> folderItem{};

            if (SUCCEEDED(::SHCreateItemFromParsingName(folder.c_str(), nullptr,
                IID_PPV_ARGS(folderItem.put()))))
            {
                picker->SetFolder(folderItem.get());
            }

            HWND hwnd{ nullptr };
            winrt::check_hresult(this->try_as<IWindowNative>()->get_WindowHandle(&hwnd));

            if (picker->Show(hwnd) != S_OK)
            {
                return;
            }

            winrt::com_ptr<IShellItem> item{};

            if (FAILED(picker->GetResult(item.put())) || item == nullptr)
            {
                return;
            }

            wil::unique_cotaskmem_string chosen{};

            if (FAILED(item->GetDisplayName(SIGDN_FILESYSPATH, chosen.put())))
            {
                return;
            }

            m_sendFilePath = chosen.get();
            SendFilePathBox().Text(winrt::hstring{ m_sendFilePath });

            SendProgressBar().Value(0);
            SendProgressText().Text(L"");

            UpdateCommandStates();
            UpdateStatus();
        }
        MIDI_SYSEXTOOL_CATCH_AND_LOG(L"Unable to choose the file.")
    }

    // ------------------------------------------------------------------------------------
    // Receiving
    // ------------------------------------------------------------------------------------

    void MainWindow::StartReceiving() noexcept
    {
        try
        {
            auto const groupNumber = SelectedGroupNumber();

            if (!EnsureConnection() || groupNumber == 0)
            {
                StatusText().Text(res::GetString(L"ReceiveErrorNoSource"));
                return;
            }

            midi2::MidiGroup const group{ static_cast<uint8_t>(groupNumber - 1) };

            ResetDisplay();

            m_receiver = midi2sysex::MidiSystemExclusiveReceiver{ m_connection, group, 256 };

            m_receiverToken = m_receiver.BytesReceived(
                [weak = get_weak(), queue = m_dispatcherQueue](auto&&, midi2sysex::MidiSystemExclusiveReceivedEventArgs const& args)
                {
                    if (queue == nullptr || args == nullptr)
                    {
                        return;
                    }

                    // copied off the callback thread: the display runs on the UI thread
                    auto const view = args.Bytes();
                    auto bytes = std::make_shared<std::vector<uint8_t>>(view.Size());

                    if (view.Size() > 0)
                    {
                        view.GetMany(0, winrt::array_view<uint8_t>{ bytes->data(), bytes->data() + bytes->size() });
                    }

                    queue.TryEnqueue([weak, bytes]()
                        {
                            if (auto strong = weak.get())
                            {
                                strong->OnBytesReceived(*bytes);
                            }
                        });
                });

            // the receiver reports bytes only, so the UMP view needs the words from the
            // connection itself
            m_messageReceivedToken = m_connection.MessageReceived(
                [weak = get_weak(), queue = m_dispatcherQueue, groupIndex = group.Index()](
                    auto&&, midi2::MidiMessageReceivedEventArgs const& args)
                {
                    if (queue == nullptr || args == nullptr)
                    {
                        return;
                    }

                    uint32_t word0{ 0 };
                    uint32_t word1{ 0 };
                    uint32_t word2{ 0 };
                    uint32_t word3{ 0 };

                    if (args.FillWords(word0, word1, word2, word3) < 2)
                    {
                        return;
                    }

                    if (((word0 >> 28) & 0x0F) != static_cast<uint32_t>(midi2::MidiMessageType::DataMessage64))
                    {
                        return;
                    }

                    if (((word0 >> 24) & 0x0F) != groupIndex)
                    {
                        return;
                    }

                    queue.TryEnqueue([weak, word0, word1]()
                        {
                            if (auto strong = weak.get())
                            {
                                strong->OnSysEx7WordsReceived(word0, word1);
                            }
                        });
                });

            if (!m_receiver.Start())
            {
                StopReceiving();
                StatusText().Text(res::GetString(L"ReceiveErrorStartFailed"));
                return;
            }

            UpdateCommandStates();
            UpdateStatus();
        }
        MIDI_SYSEXTOOL_CATCH_AND_LOG(L"Unable to start receiving.")
    }

    void MainWindow::StopReceiving() noexcept
    {
        try
        {
            if (m_receiver == nullptr)
            {
                return;
            }

            if (m_connection != nullptr && m_messageReceivedToken.value != 0)
            {
                m_connection.MessageReceived(m_messageReceivedToken);
                m_messageReceivedToken = {};
            }

            // Stop flushes what is still buffered, so the last event may arrive during this call
            m_receiver.Stop();
            m_receiver.BytesReceived(m_receiverToken);
            m_receiverToken = {};

            m_receiver.Close();
            m_receiver = nullptr;

            if (m_displayTimer != nullptr)
            {
                m_displayTimer.Stop();
            }

            RefreshDisplay();

            UpdateCommandStates();
            UpdateStatus();
        }
        MIDI_SYSEXTOOL_CATCH_AND_LOG(L"Unable to stop receiving.")
    }

    _Use_decl_annotations_
    void MainWindow::OnBytesReceived(std::vector<uint8_t> const& bytes) noexcept
    {
        try
        {
            m_buffer.AppendBytes(bytes.data(), bytes.size());
            ScheduleDisplayRefresh();
        }
        MIDI_SYSEXTOOL_CATCH_AND_LOG(L"Unable to record the received bytes.")
    }

    _Use_decl_annotations_
    void MainWindow::OnSysEx7WordsReceived(uint32_t word0, uint32_t word1) noexcept
    {
        try
        {
            m_buffer.AppendDisplayWords(word0, word1);
            ScheduleDisplayRefresh();
        }
        MIDI_SYSEXTOOL_CATCH_AND_LOG(L"Unable to record the received words.")
    }

    // ------------------------------------------------------------------------------------
    // Display
    // ------------------------------------------------------------------------------------

    void MainWindow::ResetDisplay() noexcept
    {
        try
        {
            m_buffer.Reset(native::AppSettings::Current().InitialReceiveBufferKb() * 1024);

            m_rows.Clear();
            m_renderedByteCount = 0;
            m_renderedWordCount = 0;
            m_displayTruncated = false;
            m_rowInsideMessage = false;

            SendProgressText().Text(L"");
        }
        MIDI_SYSEXTOOL_CATCH_AND_LOG(L"Unable to reset the display.")
    }

    void MainWindow::UpdateRowBrushes() noexcept
    {
        try
        {
            auto const dark = RootGrid().ActualTheme() != xaml::ElementTheme::Light;

            winrt::Windows::UI::Color green{};
            green.A = 0xFF;

            // bright on dark, deepened on light so it stays legible against white
            green.R = dark ? 0x4C : 0x0B;
            green.G = dark ? 0xE6 : 0x7A;
            green.B = dark ? 0x4C : 0x0B;

            m_framingBrush = media::SolidColorBrush{ green };

            winrt::Windows::UI::Color plain{};
            plain.A = 0xFF;
            plain.R = plain.G = plain.B = dark ? 0xE4 : 0x1B;

            m_dataBrush = media::SolidColorBrush{ plain };

            // realized rows keep the brush they were built with, so make them build again
            if (m_rows != nullptr && DumpListView().ItemsSource() != nullptr)
            {
                DumpListView().ItemsSource(nullptr);
                DumpListView().ItemsSource(m_rows);
            }
        }
        MIDI_SYSEXTOOL_CATCH_AND_LOG(L"Unable to update the row brushes.")
    }

    void MainWindow::ScheduleDisplayRefresh() noexcept
    {
        try
        {
            if (m_displayTimer != nullptr && !m_displayTimer.IsRunning())
            {
                m_displayTimer.Start();
            }
        }
        MIDI_SYSEXTOOL_CATCH_AND_LOG(L"Unable to schedule the display refresh.")
    }

    void MainWindow::RefreshDisplay() noexcept
    {
        try
        {
            AppendRows();
            UpdateStatus();

            // Save and Clear depend on the buffer, which fills while receiving is still running
            UpdateCommandStates();
        }
        MIDI_SYSEXTOOL_CATCH_AND_LOG(L"Unable to refresh the display.")
    }

    void MainWindow::AppendRows() noexcept
    {
        try
        {
            auto const& words = m_buffer.Words();

            // words arrive as message type 3 pairs, one UMP message per row
            while (m_renderedWordCount + 1 < words.size())
            {
                if (m_rows.Size() >= MaximumDisplayRows)
                {
                    m_displayTruncated = true;
                    m_renderedWordCount = words.size() & ~static_cast<size_t>(1);
                    break;
                }

                auto const word0 = words[m_renderedWordCount];
                auto const word1 = words[m_renderedWordCount + 1];

                auto const status = (word0 >> 20) & 0x0F;
                auto const count = std::min<uint32_t>((word0 >> 16) & 0x0F, 6);

                uint8_t const payload[6]
                {
                    static_cast<uint8_t>((word0 >> 8) & 0x7F),
                    static_cast<uint8_t>(word0 & 0x7F),
                    static_cast<uint8_t>((word1 >> 24) & 0x7F),
                    static_cast<uint8_t>((word1 >> 16) & 0x7F),
                    static_cast<uint8_t>((word1 >> 8) & 0x7F),
                    static_cast<uint8_t>(word1 & 0x7F)
                };

                auto const opens = (status == 0 || status == 1);
                auto const closes = (status == 0 || status == 3);

                auto const outOfSequence = opens ? m_rowInsideMessage : !m_rowInsideMessage;

                if (opens)
                {
                    m_rowInsideMessage = true;
                }

                if (closes)
                {
                    m_rowInsideMessage = false;
                }

                std::wstring data{};
                data.reserve(count * 3);

                for (uint32_t i = 0; i < count; i++)
                {
                    if (i > 0)
                    {
                        data += L' ';
                    }

                    data += std::format(L"{:02X}", payload[i]);
                }

                m_rows.Append(winrt::make<SysExRow>(
                    winrt::hstring{ std::format(L"{}", (m_renderedWordCount / 2) + 1) },
                    winrt::hstring{ std::format(L"{:08X} {:08X}", word0, word1) },
                    static_cast<uint8_t>(native::ClassifySysEx7Word(word0)),
                    // three spaces stand in for a missing "F0 ", so payloads stay in one column
                    winrt::hstring{ opens ? L"F0 " : L"   " },
                    winrt::hstring{ data },
                    winrt::hstring{ closes ? L" F7" : L"" },
                    outOfSequence));

                m_renderedWordCount += 2;
            }
        }
        MIDI_SYSEXTOOL_CATCH_AND_LOG(L"Unable to build the message rows.")
    }

    _Use_decl_annotations_
    void MainWindow::OnDumpContainerContentChanging(
        controls::ListViewBase const&,
        controls::ContainerContentChangingEventArgs const& args)
    {
        try
        {
            if (args.InRecycleQueue())
            {
                return;
            }

            auto const container = args.ItemContainer();

            if (container == nullptr)
            {
                return;
            }

            auto const root = container.ContentTemplateRoot().try_as<controls::Panel>();

            if (root == nullptr)
            {
                // the template is not realized on the first phase, so ask to be called again
                if (args.Phase() < 2)
                {
                    args.RegisterUpdateCallback({ this, &MainWindow::OnDumpContainerContentChanging });
                }

                return;
            }

            if (root.Children().Size() < 3)
            {
                return;
            }

            auto const row = args.Item().try_as<midisysextool::SysExRow>();

            if (auto const wordsBlock = root.Children().GetAt(1).try_as<controls::TextBlock>())
            {
                // always assigned: a recycled container would otherwise keep the previous color
                auto const carriesFraming =
                    static_cast<native::SysExByteKind>(row.WordKind()) != native::SysExByteKind::Data;

                wordsBlock.Foreground(carriesFraming ? m_framingBrush : m_dataBrush);
            }

            FillByteInlines(root.Children().GetAt(2).try_as<controls::TextBlock>(), row, m_framingBrush, m_dataBrush);
        }
        MIDI_SYSEXTOOL_CATCH_AND_LOG(L"Unable to color the dump row.")
    }

    // ------------------------------------------------------------------------------------
    // Saving
    // ------------------------------------------------------------------------------------

    _Use_decl_annotations_
    void MainWindow::OnSaveClick(foundation::IInspectable const&, xaml::RoutedEventArgs const&)
    {
        SaveReceivedAsync();
    }

    winrt::fire_and_forget MainWindow::SaveReceivedAsync()
    {
        auto lifetime = get_strong();

        try
        {
            if (m_buffer.IsEmpty())
            {
                co_return;
            }

            auto const picker = winrt::create_instance<IFileSaveDialog>(CLSID_FileSaveDialog);

            COMDLG_FILTERSPEC const filters[]
            {
                { L"MIDI System Exclusive (*.syx)", L"*.syx" }
            };

            picker->SetFileTypes(ARRAYSIZE(filters), filters);
            picker->SetDefaultExtension(L"syx");

            auto const folder = native::AppSettings::Current().LibraryFolder();

            winrt::com_ptr<IShellItem> folderItem{};

            if (SUCCEEDED(::SHCreateItemFromParsingName(folder.c_str(), nullptr,
                IID_PPV_ARGS(folderItem.put()))))
            {
                picker->SetFolder(folderItem.get());
            }

            HWND hwnd{ nullptr };
            winrt::check_hresult(this->try_as<IWindowNative>()->get_WindowHandle(&hwnd));

            if (picker->Show(hwnd) != S_OK)
            {
                co_return;
            }

            winrt::com_ptr<IShellItem> item{};

            if (FAILED(picker->GetResult(item.put())) || item == nullptr)
            {
                co_return;
            }

            wil::unique_cotaskmem_string chosen{};

            if (FAILED(item->GetDisplayName(SIGDN_FILESYSPATH, chosen.put())))
            {
                co_return;
            }

            auto const saved = m_buffer.WriteToFile(chosen.get());

            StatusText().Text(saved
                ? res::FormatString(L"SaveSucceededFormat", static_cast<int64_t>(m_buffer.Bytes().size()))
                : res::GetString(L"SaveFailed"));
        }
        catch (...)
        {
            LOG_CAUGHT_EXCEPTION();
        }
    }

    _Use_decl_annotations_
    void MainWindow::OnClearClick(foundation::IInspectable const&, xaml::RoutedEventArgs const&)
    {
        ResetDisplay();
        UpdateCommandStates();
        UpdateStatus();
    }

    // ------------------------------------------------------------------------------------
    // Status and commands
    // ------------------------------------------------------------------------------------

    void MainWindow::UpdateCommandStates() noexcept
    {
        try
        {
            auto const hasEndpoint = !SelectedEndpointDeviceId().empty();
            auto const hasGroup = SelectedGroupNumber() > 0;
            auto const ready = hasEndpoint && hasGroup;

            if (IsReceiveTask())
            {
                auto const receiving = m_receiver != nullptr;

                PrimaryButton().IsEnabled(ready);
                PrimaryButtonText().Text(res::GetString(receiving ? L"StopReceivingButton" : L"StartReceivingButton"));
                PrimaryButtonIcon().Glyph(receiving ? L"\uE71A" : L"\uE768");

                SaveButton().IsEnabled(!m_buffer.IsEmpty());
                ClearButton().IsEnabled(!m_buffer.IsEmpty());
            }
            else
            {
                PrimaryButton().IsEnabled(m_isSending || (ready && !m_sendFilePath.empty()));
                PrimaryButtonText().Text(res::GetString(m_isSending ? L"CancelSendButton" : L"SendButton"));
                PrimaryButtonIcon().Glyph(m_isSending ? L"\uE711" : L"\uE724");

                MessageCountBox().IsEnabled(!m_isSending);
                SpacingBox().IsEnabled(!m_isSending);
                BrowseSendFileButton().IsEnabled(!m_isSending);
            }

            // the button changes purpose, so its accessible name has to follow the label
            xaml::Automation::AutomationProperties::SetName(PrimaryButton(), PrimaryButtonText().Text());

            EndpointComboBox().IsEnabled(!m_isSending && m_receiver == nullptr);
            GroupComboBox().IsEnabled(!m_isSending && m_receiver == nullptr);
        }
        MIDI_SYSEXTOOL_CATCH_AND_LOG(L"Unable to update the command states.")
    }

    void MainWindow::UpdateStatus() noexcept
    {
        try
        {
            if (!IsReceiveTask())
            {
                if (!m_isSending && m_sendFilePath.empty())
                {
                    StatusText().Text(res::GetString(L"SendReady"));
                }

                return;
            }

            auto const& stats = m_buffer.Stats();

            std::wstring status{ res::FormatString(L"ReceiveStatusFormat",
                static_cast<int64_t>(stats.TotalBytes),
                static_cast<int32_t>(stats.CompleteMessages)) };

            if (stats.DisallowedBytes > 0)
            {
                status += L"  ";
                status += res::FormatString(L"ReceiveStatusDisallowedFormat",
                    static_cast<int32_t>(stats.DisallowedBytes));
            }
            else if (stats.IsInsideMessage)
            {
                status += L"  ";
                status += res::GetString(L"ReceiveStatusIncomplete");
            }

            if (m_displayTruncated)
            {
                status += L"  ";
                status += res::FormatString(L"ReceiveStatusTruncatedFormat",
                    static_cast<int32_t>(MaximumDisplayRows));
            }

            StatusText().Text(winrt::hstring{ status });

            StatusText().Foreground(xaml::Application::Current().Resources()
                .Lookup(winrt::box_value(stats.DisallowedBytes > 0
                    ? L"SystemFillColorCriticalBrush"
                    : L"TextFillColorSecondaryBrush"))
                .as<media::Brush>());
        }
        MIDI_SYSEXTOOL_CATCH_AND_LOG(L"Unable to update the status.")
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
        MIDI_SYSEXTOOL_CATCH_AND_LOG(L"Unable to update the window title.")
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
            native::AppSettings::Current().AlwaysOnTop(IsChecked(AlwaysOnTopToggle()));

            m_chrome.ApplyAlwaysOnTop();
        }
        MIDI_SYSEXTOOL_CATCH_AND_LOG(L"Unable to change the always on top setting.")
    }

    void MainWindow::SetLibraryFolder(std::wstring const& folder) noexcept
    {
        try
        {
            if (folder.empty())
            {
                return;
            }

            native::AppSettings::Current().LibraryFolder(folder);
        }
        MIDI_SYSEXTOOL_CATCH_AND_LOG(L"Unable to save the library folder.")
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

            // app settings that belong beside the appearance ones
            controls::StackPanel extra{};
            extra.Spacing(8.0);

            controls::TextBlock folderLabel{};
            folderLabel.Text(res::GetString(L"SettingsLibraryFolderLabel"));

            controls::TextBox folderBox{};
            folderBox.Text(winrt::hstring{ native::AppSettings::Current().LibraryFolder() });
            folderBox.IsReadOnly(true);
            xaml::Automation::AutomationProperties::SetName(folderBox, res::GetString(L"SettingsLibraryFolderLabel"));

            controls::Button browseFolder{};
            browseFolder.Content(winrt::box_value(res::GetString(L"SettingsLibraryFolderBrowse")));

            browseFolder.Click([weak = get_weak(), folderBox](auto&&, auto&&)
                {
                    auto strong = weak.get();

                    if (strong == nullptr)
                    {
                        return;
                    }

                    try
                    {
                        auto const picker = winrt::create_instance<IFileOpenDialog>(CLSID_FileOpenDialog);

                        FILEOPENDIALOGOPTIONS options{};
                        picker->GetOptions(&options);
                        picker->SetOptions(options | FOS_PICKFOLDERS);

                        HWND hwnd{ nullptr };
                        winrt::check_hresult(strong->try_as<IWindowNative>()->get_WindowHandle(&hwnd));

                        if (picker->Show(hwnd) != S_OK)
                        {
                            return;
                        }

                        winrt::com_ptr<IShellItem> item{};

                        if (FAILED(picker->GetResult(item.put())) || item == nullptr)
                        {
                            return;
                        }

                        wil::unique_cotaskmem_string chosen{};

                        if (FAILED(item->GetDisplayName(SIGDN_FILESYSPATH, chosen.put())))
                        {
                            return;
                        }

                        strong->SetLibraryFolder(chosen.get());
                        folderBox.Text(winrt::hstring{ chosen.get() });
                    }
                    catch (...)
                    {
                        LOG_CAUGHT_EXCEPTION();
                    }
                });

            controls::NumberBox bufferBox{};
            bufferBox.Header(winrt::box_value(res::GetString(L"SettingsReceiveBufferLabel")));
            bufferBox.Minimum(static_cast<double>(native::AppSettings::MinimumInitialReceiveBufferKb));
            bufferBox.Maximum(static_cast<double>(native::AppSettings::MaximumInitialReceiveBufferKb));
            bufferBox.Value(static_cast<double>(native::AppSettings::Current().InitialReceiveBufferKb()));
            bufferBox.SpinButtonPlacementMode(controls::NumberBoxSpinButtonPlacementMode::Compact);

            bufferBox.ValueChanged([](controls::NumberBox const& sender, auto&&)
                {
                    if (!std::isnan(sender.Value()))
                    {
                        native::AppSettings::Current().InitialReceiveBufferKb(
                            static_cast<uint32_t>(sender.Value()));
                    }
                });

            extra.Children().Append(folderLabel);
            extra.Children().Append(folderBox);
            extra.Children().Append(browseFolder);
            extra.Children().Append(bufferBox);

            midiapp::ShowAppearanceFlyout(
                SettingsButton(),
                native::AppSettings::Current(),
                strings,
                [weak = get_weak()]()
                {
                    if (auto strong = weak.get())
                    {
                        strong->m_chrome.ApplyTheme();
                        strong->UpdateRowBrushes();
                    }
                },
                extra);
        }
        MIDI_SYSEXTOOL_CATCH_AND_LOG(L"Unable to open the settings.")
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
        MIDI_SYSEXTOOL_CATCH_AND_LOG(L"Unable to show the message.")
    }
}
