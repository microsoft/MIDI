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

namespace native = ::midibluetoothsetup;
namespace res = ::midibluetoothsetup::resources;

namespace winrt::midibluetoothsetup::implementation
{
    namespace
    {
        // Long enough to read, short enough that it is gone before it becomes untrue.
        constexpr std::chrono::seconds StatusMessageLifetime{ 8 };

        winrt::hstring Lowered(_In_ winrt::hstring const& value) noexcept
        {
            try
            {
                std::wstring copy{ value };

                std::transform(copy.begin(), copy.end(), copy.begin(),
                    [](wchar_t c) { return static_cast<wchar_t>(::towlower(c)); });

                return winrt::hstring{ copy };
            }
            catch (...)
            {
                return value;
            }
        }

        // The data context of a control inside a DataTemplate is the row it was realized for.
        template <typename TItem>
        TItem ItemFromSender(_In_ foundation::IInspectable const& sender) noexcept
        {
            try
            {
                auto const element = sender.try_as<xaml::FrameworkElement>();

                if (element == nullptr)
                {
                    return nullptr;
                }

                return element.DataContext().try_as<TItem>();
            }
            catch (...)
            {
                return nullptr;
            }
        }

        winrt::hstring ProtocolDisplayName(_In_ midi2bt::MidiBluetoothProtocol const protocol) noexcept
        {
            switch (protocol)
            {
            case midi2bt::MidiBluetoothProtocol::BluetoothLowEnergyMidi1:
                return res::GetString(L"ProtocolMidi1");

            case midi2bt::MidiBluetoothProtocol::BluetoothLowEnergyMidi2Ump:
                return res::GetString(L"ProtocolMidi2");

            default:
                return res::GetString(L"ProtocolUnknown");
            }
        }

        // Always shown, so a duration set from the console or the configuration file which is not
        // one of the combo box presets is still visible.
        winrt::hstring OfflineRetentionDescription(_In_ int32_t const seconds)
        {
            if (seconds < 0)
            {
                return res::GetString(L"OfflineRetentionAlways");
            }

            if (seconds == 0)
            {
                return res::GetString(L"OfflineRetentionImmediate");
            }

            return res::FormatString(L"OfflineRetentionSecondsFormat", winrt::to_hstring(seconds));
        }

        // A connected device stops advertising, so its presence comes from the link. For the
        // rest, how long ago it was last heard from is the only presence signal Bluetooth offers.
        winrt::hstring PresenceDescription(
            _In_ bool const isConnected,
            _In_ bool const isPresent,
            _In_ bool const hasBeenSeen,            _In_ foundation::TimeSpan const lastSeenAgo) noexcept
        {
            try
            {
                if (isConnected)
                {
                    return res::GetString(L"PresenceConnected");
                }

                if (isPresent)
                {
                    return res::GetString(L"PresenceNearby");
                }

                // A paired device the system remembers has never been heard by the radio, so
                // there is no age to report and claiming one would be a fiction.
                if (!hasBeenSeen)
                {
                    return res::GetString(L"PresenceNotHeardYet");
                }

                auto const seconds = std::chrono::duration_cast<std::chrono::seconds>(lastSeenAgo).count();

                if (seconds <= 0)
                {
                    return res::GetString(L"PresenceAway");
                }

                if (seconds < 120)
                {
                    return res::FormatString(L"PresenceAwaySecondsFormat", static_cast<int64_t>(seconds));
                }

                auto const minutes = seconds / 60;

                if (minutes < 120)
                {
                    return res::FormatString(L"PresenceAwayMinutesFormat", static_cast<int64_t>(minutes));
                }

                return res::FormatString(L"PresenceAwayHoursFormat", static_cast<int64_t>(minutes / 60));
            }
            catch (...)
            {
                return {};
            }
        }

        winrt::hstring IntervalDescription(_In_ foundation::TimeSpan const interval) noexcept
        {
            try
            {
                auto const milliseconds =
                    std::chrono::duration_cast<std::chrono::duration<double, std::milli>>(interval).count();

                if (milliseconds <= 0.0)
                {
                    return {};
                }

                return res::FormatString(L"ConnectionIntervalFormat", milliseconds);
            }
            catch (...)
            {
                return {};
            }
        }

