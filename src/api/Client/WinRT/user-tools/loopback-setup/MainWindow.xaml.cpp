// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================
//
// Window chrome, the poll which keeps both pages honest, and the folding of what the service
// reports into the rows on screen. Everything the customer can act on lives in
// MainWindowActions.cpp.

#include "pch.h"
#include "MainWindow.xaml.h"
#include "MainWindow.g.cpp"

#include "App.xaml.h"
#include "StringResources.h"
#include "resource.h"

// Capability key names, so a build of the transport without mute or list support is detected
// rather than failing one operation at a time. Pure preprocessor defines; the SDK includes the
// same header.
#include "json_defs.h"

#include <winrt/Microsoft.UI.Xaml.Media.Animation.h>

namespace native = ::midiloopbacksetup;
namespace res = ::midiloopbacksetup::resources;
namespace animation = ::winrt::Microsoft::UI::Xaml::Media::Animation;

namespace winrt::midiloopbacksetup::implementation
{
    namespace
    {
        // Long enough to read, short enough that it is gone before it becomes untrue.
        constexpr std::chrono::seconds StatusMessageLifetime{ 8 };

        // sorts after every real position, so an entry with no stored position lands at the end
        constexpr int32_t UnorderedPosition = std::numeric_limits<int32_t>::max();

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

        // The association id is the row's identity. It travels through the UI as the unbraced
        // lowercase form, which is also what the configuration file entries normalize to.
        // to_hstring produces the braced form, so the braces come back off here.
        winrt::hstring AssociationKey(_In_ winrt::guid const& value) noexcept
        {
            try
            {
                std::wstring text{ Lowered(winrt::to_hstring(value)) };

                if (text.size() >= 2 && text.front() == L'{' && text.back() == L'}')
                {
                    text = text.substr(1, text.size() - 2);
                }

                return winrt::hstring{ text };
            }
            catch (...)
            {
                return {};
            }
        }

        bool TryParseAssociationKey(_In_ winrt::hstring const& text, _Out_ winrt::guid& value) noexcept
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

        bool Contains(_In_ std::vector<std::wstring> const& values, _In_ winrt::hstring const& value) noexcept
        {
            return std::find(values.begin(), values.end(), std::wstring{ value }) != values.end();
        }

        // "My Loopback (A)" and "My Loopback (B)" are one thing to the customer, so the card is
        // headed with the shared part when there is one, and with both names when the two sides
        // were named separately.
        winrt::hstring PairDisplayName(
            _In_ winrt::hstring const& nameA,
            _In_ winrt::hstring const& nameB) noexcept
        {
            try
            {
                std::wstring const a{ nameA };
                std::wstring const b{ nameB };

                if (a.empty() && b.empty())
                {
                    return res::GetString(L"UnnamedLoopback");
                }

                if (a.empty()) return nameB;
                if (b.empty()) return nameA;

                if (a == b)
                {
                    return nameA;
                }

                constexpr std::wstring_view suffixA{ L" (A)" };
                constexpr std::wstring_view suffixB{ L" (B)" };

                if (a.size() > suffixA.size() && b.size() > suffixB.size() &&
                    a.ends_with(suffixA) && b.ends_with(suffixB))
                {
                    auto const rootA = a.substr(0, a.size() - suffixA.size());
                    auto const rootB = b.substr(0, b.size() - suffixB.size());

                    if (rootA == rootB)
                    {
                        return winrt::hstring{ rootA };
                    }
                }

                return res::FormatString(L"LoopbackPairNameFormat", nameA, nameB);
            }
            catch (...)
            {
                return nameA;
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
        midiapp::WindowChrome::RestorePlacement(*this, native::AppSettings::Current(), 1080, 820);
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

            midiapp::EmbeddedAssets::SetSvgFromResource(BasicLoopbackDiagram(), IDR_BASIC_LOOPBACK_DIAGRAM);
            midiapp::EmbeddedAssets::SetSvgFromResource(LoopbackDiagram(), IDR_LOOPBACK_DIAGRAM);

            AlwaysOnTopToggle().IsChecked(native::AppSettings::Current().AlwaysOnTop());

            LoopbacksListView().ItemsSource(m_loopbacks);
            BasicLoopbacksListView().ItemsSource(m_basicLoopbacks);

            // the startup options were parsed before the window existed
            auto const& options = App::StartupOptions();

            if (!options.ConfigFilePath.empty())
            {
                native::LoopbackConfigFile::Current().OverridePath(options.ConfigFilePath);
            }

            ShowLoopbackPage(
                native::AppSettings::Current().SelectedPageIndex() != native::AppSettings::PageIndexBasicLoopbacks);

            MainNavigation().SelectedItem(
                native::AppSettings::Current().SelectedPageIndex() == native::AppSettings::PageIndexBasicLoopbacks ?
                BasicLoopbacksNavigationItem().as<foundation::IInspectable>() :
                LoopbacksNavigationItem().as<foundation::IInspectable>());

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

            if (native::LoopbackConfigFile::Current().IsOverridden())
            {
                auto const notice = res::FormatString(
                    L"ConfigFileOverrideNotice",
                    native::LoopbackConfigFile::Current().Path());

                SetLoopbackStatus(notice);
                SetBasicLoopbackStatus(notice);
            }

            StartRefreshTimer();

            RequestRefreshAsync();
        }
        MIDI_LOOPSETUP_CATCH_AND_LOG(L"Unable to finish loading the window.")
    }

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
            auto const isChecked = AlwaysOnTopToggle().IsChecked();

