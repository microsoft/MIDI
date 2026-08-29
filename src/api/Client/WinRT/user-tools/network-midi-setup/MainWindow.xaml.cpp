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

// Capability key names, so the app can tell a compatible transport from an older one. Pure
// preprocessor defines; the SDK includes the same header.
#include "..\..\..\..\Transport\UdpNetworkMidi2Transport\network_json_defs.h"

#include <winrt/Microsoft.UI.Xaml.Media.Animation.h>

namespace native = ::midinetworksetup;
namespace res = ::midinetworksetup::resources;
namespace animation = ::winrt::Microsoft::UI::Xaml::Media::Animation;

namespace winrt::midinetworksetup::implementation
{
    namespace
    {
        // Long enough to read, short enough that it is gone before it becomes untrue.
        constexpr std::chrono::seconds StatusMessageLifetime{ 8 };
        // Entry identifiers are written to and read from the configuration file in the unbraced
        // lowercase form, which is what winrt::to_hstring produces for a guid.
        winrt::hstring EntryKey(_In_ winrt::guid const& value) noexcept
        {
            try
            {
                return winrt::to_hstring(value);
            }
            catch (...)
            {
                return {};
            }
        }

        bool TryParseEntryKey(_In_ winrt::hstring const& text, _Out_ winrt::guid& value) noexcept
        {
            value = winrt::guid{};

            if (text.empty())
            {
                return false;
            }

            try
            {
                value = winrt::guid{ std::wstring_view{ text } };
                return true;
            }
            catch (...)
            {
                return false;
            }
        }

        winrt::hstring Lowered(_In_ winrt::hstring const& value) noexcept
        {
            try
            {
                std::wstring copy{ value };

                std::transform(copy.begin(), copy.end(), copy.begin(), [](wchar_t c) { return static_cast<wchar_t>(::towlower(c)); });

                return winrt::hstring{ copy };
            }
            catch (...)
            {
                return value;
            }
        }