        // The bare twelve hex digit form, which is what every command keys on.
        winrt::hstring AddressKey(_In_ uint64_t const address) noexcept
        {
            try
            {
                return address == 0 ? winrt::hstring{} : winrt::hstring{ std::format(L"{:012X}", address) };
            }
            catch (...)
            {
                return {};
            }
        }

        winrt::hstring AddressDescription(
            _In_ uint64_t const address,
            _In_ midi2bt::MidiBluetoothAddressType const addressType) noexcept
        {
            try
            {
                if (address == 0)
                {
                    return {};
                }

                auto const formatted = winrt::hstring{ std::format(L"{:012X}", address) };

                switch (addressType)
                {
                case midi2bt::MidiBluetoothAddressType::Public:
                    return res::FormatString(L"AddressPublicFormat", formatted);

                case midi2bt::MidiBluetoothAddressType::Random:
                    return res::FormatString(L"AddressRandomFormat", formatted);

                default:
                    return formatted;
                }
            }
            catch (...)
            {
                return {};
            }
        }
    }


    MainWindow::MainWindow()
    {
        InitializeComponent();
    }

    void MainWindow::RestoreWindowPlacement()
    {
        // static, because this runs before the chrome is initialized
        midiapp::WindowChrome::RestorePlacement(*this, native::AppSettings::Current(), 1180, 820);
    }

    _Use_decl_annotations_
    void MainWindow::OnRootSizeChanged(foundation::IInspectable const&, xaml::SizeChangedEventArgs const&)
    {
        try
        {
            m_chrome.UpdateTitleBarInsets();
        }
        MIDI_BTSETUP_CATCH_AND_LOG(L"Unable to update the title bar insets.")
    }

    _Use_decl_annotations_
    void MainWindow::OnRootLoaded(foundation::IInspectable const&, xaml::RoutedEventArgs const&)
    {
        try
        {
            Title(res::GetString(L"AppTitle"));
            AppTitleTextBlock().Text(res::GetString(L"AppTitle"));

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

            DevicesListView().ItemsSource(m_devices);
            PeripheralClientsList().ItemsSource(m_peripheralClients);
            PendingClientsList().ItemsSource(m_pendingClients);

            // Publishing is the one place a Bluetooth MIDI protocol is chosen rather than
            // reported, because a peripheral has to advertise as one or the other.
            auto protocols = winrt::single_threaded_vector<foundation::IInspectable>();

            protocols.Append(winrt::box_value(res::GetString(L"ProtocolMidi1")));
            protocols.Append(winrt::box_value(res::GetString(L"ProtocolMidi2")));

            PeripheralProtocolCombo().ItemsSource(protocols);
            PeripheralProtocolCombo().SelectedIndex(0);

            Closed([weak = get_weak()](auto&&, auto&&)
                {
                    if (auto strong = weak.get())
                    {
                        strong->m_closing = true;
                        strong->StopRefreshTimer();
                        strong->m_chrome.SavePlacement();
                        strong->m_chrome.Shutdown();
                    }
                });

            m_loaded = true;

            // Nothing here works without the transport, so the app says so once and stops rather
            // than failing one operation at a time. This has to happen before the saved page is
            // restored, because showing a page is suppressed while the transport is unusable.
            if (!VerifyTransportIsUsable())
            {
                return;
            }

            auto const pageIndex = native::AppSettings::Current().SelectedPageIndex();

            // the panels first, because selecting an item which is already selected raises no
            // selection changed event to do it for us
            ShowPage(pageIndex);

            MainNavigation().SelectedItem(
                pageIndex == native::AppSettings::PageIndexThisPc ?
                    ThisPcNavigationItem().as<foundation::IInspectable>() :
                pageIndex == native::AppSettings::PageIndexSettings ?
                    SettingsNavigationItem().as<foundation::IInspectable>() :
                    DevicesNavigationItem().as<foundation::IInspectable>());

            StartRefreshTimer();

            RequestRefreshAsync();
        }
        MIDI_BTSETUP_CATCH_AND_LOG(L"Unable to finish loading the window.")
    }