            native::AppSettings::Current().AlwaysOnTop(isChecked != nullptr && isChecked.Value());

            m_chrome.ApplyAlwaysOnTop();
        }
        MIDI_LOOPSETUP_CATCH_AND_LOG(L"Unable to change the always on top setting.")
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
        MIDI_LOOPSETUP_CATCH_AND_LOG(L"Unable to open the settings.")
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

            auto const isBasic = item != nullptr &&
                item.Tag() != nullptr &&
                winrt::unbox_value_or<winrt::hstring>(item.Tag(), L"") == L"bloop";

            ShowLoopbackPage(!isBasic);

            native::AppSettings::Current().SelectedPageIndex(
                isBasic ? native::AppSettings::PageIndexBasicLoopbacks : native::AppSettings::PageIndexLoopbacks);

            RequestRefreshAsync();
        }
        MIDI_LOOPSETUP_CATCH_AND_LOG(L"Unable to change pages.")
    }

    void MainWindow::ShowLoopbackPage(bool const showLoopbacks) noexcept
    {
        try
        {
            LoopbacksPanel().Visibility(showLoopbacks ? xaml::Visibility::Visible : xaml::Visibility::Collapsed);
            BasicLoopbacksPanel().Visibility(showLoopbacks ? xaml::Visibility::Collapsed : xaml::Visibility::Visible);
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

    void MainWindow::SetLoopbackStatus(winrt::hstring const& text) noexcept
    {
        ShowTransientStatus(LoopbackStatusText(), m_loopbackStatusTimer, text);
    }

    void MainWindow::SetBasicLoopbackStatus(winrt::hstring const& text) noexcept
    {
        ShowTransientStatus(BasicLoopbackStatusText(), m_basicLoopbackStatusTimer, text);
    }


    // ------------------------------------------------------------------------------------
    // refresh
    // ------------------------------------------------------------------------------------

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
        MIDI_LOOPSETUP_CATCH_AND_LOG(L"Unable to start the refresh timer.")
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
            snapshot.Loopback.Available = midi2loop::MidiLoopbackManager::IsTransportAvailable();

            if (snapshot.Loopback.Available)
            {
                auto const transportId = midi2loop::MidiLoopbackManager::TransportId();

                // A transport which cannot list its entries cannot be shown, and one which
                // cannot mute keeps the rest of the page. Asking is cheap and it is the only
                // way to tell a current transport from an older one.
                snapshot.Loopback.CanList = midi2svc::MidiServiceTransportPluginConfigManager::QueryCapability(
                    transportId, MIDI_CONFIG_JSON_TRANSPORT_COMMAND_CAPABILITY_LIST_ENTRIES);

                snapshot.Loopback.CanMute = midi2svc::MidiServiceTransportPluginConfigManager::QueryCapability(
                    transportId, MIDI_CONFIG_JSON_TRANSPORT_COMMAND_CAPABILITY_MUTE_ENDPOINT);

                snapshot.Loopback.CanSetImage = midi2svc::MidiServiceTransportPluginConfigManager::QueryCapability(
                    transportId, MIDI_CONFIG_JSON_TRANSPORT_COMMAND_CAPABILITY_CREATE_WITH_IMAGE);

                if (snapshot.Loopback.CanList)
                {
                    snapshot.LoopbackEntries = midi2loop::MidiLoopbackManager::GetActiveLoopbackEntries();
                }

                snapshot.Loopback.ConfiguredIds =
                    native::LoopbackConfigFile::Current().GetEntryIds(native::LoopbackKind::Loopback);

                snapshot.Loopback.DisplayOrders =
                    native::LoopbackConfigFile::Current().GetDisplayOrders(native::LoopbackKind::Loopback);
            }

            snapshot.BasicLoopback.Available = midi2bloop::MidiBasicLoopbackManager::IsTransportAvailable();

            if (snapshot.BasicLoopback.Available)
            {
                auto const transportId = midi2bloop::MidiBasicLoopbackManager::TransportId();

                snapshot.BasicLoopback.CanList = midi2svc::MidiServiceTransportPluginConfigManager::QueryCapability(
                    transportId, MIDI_CONFIG_JSON_TRANSPORT_COMMAND_CAPABILITY_LIST_ENTRIES);

                snapshot.BasicLoopback.CanMute = midi2svc::MidiServiceTransportPluginConfigManager::QueryCapability(
                    transportId, MIDI_CONFIG_JSON_TRANSPORT_COMMAND_CAPABILITY_MUTE_ENDPOINT);

                snapshot.BasicLoopback.CanSetImage = midi2svc::MidiServiceTransportPluginConfigManager::QueryCapability(
                    transportId, MIDI_CONFIG_JSON_TRANSPORT_COMMAND_CAPABILITY_CREATE_WITH_IMAGE);

                if (snapshot.BasicLoopback.CanList)
                {
                    snapshot.BasicLoopbackEntries = midi2bloop::MidiBasicLoopbackManager::GetActiveLoopbackEntries();
                }

                snapshot.BasicLoopback.ConfiguredIds =
                    native::LoopbackConfigFile::Current().GetEntryIds(native::LoopbackKind::BasicLoopback);

                snapshot.BasicLoopback.DisplayOrders =
                    native::LoopbackConfigFile::Current().GetDisplayOrders(native::LoopbackKind::BasicLoopback);
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
        if (m_closing || m_reordering || m_refreshInProgress.exchange(true))
        {
            co_return;
        }

        auto weak = get_weak();
        auto queue = DispatcherQueue();

        // every one of the calls below blocks on the service, so none of them may run here
        co_await winrt::resume_background();

        auto snapshot = GatherSnapshot();

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

                if (!strong->m_closing && !strong->m_reordering)
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
            ApplyLoopbacks(snapshot);
            ApplyBasicLoopbacks(snapshot);

            SelectUsablePageOnce(snapshot);
        }
        MIDI_LOOPSETUP_CATCH_AND_LOG(L"Unable to show the current set of loopbacks.")
    }

    // Basic loopbacks lead because that is the familiar kind, but landing on a page whose
    // transport this PC does not have would be a dead end, so the first snapshot is allowed to
    // move the selection once. After that the customer's choice stands, even if it is a page
    // with nothing on it.
    void MainWindow::SelectUsablePageOnce(ServiceSnapshot const& snapshot) noexcept
    {
        try
        {
            if (m_appliedPageFallback || !snapshot.Gathered)
            {
                return;
            }

            m_appliedPageFallback = true;

            auto const basicUsable = snapshot.BasicLoopback.Available && snapshot.BasicLoopback.CanList;
            auto const loopbackUsable = snapshot.Loopback.Available && snapshot.Loopback.CanList;

            auto const showingBasic =
                BasicLoopbacksPanel().Visibility() == xaml::Visibility::Visible;

            if (showingBasic && !basicUsable && loopbackUsable)
            {
                MainNavigation().SelectedItem(LoopbacksNavigationItem().as<foundation::IInspectable>());
            }
            else if (!showingBasic && !loopbackUsable && basicUsable)
            {
                MainNavigation().SelectedItem(BasicLoopbacksNavigationItem().as<foundation::IInspectable>());
            }
        }
        MIDI_LOOPSETUP_CATCH_AND_LOG(L"Unable to choose the opening page.")
    }

    void MainWindow::ApplyLoopbacks(ServiceSnapshot const& snapshot) noexcept
    {
        try
        {
            auto const& transport = snapshot.Loopback;

            // Nothing on the page works without the transport, and a transport which cannot be
            // asked what it is running cannot be shown truthfully, so both cases say so once
            // instead of failing an operation at a time.
            auto const usable = transport.Available && transport.CanList;

            LoopbackUnavailableBar().Message(res::GetString(
                transport.Available ? L"LoopbackTooOldMessage" : L"LoopbackMissingMessage"));

            LoopbackUnavailableBar().IsOpen(!usable);
            LoopbacksListView().Visibility(usable ? xaml::Visibility::Visible : xaml::Visibility::Collapsed);
            CreateLoopbackButton().IsEnabled(usable);

            LoopbackImagePanel().Visibility(
                transport.CanSetImage ? xaml::Visibility::Visible : xaml::Visibility::Collapsed);

            if (!usable)
            {
                m_loopbacks.Clear();
                NoLoopbacksText().Visibility(xaml::Visibility::Collapsed);

                return;
            }

            std::vector<native::LoopbackRowData> incoming{};

            if (snapshot.LoopbackEntries != nullptr)
            {
                for (auto const& entry : snapshot.LoopbackEntries)
                {
                    if (entry == nullptr || entry.EndpointA() == nullptr || entry.EndpointB() == nullptr)
                    {
                        continue;
                    }

                    native::LoopbackRowData row{};

                    row.AssociationId = AssociationKey(entry.AssociationId());

                    row.NameA = entry.EndpointA().Name();
                    row.DescriptionA = entry.EndpointA().Description();
                    row.EndpointDeviceIdA = entry.EndpointA().EndpointDeviceId();

                    row.NameB = entry.EndpointB().Name();
                    row.DescriptionB = entry.EndpointB().Description();
                    row.EndpointDeviceIdB = entry.EndpointB().EndpointDeviceId();

                    // the pair shares one picture; the A side is where it is stored
                    row.ImageFileName = entry.EndpointA().ImageFileName();

                    row.HasSecondEndpoint = true;
                    row.DisplayName = PairDisplayName(row.NameA, row.NameB);

                    row.IsMuted = entry.IsMuted();
                    row.MuteButtonLabel = res::GetString(row.IsMuted ? L"UnmuteButtonText" : L"MuteButtonText");
                    row.MuteButtonAccessibleName = res::FormatString(
                        row.IsMuted ? L"UnmuteButtonAccessibleNameFormat" : L"MuteButtonAccessibleNameFormat",
                        row.DisplayName);
                    row.DeleteButtonAccessibleName = res::FormatString(
                        L"DeleteButtonAccessibleNameFormat", row.DisplayName);

                    row.IsPersisted = Contains(transport.ConfiguredIds, row.AssociationId);
                    row.PersistenceText = res::GetString(
                        row.IsPersisted ? L"LoopbackIsPersistedText" : L"LoopbackIsTransientText");

                    incoming.push_back(row);
                }
            }

            ReconcileRows(m_loopbacks, incoming, transport.DisplayOrders, m_loopbackOrder, transport.CanMute);

            NoLoopbacksText().Visibility(
                m_loopbacks.Size() == 0 ? xaml::Visibility::Visible : xaml::Visibility::Collapsed);
        }
        MIDI_LOOPSETUP_CATCH_AND_LOG(L"Unable to show the current loopbacks.")
    }

    void MainWindow::ApplyBasicLoopbacks(ServiceSnapshot const& snapshot) noexcept
    {
        try
        {
            auto const& transport = snapshot.BasicLoopback;

            auto const usable = transport.Available && transport.CanList;

            BasicLoopbackUnavailableBar().Message(res::GetString(
                transport.Available ? L"BasicLoopbackTooOldMessage" : L"BasicLoopbackMissingMessage"));

            BasicLoopbackUnavailableBar().IsOpen(!usable);
            BasicLoopbacksListView().Visibility(usable ? xaml::Visibility::Visible : xaml::Visibility::Collapsed);
            CreateBasicLoopbackButton().IsEnabled(usable);

            BasicLoopbackImagePanel().Visibility(
                transport.CanSetImage ? xaml::Visibility::Visible : xaml::Visibility::Collapsed);

            if (!usable)
            {
                m_basicLoopbacks.Clear();
                NoBasicLoopbacksText().Visibility(xaml::Visibility::Collapsed);

                return;
            }

            std::vector<native::LoopbackRowData> incoming{};

            if (snapshot.BasicLoopbackEntries != nullptr)
            {
                for (auto const& entry : snapshot.BasicLoopbackEntries)
                {
                    if (entry == nullptr)
                    {
                        continue;
                    }

                    native::LoopbackRowData row{};

                    row.AssociationId = AssociationKey(entry.AssociationId());

                    row.NameA = entry.Name();
                    row.DescriptionA = entry.Description();
                    row.EndpointDeviceIdA = entry.EndpointDeviceId();
                    row.ImageFileName = entry.ImageFileName();

                    row.HasSecondEndpoint = false;
                    row.DisplayName = row.NameA.empty() ? res::GetString(L"UnnamedLoopback") : row.NameA;

                    row.IsMuted = entry.IsMuted();
                    row.MuteButtonLabel = res::GetString(row.IsMuted ? L"UnmuteButtonText" : L"MuteButtonText");
                    row.MuteButtonAccessibleName = res::FormatString(
                        row.IsMuted ? L"UnmuteButtonAccessibleNameFormat" : L"MuteButtonAccessibleNameFormat",
                        row.DisplayName);
                    row.DeleteButtonAccessibleName = res::FormatString(
                        L"DeleteButtonAccessibleNameFormat", row.DisplayName);

                    row.IsPersisted = Contains(transport.ConfiguredIds, row.AssociationId);
                    row.PersistenceText = res::GetString(
                        row.IsPersisted ? L"LoopbackIsPersistedText" : L"LoopbackIsTransientText");

                    incoming.push_back(row);
                }
            }

            ReconcileRows(m_basicLoopbacks, incoming, transport.DisplayOrders, m_basicLoopbackOrder, transport.CanMute);

            NoBasicLoopbacksText().Visibility(
                m_basicLoopbacks.Size() == 0 ? xaml::Visibility::Visible : xaml::Visibility::Collapsed);
        }
        MIDI_LOOPSETUP_CATCH_AND_LOG(L"Unable to show the current basic loopbacks.")
    }

    _Use_decl_annotations_
    void MainWindow::ReconcileRows(
        collections::IObservableVector<midiloopbacksetup::LoopbackItem> const& rows,
        std::vector<native::LoopbackRowData> const& incoming,
        std::unordered_map<std::wstring, int32_t> const& fileDisplayOrders,
        std::unordered_map<std::wstring, int32_t> const& sessionDisplayOrders,
        bool const canMute) noexcept
    {
        try
        {
            // update in place or add
            for (auto const& data : incoming)
            {
                midiloopbacksetup::LoopbackItem item{ nullptr };

                for (auto const& existing : rows)
                {
                    if (existing != nullptr && existing.AssociationId() == data.AssociationId)
                    {
                        item = existing;
                        break;
                    }
                }

                if (item == nullptr)
                {
                    auto created = winrt::make_self<LoopbackItem>();
                    created->InternalInitialize(data.AssociationId);

                    item = *created;

                    rows.Append(item);
                }

                auto const self = winrt::get_self<LoopbackItem>(item);

                if (self != nullptr)
                {
                    self->InternalUpdate(data);
                }

                item.CanMute(canMute);
            }

            // anything the service no longer reports has been removed, here or elsewhere
            for (int32_t i = static_cast<int32_t>(rows.Size()) - 1; i >= 0; i--)
            {
                auto const existing = rows.GetAt(static_cast<uint32_t>(i));

                auto const stillPresent = existing != nullptr &&
                    std::any_of(incoming.begin(), incoming.end(),
                        [&existing](native::LoopbackRowData const& data)
                        {
                            return data.AssociationId == existing.AssociationId();
                        });

                if (!stillPresent)
                {
                    rows.RemoveAt(static_cast<uint32_t>(i));
                }
            }

            // The customer's arrangement wins over the file, which wins over alphabetical. A
            // position set during this session has not necessarily reached the file: a loopback
            // which was never saved has no entry to record one in.
            auto const positionOf = [&](winrt::hstring const& id) -> int32_t
                {
                    std::wstring const key{ id };

                    if (auto const session = sessionDisplayOrders.find(key); session != sessionDisplayOrders.end())
                    {
                        return session->second;
                    }

                    if (auto const file = fileDisplayOrders.find(key); file != fileDisplayOrders.end())
                    {
                        return file->second;
                    }

                    return UnorderedPosition;
                };

            std::vector<midiloopbacksetup::LoopbackItem> sorted{};

            for (auto const& row : rows)
            {
                sorted.push_back(row);
            }

            std::stable_sort(sorted.begin(), sorted.end(),
                [&](midiloopbacksetup::LoopbackItem const& left, midiloopbacksetup::LoopbackItem const& right)
                {
                    auto const leftPosition = positionOf(left.AssociationId());
                    auto const rightPosition = positionOf(right.AssociationId());

                    if (leftPosition != rightPosition)
                    {
                        return leftPosition < rightPosition;
                    }

                    return std::wstring{ Lowered(left.DisplayName()) } < std::wstring{ Lowered(right.DisplayName()) };
                });

            for (uint32_t index = 0; index < sorted.size(); index++)
            {
                sorted[index].DisplayOrder(static_cast<int32_t>(index));

                if (rows.GetAt(index) != sorted[index])
                {
                    // moving rather than replacing, so the item keeps its container and the row
                    // does not flash on every poll
                    uint32_t current{ 0 };

                    if (rows.IndexOf(sorted[index], current))
                    {
                        rows.RemoveAt(current);
                    }

                    rows.InsertAt(index, sorted[index]);
                }
            }
        }
        MIDI_LOOPSETUP_CATCH_AND_LOG(L"Unable to update the list of loopbacks.")
    }
}