        winrt::hstring TrimmedText(_In_ winrt::hstring const& value) noexcept
        {
            try
            {
                std::wstring copy{ value };

                auto const first = copy.find_first_not_of(L" \t\r\n");

                if (first == std::wstring::npos)
                {
                    return {};
                }

                auto const last = copy.find_last_not_of(L" \t\r\n");

                return winrt::hstring{ copy.substr(first, last - first + 1) };
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

        winrt::hstring FormatCount(_In_ uint64_t const value) noexcept
        {
            try
            {
                return winrt::hstring{ std::format(L"{}", value) };
            }
            catch (...)
            {
                return L"0";
            }
        }

        // The service has no discovery based connect verb, so a discovered host has to be
        // invited at a resolved address. A routable IPv4 address is the most likely to work:
        // an automatic private address only works on the same link, and a link local IPv6
        // address carries a scope id the service would have to interpret.
        winrt::hstring PreferredAddress(_In_ midi2net::MidiNetworkAdvertisedHost const& host) noexcept
        {
            try
            {
                if (host == nullptr)
                {
                    return {};
                }

                auto const addresses = host.IPAddresses();

                if (addresses != nullptr)
                {
                    for (auto const& address : addresses)
                    {
                        std::wstring const value{ address };

                        if (value.find(L':') == std::wstring::npos &&
                            !value.starts_with(L"169.254."))
                        {
                            return address;
                        }
                    }
                }

                if (!host.HostName().empty())
                {
                    return host.HostName();
                }

                if (addresses != nullptr && addresses.Size() > 0)
                {
                    return addresses.GetAt(0);
                }
            }
            catch (...)
            {
            }

            return {};
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

            PendingInvitationsList().ItemsSource(m_pendingInvitations);
            RemoteHostsListView().ItemsSource(m_remoteHosts);
            LocalHostsListView().ItemsSource(m_localHosts);

            // the startup options were parsed before the window existed
            auto const& options = App::StartupOptions();

            if (!options.ConfigFilePath.empty())
            {
                native::NetworkConfigFile::Current().OverridePath(options.ConfigFilePath);
            }

            ShowPage(native::AppSettings::Current().SelectedPageIndex());

            MainNavigation().SelectedItem(NavigationItemForPage(native::AppSettings::Current().SelectedPageIndex()));

            Closed([weak = get_weak()](auto&&, auto&&)
                {
                    if (auto strong = weak.get())
                    {
                        // Before m_closing, which the timer callback treats as a reason to skip
                        strong->FlushPendingTransportSettingsWrite();

                        strong->m_closing = true;
                        strong->StopRefreshTimer();
                        strong->StopWatcher();
                        strong->m_chrome.SavePlacement();
                        strong->m_chrome.Shutdown();
                    }
                });

            m_loaded = true;

            // The network transport ships out of band today, and older builds are in the wild.
            // Nothing here works against one which is missing or too old, so the app says so and
            // stops rather than failing one operation at a time.
            if (!VerifyTransportIsUsable())
            {
                return;
            }

            if (native::NetworkConfigFile::Current().IsOverridden())
            {
                SetRemoteStatus(res::FormatString(
                    L"ConfigFileOverrideNotice",
                    native::NetworkConfigFile::Current().Path()));
            }

            StartWatcher();
            StartRefreshTimer();

            RequestRefreshAsync();
        }
        MIDI_NETSETUP_CATCH_AND_LOG(L"Unable to finish loading the window.")
    }

    // The network transport is an out-of-band install today and older builds are still in use, so
    // presence alone is not enough: the verbs this app relies on have to be there too. Anything
    // missing means nothing on either page would work, and one clear message beats a series of
    // individual failures.
    bool MainWindow::VerifyTransportIsUsable() noexcept
    {
        bool usable{ false };

        try
        {
            if (midi2net::MidiNetworkTransportManager::IsTransportAvailable())
            {
                auto const transportId = midi2net::MidiNetworkTransportManager::TransportId();

                static wchar_t const* const requiredCapabilities[]
                {
                    MIDI_CONFIG_JSON_NETWORK_MIDI_COMMAND_VERB_ENUMERATE_HOSTS,
                    MIDI_CONFIG_JSON_NETWORK_MIDI_COMMAND_VERB_ENUMERATE_CLIENTS,
                    MIDI_CONFIG_JSON_NETWORK_MIDI_COMMAND_VERB_START_HOST,
                    MIDI_CONFIG_JSON_NETWORK_MIDI_COMMAND_VERB_STOP_HOST,
                    MIDI_CONFIG_JSON_NETWORK_MIDI_COMMAND_VERB_REMOVE_HOST,
                    MIDI_CONFIG_JSON_NETWORK_MIDI_COMMAND_VERB_CONNECT_DIRECT,
                    MIDI_CONFIG_JSON_NETWORK_MIDI_COMMAND_VERB_CONNECT_MDNS,
                    MIDI_CONFIG_JSON_NETWORK_MIDI_COMMAND_VERB_DISCONNECT_CLIENT,
                    MIDI_CONFIG_JSON_NETWORK_MIDI_COMMAND_VERB_APPROVE_REMOTE_CLIENT,
                    MIDI_CONFIG_JSON_NETWORK_MIDI_COMMAND_VERB_DENY_REMOTE_CLIENT,
                    MIDI_CONFIG_JSON_NETWORK_MIDI_COMMAND_VERB_DISCONNECT_REMOTE_CLIENT,
                    MIDI_CONFIG_JSON_NETWORK_MIDI_COMMAND_VERB_GET_PENDING_REMOTE_CLIENTS,
                    MIDI_CONFIG_JSON_NETWORK_MIDI_CAPABILITY_CUSTOM_ENDPOINT_NAME_ON_CREATE,
                };

                usable = true;

                for (auto const& capability : requiredCapabilities)
                {
                    if (!winrt::Windows::Devices::Midi2::ServiceConfig::MidiServiceTransportPluginConfigManager::QueryCapability(transportId, capability))
                    {
                        TraceLoggingWrite(
                            MidiNetworkSetupTelemetryProvider::Provider(),
                            MIDI_NETSETUP_TRACE_EVENT_WARNING,
                            TraceLoggingString(__FUNCTION__, MIDI_NETSETUP_TRACE_LOCATION_FIELD),
                            TraceLoggingLevel(WINEVENT_LEVEL_WARNING),
                            TraceLoggingWideString(L"Required network transport capability is missing.", MIDI_NETSETUP_TRACE_MESSAGE_FIELD),
                            TraceLoggingWideString(capability, "capability")
                        );

                        usable = false;
                        break;
                    }
                }
            }
        }
        catch (...)
        {
            usable = false;
        }

        if (!usable)
        {
            TransportUnavailableBar().IsOpen(true);

            RemoteHostsPanel().Visibility(xaml::Visibility::Collapsed);
            LocalHostsPanel().Visibility(xaml::Visibility::Collapsed);
            SettingsPanel().Visibility(xaml::Visibility::Collapsed);
            MainNavigation().IsEnabled(false);
        }

        return usable;
    }

    _Use_decl_annotations_
    void MainWindow::OnRootSizeChanged(foundation::IInspectable const&, xaml::SizeChangedEventArgs const&)    {
        m_chrome.UpdateTitleBarInsets();
    }

    _Use_decl_annotations_
    void MainWindow::OnAlwaysOnTopToggled(foundation::IInspectable const&, xaml::RoutedEventArgs const&)
    {
        try
        {
            auto const isChecked = AlwaysOnTopToggle().IsChecked();

            native::AppSettings::Current().AlwaysOnTop(isChecked != nullptr && isChecked.Value());

            m_chrome.ApplyAlwaysOnTop();
        }
        MIDI_NETSETUP_CATCH_AND_LOG(L"Unable to change the always on top setting.")
    }

    _Use_decl_annotations_
    void MainWindow::OnAppearanceButtonClick(foundation::IInspectable const&, xaml::RoutedEventArgs const&)
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

            // the refresh interval belongs with the other settings rather than in a panel of
            // its own, so it goes into the shared flyout's extra content slot
            controls::StackPanel extra{};
            extra.Spacing(4);
            extra.Margin(xaml::Thickness{ 0, 12, 0, 0 });

            controls::TextBlock label{};
            label.Text(res::GetString(L"SettingsRefreshIntervalLabel"));

            controls::NumberBox box{};
            box.Minimum(static_cast<double>(native::AppSettings::MinimumRefreshIntervalSeconds));
            box.Maximum(static_cast<double>(native::AppSettings::MaximumRefreshIntervalSeconds));
            box.Value(static_cast<double>(native::AppSettings::Current().RefreshIntervalSeconds()));
            box.SpinButtonPlacementMode(controls::NumberBoxSpinButtonPlacementMode::Compact);
            box.HorizontalAlignment(xaml::HorizontalAlignment::Left);
            box.Width(160);
            xaml::Automation::AutomationProperties::SetName(box, res::GetString(L"SettingsRefreshIntervalLabel"));

            box.ValueChanged([weak = get_weak()](controls::NumberBox const& sender, controls::NumberBoxValueChangedEventArgs const&)
                {
                    auto strong = weak.get();

                    if (strong == nullptr)
                    {
                        return;
                    }

                    auto const value = sender.Value();

                    // an empty NumberBox reports NaN
                    if (value != value)
                    {
                        return;
                    }

                    native::AppSettings::Current().RefreshIntervalSeconds(static_cast<uint32_t>(value));

                    strong->StartRefreshTimer();
                });

            controls::TextBlock help{};
            help.Text(res::GetString(L"SettingsRefreshIntervalHelp"));
            help.TextWrapping(xaml::TextWrapping::Wrap);
            help.FontSize(12);

            extra.Children().Append(label);
            extra.Children().Append(box);
            extra.Children().Append(help);

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
                },
                extra);
        }
        MIDI_NETSETUP_CATCH_AND_LOG(L"Unable to open the settings.")
    }

    _Use_decl_annotations_
    void MainWindow::OnNavigationSelectionChanged(
        controls::NavigationView const&,
        controls::NavigationViewSelectionChangedEventArgs const& args)
    {
        try
        {
            if (!m_loaded)
            {
                return;
            }

            auto const item = args.SelectedItem().try_as<controls::NavigationViewItem>();

            auto const tag = item != nullptr && item.Tag() != nullptr ?
                winrt::unbox_value_or<winrt::hstring>(item.Tag(), L"") :
                winrt::hstring{};

            auto const pageIndex =
                tag == L"local" ? native::AppSettings::PageIndexLocalHosts :
                tag == L"settings" ? native::AppSettings::PageIndexTransportSettings :
                native::AppSettings::PageIndexRemoteHosts;

            // Leaving the page with a debounced write still waiting would quietly discard the
            // customer's last change
            if (pageIndex != native::AppSettings::PageIndexTransportSettings)
            {
                FlushPendingTransportSettingsWrite();
            }

            ShowPage(pageIndex);

            native::AppSettings::Current().SelectedPageIndex(pageIndex);

            if (pageIndex == native::AppSettings::PageIndexTransportSettings)
            {
                LoadTransportSettings();
                return;
            }

            RequestRefreshAsync();
        }
        MIDI_NETSETUP_CATCH_AND_LOG(L"Unable to change pages.")
    }

    _Use_decl_annotations_
    foundation::IInspectable MainWindow::NavigationItemForPage(uint32_t const pageIndex) noexcept
    {
        try
        {
            if (pageIndex == native::AppSettings::PageIndexLocalHosts)
            {
                return LocalHostsNavigationItem().as<foundation::IInspectable>();
            }

            if (pageIndex == native::AppSettings::PageIndexTransportSettings)
            {
                return SettingsNavigationItem().as<foundation::IInspectable>();
            }

            return RemoteHostsNavigationItem().as<foundation::IInspectable>();
        }
        catch (...)
        {
            return nullptr;
        }
    }

    void MainWindow::ShowPage(uint32_t const pageIndex) noexcept
    {
        try
        {
            RemoteHostsPanel().Visibility(
                pageIndex == native::AppSettings::PageIndexRemoteHosts ? xaml::Visibility::Visible : xaml::Visibility::Collapsed);

            LocalHostsPanel().Visibility(
                pageIndex == native::AppSettings::PageIndexLocalHosts ? xaml::Visibility::Visible : xaml::Visibility::Collapsed);

            SettingsPanel().Visibility(
                pageIndex == native::AppSettings::PageIndexTransportSettings ? xaml::Visibility::Visible : xaml::Visibility::Collapsed);
        }
        catch (...)
        {
        }
    }

    _Use_decl_annotations_
    void MainWindow::ShowTransientStatus(
        xaml::Controls::TextBlock const& target,
        winrt::Microsoft::UI::Dispatching::DispatcherQueueTimer& timer,
        winrt::hstring const& text) noexcept
    {
        try
        {
            if (target == nullptr)
            {
                return;
            }

            if (timer != nullptr)
            {
                timer.Stop();
            }

            target.Opacity(1.0);
            target.Text(text);

            if (text.empty())
            {
                return;
            }

            auto queue = DispatcherQueue();

            if (queue == nullptr)
            {
                return;
            }

            if (timer == nullptr)
            {
                timer = queue.CreateTimer();

                if (timer == nullptr)
                {
                    return;
                }

                timer.IsRepeating(false);
            }

            timer.Interval(StatusMessageLifetime);

            timer.Tick([weak = get_weak(), target](auto&& sender, auto&&)
                {
                    sender.Stop();

                    auto strong = weak.get();

                    if (strong == nullptr || strong->m_closing)
                    {
                        return;
                    }

                    try
                    {
                        animation::DoubleAnimation fade{};

                        fade.To(0.0);
                        fade.Duration(winrt::Microsoft::UI::Xaml::DurationHelper::FromTimeSpan(
                            std::chrono::milliseconds{ 600 }));

                        animation::Storyboard::SetTarget(fade, target);
                        animation::Storyboard::SetTargetProperty(fade, L"Opacity");

                        animation::Storyboard storyboard{};
                        storyboard.Children().Append(fade);

                        storyboard.Completed([target](auto&&, auto&&)
                            {
                                try
                                {
                                    target.Text(L"");
                                    target.Opacity(1.0);
                                }
                                catch (...)
                                {
                                }
                            });

                        storyboard.Begin();
                    }
                    catch (...)
                    {
                    }
                });

            timer.Start();
        }
        catch (...)
        {
        }
    }

    void MainWindow::SetRemoteStatus(winrt::hstring const& text) noexcept
    {
        ShowTransientStatus(RemoteStatusText(), m_remoteStatusTimer, text);
    }

    void MainWindow::SetLocalStatus(winrt::hstring const& text) noexcept
    {
        ShowTransientStatus(LocalStatusText(), m_localStatusTimer, text);
    }


    // ------------------------------------------------------------------------------------
    // discovery and refresh
    // ------------------------------------------------------------------------------------

    void MainWindow::StartWatcher() noexcept
    {
        try
        {
            if (m_watcher != nullptr)
            {
                return;
            }

            m_watcher = midi2net::MidiNetworkAdvertisedHostWatcher::Create();

            if (m_watcher == nullptr)
            {
                return;
            }

            // the watcher raises on a background thread, so a change only asks for a refresh
            // rather than touching anything the UI is bound to
            auto const request = [weak = get_weak()]()
                {
                    if (auto strong = weak.get())
                    {
                        if (auto queue = strong->DispatcherQueue())
                        {
                            queue.TryEnqueue([weak]()
                                {
                                    if (auto inner = weak.get())
                                    {
                                        inner->RequestRefreshAsync();
                                    }
                                });
                        }
                    }
                };

            m_watcherAddedToken = m_watcher.Added([request](auto&&, auto&&) { request(); });
            m_watcherRemovedToken = m_watcher.Removed([request](auto&&, auto&&) { request(); });
            m_watcherUpdatedToken = m_watcher.Updated([request](auto&&, auto&&) { request(); });

            m_watcher.Start();
        }
        MIDI_NETSETUP_CATCH_AND_LOG(L"Unable to start watching for network hosts.")
    }

    void MainWindow::StopWatcher() noexcept
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
        catch (...)
        {
        }
    }

    void MainWindow::StartRefreshTimer() noexcept
    {
        try
        {
            StopRefreshTimer();

            auto queue = DispatcherQueue();

            if (queue == nullptr)
            {
                return;
            }

            m_refreshTimer = queue.CreateTimer();

            if (m_refreshTimer == nullptr)
            {
                return;
            }

            m_refreshTimer.Interval(
                std::chrono::seconds{ native::AppSettings::Current().RefreshIntervalSeconds() });

            m_refreshTimer.Tick([weak = get_weak()](auto&&, auto&&)
                {
                    if (auto strong = weak.get())
                    {
                        strong->RequestRefreshAsync();
                    }
                });

            m_refreshTimer.Start();
        }
        MIDI_NETSETUP_CATCH_AND_LOG(L"Unable to start the refresh timer.")
    }

    void MainWindow::StopRefreshTimer() noexcept
    {
        try
        {
            if (m_refreshTimer != nullptr)
            {
                m_refreshTimer.Stop();
                m_refreshTimer = nullptr;
            }
        }
        catch (...)
        {
        }
    }

    MainWindow::ServiceSnapshot MainWindow::GatherSnapshot() noexcept
    {
        ServiceSnapshot snapshot{};

        try
        {
            snapshot.TransportAvailable = midi2net::MidiNetworkTransportManager::IsTransportAvailable();

            if (!snapshot.TransportAvailable)
            {
                return snapshot;
            }

            // Both pages are gathered even when only one is visible. The latency graphs plot
            // against elapsed time, so a page which stopped sampling while hidden would come back
            // with a flat segment across the gap and read as a bug.
            snapshot.ConfiguredHosts = midi2net::MidiNetworkTransportManager::GetConfiguredHosts();
            snapshot.ConfiguredClients = midi2net::MidiNetworkTransportManager::GetConfiguredClients();
            snapshot.PendingRemoteClients = midi2net::MidiNetworkTransportManager::GetPendingRemoteClients();

            snapshot.ClientDisplayNames = native::NetworkConfigFile::Current().GetClientDisplayNames();
            snapshot.ConfiguredClientIds = native::NetworkConfigFile::Current().GetClientEntryIds();

            if (snapshot.ConfiguredHosts != nullptr)
            {
                for (auto const& host : snapshot.ConfiguredHosts)
                {
                    if (host == nullptr)
                    {
                        continue;
                    }

                    auto const key = EntryKey(host.HostId());

                    snapshot.KnownClients.insert_or_assign(
                        std::wstring{ key },
                        native::NetworkConfigFile::Current().GetKnownClients(key));
                }
            }

            snapshot.Gathered = true;
        }
        catch (...)
        {
        }

        return snapshot;
    }

    winrt::fire_and_forget MainWindow::RequestRefreshAsync() noexcept
    {
        if (m_closing || m_refreshInProgress.exchange(true))
        {
            co_return;
        }

        auto weak = get_weak();
        auto queue = DispatcherQueue();

        // read on the UI thread, because the watcher's map is what the pages fold together
        std::vector<midi2net::MidiNetworkAdvertisedHost> advertised{};

        try
        {
            if (m_watcher != nullptr)
            {
                for (auto const& pair : m_watcher.EnumeratedHosts())
                {
                    if (pair.Value() != nullptr)
                    {
                        advertised.push_back(pair.Value());
                    }
                }
            }
        }
        catch (...)
        {
        }

        // every one of the calls below blocks on the service, so none of them may run here
        co_await winrt::resume_background();

        auto snapshot = GatherSnapshot();
        snapshot.AdvertisedHosts = std::move(advertised);

        if (queue == nullptr)
        {
            co_return;
        }

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

                strong->m_refreshInProgress = false;
            });
    }

    void MainWindow::ApplySnapshot(ServiceSnapshot const& snapshot) noexcept
    {
        try
        {
            if (!snapshot.TransportAvailable)
            {
                if (!m_transportMissingReported)
                {
                    m_transportMissingReported = true;

                    SetRemoteStatus(res::GetString(L"TransportUnavailableError"));
                    SetLocalStatus(res::GetString(L"TransportUnavailableError"));
                }

                return;
            }

            m_transportMissingReported = false;

            ApplyPendingInvitations(snapshot);
            ApplyLocalHosts(snapshot);
            ApplyRemoteHosts(snapshot);
        }
        MIDI_NETSETUP_CATCH_AND_LOG(L"Unable to show the current network state.")
    }


    // ------------------------------------------------------------------------------------
    // pending invitations
    // ------------------------------------------------------------------------------------

    void MainWindow::ApplyPendingInvitations(ServiceSnapshot const& snapshot) noexcept
    {
        try
        {
            std::vector<winrt::hstring> seen{};

            if (snapshot.PendingRemoteClients != nullptr)
            {
                for (auto const& pending : snapshot.PendingRemoteClients)
                {
                    if (pending == nullptr)
                    {
                        continue;
                    }

                    auto const hostKey = EntryKey(pending.HostId());

                    auto const name = pending.UmpEndpointName();
                    auto const productInstanceId = pending.ProductInstanceId();

                    auto const matchKey = Lowered(winrt::hstring{
                        std::wstring{ hostKey } + L"|" + std::wstring{ productInstanceId } + L"|" + std::wstring{ name } });

                    seen.push_back(matchKey);

                    midinetworksetup::PendingInvitationItem item{ nullptr };

                    for (auto const& existing : m_pendingInvitations)
                    {
                        auto const self = winrt::get_self<PendingInvitationItem>(existing);

                        if (self != nullptr &&
                            Lowered(winrt::hstring{
                                std::wstring{ self->HostId() } + L"|" +
                                std::wstring{ self->RemoteProductInstanceId() } + L"|" +
                                std::wstring{ self->RemoteName() } }) == matchKey)
                        {
                            item = existing;
                            break;
                        }
                    }

                    if (item == nullptr)
                    {
                        auto created = winrt::make_self<PendingInvitationItem>();
                        created->InternalInitialize(hostKey, name, productInstanceId);

                        item = *created;

                        m_pendingInvitations.Append(item);
                    }

                    auto const hostName = pending.HostUmpEndpointName().empty() ?
                        pending.HostServiceInstanceName() : pending.HostUmpEndpointName();

                    auto const displayName = name.empty() ? res::GetString(L"UnnamedDevice") : name;

                    winrt::get_self<PendingInvitationItem>(item)->InternalUpdateText(
                        res::FormatString(L"PendingInvitationHeadlineFormat", displayName, hostName),
                        res::FormatString(L"PendingInvitationDetailFormat", pending.RemoteAddress(), productInstanceId));
                }
            }

            // anything the service no longer reports has been answered, here or elsewhere
            for (int32_t i = static_cast<int32_t>(m_pendingInvitations.Size()) - 1; i >= 0; i--)
            {
                auto const existing = m_pendingInvitations.GetAt(static_cast<uint32_t>(i));
                auto const self = winrt::get_self<PendingInvitationItem>(existing);

                if (self == nullptr)
                {
                    m_pendingInvitations.RemoveAt(static_cast<uint32_t>(i));
                    continue;
                }

                auto const key = Lowered(winrt::hstring{
                    std::wstring{ self->HostId() } + L"|" +
                    std::wstring{ self->RemoteProductInstanceId() } + L"|" +
                    std::wstring{ self->RemoteName() } });

                if (std::find(seen.begin(), seen.end(), key) == seen.end())
                {
                    m_pendingInvitations.RemoveAt(static_cast<uint32_t>(i));
                }
            }

            PendingInvitationsBar().IsOpen(m_pendingInvitations.Size() > 0);
        }
        MIDI_NETSETUP_CATCH_AND_LOG(L"Unable to show pending invitations.")
    }


    // ------------------------------------------------------------------------------------
    // page 1: remote hosts
    // ------------------------------------------------------------------------------------

    winrt::hstring MainWindow::DescribeLatency(uint64_t const ticks) noexcept
    {
        try
        {
            if (ticks == 0)
            {
                return res::GetString(L"LatencyUnknown");
            }

            // a tick is 100 nanoseconds
            auto const milliseconds = static_cast<double>(ticks) / 10000.0;

            return res::FormatString(L"LatencyFormat", std::format(L"{:.2f}", milliseconds));
        }
        catch (...)
        {
            return {};
        }
    }

    winrt::hstring MainWindow::JoinAddresses(collections::IVectorView<winrt::hstring> const& addresses) noexcept
    {
        try
        {
            if (addresses == nullptr || addresses.Size() == 0)
            {
                return {};
            }

            std::wstring result{};

            for (auto const& address : addresses)
            {
                if (!result.empty())
                {
                    result += L", ";
                }

                result += address;
            }

            return winrt::hstring{ result };
        }
        catch (...)
        {
            return {};
        }
    }

    _Use_decl_annotations_
    winrt::hstring MainWindow::DisplayAddressForLocalHost(winrt::hstring const& actualAddress) noexcept
    {
        try
        {
            // "::" and "0.0.0.0" both mean "every interface". Anything else is a real address
            // the host was pinned to, and is worth showing as it is.
            if (!actualAddress.empty() && actualAddress != L"::" && actualAddress != L"0.0.0.0")
            {
                return actualAddress;
            }

            std::wstring ipv4{};
            std::wstring ipv6{};

            for (auto const& hostName : networking::Connectivity::NetworkInformation::GetHostNames())
            {
                if (hostName == nullptr)
                {
                    continue;
                }

                // Without adapter information the name is not something another device can reach,
                // which also drops the loopback entries.
                if (hostName.IPInformation() == nullptr)
                {
                    continue;
                }

                auto const type = hostName.Type();

                if (type != networking::HostNameType::Ipv4 && type != networking::HostNameType::Ipv6)
                {
                    continue;
                }

                auto& target = type == networking::HostNameType::Ipv4 ? ipv4 : ipv6;

                if (!target.empty())
                {
                    target += L", ";
                }

                target += hostName.CanonicalName();
            }

            // IPv4 is what people are asked for by almost every device, so IPv6 is only offered
            // when there is no IPv4 address to give.
            if (!ipv4.empty())
            {
                return winrt::hstring{ ipv4 };
            }

            if (!ipv6.empty())
            {
                return winrt::hstring{ ipv6 };
            }

            return actualAddress;
        }
        catch (...)
        {
            return actualAddress;
        }
    }

    void MainWindow::ApplyRemoteHosts(ServiceSnapshot const& snapshot) noexcept
    {
        try
        {
            struct RowData
            {
                winrt::hstring Key{};
                winrt::hstring DisplayName{};
                winrt::hstring Subtitle{};
                winrt::hstring ProductInstanceId{};
                winrt::hstring Addresses{};
                winrt::hstring DeviceId{};
                winrt::hstring ConnectAddress{};
                uint16_t ConnectPort{ 0 };
                winrt::hstring Status{};
                winrt::hstring Statistics{};
                winrt::hstring EndpointDeviceId{};
                winrt::hstring ClientId{};
                uint64_t LatencyTicks{ 0 };
                bool Connected{ false };
                bool Configured{ false };
                bool Advertised{ false };
            };

            std::vector<RowData> rows{};

            // address and port of every advertised host, so a client entry the service created
            // from a resolved address still lands on the row the customer clicked Connect on
            std::unordered_map<std::wstring, winrt::hstring> addressToKey{};

            // this PC's own hosts are advertised too, and offering to connect to yourself is
            // only confusing
            std::vector<std::wstring> ownServiceInstanceNames{};

            if (snapshot.ConfiguredHosts != nullptr)
            {
                for (auto const& host : snapshot.ConfiguredHosts)
                {
                    if (host != nullptr && !host.ServiceInstanceName().empty())
                    {
                        ownServiceInstanceNames.push_back(std::wstring{ Lowered(host.ServiceInstanceName()) });
                    }
                }
            }

            auto const findRow = [&rows](winrt::hstring const& key) -> RowData*
                {
                    for (auto& row : rows)
                    {
                        if (row.Key == key)
                        {
                            return &row;
                        }
                    }

                    return nullptr;
                };

            for (auto const& host : snapshot.AdvertisedHosts)
            {
                if (host == nullptr)
                {
                    continue;
                }

                if (std::find(
                    ownServiceInstanceNames.begin(),
                    ownServiceInstanceNames.end(),
                    std::wstring{ Lowered(host.ServiceInstanceName()) }) != ownServiceInstanceNames.end())
                {
                    continue;
                }

                RowData row{};

                row.Key = Lowered(winrt::hstring{ L"d:" + std::wstring{ host.DeviceId() } });
                row.DeviceId = host.DeviceId();
                row.Advertised = true;

                row.DisplayName = host.UmpEndpointName();

                if (row.DisplayName.empty())
                {
                    row.DisplayName = host.DeviceName();
                }

                if (row.DisplayName.empty())
                {
                    row.DisplayName = host.ServiceInstanceName();
                }

                row.Subtitle = res::FormatString(L"RemoteHostSubtitleFormat", host.HostName(), host.Port());
                row.ProductInstanceId = host.ProductInstanceId();
                row.Addresses = JoinAddresses(host.IPAddresses());
                row.Status = res::GetString(L"RemoteHostAvailable");
                row.ConnectAddress = PreferredAddress(host);
                row.ConnectPort = host.Port();

                if (host.IPAddresses() != nullptr)
                {
                    for (auto const& address : host.IPAddresses())
                    {
                        addressToKey.insert_or_assign(
                            std::wstring{ Lowered(address) } + L"|" + std::to_wstring(host.Port()),
                            row.Key);
                    }
                }

                if (!host.HostName().empty())
                {
                    addressToKey.insert_or_assign(
                        std::wstring{ Lowered(host.HostName()) } + L"|" + std::to_wstring(host.Port()),
                        row.Key);
                }

                rows.push_back(row);
            }

            if (snapshot.ConfiguredClients != nullptr)
            {
                for (auto const& client : snapshot.ConfiguredClients)
                {
                    if (client == nullptr)
                    {
                        continue;
                    }

                    auto const clientKey = EntryKey(client.ClientId());

                    // The configuration file is this app's record of which entries are meant to
                    // exist, and an entry the service still reports but the file no longer has is
                    // usually one on its way out. A live session is the exception: the service is
                    // the authority on what is actually connected, and hiding a connection which
                    // is passing traffic tells the customer a plain untruth. This also covers an
                    // entry created before the file could be written, or by another tool.
                    auto const listedInConfigFile = std::find(
                        snapshot.ConfiguredClientIds.begin(),
                        snapshot.ConfiguredClientIds.end(),
                        std::wstring{ Lowered(clientKey) }) != snapshot.ConfiguredClientIds.end();

                    auto const liveInService =
                        client.IsSessionActive() ||
                        client.EntryState() == midi2net::MidiNetworkClientEntryState::Active;

                    if (!listedInConfigFile && !liveInService)
                    {
                        continue;
                    }

                    winrt::hstring matchKey{};

                    if (!client.MatchDeviceId().empty())
                    {
                        matchKey = Lowered(winrt::hstring{ L"d:" + std::wstring{ client.MatchDeviceId() } });
                    }
                    else
                    {
                        auto const configured = addressToKey.find(
                            std::wstring{ Lowered(client.ConfiguredDirectAddress()) } + L"|" +
                            std::wstring{ client.ConfiguredDirectPort() });

                        matchKey = configured != addressToKey.end() ?
                            configured->second :
                            Lowered(winrt::hstring{ L"c:" + std::wstring{ clientKey } });
                    }

                    auto row = findRow(matchKey);

                    if (row == nullptr)
                    {
                        RowData created{};

                        created.Key = matchKey;
                        created.Advertised = false;

                        auto const named = snapshot.ClientDisplayNames.find(std::wstring{ clientKey });

                        created.DisplayName = named != snapshot.ClientDisplayNames.end() ?
                            named->second : winrt::hstring{};

                        if (created.DisplayName.empty())
                        {
                            created.DisplayName = client.ConfiguredDirectAddress().empty() ?
                                res::GetString(L"UnnamedDevice") : client.ConfiguredDirectAddress();
                        }

                        created.Subtitle = client.ConfiguredDirectAddress().empty() ?
                            res::GetString(L"RemoteHostNotFound") :
                            res::FormatString(
                                L"RemoteHostDirectSubtitleFormat",
                                client.ConfiguredDirectAddress(),
                                client.ConfiguredDirectPort());

                        created.Addresses = client.ConfiguredDirectAddress();

                        created.ConnectAddress = client.ConfiguredDirectAddress();

                        try
                        {
                            created.ConnectPort = static_cast<uint16_t>(
                                std::stoul(std::wstring{ client.ConfiguredDirectPort() }));
                        }
                        catch (...)
                        {
                            created.ConnectPort = 0;
                        }

                        rows.push_back(created);

                        row = &rows.back();
                    }

                    row->Configured = true;
                    row->ClientId = clientKey;
                    row->EndpointDeviceId = client.EndpointDeviceId();

                    row->Connected = client.IsSessionActive();

                    // "Connecting" is not what is happening when the device cannot be seen at
                    // all. Nothing is attempted until it announces itself, so somebody looking at
                    // a device which is switched off should be told that, rather than watching a
                    // connect which is not being tried.
                    auto const notConnectedStatus = [&client, &row]()
                        {
                            if (row->Advertised)
                            {
                                return res::GetString(L"RemoteHostTryingToConnect");
                            }

                            return client.ConfiguredDirectAddress().empty() ?
                                res::GetString(L"RemoteHostWaitingToAppear") :
                                res::GetString(L"RemoteHostWaitingToAnswer");
                        };

                    switch (client.EntryState())
                    {
                    case midi2net::MidiNetworkClientEntryState::Active:
                        row->Status = client.IsSessionActive() ?
                            res::GetString(L"RemoteHostConnected") :
                            notConnectedStatus();
                        break;

                    case midi2net::MidiNetworkClientEntryState::Failed:
                        row->Status = res::GetString(L"RemoteHostFailed");
                        break;

                    case midi2net::MidiNetworkClientEntryState::Unavailable:
                        row->Status = res::GetString(L"RemoteHostUnavailable");
                        break;

                    default:
                        row->Status = notConnectedStatus();
                        break;
                    }

                    if (client.IsSessionActive())
                    {
                        row->LatencyTicks = client.CurrentLatencyTicks();

                        row->Statistics = res::FormatString(
                            L"RemoteHostStatisticsFormat",
                            DescribeLatency(client.CurrentLatencyTicks()),
                            FormatCount(client.TotalCountNetworkPacketsSent()),
                            FormatCount(client.TotalCountNetworkPacketsReceived()),
                            FormatCount(client.RetransmitCount()));

                        if (!client.ConnectedRemoteAddress().empty())
                        {
                            row->Subtitle = res::FormatString(
                                L"RemoteHostDirectSubtitleFormat",
                                client.ConnectedRemoteAddress(),
                                client.ConnectedRemotePort());
                        }
                    }
                    else
                    {
                        row->Statistics = winrt::hstring{};
                    }
                }
            }

            // discovery reports hosts in whatever order they answered, so without this the rows
            // shuffle on every refresh
            std::sort(rows.begin(), rows.end(), [](RowData const& left, RowData const& right)
                {
                    auto const leftName = std::wstring{ Lowered(left.DisplayName) };
                    auto const rightName = std::wstring{ Lowered(right.DisplayName) };

                    if (leftName != rightName)
                    {
                        return leftName < rightName;
                    }

                    return std::wstring{ left.Key } < std::wstring{ right.Key };
                });

            // reconcile against what is on screen, so rows are updated rather than replaced
            for (auto const& row : rows)
            {
                midinetworksetup::RemoteHostItem item{ nullptr };

                for (auto const& existing : m_remoteHosts)
                {
                    if (existing != nullptr && existing.MatchKey() == row.Key)
                    {
                        item = existing;
                        break;
                    }
                }

                if (item == nullptr)
                {
                    auto created = winrt::make_self<RemoteHostItem>();
                    created->InternalInitialize(row.Key);

                    item = *created;

                    m_remoteHosts.Append(item);
                }

                winrt::get_self<RemoteHostItem>(item)->InternalUpdate(
                    row.DisplayName,
                    row.Subtitle,
                    row.ProductInstanceId,
                    row.Addresses,
                    row.DeviceId,
                    row.ConnectAddress,
                    row.ConnectPort,
                    row.Status,
                    row.Statistics,
                    row.EndpointDeviceId,
                    row.ClientId,
                    row.LatencyTicks,
                    row.Connected,
                    row.Configured,
                    row.Advertised,
                    row.Connected ?
                        res::GetString(L"RemoteHostDisconnectAndForgetLabel") :
                        res::GetString(L"RemoteHostForgetLabel"));
            }

            for (int32_t i = static_cast<int32_t>(m_remoteHosts.Size()) - 1; i >= 0; i--)
            {
                auto const existing = m_remoteHosts.GetAt(static_cast<uint32_t>(i));

                auto const stillThere = existing != nullptr && std::any_of(
                    rows.begin(),
                    rows.end(),
                    [&existing](RowData const& row) { return row.Key == existing.MatchKey(); });

                if (!stillThere)
                {
                    m_remoteHosts.RemoveAt(static_cast<uint32_t>(i));
                }
            }

            // put the rows into the sorted order without rebuilding the collection
            for (uint32_t target = 0; target < rows.size() && target < m_remoteHosts.Size(); target++)
            {
                if (m_remoteHosts.GetAt(target).MatchKey() == rows[target].Key)
                {
                    continue;
                }

                for (uint32_t search = target + 1; search < m_remoteHosts.Size(); search++)
                {
                    if (m_remoteHosts.GetAt(search).MatchKey() == rows[target].Key)
                    {
                        auto const moved = m_remoteHosts.GetAt(search);

                        m_remoteHosts.RemoveAt(search);
                        m_remoteHosts.InsertAt(target, moved);

                        break;
                    }
                }
            }

            NoRemoteHostsText().Visibility(
                m_remoteHosts.Size() == 0 ? xaml::Visibility::Visible : xaml::Visibility::Collapsed);
        }
        MIDI_NETSETUP_CATCH_AND_LOG(L"Unable to show the network devices.")
    }


    // ------------------------------------------------------------------------------------
    // page 2: hosts on this PC
    // ------------------------------------------------------------------------------------

    void MainWindow::ApplyLocalHosts(ServiceSnapshot const& snapshot) noexcept
    {
        try
        {
            std::vector<winrt::hstring> seen{};

            if (snapshot.ConfiguredHosts != nullptr)
            {
                for (auto const& host : snapshot.ConfiguredHosts)
                {
                    if (host == nullptr)
                    {
                        continue;
                    }

                    auto const hostKey = EntryKey(host.HostId());

                    seen.push_back(hostKey);

                    midinetworksetup::LocalHostItem item{ nullptr };

                    for (auto const& existing : m_localHosts)
                    {
                        if (existing != nullptr && existing.HostId() == hostKey)
                        {
                            item = existing;
                            break;
                        }
                    }

                    if (item == nullptr)
                    {
                        auto created = winrt::make_self<LocalHostItem>();
                        created->InternalInitialize(hostKey);

                        item = *created;

                        m_localHosts.Append(item);
                    }

                    auto const self = winrt::get_self<LocalHostItem>(item);

                    auto const connections = host.Connections();
                    auto const connectionCount = connections == nullptr ? 0u : connections.Size();

                    self->InternalUpdate(
                        host.UmpEndpointName().empty() ? host.ServiceInstanceName() : host.UmpEndpointName(),
                        // What other devices actually see, which is not the configured name if a
                        // collision made the responder rename it.
                        host.ServiceInstanceNameWasChanged() ?
                            res::FormatString(L"HostServiceInstanceNameChangedFormat",
                                host.ActualServiceInstanceName(), host.ServiceInstanceName()) :
                            host.ServiceInstanceName(),
                        host.ProductInstanceId(),
                        res::FormatString(L"HostAddressValueFormat", DisplayAddressForLocalHost(host.ActualAddress()), host.ActualPort()),
                        host.ActualPort(),
                        host.HasStarted() ?
                            (host.UsedPortFallback() ?
                                res::FormatString(L"HostStartedPortFallbackFormat", host.ActualPort(), host.ConfiguredPort()) :
                                res::FormatString(L"HostStartedFormat", host.ActualPort())) :
                            res::GetString(L"HostStopped"),
                        host.RemoteClientPolicy() == midi2net::MidiNetworkRemoteClientPolicy::RequireApproval ?
                            res::GetString(L"HostPolicyRequireApproval") :
                            res::GetString(L"HostPolicyAllowAny"),
                        connectionCount == 0 ?
                            res::GetString(L"HostNoConnections") :
                            res::FormatString(L"HostConnectionCountFormat", connectionCount),
                        host.HasStarted() ? res::GetString(L"StopHostButton") : res::GetString(L"StartHostButton"),
                        host.HasStarted(),
                        host.CreateMidi1Ports());

                    // connected remote clients
                    std::vector<winrt::hstring> connectionKeys{};

                    if (connections != nullptr)
                    {
                        for (auto const& connection : connections)
                        {
                            if (connection == nullptr)
                            {
                                continue;
                            }

                            auto const connectionKey = Lowered(winrt::hstring{
                                std::wstring{ connection.ProductInstanceId() } + L"|" +
                                std::wstring{ connection.UmpEndpointName() } });

                            connectionKeys.push_back(connectionKey);

                            midinetworksetup::HostConnectionItem connectionItem{ nullptr };

                            for (auto const& existing : self->Connections())
                            {
                                if (existing != nullptr && existing.MatchKey() == connectionKey)
                                {
                                    connectionItem = existing;
                                    break;
                                }
                            }

                            if (connectionItem == nullptr)
                            {
                                auto created = winrt::make_self<HostConnectionItem>();
                                created->InternalInitialize(connectionKey, hostKey, connection.ProductInstanceId());

                                connectionItem = *created;

                                self->Connections().Append(connectionItem);
                            }

                            auto const status = connection.IsPendingApproval() ?
                                res::GetString(L"ConnectionPendingApproval") :
                                (connection.IsSessionActive() ?
                                    res::FormatString(L"ConnectionActiveFormat", connection.RemoteAddress(), connection.RemotePort()) :
                                    res::FormatString(L"ConnectionInactiveFormat", connection.RemoteAddress(), connection.RemotePort()));

                            winrt::get_self<HostConnectionItem>(connectionItem)->InternalUpdate(
                                connection.UmpEndpointName().empty() ?
                                    res::GetString(L"UnnamedDevice") : connection.UmpEndpointName(),
                                res::FormatString(L"AddressesFormat", connection.RemoteAddress()),
                                status,
                                connection.IsSessionActive() ?
                                    res::FormatString(
                                        L"RemoteHostStatisticsFormat",
                                        DescribeLatency(connection.CurrentLatencyTicks()),
                                        FormatCount(connection.TotalCountNetworkPacketsSent()),
                                        FormatCount(connection.TotalCountNetworkPacketsReceived()),
                                        FormatCount(connection.RetransmitCount())) :
                                    winrt::hstring{},
                                connection.CurrentLatencyTicks(),
                                connection.IsSessionActive(),
                                connection.IsPendingApproval());
                        }
                    }

                    for (int32_t i = static_cast<int32_t>(self->Connections().Size()) - 1; i >= 0; i--)
                    {
                        auto const existing = self->Connections().GetAt(static_cast<uint32_t>(i));

                        if (existing == nullptr ||
                            std::find(connectionKeys.begin(), connectionKeys.end(), existing.MatchKey()) == connectionKeys.end())
                        {
                            self->Connections().RemoveAt(static_cast<uint32_t>(i));
                        }
                    }

                    // remembered allow and deny decisions. These change rarely, so the list is
                    // only rebuilt when its contents actually differ.
                    auto const known = snapshot.KnownClients.find(std::wstring{ hostKey });

                    std::vector<::midinetworksetup::KnownClientEntry> knownEntries{};

                    if (known != snapshot.KnownClients.end())
                    {
                        knownEntries = known->second;
                    }

                    bool knownChanged = knownEntries.size() != self->KnownClients().Size();

                    if (!knownChanged)
                    {
                        for (uint32_t i = 0; i < self->KnownClients().Size(); i++)
                        {
                            auto const existing = self->KnownClients().GetAt(i);

                            if (existing == nullptr ||
                                existing.DisplayName() != knownEntries[i].UmpEndpointName ||
                                existing.IsAllowed() != knownEntries[i].Allowed)
                            {
                                knownChanged = true;
                                break;
                            }
                        }
                    }

                    if (knownChanged)
                    {
                        self->KnownClients().Clear();

                        for (auto const& entry : knownEntries)
                        {
                            auto created = winrt::make_self<KnownClientItem>();

                            created->InternalInitialize(
                                Lowered(winrt::hstring{
                                    std::wstring{ entry.ProductInstanceId } + L"|" + std::wstring{ entry.UmpEndpointName } }),
                                hostKey,
                                entry.UmpEndpointName.empty() ? res::GetString(L"UnnamedDevice") : entry.UmpEndpointName,
                                entry.ProductInstanceId,
                                entry.Allowed ? res::GetString(L"KnownClientAllowed") : res::GetString(L"KnownClientBlocked"),
                                entry.Allowed);

                            self->KnownClients().Append(*created);
                        }
                    }
                }
            }

            for (int32_t i = static_cast<int32_t>(m_localHosts.Size()) - 1; i >= 0; i--)
            {
                auto const existing = m_localHosts.GetAt(static_cast<uint32_t>(i));

                if (existing == nullptr ||
                    std::find(seen.begin(), seen.end(), existing.HostId()) == seen.end())
                {
                    m_localHosts.RemoveAt(static_cast<uint32_t>(i));
                }
            }

            NoLocalHostsText().Visibility(
                m_localHosts.Size() == 0 ? xaml::Visibility::Visible : xaml::Visibility::Collapsed);
        }
        MIDI_NETSETUP_CATCH_AND_LOG(L"Unable to show this PC's hosts.")
    }
}