    bool MainWindow::VerifyTransportIsUsable() noexcept
    {
        try
        {
            m_transportUsable = midi2bt::MidiBluetoothTransportManager::IsTransportAvailable();
        }
        MIDI_BTSETUP_CATCH_AND_LOG(L"Unable to check whether the Bluetooth transport is available.")

        if (!m_transportUsable)
        {
            try
            {
                TransportUnavailableBar().IsOpen(true);
                DevicesPanel().Visibility(xaml::Visibility::Collapsed);
                ThisPcPanel().Visibility(xaml::Visibility::Collapsed);
                SettingsPanel().Visibility(xaml::Visibility::Collapsed);
            }
            MIDI_BTSETUP_CATCH_AND_LOG(L"Unable to show the transport unavailable message.")
        }

        return m_transportUsable;
    }

    _Use_decl_annotations_
    void MainWindow::OnNavigationSelectionChanged(
        controls::NavigationView const&,
        controls::NavigationViewSelectionChangedEventArgs const& args)
    {
        try
        {
            auto const item = args.SelectedItem().try_as<controls::NavigationViewItem>();

            if (item == nullptr)
            {
                return;
            }

            auto const tag = winrt::unbox_value_or<winrt::hstring>(item.Tag(), winrt::hstring{});

            auto const pageIndex =
                tag == L"thispc" ? native::AppSettings::PageIndexThisPc :
                tag == L"settings" ? native::AppSettings::PageIndexSettings :
                native::AppSettings::PageIndexDevices;

            ShowPage(pageIndex);

            native::AppSettings::Current().SelectedPageIndex(pageIndex);

            // the page the customer just switched to should not show stale numbers
            RequestRefreshAsync();
        }
        MIDI_BTSETUP_CATCH_AND_LOG(L"Unable to switch pages.")
    }

    void MainWindow::ShowPage(uint32_t const pageIndex) noexcept
    {
        try
        {
            if (!m_transportUsable)
            {
                return;
            }

            DevicesPanel().Visibility(
                pageIndex == native::AppSettings::PageIndexDevices ? xaml::Visibility::Visible : xaml::Visibility::Collapsed);

            ThisPcPanel().Visibility(
                pageIndex == native::AppSettings::PageIndexThisPc ? xaml::Visibility::Visible : xaml::Visibility::Collapsed);

            SettingsPanel().Visibility(
                pageIndex == native::AppSettings::PageIndexSettings ? xaml::Visibility::Visible : xaml::Visibility::Collapsed);
        }
        MIDI_BTSETUP_CATCH_AND_LOG(L"Unable to show the requested page.")
    }

    _Use_decl_annotations_
    void MainWindow::StartRefreshTimer() noexcept
    {
        try
        {
            if (m_refreshTimer == nullptr)
            {
                m_refreshTimer = DispatcherQueue().CreateTimer();

                m_refreshTimer.Tick([weak = get_weak()](auto&&, auto&&)
                    {
                        if (auto strong = weak.get())
                        {
                            strong->RequestRefreshAsync();
                        }
                    });
            }

            auto const seconds = native::AppSettings::Current().RefreshIntervalSeconds();

            m_refreshTimer.Interval(std::chrono::seconds{ seconds == 0 ? 1 : seconds });
            m_refreshTimer.Start();
        }
        MIDI_BTSETUP_CATCH_AND_LOG(L"Unable to start the refresh timer.")
    }

    void MainWindow::StopRefreshTimer() noexcept
    {
        try
        {
            if (m_refreshTimer != nullptr)
            {
                m_refreshTimer.Stop();
            }
        }
        MIDI_BTSETUP_CATCH_AND_LOG(L"Unable to stop the refresh timer.")
    }

