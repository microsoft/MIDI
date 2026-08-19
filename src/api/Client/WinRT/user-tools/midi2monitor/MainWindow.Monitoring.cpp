// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================
//
// Endpoint enumeration, group discovery, and the connection lifecycle.
//

#include "pch.h"
#include "MainWindow.xaml.h"

#include "App.xaml.h"
#include "EndpointChoice.h"
#include "NamedChoice.h"
#include "StringResources.h"

namespace native = ::midi2monitor;
namespace res = ::midi2monitor::resources;

namespace winrt::midi2monitor::implementation
{
    namespace
    {
        constexpr wchar_t EndpointImageFolder[] = LR"(%allusersprofile%\Microsoft\MIDI\Assets\Endpoints\)";

        winrt::hstring ExpandPath(std::wstring const& path) noexcept
        {
            wchar_t expanded[MAX_PATH + 1]{};

            if (::ExpandEnvironmentStringsW(path.c_str(), expanded, ARRAYSIZE(expanded)) == 0)
            {
                return {};
            }

            return winrt::hstring{ expanded };
        }

        // The stored value is a bare file name inside the shared assets folder. Anything with a
        // path separator, a wildcard, or an environment variable is rejected rather than
        // resolved, so a tampered configuration cannot point us at an arbitrary file.
        winrt::hstring ResolveEndpointImagePath(winrt::hstring const& imageFileName) noexcept
        {
            try
            {
                if (imageFileName.empty() || imageFileName.size() > MAX_PATH)
                {
                    return {};
                }

                for (auto const ch : imageFileName)
                {
                    if (ch == L'\\' || ch == L'/' || ch == L':' || ch == L'%' || ch == L'?' || ch == L'*' || ch == L'"')
                    {
                        return {};
                    }
                }

                if (imageFileName == L"." || imageFileName == L"..")
                {
                    return {};
                }

                auto const fullPath = ExpandPath(std::wstring{ EndpointImageFolder } + std::wstring{ imageFileName });

                if (fullPath.empty() || !::PathFileExistsW(fullPath.c_str()))
                {
                    return {};
                }

                return fullPath;
            }
            MIDI_MONITOR_CATCH_AND_LOG(L"Unable to resolve the endpoint image path.")

            return {};
        }

        winrt::hstring DescribeGroup(midi2enum::MidiEndpointDeviceInformation const& endpoint, uint8_t groupIndex) noexcept
        {
            try
            {
                if (endpoint != nullptr)
                {
                    midi2::MidiGroup const group{ groupIndex };

                    for (auto const& functionBlock : endpoint.GetDeclaredFunctionBlocks())
                    {
                        if (functionBlock.IncludesGroup(group) && !functionBlock.Name().empty())
                        {
                            return functionBlock.Name();
                        }
                    }

                    for (auto const& terminalBlock : endpoint.GetGroupTerminalBlocks())
                    {
                        if (terminalBlock.IncludesGroup(group) && !terminalBlock.Name().empty())
                        {
                            return terminalBlock.Name();
                        }
                    }
                }
            }
            MIDI_MONITOR_CATCH_AND_LOG(L"Unable to describe the group.")

            return {};
        }

