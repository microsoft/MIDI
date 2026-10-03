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
#include "..\..\Transport\UdpNetworkMidi2Transport\network_json_defs.h"
#include "..\..\Transport\RtpMidiTransport\rtp_json_defs.h"

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

        // What a host card says about its network adapter. A host on every adapter has nothing to
        // warn about. hasStarted tells a host running on every adapter in the meantime from one
        // which is waiting.
        void ApplyHostNetworkAdapter(
            _Inout_ LocalHostItem& row,
            _In_ winrt::guid const& adapterId,
            _In_ winrt::hstring const& adapterName,
            _In_ bool const allowFallback,
            _In_ bool const isMissing,
            _In_ bool const hasStarted) noexcept
        {
            try
            {
                winrt::hstring adapterText{};
                winrt::hstring warningText{};

                if (adapterId == winrt::guid{})
                {
                    adapterText = res::GetString(L"NetworkAdapterEvery");
                }
                else
                {
                    auto const shownName = adapterName.empty() ? res::GetString(L"NetworkAdapterUnknown") : adapterName;

                    adapterText = isMissing ? res::FormatString(L"NetworkAdapterMissingFormat", shownName) : shownName;

                    if (isMissing)
                    {
                        warningText = hasStarted ?
                            res::FormatString(L"NetworkAdapterFallbackWarningFormat", shownName) :
                            res::FormatString(L"NetworkAdapterWaitingWarningFormat", shownName);
                    }
                }

                row.InternalUpdateNetworkAdapter(
                    winrt::hstring{ ::WindowsMidiServicesInternal::MidiNetworkAdapterIdToString(adapterId) },
                    adapterName,
                    allowFallback,
                    adapterText,
                    warningText);
            }
            catch (...)
            {
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

            midiapp::ApplyPreviewBadgeVisibility(PreviewChiclet());

            midiapp::MakeLiveStatusRegion(RemoteStatusText());
            midiapp::MakeLiveStatusRegion(LocalStatusText());
            midiapp::MakeLiveStatusRegion(SettingsStatusText());
            midiapp::MakeLiveStatusRegion(CreateHostStatusText());
            midiapp::MakeLiveStatusRegion(RtpRemoteStatusText());
            midiapp::MakeLiveStatusRegion(RtpLocalStatusText());
            midiapp::MakeLiveStatusRegion(RtpCreateHostStatusText());

            midiapp::WindowChromeElements elements{};

            elements.Window = *this;
            elements.Root = RootGrid();
            elements.Fill = WindowFill();
            elements.Tint = WindowTint();
            elements.TitleBar = AppTitleBar();
            elements.LeftInset = TitleBarLeftInsetColumn();
            elements.RightInset = TitleBarRightInsetColumn();

            m_chrome.Initialize(elements, native::AppSettings::Current());

            // Now that there is a window, a later launch has something to bring forward.
            ::midiapp::SingleInstance::PublishMainWindow(m_chrome.WindowHandle());
            m_chrome.SetWindowIconFromResource(IDI_APPICON);

            // 32px source for a 16px slot, so it stays crisp on a high DPI display
            AppTitleBarIcon().Source(midiapp::WindowChrome::LoadIconImageSource(IDI_APPICON, 32));

            AlwaysOnTopToggle().IsChecked(native::AppSettings::Current().AlwaysOnTop());

            // A PC which has only ever had a transport package installed has no configuration
            // file registered, and without one nothing done here can be persisted.
            midi2svc::MidiServiceTransportPluginConfigManager::EnsureConfigurationFile();

            PendingInvitationsList().ItemsSource(m_pendingInvitations);
            RemoteHostsListView().ItemsSource(m_remoteHosts);
            LocalHostsListView().ItemsSource(m_localHosts);
            RtpRemoteHostsListView().ItemsSource(m_rtpRemoteHosts);
            RtpLocalHostsListView().ItemsSource(m_rtpLocalHosts);

            // the startup options were parsed before the window existed
            auto const& options = App::StartupOptions();

#ifdef _DEBUG
            // Developer switch: the SDK reads and saves this file instead of the PC's own. Release
            // builds of the SDK have no override, so the switch does nothing there.
            if (!options.ConfigFilePath.empty())
            {
                midi2svc::MidiServiceTransportPluginConfigManager::ConfigFilePathOverride(
                    winrt::hstring{ options.ConfigFilePath });
            }
#endif

            // The network transports ship out of band today, and older builds are in the wild.
            // This also decides which pages are offered, so it comes before choosing one.
            auto const transportUsable = VerifyTransportIsUsable();

            if (transportUsable)
            {
                // The pending invitations bar sits above every page, so a notification does not
                // need to navigate anywhere to show it. Landing on this PC is context: it is this
                // PC's hosts the remote is asking to join.
                auto startupPage = options.ShowRtpLocalHosts ?
                    native::AppSettings::PageIndexRtpLocalHosts :
                    (options.ShowPendingApprovals || options.ShowLocalHosts) ?
                        native::AppSettings::PageIndexLocalHosts :
                        native::AppSettings::Current().SelectedPageIndex();

                auto const isRtpPage =
                    startupPage == native::AppSettings::PageIndexRtpRemoteHosts ||
                    startupPage == native::AppSettings::PageIndexRtpLocalHosts;

                // the page saved last time may belong to a transport which is not here now
                if (isRtpPage && !m_rtpUsable)
                {
                    startupPage = native::AppSettings::PageIndexRemoteHosts;
                }
                else if (!isRtpPage && !m_networkMidi2Usable)
                {
                    startupPage = startupPage == native::AppSettings::PageIndexLocalHosts ?
                        native::AppSettings::PageIndexRtpLocalHosts :
                        native::AppSettings::PageIndexRtpRemoteHosts;
                }

                ShowPage(startupPage);

                MainNavigation().SelectedItem(NavigationItemForPage(startupPage));

                // The title bar gear is first in tab order, so focus would otherwise start there.
                if (auto const selected = MainNavigation().SelectedItem().try_as<xaml::UIElement>())
                {
                    selected.Focus(xaml::FocusState::Programmatic);
                }
            }

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

            // Nothing here works without a usable transport, so the app says so and stops rather
            // than failing one operation at a time.
            if (!transportUsable)
            {
                return;
            }

#ifdef _DEBUG
            if (auto const overridePath = midi2svc::MidiServiceTransportPluginConfigManager::ConfigFilePathOverride();
                !overridePath.empty())
            {
                auto const notice = res::FormatString(L"ConfigFileOverrideNotice", overridePath);

                if (m_networkMidi2Usable)
                {
                    SetRemoteStatus(notice);
                }
                else
                {
                    SetRtpRemoteStatus(notice);
                }
            }
#endif

            // RTP-MIDI has no watcher: the service browses all the time, and every refresh asks
            // it what it has found
            if (m_networkMidi2Usable)
            {
                StartWatcher();
            }

            StartRefreshTimer();

            RefreshNotificationsBanner();

            RequestRefreshAsync();
        }
        MIDI_NETSETUP_CATCH_AND_LOG(L"Unable to finish loading the window.")
    }

    // A transport which is missing or too old means none of its pages would work, and hiding them
    // beats a series of individual failures. With neither, one clear message is all there is.
    bool MainWindow::VerifyTransportIsUsable() noexcept
    {
        m_networkMidi2Usable = IsNetworkMidi2TransportUsable();
        m_rtpUsable = IsRtpTransportUsable();

        try
        {
            auto const noTransport = !m_networkMidi2Usable && !m_rtpUsable;

            // With neither, the Network MIDI 2.0 pages stay listed, disabled, under the message.
            auto const networkMidi2Visibility = m_networkMidi2Usable || noTransport ?
                xaml::Visibility::Visible : xaml::Visibility::Collapsed;

            auto const rtpVisibility = m_rtpUsable ?
                xaml::Visibility::Visible : xaml::Visibility::Collapsed;

            // "This PC" means nothing on its own once there are two of them
            NetworkMidi2NavigationHeader().Visibility(m_networkMidi2Usable && m_rtpUsable ?
                xaml::Visibility::Visible : xaml::Visibility::Collapsed);

            RemoteHostsNavigationItem().Visibility(networkMidi2Visibility);
            LocalHostsNavigationItem().Visibility(networkMidi2Visibility);
            SettingsNavigationItem().Visibility(networkMidi2Visibility);

            RtpNavigationHeader().Visibility(rtpVisibility);
            RtpRemoteHostsNavigationItem().Visibility(rtpVisibility);
            RtpLocalHostsNavigationItem().Visibility(rtpVisibility);

            if (noTransport)
            {
                TransportUnavailableBar().IsOpen(true);

                RemoteHostsPanel().Visibility(xaml::Visibility::Collapsed);
                LocalHostsPanel().Visibility(xaml::Visibility::Collapsed);
                SettingsPanel().Visibility(xaml::Visibility::Collapsed);
                RtpRemoteHostsPanel().Visibility(xaml::Visibility::Collapsed);
                RtpLocalHostsPanel().Visibility(xaml::Visibility::Collapsed);
                MainNavigation().IsEnabled(false);

                return false;
            }

            return true;
        }
        catch (...)
        {
            return false;
        }
    }

    // Presence alone is not enough: older builds are still in use, and the verbs this app relies
    // on have to be there too.
    bool MainWindow::IsNetworkMidi2TransportUsable() noexcept
    {
        try
        {
            if (!midi2net::MidiNetworkTransportManager::IsTransportAvailable())
            {
                return false;
            }

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

            for (auto const& capability : requiredCapabilities)
            {
                if (!midi2svc::MidiServiceTransportPluginConfigManager::QueryCapability(transportId, capability))
                {
                    TraceLoggingWrite(
                        MidiNetworkSetupTelemetryProvider::Provider(),
                        MIDI_NETSETUP_TRACE_EVENT_WARNING,
                        TraceLoggingString(__FUNCTION__, MIDI_NETSETUP_TRACE_LOCATION_FIELD),
                        TraceLoggingLevel(WINEVENT_LEVEL_WARNING),
                        TraceLoggingWideString(L"Required network transport capability is missing.", MIDI_NETSETUP_TRACE_MESSAGE_FIELD),
                        TraceLoggingWideString(capability, "capability")
                    );

                    return false;
                }
            }

            return true;
        }
        catch (...)
        {
            return false;
        }
    }

    bool MainWindow::IsRtpTransportUsable() noexcept
    {
        try
        {
            if (!midi2rtp::MidiRtpTransportManager::IsTransportAvailable())
            {
                return false;
            }

            auto const transportId = midi2rtp::MidiRtpTransportManager::TransportId();

            static wchar_t const* const requiredCapabilities[]
            {
                MIDI_CONFIG_JSON_RTP_MIDI_COMMAND_VERB_ENUMERATE_HOSTS,
                MIDI_CONFIG_JSON_RTP_MIDI_COMMAND_VERB_ENUMERATE_CLIENTS,
                MIDI_CONFIG_JSON_RTP_MIDI_COMMAND_VERB_ENUMERATE_ADVERTISED,
                MIDI_CONFIG_JSON_RTP_MIDI_COMMAND_VERB_GET_PENDING_REMOTE_CLIENTS,
            };

            for (auto const& capability : requiredCapabilities)
            {
                if (!midi2svc::MidiServiceTransportPluginConfigManager::QueryCapability(transportId, capability))
                {
                    TraceLoggingWrite(
                        MidiNetworkSetupTelemetryProvider::Provider(),
                        MIDI_NETSETUP_TRACE_EVENT_WARNING,
                        TraceLoggingString(__FUNCTION__, MIDI_NETSETUP_TRACE_LOCATION_FIELD),
                        TraceLoggingLevel(WINEVENT_LEVEL_WARNING),
                        TraceLoggingWideString(L"Required RTP-MIDI transport capability is missing.", MIDI_NETSETUP_TRACE_MESSAGE_FIELD),
                        TraceLoggingWideString(capability, "capability")
                    );

                    return false;
                }
            }

            return true;
        }
        catch (...)
        {
            return false;
        }
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

    // Nothing on this PC watches for a connection request unless the customer has asked for it,
    // so the bar explains what the notifications app does and offers to set it up. It is only
    // ever a report and an offer: this app never starts that app behind them.
    void MainWindow::RefreshNotificationsBanner() noexcept
    {
        try
        {
            if (midiapp::SingleInstance::IsRunning(MIDI_NOTIFICATIONS_INSTANCE_KEY))
            {
                NotificationsBar().IsOpen(false);

                // It has run since, so a later stop is worth mentioning again.
                m_notificationsBannerDismissed = false;

                return;
            }

            // The file probes behind IsSettingsAppAvailable only run on the way up, not on every
            // refresh tick.
            if (m_notificationsBannerDismissed || NotificationsBar().IsOpen())
            {
                return;
            }

            auto const settingsAvailable = midiapp::IsSettingsAppAvailable();

            NotificationsBarButton().Visibility(
                settingsAvailable ? xaml::Visibility::Visible : xaml::Visibility::Collapsed);

            NotificationsBar().Message(res::GetString(
                settingsAvailable ? L"NotificationsBarMessage" : L"NotificationsBarSettingsMissingMessage"));

            NotificationsBar().IsOpen(true);
        }
        MIDI_NETSETUP_CATCH_AND_LOG(L"Unable to show the notifications banner.")
    }

    _Use_decl_annotations_
    void MainWindow::OnNotificationsBarButtonClick(foundation::IInspectable const&, xaml::RoutedEventArgs const&)
    {
        try
        {
            switch (midiapp::ShowSettingsNotifications())
            {
            case midiapp::SettingsAppRequestResult::Shown:
            case midiapp::SettingsAppRequestResult::Declined:
                // The refresh closes the bar by itself once the app is actually running.
                break;

            case midiapp::SettingsAppRequestResult::NotInstalled:
                NotificationsBarButton().Visibility(xaml::Visibility::Collapsed);
                NotificationsBar().Message(res::GetString(L"NotificationsBarSettingsMissingMessage"));
                break;

            default:
                NotificationsBar().Message(res::GetString(L"NotificationsBarLaunchFailedMessage"));
                break;
            }
        }
        MIDI_NETSETUP_CATCH_AND_LOG(L"Unable to open the notification settings.")
    }

    _Use_decl_annotations_
    void MainWindow::OnNotificationsBarCloseClick(controls::InfoBar const&, foundation::IInspectable const&)
    {
        m_notificationsBannerDismissed = true;
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
                tag == L"rtp-remote" ? native::AppSettings::PageIndexRtpRemoteHosts :
                tag == L"rtp-local" ? native::AppSettings::PageIndexRtpLocalHosts :
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

            if (pageIndex == native::AppSettings::PageIndexRtpRemoteHosts)
            {
                return RtpRemoteHostsNavigationItem().as<foundation::IInspectable>();
            }

            if (pageIndex == native::AppSettings::PageIndexRtpLocalHosts)
            {
                return RtpLocalHostsNavigationItem().as<foundation::IInspectable>();
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

            RtpRemoteHostsPanel().Visibility(
                pageIndex == native::AppSettings::PageIndexRtpRemoteHosts ? xaml::Visibility::Visible : xaml::Visibility::Collapsed);

            RtpLocalHostsPanel().Visibility(
                pageIndex == native::AppSettings::PageIndexRtpLocalHosts ? xaml::Visibility::Visible : xaml::Visibility::Collapsed);
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

    void MainWindow::SetRtpRemoteStatus(winrt::hstring const& text) noexcept
    {
        ShowTransientStatus(RtpRemoteStatusText(), m_rtpRemoteStatusTimer, text);
    }

    void MainWindow::SetRtpLocalStatus(winrt::hstring const& text) noexcept
    {
        ShowTransientStatus(RtpLocalStatusText(), m_rtpLocalStatusTimer, text);
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

    _Use_decl_annotations_
    MainWindow::ServiceSnapshot MainWindow::GatherSnapshot(bool const includeNetworkMidi2, bool const includeRtp) noexcept
    {
        ServiceSnapshot snapshot{};

        // Both pages of a transport are gathered even when only one is visible. The latency graphs
        // plot against elapsed time, so a page which stopped sampling while hidden would come back
        // with a flat segment across the gap and read as a bug.
        if (includeNetworkMidi2)
        {
            try
            {
                snapshot.TransportAvailable = midi2net::MidiNetworkTransportManager::IsTransportAvailable();

                if (snapshot.TransportAvailable)
                {
                    snapshot.ConfiguredHosts = midi2net::MidiNetworkTransportManager::GetConfiguredHosts();
                    snapshot.ConfiguredClients = midi2net::MidiNetworkTransportManager::GetConfiguredClients();
                    snapshot.PendingRemoteClients = midi2net::MidiNetworkTransportManager::GetPendingRemoteClients();

                    // What is saved, which is not always what the service is running now
                    for (auto const& saved : midi2net::MidiNetworkTransportManager::GetSavedClients())
                    {
                        if (saved != nullptr)
                        {
                            snapshot.SavedClients.insert_or_assign(std::wstring{ EntryKey(saved.ClientId()) }, saved);
                        }
                    }

                    for (auto const& saved : midi2net::MidiNetworkTransportManager::GetSavedHosts())
                    {
                        if (saved == nullptr)
                        {
                            continue;
                        }

                        std::vector<midi2net::MidiNetworkKnownRemoteClient> known{};

                        if (auto const decisions = saved.KnownRemoteClients())
                        {
                            for (auto const& decision : decisions)
                            {
                                if (decision != nullptr)
                                {
                                    known.push_back(decision);
                                }
                            }
                        }

                        snapshot.KnownClients.insert_or_assign(std::wstring{ EntryKey(saved.HostId()) }, std::move(known));
                    }

                    snapshot.Gathered = true;
                }
            }
            catch (...)
            {
            }
        }

        if (includeRtp)
        {
            try
            {
                snapshot.RtpAvailable = midi2rtp::MidiRtpTransportManager::IsTransportAvailable();

                if (snapshot.RtpAvailable)
                {
                    snapshot.RtpConfiguredHosts = midi2rtp::MidiRtpTransportManager::GetConfiguredHosts();
                    snapshot.RtpConfiguredClients = midi2rtp::MidiRtpTransportManager::GetConfiguredClients();
                    snapshot.RtpPendingRemoteClients = midi2rtp::MidiRtpTransportManager::GetPendingRemoteClients();
                    snapshot.RtpAdvertisedHosts = midi2rtp::MidiRtpTransportManager::GetAdvertisedHosts();

                    snapshot.RtpGathered = true;
                }
            }
            catch (...)
            {
            }
        }

        return snapshot;
    }

    winrt::fire_and_forget MainWindow::RequestRefreshAsync() noexcept
    {
        // GatherSnapshot and the flag below are members, and both are touched after the thread
        // switch, so this has to outlive a close which happens while the gather is running.
        auto strongThis = get_strong();

        if (m_closing || m_refreshInProgress.exchange(true))
        {
            co_return;
        }

        auto weak = get_weak();
        auto queue = DispatcherQueue();

        auto const includeNetworkMidi2 = m_networkMidi2Usable;
        auto const includeRtp = m_rtpUsable;

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

        auto snapshot = GatherSnapshot(includeNetworkMidi2, includeRtp);
        snapshot.AdvertisedHosts = std::move(advertised);

        if (queue == nullptr)
        {
            m_refreshInProgress = false;

            co_return;
        }

        // The flag is cleared in the lambda, so a queue which refuses the work has to clear it
        // here or every later refresh is skipped and the page silently stops updating.
        if (!queue.TryEnqueue([weak, snapshot = std::move(snapshot)]()
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
            }))
        {
            m_refreshInProgress = false;
        }
    }

    void MainWindow::ApplySnapshot(ServiceSnapshot const& snapshot) noexcept
    {
        try
        {
            RefreshNotificationsBanner();

            m_networkMidi2Identities = CollectNetworkMidi2Identities(snapshot);

            if (m_networkMidi2Usable)
            {
                if (!snapshot.TransportAvailable)
                {
                    if (!m_transportMissingReported)
                    {
                        m_transportMissingReported = true;

                        SetRemoteStatus(res::GetString(L"TransportUnavailableError"));
                        SetLocalStatus(res::GetString(L"TransportUnavailableError"));
                    }
                }
                else
                {
                    m_transportMissingReported = false;

                    ApplyLocalHosts(snapshot);
                    ApplyRemoteHosts(snapshot);
                }
            }

            if (m_rtpUsable)
            {
                if (!snapshot.RtpAvailable)
                {
                    if (!m_rtpMissingReported)
                    {
                        m_rtpMissingReported = true;

                        SetRtpRemoteStatus(res::GetString(L"RtpTransportUnavailableError"));
                        SetRtpLocalStatus(res::GetString(L"RtpTransportUnavailableError"));
                    }
                }
                else
                {
                    m_rtpMissingReported = false;

                    ApplyRtpLocalHosts(snapshot);
                    ApplyRtpRemoteHosts(snapshot);
                }
            }

            ApplyPendingInvitations(snapshot);
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
            // RTP-MIDI remotes have no product instance id, and a name is unique only per transport
            auto const keyFor = [](
                bool const isRtpMidi,
                winrt::hstring const& hostKey,
                winrt::hstring const& productInstanceId,
                winrt::hstring const& name)
                {
                    return Lowered(winrt::hstring{
                        std::wstring{ isRtpMidi ? L"rtp|" : L"" } +
                        std::wstring{ hostKey } + L"|" + std::wstring{ productInstanceId } + L"|" + std::wstring{ name } });
                };

            std::vector<winrt::hstring> seen{};

            // an existing row is kept, so an answer in progress keeps its busy state
            auto const rowFor = [&](
                bool const isRtpMidi,
                winrt::hstring const& hostKey,
                winrt::hstring const& productInstanceId,
                winrt::hstring const& name)
                {
                    auto const matchKey = keyFor(isRtpMidi, hostKey, productInstanceId, name);

                    seen.push_back(matchKey);

                    for (auto const& existing : m_pendingInvitations)
                    {
                        auto const self = winrt::get_self<PendingInvitationItem>(existing);

                        if (self != nullptr &&
                            keyFor(self->IsRtpMidi(), self->HostId(), self->RemoteProductInstanceId(), self->RemoteName()) == matchKey)
                        {
                            return existing;
                        }
                    }

                    auto created = winrt::make_self<PendingInvitationItem>();
                    created->InternalInitialize(hostKey, name, productInstanceId, isRtpMidi);

                    midinetworksetup::PendingInvitationItem item = *created;

                    m_pendingInvitations.Append(item);

                    return item;
                };

            if (snapshot.PendingRemoteClients != nullptr)
            {
                for (auto const& pending : snapshot.PendingRemoteClients)
                {
                    if (pending == nullptr)
                    {
                        continue;
                    }

                    auto const name = pending.UmpEndpointName();
                    auto const productInstanceId = pending.ProductInstanceId();

                    auto const item = rowFor(false, EntryKey(pending.HostId()), productInstanceId, name);

                    auto const hostName = pending.HostUmpEndpointName().empty() ?
                        pending.HostServiceInstanceName() : pending.HostUmpEndpointName();

                    auto const displayName = name.empty() ? res::GetString(L"UnnamedDevice") : name;

                    winrt::get_self<PendingInvitationItem>(item)->InternalUpdateText(
                        res::FormatString(L"PendingInvitationHeadlineFormat", displayName, hostName),
                        res::FormatString(L"PendingInvitationDetailFormat", pending.RemoteAddress(), productInstanceId));
                }
            }

            if (snapshot.RtpPendingRemoteClients != nullptr)
            {
                for (auto const& pending : snapshot.RtpPendingRemoteClients)
                {
                    // An approved remote is only waiting to ask again, and offering the same
                    // question a second time would be confusing
                    if (pending == nullptr || pending.IsApproved())
                    {
                        continue;
                    }

                    auto const name = pending.RemoteClientName();

                    auto const item = rowFor(true, EntryKey(pending.HostId()), winrt::hstring{}, name);

                    // a host which is not advertised may have no advertised name to show
                    auto hostName = pending.HostServiceInstanceName();

                    if (snapshot.RtpConfiguredHosts != nullptr)
                    {
                        for (auto const& host : snapshot.RtpConfiguredHosts)
                        {
                            if (host != nullptr && host.HostId() == pending.HostId() && !host.Name().empty())
                            {
                                hostName = host.Name();
                                break;
                            }
                        }
                    }

                    winrt::get_self<PendingInvitationItem>(item)->InternalUpdateText(
                        res::FormatString(
                            L"PendingInvitationHeadlineFormat",
                            name.empty() ? res::GetString(L"UnnamedDevice") : name,
                            hostName),
                        res::FormatString(L"RtpPendingInvitationDetailFormat", pending.RemoteAddress()));
                }
            }

            // Anything the service no longer reports has been answered, here or elsewhere. A
            // transport which could not be read this time keeps its rows until it can.
            for (int32_t i = static_cast<int32_t>(m_pendingInvitations.Size()) - 1; i >= 0; i--)
            {
                auto const existing = m_pendingInvitations.GetAt(static_cast<uint32_t>(i));
                auto const self = winrt::get_self<PendingInvitationItem>(existing);

                if (self == nullptr)
                {
                    m_pendingInvitations.RemoveAt(static_cast<uint32_t>(i));
                    continue;
                }

                if (!(self->IsRtpMidi() ? snapshot.RtpGathered : snapshot.Gathered))
                {
                    continue;
                }

                auto const key = keyFor(self->IsRtpMidi(), self->HostId(), self->RemoteProductInstanceId(), self->RemoteName());

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

            // The host listens on every address, so IPv6 is listed too. IPv4 goes first because it
            // is what almost every device asks for.
            auto addresses = ipv4;

            if (!ipv6.empty())
            {
                addresses += addresses.empty() ? ipv6 : L", " + ipv6;
            }

            return addresses.empty() ? actualAddress : winrt::hstring{ addresses };
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
                winrt::hstring ProductInstanceIdLabel{};
                winrt::hstring Addresses{};
                winrt::hstring DeviceId{};
                winrt::hstring ConnectAddress{};
                uint16_t ConnectPort{ 0 };
                winrt::hstring Status{};
                winrt::hstring Statistics{};
                winrt::hstring EndpointDeviceId{};
                winrt::hstring ImagePath{};
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

                    // The saved configuration is this app's record of which entries are meant to
                    // exist, and an entry the service still reports but the configuration no longer
                    // has is usually one on its way out. A live session is the exception: the
                    // service is the authority on what is actually connected, and hiding a
                    // connection which is passing traffic tells the customer a plain untruth. This
                    // also covers an entry created before it could be saved, or by another tool.
                    auto const saved = snapshot.SavedClients.find(std::wstring{ clientKey });
                    auto const isSaved = saved != snapshot.SavedClients.end();

                    auto const liveInService =
                        client.IsSessionActive() ||
                        client.EntryState() == midi2net::MidiNetworkClientEntryState::Active;

                    if (!isSaved && !liveInService)
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

                        // The name the customer gave the device, or failing that the name it
                        // matches on. The saved UmpEndpointName is the name this PC announces.
                        if (isSaved)
                        {
                            created.DisplayName = saved->second.CustomEndpointName();

                            if (created.DisplayName.empty())
                            {
                                if (auto const criteria = saved->second.MatchCriteria())
                                {
                                    created.DisplayName = criteria.UmpEndpointName();
                                }
                            }
                        }

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

                    // Resolved here rather than in the row type, because it is a file system
                    // lookup and the rows are rebuilt on every poll.
                    if (!row->EndpointDeviceId.empty())
                    {
                        try
                        {
                            auto const info = midi2enum::MidiEndpointDeviceInformation::CreateFromEndpointDeviceId(
                                row->EndpointDeviceId);

                            if (info != nullptr)
                            {
                                if (auto const userInfo = info.GetUserSuppliedInfo())
                                {
                                    row->ImagePath = midiapp::ResolveEndpointImagePath(userInfo.ImageFileName());
                                }
                            }
                        }
                        catch (...)
                        {
                            row->ImagePath = winrt::hstring{};
                        }
                    }

                    row->Connected = client.IsSessionActive();

                    // A device which is not advertising reports nothing, so the row would show an
                    // empty identity and no reason for the entry never matching. The saved entry
                    // still holds what it is looking for, and when a firmware update changes a
                    // device's identity that stale value is the whole explanation.
                    if (row->ProductInstanceId.empty() && isSaved)
                    {
                        auto const criteria = saved->second.MatchCriteria();
                        auto const expected = criteria != nullptr ? criteria.ProductInstanceId() : winrt::hstring{};

                        if (!expected.empty())
                        {
                            row->ProductInstanceId = expected;
                            row->ProductInstanceIdLabel = res::GetString(L"ExpectedProductInstanceIdLabel");
                        }
                    }

                    auto const problem = DescribeNetworkClientProblem(client.LastErrorCode());

                    // "Connecting" is not what is happening when the device cannot be seen at
                    // all. Nothing is attempted until it announces itself, so somebody looking at
                    // a device which is switched off should be told that, rather than watching a
                    // connect which is not being tried.
                    auto const notConnectedStatus = [&client, &row, &problem]()
                        {
                            if (!row->Advertised && client.ConfiguredDirectAddress().empty())
                            {
                                return res::GetString(L"RemoteHostWaitingToAppear");
                            }

                            if (!problem.empty())
                            {
                                return res::FormatString(L"NetworkRemoteHostRetryingFormat", problem);
                            }

                            return row->Advertised ?
                                res::GetString(L"RemoteHostTryingToConnect") :
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
                        row->Status = problem.empty() ?
                            res::GetString(L"RemoteHostFailed") :
                            res::FormatString(L"NetworkRemoteHostStoppedFormat", problem);
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
                    row.ProductInstanceIdLabel.empty() ?
                        res::GetString(L"ProductInstanceIdLabel") :
                        row.ProductInstanceIdLabel,
                    row.Addresses,
                    row.DeviceId,
                    row.ConnectAddress,
                    row.ConnectPort,
                    row.Status,
                    row.Statistics,
                    row.EndpointDeviceId,
                    row.ImagePath,
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

                    // A host waiting for its adapter is still switched on, so it offers Stop
                    auto const waitingForAdapter = !host.HasStarted() && host.IsNetworkAdapterMissing();

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
                            (waitingForAdapter ? res::GetString(L"HostWaitingForNetworkAdapter") : res::GetString(L"HostStopped")),
                        host.RemoteClientPolicy() == midi2net::MidiNetworkRemoteClientPolicy::RequireApproval ?
                            res::GetString(L"HostPolicyRequireApproval") :
                            res::GetString(L"HostPolicyAllowAny"),
                        connectionCount == 0 ?
                            res::GetString(L"HostNoConnections") :
                            res::FormatString(L"HostConnectionCountFormat", connectionCount),
                        host.HasStarted() || waitingForAdapter ? res::GetString(L"StopHostButton") : res::GetString(L"StartHostButton"),
                        host.HasStarted() || waitingForAdapter,
                        host.CreateMidi1Ports());

                    ApplyHostNetworkAdapter(
                        *self,
                        host.NetworkAdapterId(),
                        host.NetworkAdapterName(),
                        host.AllowNetworkAdapterFallback(),
                        host.IsNetworkAdapterMissing(),
                        host.HasStarted());

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
                                connection.EndpointDeviceId(),
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

                    std::vector<midi2net::MidiNetworkKnownRemoteClient> knownEntries{};

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
                                existing.DisplayName() != knownEntries[i].RemoteClientName() ||
                                existing.IsAllowed() != knownEntries[i].IsAllowed())
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
                            auto const knownName = entry.RemoteClientName();
                            auto const knownProductInstanceId = entry.RemoteClientProductInstanceId();
                            auto const knownAllowed = entry.IsAllowed();

                            auto created = winrt::make_self<KnownClientItem>();

                            created->InternalInitialize(
                                Lowered(winrt::hstring{
                                    std::wstring{ knownProductInstanceId } + L"|" + std::wstring{ knownName } }),
                                hostKey,
                                knownName.empty() ? res::GetString(L"UnnamedDevice") : knownName,
                                knownProductInstanceId,
                                knownAllowed ? res::GetString(L"KnownClientAllowed") : res::GetString(L"KnownClientBlocked"),
                                knownAllowed);

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


    // ------------------------------------------------------------------------------------
    // RTP-MIDI
    // ------------------------------------------------------------------------------------

    namespace
    {
        // Some answers end a host name with a dot and some do not, and a link local IPv6 address
        // may carry an interface after a percent sign. Neither says anything about the machine.
        std::wstring MachineIdentity(_In_ winrt::hstring const& hostNameOrAddress) noexcept
        {
            try
            {
                std::wstring value{ Lowered(TrimmedText(hostNameOrAddress)) };

                if (auto const scope = value.find(L'%'); scope != std::wstring::npos)
                {
                    value.resize(scope);
                }

                while (!value.empty() && value.back() == L'.')
                {
                    value.pop_back();
                }

                return value;
            }
            catch (...)
            {
                return {};
            }
        }
    }

    _Use_decl_annotations_
    std::vector<std::wstring> MainWindow::CollectNetworkMidi2Identities(ServiceSnapshot const& snapshot) noexcept
    {
        std::vector<std::wstring> identities{};

        try
        {
            auto const add = [&identities](winrt::hstring const& value)
                {
                    auto identity = MachineIdentity(value);

                    if (!identity.empty() &&
                        std::find(identities.begin(), identities.end(), identity) == identities.end())
                    {
                        identities.push_back(std::move(identity));
                    }
                };

            for (auto const& host : snapshot.AdvertisedHosts)
            {
                if (host == nullptr)
                {
                    continue;
                }

                add(host.HostName());

                if (auto const addresses = host.IPAddresses())
                {
                    for (auto const& address : addresses)
                    {
                        add(address);
                    }
                }
            }

            // a device reached by address may not advertise at all
            if (snapshot.ConfiguredClients != nullptr)
            {
                for (auto const& client : snapshot.ConfiguredClients)
                {
                    if (client != nullptr)
                    {
                        add(client.ConfiguredDirectAddress());
                        add(client.ConnectedRemoteAddress());
                    }
                }
            }
        }
        catch (...)
        {
        }

        return identities;
    }

    _Use_decl_annotations_
    bool MainWindow::IsNetworkMidi2Machine(winrt::hstring const& hostNameOrAddress) const noexcept
    {
        auto const identity = MachineIdentity(hostNameOrAddress);

        return !identity.empty() &&
            std::find(m_networkMidi2Identities.begin(), m_networkMidi2Identities.end(), identity) != m_networkMidi2Identities.end();
    }

    _Use_decl_annotations_
    bool MainWindow::IsNetworkMidi2Machine(
        winrt::hstring const& hostName,
        collections::IVectorView<winrt::hstring> const& addresses) const noexcept
    {
        if (IsNetworkMidi2Machine(hostName))
        {
            return true;
        }

        try
        {
            if (addresses != nullptr)
            {
                for (auto const& address : addresses)
                {
                    if (IsNetworkMidi2Machine(address))
                    {
                        return true;
                    }
                }
            }
        }
        catch (...)
        {
        }

        return false;
    }

    _Use_decl_annotations_
    winrt::hstring MainWindow::DescribeNetworkClientProblem(midi2net::MidiNetworkClientConnectErrorCode const lastErrorCode) noexcept
    {
        switch (lastErrorCode)
        {
        case midi2net::MidiNetworkClientConnectErrorCode::NoErrorInformationAvailable:
            return {};

        case midi2net::MidiNetworkClientConnectErrorCode::NoReplyToInvitation:
            return res::GetString(L"NetworkRemoteHostNoAnswer");

        case midi2net::MidiNetworkClientConnectErrorCode::InvitationNotApproved:
            return res::GetString(L"NetworkRemoteHostNotApproved");

        case midi2net::MidiNetworkClientConnectErrorCode::HostBusy:
            return res::GetString(L"NetworkRemoteHostBusy");

        case midi2net::MidiNetworkClientConnectErrorCode::InvitationRefused:
            return res::GetString(L"NetworkRemoteHostRefused");

        case midi2net::MidiNetworkClientConnectErrorCode::AuthenticationRequired:
            return res::GetString(L"NetworkRemoteHostNeedsPassword");

        case midi2net::MidiNetworkClientConnectErrorCode::InvitationEndedByHost:
            return res::GetString(L"NetworkRemoteHostEnded");

        default:
            return res::GetString(L"RemoteHostFailed");
        }
    }

    _Use_decl_annotations_
    winrt::hstring MainWindow::DescribeRtpClientProblem(int32_t const lastErrorCode) noexcept
    {
        auto const hr = static_cast<HRESULT>(lastErrorCode);

        if (hr == S_OK)
        {
            return {};
        }

        if (hr == HRESULT_FROM_WIN32(ERROR_HOST_UNREACHABLE))
        {
            return res::GetString(L"RtpRemoteHostNotFound");
        }

        if (hr == HRESULT_FROM_WIN32(ERROR_TIMEOUT))
        {
            return res::GetString(L"RtpRemoteHostNoAnswer");
        }

        if (hr == E_ACCESSDENIED)
        {
            return res::GetString(L"RtpRemoteHostRefused");
        }

        if (hr == HRESULT_FROM_WIN32(ERROR_GRACEFUL_DISCONNECT))
        {
            return res::GetString(L"RtpRemoteHostEnded");
        }

        if (hr == HRESULT_FROM_WIN32(ERROR_CONNECTION_ABORTED))
        {
            return res::GetString(L"RtpRemoteHostLost");
        }

        return res::GetString(L"RemoteHostFailed");
    }

    void MainWindow::ApplyRtpRemoteHosts(ServiceSnapshot const& snapshot) noexcept
    {
        try
        {
            struct RowData
            {
                winrt::hstring Key{};
                winrt::hstring DisplayName{};
                winrt::hstring Subtitle{};
                winrt::hstring HostName{};
                winrt::hstring Addresses{};
                winrt::hstring ServiceInstanceName{};
                winrt::hstring ConnectAddress{};
                uint16_t ConnectPort{ 0 };
                winrt::hstring Status{};
                winrt::hstring Statistics{};
                winrt::hstring EndpointDeviceId{};
                winrt::hstring ImagePath{};
                winrt::hstring ClientId{};
                uint64_t LatencyTicks{ 0 };
                bool Connected{ false };
                bool Configured{ false };
                bool Advertised{ false };
                bool AlsoNetworkMidi2{ false };
            };

            std::vector<RowData> rows{};

            // address and port of every advertised session, so an entry made by address still
            // lands on the row for that session
            std::unordered_map<std::wstring, winrt::hstring> addressToKey{};

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

            if (snapshot.RtpAdvertisedHosts != nullptr)
            {
                for (auto const& host : snapshot.RtpAdvertisedHosts)
                {
                    // offering to connect to yourself is only confusing
                    if (host == nullptr || host.IsThisPc() || host.ServiceInstanceName().empty())
                    {
                        continue;
                    }

                    auto const key = Lowered(winrt::hstring{ L"s:" + std::wstring{ host.ServiceInstanceName() } });

                    if (findRow(key) != nullptr)
                    {
                        continue;
                    }

                    auto const addresses = host.IPAddresses();

                    auto const where = !host.HostName().empty() ?
                        host.HostName() :
                        (addresses != nullptr && addresses.Size() > 0 ? addresses.GetAt(0) : winrt::hstring{});

                    RowData row{};

                    row.Key = key;
                    row.DisplayName = host.ServiceInstanceName();
                    row.ServiceInstanceName = host.ServiceInstanceName();
                    row.HostName = host.HostName();
                    row.Addresses = JoinAddresses(addresses);
                    row.Subtitle = where.empty() ?
                        winrt::hstring{} :
                        res::FormatString(L"RemoteHostSubtitleFormat", where, host.Port());
                    row.Status = res::GetString(L"RemoteHostAvailable");
                    row.ConnectAddress = where;
                    row.ConnectPort = host.Port();
                    row.Advertised = true;
                    row.AlsoNetworkMidi2 = IsNetworkMidi2Machine(host.HostName(), addresses);

                    auto const portSuffix = L"|" + std::to_wstring(host.Port());

                    if (addresses != nullptr)
                    {
                        for (auto const& address : addresses)
                        {
                            addressToKey.insert_or_assign(MachineIdentity(address) + portSuffix, row.Key);
                        }
                    }

                    if (!host.HostName().empty())
                    {
                        addressToKey.insert_or_assign(MachineIdentity(host.HostName()) + portSuffix, row.Key);
                    }

                    rows.push_back(row);
                }
            }

            if (snapshot.RtpConfiguredClients != nullptr)
            {
                for (auto const& client : snapshot.RtpConfiguredClients)
                {
                    if (client == nullptr)
                    {
                        continue;
                    }

                    auto const clientKey = EntryKey(client.ClientId());
                    auto const ownKey = Lowered(winrt::hstring{ L"c:" + std::wstring{ clientKey } });

                    auto const connection = client.Connection();
                    auto const isConnected = connection != nullptr && connection.IsConnected();

                    winrt::hstring matchKey{ ownKey };

                    if (!client.IsDirectConnection() && !client.RemoteServiceInstanceName().empty())
                    {
                        matchKey = Lowered(winrt::hstring{ L"s:" + std::wstring{ client.RemoteServiceInstanceName() } });
                    }
                    else
                    {
                        auto const configured = addressToKey.find(
                            MachineIdentity(client.ConfiguredDirectAddress()) + L"|" + std::to_wstring(client.ConfiguredDirectPort()));

                        if (configured != addressToKey.end())
                        {
                            matchKey = configured->second;
                        }
                    }

                    auto row = findRow(matchKey);

                    // a second saved entry for the same device keeps a row of its own, so it can
                    // still be seen and forgotten
                    if (row != nullptr && row->Configured)
                    {
                        matchKey = ownKey;
                        row = nullptr;
                    }

                    if (row == nullptr)
                    {
                        RowData created{};

                        created.Key = matchKey;
                        created.ServiceInstanceName = client.RemoteServiceInstanceName();
                        created.DisplayName = client.RemoteServiceInstanceName().empty() ?
                            client.ConfiguredDirectAddress() :
                            client.RemoteServiceInstanceName();
                        created.Subtitle = client.IsDirectConnection() ?
                            res::FormatString(
                                L"RemoteHostDirectSubtitleFormat",
                                client.ConfiguredDirectAddress(),
                                client.ConfiguredDirectPort()) :
                            res::GetString(L"RemoteHostNotFound");
                        created.Addresses = client.ConfiguredDirectAddress();
                        created.ConnectAddress = client.ConfiguredDirectAddress();
                        created.ConnectPort = client.ConfiguredDirectPort();
                        created.AlsoNetworkMidi2 = IsNetworkMidi2Machine(client.ConfiguredDirectAddress());

                        rows.push_back(created);

                        row = &rows.back();
                    }

                    row->Configured = true;
                    row->ClientId = clientKey;
                    row->Connected = isConnected;

                    // As on the Network MIDI 2.0 page, an advertised device keeps its advertised
                    // name. Anything else shows the best name there is.
                    if (!row->Advertised)
                    {
                        if (!client.CustomEndpointName().empty())
                        {
                            row->DisplayName = client.CustomEndpointName();
                        }
                        else if (connection != nullptr && !connection.RemoteName().empty())
                        {
                            row->DisplayName = connection.RemoteName();
                        }
                    }

                    if (row->DisplayName.empty())
                    {
                        row->DisplayName = res::GetString(L"UnnamedDevice");
                    }

                    if (connection != nullptr)
                    {
                        row->EndpointDeviceId = connection.EndpointDeviceId();

                        // an address typed in says nothing about the machine, but its connection does
                        row->AlsoNetworkMidi2 = row->AlsoNetworkMidi2 ||
                            IsNetworkMidi2Machine(connection.RemoteHostName()) ||
                            IsNetworkMidi2Machine(connection.RemoteAddress());
                    }

                    // Resolved here rather than in the row type, because it is a file system
                    // lookup and the rows are rebuilt on every poll.
                    if (!row->EndpointDeviceId.empty())
                    {
                        try
                        {
                            auto const info = midi2enum::MidiEndpointDeviceInformation::CreateFromEndpointDeviceId(
                                row->EndpointDeviceId);

                            if (info != nullptr)
                            {
                                if (auto const userInfo = info.GetUserSuppliedInfo())
                                {
                                    row->ImagePath = midiapp::ResolveEndpointImagePath(userInfo.ImageFileName());
                                }
                            }
                        }
                        catch (...)
                        {
                            row->ImagePath = winrt::hstring{};
                        }
                    }

                    auto const problem = DescribeRtpClientProblem(client.LastErrorCode());

                    switch (client.EntryState())
                    {
                    case midi2rtp::MidiRtpClientEntryState::Active:
                        row->Status = isConnected ?
                            res::GetString(L"RemoteHostConnected") :
                            res::GetString(L"RemoteHostTryingToConnect");
                        break;

                    case midi2rtp::MidiRtpClientEntryState::Retrying:
                        row->Status = problem.empty() ?
                            res::GetString(L"RemoteHostTryingToConnect") :
                            res::FormatString(L"RtpRemoteHostRetryingFormat", problem);
                        break;

                    case midi2rtp::MidiRtpClientEntryState::Unavailable:
                        row->Status = problem.empty() ?
                            res::GetString(L"RemoteHostUnavailable") :
                            res::FormatString(L"RtpRemoteHostStoppedFormat", problem);
                        break;

                    default:
                        // Nothing is attempted for a device which cannot be seen, so somebody
                        // looking at one which is switched off is told that instead.
                        if (row->Advertised)
                        {
                            row->Status = res::GetString(L"RemoteHostTryingToConnect");
                        }
                        else if (!client.IsDirectConnection())
                        {
                            row->Status = res::GetString(L"RemoteHostWaitingToAppear");
                        }
                        else
                        {
                            row->Status = problem.empty() ?
                                res::GetString(L"RemoteHostWaitingToAnswer") :
                                res::FormatString(L"RtpRemoteHostRetryingFormat", problem);
                        }
                        break;
                    }

                    if (isConnected)
                    {
                        row->LatencyTicks = connection.CurrentLatencyTicks();

                        row->Statistics = res::FormatString(
                            L"RtpStatisticsFormat",
                            DescribeLatency(connection.CurrentLatencyTicks()),
                            FormatCount(connection.TotalCountNetworkPacketsSent()),
                            FormatCount(connection.TotalCountNetworkPacketsReceived()),
                            FormatCount(connection.TotalCountPacketsLost()),
                            FormatCount(connection.TotalCountLossesRepairedFromJournal()));

                        if (!connection.RemoteAddress().empty())
                        {
                            row->Subtitle = res::FormatString(
                                L"RemoteHostDirectSubtitleFormat",
                                connection.RemoteAddress(),
                                connection.RemotePort());
                        }
                    }
                    else
                    {
                        row->Statistics = winrt::hstring{};
                    }
                }
            }

            // discovery reports sessions in whatever order they answered, so without this the
            // rows shuffle on every refresh
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
                midinetworksetup::RtpRemoteHostItem item{ nullptr };

                for (auto const& existing : m_rtpRemoteHosts)
                {
                    if (existing != nullptr && existing.MatchKey() == row.Key)
                    {
                        item = existing;
                        break;
                    }
                }

                if (item == nullptr)
                {
                    auto created = winrt::make_self<RtpRemoteHostItem>();
                    created->InternalInitialize(row.Key);

                    item = *created;

                    m_rtpRemoteHosts.Append(item);
                }

                winrt::get_self<RtpRemoteHostItem>(item)->InternalUpdate(
                    row.DisplayName,
                    row.Subtitle,
                    row.HostName,
                    row.Addresses,
                    row.ServiceInstanceName,
                    row.ConnectAddress,
                    row.ConnectPort,
                    row.Status,
                    row.Statistics,
                    row.EndpointDeviceId,
                    row.ImagePath,
                    row.ClientId,
                    row.LatencyTicks,
                    row.Connected,
                    row.Configured,
                    row.Advertised,
                    row.AlsoNetworkMidi2,
                    row.Connected ?
                        res::GetString(L"RemoteHostDisconnectAndForgetLabel") :
                        res::GetString(L"RemoteHostForgetLabel"));
            }

            for (int32_t i = static_cast<int32_t>(m_rtpRemoteHosts.Size()) - 1; i >= 0; i--)
            {
                auto const existing = m_rtpRemoteHosts.GetAt(static_cast<uint32_t>(i));

                auto const stillThere = existing != nullptr && std::any_of(
                    rows.begin(),
                    rows.end(),
                    [&existing](RowData const& row) { return row.Key == existing.MatchKey(); });

                if (!stillThere)
                {
                    m_rtpRemoteHosts.RemoveAt(static_cast<uint32_t>(i));
                }
            }

            // put the rows into the sorted order without rebuilding the collection
            for (uint32_t target = 0; target < rows.size() && target < m_rtpRemoteHosts.Size(); target++)
            {
                if (m_rtpRemoteHosts.GetAt(target).MatchKey() == rows[target].Key)
                {
                    continue;
                }

                for (uint32_t search = target + 1; search < m_rtpRemoteHosts.Size(); search++)
                {
                    if (m_rtpRemoteHosts.GetAt(search).MatchKey() == rows[target].Key)
                    {
                        auto const moved = m_rtpRemoteHosts.GetAt(search);

                        m_rtpRemoteHosts.RemoveAt(search);
                        m_rtpRemoteHosts.InsertAt(target, moved);

                        break;
                    }
                }
            }

            NoRtpRemoteHostsText().Visibility(
                m_rtpRemoteHosts.Size() == 0 ? xaml::Visibility::Visible : xaml::Visibility::Collapsed);
        }
        MIDI_NETSETUP_CATCH_AND_LOG(L"Unable to show the RTP-MIDI devices.")
    }

    void MainWindow::ApplyRtpLocalHosts(ServiceSnapshot const& snapshot) noexcept
    {
        try
        {
            std::vector<winrt::hstring> seen{};

            if (snapshot.RtpConfiguredHosts != nullptr)
            {
                for (auto const& host : snapshot.RtpConfiguredHosts)
                {
                    if (host == nullptr)
                    {
                        continue;
                    }

                    auto const hostKey = EntryKey(host.HostId());

                    seen.push_back(hostKey);

                    midinetworksetup::LocalHostItem item{ nullptr };

                    for (auto const& existing : m_rtpLocalHosts)
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

                        m_rtpLocalHosts.Append(item);
                    }

                    auto const self = winrt::get_self<LocalHostItem>(item);

                    auto const connections = host.Connections();
                    auto const connectionCount = connections == nullptr ? 0u : connections.Size();

                    // what other devices actually see, which is not the configured name if a
                    // collision made the responder rename it
                    auto const advertisedName = host.ActualServiceInstanceName().empty() ?
                        host.ServiceInstanceName() :
                        host.ActualServiceInstanceName();

                    auto displayName = host.Name();

                    if (displayName.empty())
                    {
                        displayName = advertisedName.empty() ? res::GetString(L"RtpHostDefaultName") : advertisedName;
                    }

                    winrt::hstring status{};

                    if (!host.IsEnabled())
                    {
                        status = res::GetString(L"HostStopped");
                    }
                    else if (host.HasStarted())
                    {
                        // an automatic host wanted the default port
                        auto const wantedPort = host.ConfiguredPort() == MIDI_CONFIG_JSON_RTP_MIDI_PORT_VALUE_AUTO ?
                            winrt::to_hstring(midi2rtp::MidiRtpTransportManager::DefaultHostPort()) :
                            host.ConfiguredPort();

                        status = host.UsedPortFallback() ?
                            res::FormatString(L"HostStartedPortFallbackFormat", host.ActualPort(), wantedPort) :
                            res::FormatString(L"HostStartedFormat", host.ActualPort());
                    }
                    else if (host.IsNetworkAdapterMissing())
                    {
                        status = res::GetString(L"HostWaitingForNetworkAdapter");
                    }
                    else if (host.LastErrorCode() != 0)
                    {
                        status = res::FormatString(
                            L"RtpHostNotStartedFormat",
                            std::format(L"0x{:08X}", static_cast<uint32_t>(host.LastErrorCode())));
                    }
                    else
                    {
                        status = res::GetString(L"RtpHostStarting");
                    }

                    auto const address = DisplayAddressForLocalHost(winrt::hstring{});

                    self->InternalUpdate(
                        displayName,
                        !host.Advertise() ?
                            res::GetString(L"RtpHostNotAdvertised") :
                            (host.ServiceInstanceNameWasChanged() ?
                                res::FormatString(L"HostServiceInstanceNameChangedFormat",
                                    host.ActualServiceInstanceName(), host.ServiceInstanceName()) :
                                advertisedName),
                        winrt::hstring{},
                        host.ActualPort() == 0 ?
                            address :
                            res::FormatString(L"HostAddressValueFormat", address, host.ActualPort()),
                        FormatCount(host.ActualPort()),
                        status,
                        host.RemoteClientPolicy() == midi2rtp::MidiRtpRemoteClientPolicy::RequireApproval ?
                            res::GetString(L"HostPolicyRequireApproval") :
                            res::GetString(L"HostPolicyAllowAny"),
                        connectionCount == 0 ?
                            res::GetString(L"HostNoConnections") :
                            res::FormatString(L"HostConnectionCountFormat", connectionCount),
                        // An enabled host which has not started is still being tried, so it
                        // offers Stop, and the start and stop handler reads this as its state.
                        host.IsEnabled() ? res::GetString(L"StopHostButton") : res::GetString(L"StartHostButton"),
                        host.IsEnabled(),
                        true);

                    ApplyHostNetworkAdapter(
                        *self,
                        host.NetworkAdapterId(),
                        host.NetworkAdapterName(),
                        host.AllowNetworkAdapterFallback(),
                        host.IsNetworkAdapterMissing(),
                        host.HasStarted());

                    std::vector<winrt::hstring> connectionKeys{};

                    if (connections != nullptr)
                    {
                        for (auto const& connection : connections)
                        {
                            if (connection == nullptr)
                            {
                                continue;
                            }

                            // two remotes can send the same name, so the service's id tells them apart
                            auto const connectionKey = winrt::hstring{ L"rtp|" + std::to_wstring(connection.ConnectionId()) };

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
                                created->InternalInitialize(
                                    connectionKey, hostKey, winrt::hstring{}, connection.ConnectionId(), connection.RemoteName());

                                connectionItem = *created;

                                self->Connections().Append(connectionItem);
                            }

                            winrt::get_self<HostConnectionItem>(connectionItem)->InternalUpdate(
                                connection.RemoteName().empty() ?
                                    res::GetString(L"UnnamedDevice") : connection.RemoteName(),
                                res::FormatString(L"AddressesFormat", connection.RemoteAddress()),
                                connection.IsConnected() ?
                                    res::FormatString(L"ConnectionActiveFormat", connection.RemoteAddress(), connection.RemotePort()) :
                                    res::FormatString(L"RtpConnectionConnectingFormat", connection.RemoteAddress(), connection.RemotePort()),
                                connection.IsConnected() ?
                                    res::FormatString(
                                        L"RtpStatisticsFormat",
                                        DescribeLatency(connection.CurrentLatencyTicks()),
                                        FormatCount(connection.TotalCountNetworkPacketsSent()),
                                        FormatCount(connection.TotalCountNetworkPacketsReceived()),
                                        FormatCount(connection.TotalCountPacketsLost()),
                                        FormatCount(connection.TotalCountLossesRepairedFromJournal())) :
                                    winrt::hstring{},
                                connection.EndpointDeviceId(),
                                connection.CurrentLatencyTicks(),
                                connection.IsConnected(),
                                false);
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
                    std::vector<std::pair<winrt::hstring, bool>> knownEntries{};

                    if (auto const known = host.KnownRemoteClients())
                    {
                        for (auto const& entry : known)
                        {
                            if (entry != nullptr && !entry.RemoteClientName().empty())
                            {
                                knownEntries.emplace_back(entry.RemoteClientName(), entry.IsAllowed());
                            }
                        }
                    }

                    bool knownChanged = knownEntries.size() != self->KnownClients().Size();

                    if (!knownChanged)
                    {
                        for (uint32_t i = 0; i < self->KnownClients().Size(); i++)
                        {
                            auto const existing = self->KnownClients().GetAt(i);

                            if (existing == nullptr ||
                                existing.DisplayName() != knownEntries[i].first ||
                                existing.IsAllowed() != knownEntries[i].second)
                            {
                                knownChanged = true;
                                break;
                            }
                        }
                    }

                    if (knownChanged)
                    {
                        self->KnownClients().Clear();

                        for (auto const& [name, allowed] : knownEntries)
                        {
                            auto created = winrt::make_self<KnownClientItem>();

                            created->InternalInitialize(
                                Lowered(winrt::hstring{ L"rtp|" + std::wstring{ name } }),
                                hostKey,
                                name,
                                winrt::hstring{},
                                allowed ? res::GetString(L"KnownClientAllowed") : res::GetString(L"KnownClientBlocked"),
                                allowed);

                            self->KnownClients().Append(*created);
                        }
                    }
                }
            }

            for (int32_t i = static_cast<int32_t>(m_rtpLocalHosts.Size()) - 1; i >= 0; i--)
            {
                auto const existing = m_rtpLocalHosts.GetAt(static_cast<uint32_t>(i));

                if (existing == nullptr ||
                    std::find(seen.begin(), seen.end(), existing.HostId()) == seen.end())
                {
                    m_rtpLocalHosts.RemoveAt(static_cast<uint32_t>(i));
                }
            }

            NoRtpLocalHostsText().Visibility(
                m_rtpLocalHosts.Size() == 0 ? xaml::Visibility::Visible : xaml::Visibility::Collapsed);
        }
        MIDI_NETSETUP_CATCH_AND_LOG(L"Unable to show this PC's RTP-MIDI hosts.")
    }
}