    // The service calls block, so they never run on the UI thread. A tick is skipped rather than
    // queued when the previous refresh is still going, or a slow service would build a backlog.
    winrt::fire_and_forget MainWindow::RequestRefreshAsync() noexcept
    {
        auto weak = get_weak();

        if (!m_loaded || m_closing || !m_transportUsable)
        {
            co_return;
        }

        if (m_refreshInFlight.exchange(true))
        {
            co_return;
        }

        auto const queue = DispatcherQueue();

        co_await winrt::resume_background();

        auto snapshot = GatherSnapshot();

        if (queue == nullptr)
        {
            co_return;
        }

        // there is no resume_foreground overload for the Microsoft.UI dispatcher in this
        // projection, so the result is marshaled back with TryEnqueue
        queue.TryEnqueue([weak, snapshot = std::move(snapshot)]()
            {
                auto strong = weak.get();

                if (strong == nullptr)
                {
                    return;
                }

                if (!strong->m_closing)
                {
                    strong->ApplySnapshot(snapshot);
                }

                strong->m_refreshInFlight.store(false);
            });
    }

    MainWindow::ServiceSnapshot MainWindow::GatherSnapshot() noexcept
    {
        ServiceSnapshot snapshot{};

        try
        {
            snapshot.TransportAvailable = midi2bt::MidiBluetoothTransportManager::IsTransportAvailable();

            if (!snapshot.TransportAvailable)
            {
                return snapshot;
            }

            snapshot.Devices = midi2bt::MidiBluetoothTransportManager::GetAvailableDevices();
            snapshot.Peripheral = midi2bt::MidiBluetoothTransportManager::GetPeripheralStatus();
            snapshot.PendingClients = midi2bt::MidiBluetoothTransportManager::GetPendingPeripheralClients();
            snapshot.Radio = midi2bt::MidiBluetoothTransportManager::GetRadioInformation();

            // The configuration file is this app's record of which devices are meant to come
            // back on their own, and the service does not report that separately.
            try
            {
                auto const path = midi2svc::MidiServiceTransportPluginConfigManager::ConfigFilePath();

                if (!path.empty())
                {
                    std::wifstream file{ std::wstring{ path } };

                    if (file)
                    {
                        std::wstringstream buffer;
                        buffer << file.rdbuf();

                        json::JsonObject root{ nullptr };

                        if (json::JsonObject::TryParse(winrt::hstring{ buffer.str() }, root))
                        {
                            auto const transportKey = winrt::hstring{ std::format(
                                L"{{{:08X}-{:04X}-{:04X}-{:02X}{:02X}-{:02X}{:02X}{:02X}{:02X}{:02X}{:02X}}}",
                                midi2bt::MidiBluetoothTransportManager::TransportId().Data1,
                                midi2bt::MidiBluetoothTransportManager::TransportId().Data2,
                                midi2bt::MidiBluetoothTransportManager::TransportId().Data3,
                                midi2bt::MidiBluetoothTransportManager::TransportId().Data4[0],
                                midi2bt::MidiBluetoothTransportManager::TransportId().Data4[1],
                                midi2bt::MidiBluetoothTransportManager::TransportId().Data4[2],
                                midi2bt::MidiBluetoothTransportManager::TransportId().Data4[3],
                                midi2bt::MidiBluetoothTransportManager::TransportId().Data4[4],
                                midi2bt::MidiBluetoothTransportManager::TransportId().Data4[5],
                                midi2bt::MidiBluetoothTransportManager::TransportId().Data4[6],
                                midi2bt::MidiBluetoothTransportManager::TransportId().Data4[7]) };

                            auto const settings = root.GetNamedObject(L"endpointTransportPluginSettings", nullptr);

                            if (settings != nullptr)
                            {
                                for (auto const& pair : settings)
                                {
                                    if (Lowered(pair.Key()) != Lowered(transportKey))
                                    {
                                        continue;
                                    }

                                    // Iterating a JsonObject yields IJsonValue, which does not
                                    // cast to JsonObject. It has to be asked for its object.
                                    if (pair.Value() == nullptr ||
                                        pair.Value().ValueType() != json::JsonValueType::Object)
                                    {
                                        break;
                                    }

                                    auto const section = pair.Value().GetObject();

                                    if (section == nullptr || !section.HasKey(L"devices"))
                                    {
                                        break;
                                    }

                                    auto const devices = section.GetNamedArray(L"devices", nullptr);

                                    if (devices == nullptr)
                                    {
                                        break;
                                    }

                                    for (uint32_t i = 0; i < devices.Size(); i++)
                                    {
                                        auto const entry = devices.GetObjectAt(i);

                                        if (entry == nullptr)
                                        {
                                            continue;
                                        }

                                        auto const id = entry.GetNamedString(L"deviceId", L"");

                                        if (!id.empty())
                                        {
                                            snapshot.RememberedDeviceIds.push_back(std::wstring{ Lowered(id) });
                                        }
                                    }

                                    break;
                                }
                            }
                        }
                    }
                }
            }
            catch (...)
            {
                // a configuration file which cannot be read only costs the Forget button
            }

            snapshot.Gathered = true;
        }
        catch (...)
        {
        }

        return snapshot;
    }