        // Union of every group covered by a declared function block or group terminal block.
        // A block covers GroupCount contiguous groups starting at FirstGroup, so FirstGroup 3
        // with GroupCount 2 contributes groups 3 and 4.
        std::array<bool, 16> DeclaredGroups(midi2enum::MidiEndpointDeviceInformation const& endpoint) noexcept
        {
            std::array<bool, 16> declared{};

            try
            {
                if (endpoint != nullptr)
                {
                    auto const cover = [&declared](uint8_t firstGroupIndex, uint8_t groupCount) noexcept
                        {
                            auto const last = static_cast<uint32_t>(firstGroupIndex) + groupCount;

                            for (uint32_t i = firstGroupIndex; i < last && i < declared.size(); i++)
                            {
                                declared[i] = true;
                            }
                        };

                    for (auto const& functionBlock : endpoint.GetDeclaredFunctionBlocks())
                    {
                        if (auto const first = functionBlock.FirstGroup())
                        {
                            cover(first.Index(), functionBlock.GroupCount());
                        }
                    }

                    for (auto const& terminalBlock : endpoint.GetGroupTerminalBlocks())
                    {
                        if (auto const first = terminalBlock.FirstGroup())
                        {
                            cover(first.Index(), terminalBlock.GroupCount());
                        }
                    }
                }
            }
            MIDI_MONITOR_CATCH_AND_LOG(L"Unable to read the declared groups.")

            // a device that declares nothing still has to be watchable
            if (std::none_of(declared.begin(), declared.end(), [](bool value) { return value; }))
            {
                declared.fill(true);
            }

            return declared;
        }
    }

    // ------------------------------------------------------------------------------------
    // Endpoint watcher
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
        MIDI_MONITOR_CATCH_AND_LOG(L"Unable to start the endpoint device watcher.")
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
        MIDI_MONITOR_CATCH_AND_LOG(L"Unable to stop the endpoint device watcher.")
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

            std::vector<midi2enum::MidiEndpointDeviceInformation> devices{};

            for (auto const& entry : m_watcher.EnumeratedEndpointDevices())
            {
                devices.push_back(entry.Value());
            }

            std::sort(devices.begin(), devices.end(),
                [](auto const& left, auto const& right)
                {
                    return ::CompareStringOrdinal(
                        left.Name().c_str(), -1, right.Name().c_str(), -1, TRUE) == CSTR_LESS_THAN;
                });

            m_suppressSelectionHandling = true;

            m_endpoints.Clear();
            m_endpointDevices.clear();

            for (auto const& device : devices)
            {
                winrt::hstring imagePath{};

                if (auto const userInfo = device.GetUserSuppliedInfo())
                {
                    imagePath = ResolveEndpointImagePath(userInfo.ImageFileName());
                }

                m_endpoints.Append(winrt::make<EndpointChoice>(device.Name(), device.EndpointDeviceId(), imagePath));
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
                if (::CompareStringOrdinal(
                    m_endpoints.GetAt(i).EndpointDeviceId().c_str(), -1,
                    desiredEndpointId.c_str(), -1, TRUE) == CSTR_EQUAL)
                {
                    selectedIndex = static_cast<int32_t>(i);
                    break;
                }
            }