    void MainWindow::ApplySnapshot(ServiceSnapshot const& snapshot) noexcept
    {
        ApplyRadio(snapshot);
        ApplyDevices(snapshot);
        ApplyPeripheral(snapshot);
        ApplyPendingClients(snapshot);
        ApplyTransportSettings();
    }

    void MainWindow::ApplyTransportSettings() noexcept
    {
        try
        {
            auto const seconds = midi2bt::MidiBluetoothTransportManager::GetDefaultOfflineRetentionSeconds();

            if (seconds == m_defaultOfflineRetentionSeconds)
            {
                return;
            }

            m_defaultOfflineRetentionSeconds = seconds;

            // This combo has no "use default" entry, so its indices are one lower than the
            // per-device one's.
            auto const deviceIndex = implementation::BluetoothDeviceItem::OfflineRetentionIndexFromSeconds(seconds);

            DefaultOfflineRetentionCombo().SelectedIndex(deviceIndex < 1 ? -1 : deviceIndex - 1);
        }
        MIDI_BTSETUP_CATCH_AND_LOG(L"Unable to show the transport settings.")
    }

    void MainWindow::ApplyRadio(ServiceSnapshot const& snapshot) noexcept
    {
        try
        {
            // Null means an older service which does not report this. Saying "no radio" then
            // would be worse than saying nothing at all.
            if (snapshot.Radio == nullptr)
            {
                RadioLimitationBar().IsOpen(false);
                return;
            }

            winrt::hstring message{};

            if (!snapshot.Radio.IsPresent())
            {
                message = res::GetString(L"RadioNotPresent");
            }
            else if (!snapshot.Radio.IsLowEnergySupported())
            {
                message = res::GetString(L"RadioNoLowEnergy");
            }
            else if (!snapshot.Radio.IsCentralRoleSupported())
            {
                message = res::GetString(L"RadioNoCentralRole");
            }
            else if (!snapshot.Radio.IsPeripheralRoleSupported())
            {
                message = res::GetString(L"RadioNoPeripheralRole");
            }

            if (message.empty())
            {
                RadioLimitationBar().IsOpen(false);
                return;
            }

            RadioLimitationBar().Message(message);
            RadioLimitationBar().IsOpen(true);
        }
        MIDI_BTSETUP_CATCH_AND_LOG(L"Unable to report the radio capabilities.")
    }

    // Rows are matched on the Bluetooth address and updated in place. Rebuilding the collection
    // would throw away the scroll position and the signal history on every tick.
    void MainWindow::ApplyDevices(ServiceSnapshot const& snapshot) noexcept
    {
        try
        {
            if (!snapshot.Gathered || snapshot.Devices == nullptr)
            {
                return;
            }

            std::vector<std::wstring> seen{};

            for (auto const& device : snapshot.Devices)
            {
                if (device == nullptr)
                {
                    continue;
                }

                auto const id = device.BluetoothDeviceId();

                if (id.empty())
                {
                    continue;
                }

                seen.push_back(std::wstring{ Lowered(id) });

                midibluetoothsetup::BluetoothDeviceItem item{ nullptr };

                for (auto const& existing : m_devices)
                {
                    if (Lowered(existing.BluetoothDeviceId()) == Lowered(id))
                    {
                        item = existing;
                        break;
                    }
                }

                if (item == nullptr)
                {
                    auto created = winrt::make_self<implementation::BluetoothDeviceItem>();
                    created->InternalInitialize(id);

                    item = *created;
                    m_devices.Append(item);
                }

                auto const remembered = std::find(
                    snapshot.RememberedDeviceIds.begin(),
                    snapshot.RememberedDeviceIds.end(),
                    std::wstring{ Lowered(id) }) != snapshot.RememberedDeviceIds.end();

                auto const name = device.Name().empty() ?
                    res::GetString(L"UnknownDeviceName") : device.Name();

                auto const statistics = device.HasEndpoint() ?
                    res::FormatString(
                        L"MessageCountsFormat",
                        static_cast<int64_t>(device.MessagesReceived()),
                        static_cast<int64_t>(device.MessagesSent())) :
                    winrt::hstring{};

                auto const protocol = device.SelectedProtocol();

                // The protocol is only known once the device has been connected to. Saying so
                // reads as though something is wrong, so the address stands on its own instead.
                auto const subtitle = protocol == midi2bt::MidiBluetoothProtocol::Unknown ?
                    id :
                    res::FormatString(L"DeviceSubtitleFormat", id, ProtocolDisplayName(protocol));

                winrt::get_self<implementation::BluetoothDeviceItem>(item)->InternalUpdate(
                    name,
                    subtitle,
                    PresenceDescription(device.IsConnected(), device.IsPresent(), device.HasBeenSeen(), device.LastSeenAgo()),
                    statistics,
                    device.IsConnected() ? IntervalDescription(device.ConnectionInterval()) : winrt::hstring{},
                    device.EndpointDeviceId(),
                    device.EndpointDeviceInstanceId(),
                    device.LastConnectError(),
                    device.SignalStrengthDecibelMilliwatts(),
                    device.IsConnected(),
                    device.IsPresent(),
                    device.IsPaired(),
                    device.HasEndpoint(),
                    remembered,
                    device.OfflineRetentionSeconds(),
                    OfflineRetentionDescription(device.EffectiveOfflineRetentionSeconds()));
            }

            // The service drops a device it has not heard from for a while, so rows go away too
            for (int32_t index = static_cast<int32_t>(m_devices.Size()) - 1; index >= 0; index--)
            {
                auto const existing = m_devices.GetAt(static_cast<uint32_t>(index));

                if (std::find(seen.begin(), seen.end(), std::wstring{ Lowered(existing.BluetoothDeviceId()) }) == seen.end())
                {
                    m_devices.RemoveAt(static_cast<uint32_t>(index));
                }
            }

            auto const empty = m_devices.Size() == 0;

            NoDevicesText().Visibility(empty ? xaml::Visibility::Visible : xaml::Visibility::Collapsed);
            DevicesListView().Visibility(empty ? xaml::Visibility::Collapsed : xaml::Visibility::Visible);
        }
        MIDI_BTSETUP_CATCH_AND_LOG(L"Unable to apply the device list.")
    }

    void MainWindow::ApplyPendingClients(ServiceSnapshot const& snapshot) noexcept
    {
        try
        {
            if (!snapshot.Gathered)
            {
                return;
            }

            auto const pending = snapshot.PendingClients;

            if (pending == nullptr || pending.Size() == 0)
            {
                m_pendingClients.Clear();
                PendingClientsBar().IsOpen(false);

                return;
            }

            // Rebuilt rather than reconciled: there is at most one, and nothing here holds
            // selection or scroll state worth preserving.
            m_pendingClients.Clear();

            for (auto const& client : pending)
            {
                auto item = winrt::make_self<implementation::PendingClientItem>();

                auto const name = client.Name().empty() ?
                    res::GetString(L"UnknownDeviceName") : client.Name();

                item->InternalUpdate(
                    AddressKey(client.BluetoothAddress()),
                    res::FormatString(L"PendingClientHeadlineFormat", name),
                    res::FormatString(
                        L"PendingClientDetailFormat",
                        AddressDescription(client.BluetoothAddress(), client.BluetoothAddressType())),
                    client.IsRememberable());

                m_pendingClients.Append(*item);
            }

            PendingClientsBar().IsOpen(true);
        }
        MIDI_BTSETUP_CATCH_AND_LOG(L"Unable to show the devices waiting for permission.")
    }