            EndpointComboBox().SelectedIndex(selectedIndex);
        }
        MIDI_MONITOR_CATCH_AND_LOG(L"Unable to refresh the endpoint list.")
    }

    winrt::hstring MainWindow::SelectedEndpointDeviceId() noexcept
    {
        try
        {
            auto const selected = EndpointComboBox().SelectedItem();

            if (auto const choice = selected.try_as<midi2monitor::EndpointChoice>())
            {
                return choice.EndpointDeviceId();
            }
        }
        MIDI_MONITOR_CATCH_AND_LOG(L"Unable to read the selected endpoint.")

        return {};
    }

    uint8_t MainWindow::SelectedGroupNumber() noexcept
    {
        try
        {
            if (auto const choice = GroupComboBox().SelectedItem().try_as<midi2monitor::NamedChoice>())
            {
                return static_cast<uint8_t>(std::clamp(choice.Value(), 0, 16));
            }
        }
        MIDI_MONITOR_CATCH_AND_LOG(L"Unable to read the selected group.")

        return 0;
    }

    uint8_t MainWindow::SelectedChannelNumber() noexcept
    {
        try
        {
            if (auto const choice = ChannelComboBox().SelectedItem().try_as<midi2monitor::NamedChoice>())
            {
                return static_cast<uint8_t>(std::clamp(choice.Value(), 0, 16));
            }
        }
        MIDI_MONITOR_CATCH_AND_LOG(L"Unable to read the selected channel.")

        return 0;
    }

    _Use_decl_annotations_
    void MainWindow::OnEndpointSelectionChanged(foundation::IInspectable const&, controls::SelectionChangedEventArgs const&)
    {
        if (m_suppressSelectionHandling)
        {
            return;
        }

        try
        {
            RefreshGroupList();
            UpdateEndpointImage();
            UpdateCommandStates();
        }
        MIDI_MONITOR_CATCH_AND_LOG(L"Unable to handle the endpoint selection change.")
    }

    _Use_decl_annotations_
    void MainWindow::OnGroupSelectionChanged(foundation::IInspectable const&, controls::SelectionChangedEventArgs const&)
    {
        if (m_suppressSelectionHandling)
        {
            return;
        }

        try
        {
            ApplyFilterToPipeline();
            UpdateCommandStates();
            UpdateWindowTitle();
        }
        MIDI_MONITOR_CATCH_AND_LOG(L"Unable to handle the group selection change.")
    }

    _Use_decl_annotations_
    void MainWindow::OnChannelSelectionChanged(foundation::IInspectable const&, controls::SelectionChangedEventArgs const&)
    {
        if (m_suppressSelectionHandling)
        {
            return;
        }

        try
        {
            ApplyFilterToPipeline();
        }
        MIDI_MONITOR_CATCH_AND_LOG(L"Unable to handle the channel selection change.")
    }

    void MainWindow::RefreshGroupList() noexcept
    {
        try
        {
            auto const endpointIndex = EndpointComboBox().SelectedIndex();

            // the watcher rebuilds this list whenever a device changes, so remember what the
            // customer picked and put it back
            auto const previousGroupNumber = SelectedGroupNumber();
            auto const previousChannelNumber = SelectedChannelNumber();

            m_suppressSelectionHandling = true;

            m_groups.Clear();
            m_groups.Append(winrt::make<NamedChoice>(res::GetString(L"GroupChoiceAll"), 0));

            if (endpointIndex >= 0 && static_cast<size_t>(endpointIndex) < m_endpointDevices.size())
            {
                auto const& endpoint = m_endpointDevices[static_cast<size_t>(endpointIndex)];
                auto const declared = DeclaredGroups(endpoint);

                for (uint8_t groupIndex = 0; groupIndex < 16; groupIndex++)
                {
                    if (!declared[groupIndex])
                    {
                        continue;
                    }

                    auto const description = DescribeGroup(endpoint, groupIndex);
                    auto const groupNumber = static_cast<int32_t>(groupIndex) + 1;

                    auto const label = description.empty()
                        ? res::FormatString(L"GroupChoiceFormat", groupNumber)
                        : res::FormatString(L"GroupChoiceNamedFormat", groupNumber, description);

                    m_groups.Append(winrt::make<NamedChoice>(label, groupNumber));
                }
            }

            m_suppressSelectionHandling = false;

            int32_t selectedIndex{ 0 };

            // the command line choice is a starting point, not a lock. Only consume it once a
            // real endpoint is selected, since the watcher's first pass enumerates nothing.
            if (!m_startupFilterApplied && endpointIndex >= 0)
            {
                m_startupFilterApplied = true;

                auto const& options = App::StartupOptions();

                if (options.GroupNumber.has_value())
                {
                    selectedIndex = FindGroupChoiceIndex(options.GroupNumber.value());
                }

                if (options.ChannelNumber.has_value() && m_channels.Size() > options.ChannelNumber.value())
                {
                    ChannelComboBox().SelectedIndex(options.ChannelNumber.value());
                }
            }
            else if (previousGroupNumber > 0)
            {
                selectedIndex = FindGroupChoiceIndex(previousGroupNumber);

                if (previousChannelNumber > 0 && m_channels.Size() > previousChannelNumber)
                {
                    ChannelComboBox().SelectedIndex(previousChannelNumber);
                }
            }

            GroupComboBox().SelectedIndex(selectedIndex);

            ApplyFilterToPipeline();
        }
        MIDI_MONITOR_CATCH_AND_LOG(L"Unable to refresh the group list.")
    }

    _Use_decl_annotations_
    int32_t MainWindow::FindGroupChoiceIndex(int32_t groupNumber) noexcept
    {
        try
        {
            for (uint32_t i = 0; i < m_groups.Size(); i++)
            {
                if (m_groups.GetAt(i).Value() == groupNumber)
                {
                    return static_cast<int32_t>(i);
                }
            }
        }
        MIDI_MONITOR_CATCH_AND_LOG(L"Unable to locate the group choice.")

        // fall back to "All groups"
        return 0;
    }

    void MainWindow::UpdateEndpointImage() noexcept
    {
        try
        {
            auto const choice = EndpointComboBox().SelectedItem().try_as<midi2monitor::EndpointChoice>();

            if (choice == nullptr || choice.ImagePath().empty())
            {
                EndpointImageBorder().Visibility(xaml::Visibility::Collapsed);
                EndpointImage().Source(nullptr);
                return;
            }

            foundation::Uri const uri{ L"file:///" + choice.ImagePath() };

            media::Imaging::BitmapImage bitmap{};
            bitmap.UriSource(uri);

            EndpointImage().Source(bitmap);
            EndpointImageBorder().Visibility(xaml::Visibility::Visible);
        }
        catch (...)
        {
            // a bad image must never stop the customer from monitoring
            EndpointImageBorder().Visibility(xaml::Visibility::Collapsed);
        }
    }

    void MainWindow::ApplyFilterToPipeline() noexcept
    {
        auto const group = SelectedGroupNumber();
        auto const channel = group == 0 ? uint8_t{ 0 } : SelectedChannelNumber();

        m_pipeline.Filter(group, channel);

        ChannelComboBox().IsEnabled(group != 0);
    }

    // ------------------------------------------------------------------------------------
    // Connection lifecycle
    // ------------------------------------------------------------------------------------

    _Use_decl_annotations_
    void MainWindow::OnStartMonitoringClick(foundation::IInspectable const&, xaml::RoutedEventArgs const&)
    {
        StartMonitoring();
    }

    _Use_decl_annotations_
    void MainWindow::OnStopMonitoringClick(foundation::IInspectable const&, xaml::RoutedEventArgs const&)
    {
        StopMonitoring(true);
    }

    void MainWindow::StartMonitoring() noexcept
    {
        StartMonitoringAsync();
    }

    // The SDK's session and connection calls talk to midisrv synchronously. On the STA UI
    // thread that deadlocks, because the nested call back into the apartment can never be
    // serviced while we are blocked. Everything up to and including Open therefore runs on a
    // background (MTA) thread, and only the resulting objects are handed back to the UI.
    winrt::fire_and_forget MainWindow::StartMonitoringAsync()
    {
        auto lifetime = get_strong();

        if (m_monitoring || m_connecting)
        {
            co_return;
        }

        winrt::hstring endpointId{};
        winrt::hstring endpointName{};

        try
        {
            endpointId = SelectedEndpointDeviceId();

            if (auto const choice = EndpointComboBox().SelectedItem().try_as<midi2monitor::EndpointChoice>())
            {
                endpointName = choice.DisplayName();
            }
        }
        MIDI_MONITOR_CATCH_AND_LOG(L"Unable to read the selected endpoint.")

        if (endpointId.empty())
        {
            co_return;
        }

        m_connecting = true;
        UpdateCommandStates();

        auto queue = m_dispatcherQueue;
        auto* const callback = m_pipeline.CallbackInterface();
        auto const sessionName = res::GetString(L"SessionName");

        co_await winrt::resume_background();

        midi2::MidiSession session{ nullptr };
        midi2::MidiEndpointConnection connection{ nullptr };
        winrt::com_ptr<IMidiEndpointConnectionRaw> connectionRaw{ nullptr };
        winrt::hstring failureBodyKey{};

        try
        {
            if (!midi2::MidiApi::EnsureServiceAvailable())
            {
                failureBodyKey = L"ServiceUnavailableBody";
            }
            else if (session = midi2::MidiSession::Create(sessionName); session == nullptr)
            {
                failureBodyKey = L"SessionCreateFailedBody";
            }
            else if (connection = session.CreateEndpointConnection(endpointId); connection == nullptr)
            {
                failureBodyKey = L"ConnectionCreateFailedBody";
            }
            else if (connectionRaw = connection.try_as<IMidiEndpointConnectionRaw>(); connectionRaw == nullptr)
            {
                failureBodyKey = L"ComExtensionsUnavailableBody";
            }
            // the callback is registered before opening so nothing is missed on connect
            else if (FAILED(connectionRaw->SetMessagesReceivedCallback(callback)))
            {
                failureBodyKey = L"ComExtensionsUnavailableBody";
            }
            else if (!connection.Open())
            {
                failureBodyKey = L"ConnectionOpenFailedBody";
            }
        }
        catch (winrt::hresult_error const& ex)
        {
            MIDI_MONITOR_LOG_HRESULT_EXCEPTION(ex, L"hresult error opening the MIDI connection.");
            failureBodyKey = L"ConnectionOpenFailedBody";
        }
        catch (...)
        {
            MIDI_MONITOR_LOG_GENERAL_EXCEPTION(L"Exception opening the MIDI connection.");
            failureBodyKey = L"ConnectionOpenFailedBody";
        }

        if (queue == nullptr)
        {
            co_return;
        }

        queue.TryEnqueue([lifetime, session, connection, connectionRaw, endpointName, failureBodyKey]()
            {
                lifetime->CompleteMonitoringStart(session, connection, connectionRaw, endpointName, failureBodyKey);
            });
    }

    _Use_decl_annotations_
    void MainWindow::CompleteMonitoringStart(
        midi2::MidiSession session,
        midi2::MidiEndpointConnection connection,
        winrt::com_ptr<IMidiEndpointConnectionRaw> connectionRaw,
        winrt::hstring endpointName,
        winrt::hstring failureBodyKey) noexcept
    {
        try
        {
            m_connecting = false;

            if (!failureBodyKey.empty())
            {
                TearDownConnectionAsync(session, connection, connectionRaw);

                auto const titleKey = (failureBodyKey == L"ServiceUnavailableBody")
                    ? L"ServiceUnavailableTitle"
                    : L"ConnectionFailedTitle";

                ShowMessageAsync(res::GetString(titleKey), res::GetString(failureBodyKey));

                UpdateCommandStates();
                return;
            }

            m_session = session;
            m_connection = connection;
            m_connectionRaw = connectionRaw;
            m_monitoredEndpointName = endpointName;
            m_monitoring = true;

            // relative timestamps count from the moment monitoring begins, and restart on every
            // subsequent start
            m_pipeline.CaptureTimebase(midi2::MidiClock::Now());

            // posted before capture is enabled so it lands ahead of any message
            m_pipeline.PostNotice(res::FormatString(
                L"MonitoringStartedNoticeFormat",
                std::wstring{ endpointName },
                std::wstring{ DescribeSelectedGroup() },
                std::wstring{ DescribeSelectedChannel() }));

            m_pipeline.CaptureEnabled(true);

            MIDI_MONITOR_LOG_INFO(L"Monitoring started.");

            UpdateCommandStates();
            UpdateWindowTitle();
        }
        MIDI_MONITOR_CATCH_AND_LOG(L"Unable to finish starting monitoring.")
    }

    _Use_decl_annotations_
    winrt::fire_and_forget MainWindow::TearDownConnectionAsync(
        midi2::MidiSession session,
        midi2::MidiEndpointConnection connection,
        winrt::com_ptr<IMidiEndpointConnectionRaw> connectionRaw)
    {
        // keeps the pipeline, which owns the callback object, alive for the teardown
        auto lifetime = get_strong();

        co_await winrt::resume_background();

        try
        {
            if (connectionRaw != nullptr)
            {
                // must happen before the connection goes away: the SDK holds this pointer
                // without a reference of its own
                LOG_IF_FAILED(connectionRaw->RemoveMessagesReceivedCallback());
                connectionRaw = nullptr;
            }

            if (session != nullptr && connection != nullptr)
            {
                session.DisconnectEndpointConnection(connection.ConnectionId());
            }

            connection = nullptr;

            if (session != nullptr)
            {
                session.Close();
                session = nullptr;
            }
        }
        MIDI_MONITOR_CATCH_AND_LOG(L"Unable to tear down the MIDI connection.")
    }

    _Use_decl_annotations_
    void MainWindow::StopMonitoring(bool addNotice) noexcept
    {
        try
        {
            m_pipeline.CaptureEnabled(false);

            auto session = m_session;
            auto connection = m_connection;
            auto connectionRaw = m_connectionRaw;

            m_session = nullptr;
            m_connection = nullptr;
            m_connectionRaw = nullptr;

            auto const wasMonitoring = m_monitoring;

            m_monitoring = false;
            m_connecting = false;

            if (session != nullptr || connectionRaw != nullptr)
            {
                TearDownConnectionAsync(session, connection, connectionRaw);
            }

            if (addNotice && wasMonitoring)
            {
                m_pipeline.PostNotice(res::GetString(L"MonitoringStoppedNotice"));
            }

            UpdateCommandStates();
            UpdateWindowTitle();
        }
        MIDI_MONITOR_CATCH_AND_LOG(L"Unable to stop monitoring cleanly.")
    }

    void MainWindow::UpdateCommandStates() noexcept
    {
        try
        {
            auto const hasEndpoint = !SelectedEndpointDeviceId().empty();

            StartButton().IsEnabled(hasEndpoint && !m_monitoring && !m_connecting);
            StopButton().IsEnabled(m_monitoring);
            EndpointComboBox().IsEnabled(!m_monitoring && !m_connecting);
        }
        MIDI_MONITOR_CATCH_AND_LOG(L"Unable to update the command states.")
    }

    void MainWindow::UpdateWindowTitle() noexcept
    {
        try
        {
            winrt::hstring title{};

            if (!m_monitoring || m_monitoredEndpointName.empty())
            {
                title = res::GetString(L"AppDisplayName");
            }
            else
            {
                title = res::FormatString(
                    L"WindowTitleMonitoringFormat",
                    std::wstring{ m_monitoredEndpointName },
                    std::wstring{ DescribeSelectedGroup() },
                    std::wstring{ res::GetString(L"AppDisplayName") });
            }

            Title(title);
            AppTitleTextBlock().Text(title);
        }
        MIDI_MONITOR_CATCH_AND_LOG(L"Unable to update the window title.")
    }

    winrt::hstring MainWindow::DescribeSelectedGroup() noexcept
    {
        try
        {
            auto const groupNumber = SelectedGroupNumber();

            if (groupNumber == 0)
            {
                return res::GetString(L"GroupChoiceAll");
            }

            midi2::MidiGroup const group{ static_cast<uint8_t>(groupNumber - 1) };
            return group.ToString();
        }
        MIDI_MONITOR_CATCH_AND_LOG(L"Unable to describe the selected group.")

        return {};
    }

    winrt::hstring MainWindow::DescribeSelectedChannel() noexcept
    {
        try
        {
            auto const channelNumber = SelectedChannelNumber();

            if (channelNumber == 0 || SelectedGroupNumber() == 0)
            {
                return res::GetString(L"ChannelChoiceAll");
            }

            return res::FormatString(L"ChannelChoiceFormat", static_cast<int32_t>(channelNumber));
        }
        MIDI_MONITOR_CATCH_AND_LOG(L"Unable to describe the selected channel.")

        return {};
    }
}