    void MainWindow::ApplyPeripheral(ServiceSnapshot const& snapshot) noexcept
    {        try
        {
            if (!snapshot.Gathered || snapshot.Peripheral == nullptr)
            {
                return;
            }

            auto const status = snapshot.Peripheral;
            auto const isRunning = status.IsRunning();

            PeripheralNameText().Text(status.AdvertisedName().empty() ?
                res::GetString(L"UnknownDeviceName") : status.AdvertisedName());

            PeripheralStateText().Text(isRunning ?
                res::FormatString(L"PeripheralRunningFormat", ProtocolDisplayName(status.Protocol())) :
                res::GetString(L"PeripheralStopped"));

            StartPeripheralButton().Visibility(isRunning ? xaml::Visibility::Collapsed : xaml::Visibility::Visible);
            StopPeripheralButton().Visibility(isRunning ? xaml::Visibility::Visible : xaml::Visibility::Collapsed);

            // the protocol cannot be changed while advertising, because only one is published
            PeripheralProtocolCombo().IsEnabled(!isRunning);

            if (isRunning)
            {
                PeripheralProtocolCombo().SelectedIndex(
                    status.Protocol() == midi2bt::MidiBluetoothProtocol::BluetoothLowEnergyMidi2Ump ? 1 : 0);
            }

            auto const client = status.ConnectedClient();

            if (client == nullptr || !status.IsClientConnected())
            {
                m_peripheralClients.Clear();

                NoPeripheralClientText().Visibility(xaml::Visibility::Visible);

                return;
            }

            NoPeripheralClientText().Visibility(xaml::Visibility::Collapsed);

            if (m_peripheralClients.Size() == 0)
            {
                m_peripheralClients.Append(*winrt::make_self<implementation::PeripheralClientItem>());
            }

            auto const item = m_peripheralClients.GetAt(0);

            winrt::get_self<implementation::PeripheralClientItem>(item)->InternalUpdate(
                client.Name().empty() ? res::GetString(L"UnknownDeviceName") : client.Name(),
                AddressDescription(client.BluetoothAddress(), client.BluetoothAddressType()),
                client.IsPaired() ? res::GetString(L"ClientPaired") : res::GetString(L"ClientNotPaired"),
                res::FormatString(
                    L"MessageCountsFormat",
                    static_cast<int64_t>(status.MessagesReceived()),
                    static_cast<int64_t>(status.MessagesSent())),
                IntervalDescription(client.ConnectionInterval()),
                status.EndpointDeviceId(),
                status.EndpointDeviceInstanceId(),
                client.IsPaired(),
                client.HasGenericName());
        }
        MIDI_BTSETUP_CATCH_AND_LOG(L"Unable to apply the peripheral status.")
    }

    void MainWindow::SetDevicesStatus(winrt::hstring const& text) noexcept
    {
        ShowTransientStatus(DevicesStatusText(), m_devicesStatusTimer, text);
    }

    void MainWindow::SetPeripheralStatus(winrt::hstring const& text) noexcept
    {
        ShowTransientStatus(PeripheralStatusText(), m_peripheralStatusTimer, text);
    }

    void MainWindow::ShowTransientStatus(
        controls::TextBlock const& target,
        winrt::Microsoft::UI::Dispatching::DispatcherQueueTimer& timer,
        winrt::hstring const& text) noexcept
    {
        try
        {
            target.Text(text);

            if (timer == nullptr)
            {
                timer = DispatcherQueue().CreateTimer();
                timer.IsRepeating(false);
            }

            timer.Stop();

            auto weakTarget = winrt::make_weak(target);

            timer.Tick([weakTarget](auto&& sender, auto&&)
                {
                    sender.Stop();

                    if (auto strong = weakTarget.get())
                    {
                        strong.Text(L"");
                    }
                });

            timer.Interval(StatusMessageLifetime);
            timer.Start();
        }
        MIDI_BTSETUP_CATCH_AND_LOG(L"Unable to show a status message.")
    }
}
