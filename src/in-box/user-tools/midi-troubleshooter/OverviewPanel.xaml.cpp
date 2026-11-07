// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#include "pch.h"
#include "OverviewPanel.xaml.h"
#if __has_include("OverviewPanel.g.cpp")
#include "OverviewPanel.g.cpp"
#endif

#include "StringResources.h"

namespace res = ::miditroubleshooter::resources;

namespace winrt::miditroubleshooter::implementation
{
    namespace
    {
        // Matches StatusPill.Severity and OverviewTile.Severity.
        constexpr int32_t SeverityOk = 0;
        constexpr int32_t SeverityWarning = 1;
        constexpr int32_t SeverityError = 2;
        constexpr int32_t SeverityInfo = 3;
        constexpr int32_t SeverityNeutral = 4;
        constexpr int32_t SeverityNotChecked = 5;

        void ShowChecking(_In_ miditroubleshooter::OverviewTile const& tile) noexcept
        {
            try
            {
                tile.Severity(SeverityInfo);
                tile.PillText({});
                tile.Value(res::GetString(L"OverviewChecking"));
                tile.ValueSuffix({});
                tile.Detail({});
            }
            MIDI_TSHOOT_CATCH_AND_LOG(L"Unable to show a checking tile.")
        }

        void SetPill(
            _In_ miditroubleshooter::OverviewTile const& tile,
            _In_ int32_t const severity,
            _In_ std::wstring_view const key) noexcept
        {
            try
            {
                tile.PillSeverity(severity);
                tile.PillText(key.empty() ? winrt::hstring{} : res::GetString(key));
            }
            MIDI_TSHOOT_CATCH_AND_LOG(L"Unable to set a tile pill.")
        }

        // A star column of zero width collapses, so an empty segment takes no space.
        xaml::GridLength Share(_In_ uint32_t const count) noexcept
        {
            return xaml::GridLength{ static_cast<double>(count), xaml::GridUnitType::Star };
        }
    }

    OverviewPanel::OverviewPanel()
    {
        InitializeComponent();
    }

    _Use_decl_annotations_
    void OverviewPanel::Initialize(Host host) noexcept
    {
        m_host = std::move(host);

        try
        {
            ServiceTile().Label(res::GetString(L"OverviewServiceLabel"));
            ServiceTile().LinkText(res::GetString(L"OverviewServiceLink"));
            TransportsTile().Label(res::GetString(L"OverviewTransportsLabel"));
            TransportsTile().LinkText(res::GetString(L"OverviewTransportsLink"));
            RegistryTile().Label(res::GetString(L"OverviewRegistryLabel"));
            RegistryTile().LinkText(res::GetString(L"OverviewRegistryLink"));
            DevicesTile().Label(res::GetString(L"OverviewDevicesLabel"));
            DevicesTile().LinkText(res::GetString(L"OverviewDevicesLink"));
            SessionsTile().Label(res::GetString(L"OverviewSessionsLabel"));
            SessionsTile().LinkText(res::GetString(L"OverviewSessionsLink"));
            CrashTile().Label(res::GetString(L"OverviewCrashLabel"));
            CrashTile().LinkText(res::GetString(L"OverviewCrashLink"));
            ApiModeTile().Label(res::GetString(L"OverviewApiModeLabel"));
            ApiModeTile().LinkText(res::GetString(L"OverviewApiModeLink"));
            MmcssTile().Label(res::GetString(L"OverviewMmcssLabel"));
            MmcssTile().LinkText(res::GetString(L"OverviewMmcssLink"));
            DpcTile().Label(res::GetString(L"OverviewDpcLabel"));
            DpcTile().LinkText(res::GetString(L"OverviewDpcLink"));

            LayoutTiles(3);
            Apply(OverviewState{});
        }
        MIDI_TSHOOT_CATCH_AND_LOG(L"Unable to set up the overview.")
    }

    _Use_decl_annotations_
    void OverviewPanel::OnTilesSizeChanged(foundation::IInspectable const&, xaml::SizeChangedEventArgs const& args)
    {
        // a card needs about 280 pixels before its text starts to wrap badly
        auto const width = args.NewSize().Width;
        auto const columns = width >= 880 ? 3u : width >= 560 ? 2u : 1u;

        LayoutTiles(columns);
    }

    _Use_decl_annotations_
    void OverviewPanel::LayoutTiles(uint32_t const columns) noexcept
    {
        try
        {
            if (columns == m_tileColumns || columns == 0)
            {
                return;
            }

            m_tileColumns = columns;

            auto const grid = TilesGrid();

            grid.ColumnDefinitions().Clear();
            grid.RowDefinitions().Clear();

            for (uint32_t i = 0; i < columns; i++)
            {
                controls::ColumnDefinition column{};
                column.Width(xaml::GridLength{ 1, xaml::GridUnitType::Star });
                grid.ColumnDefinitions().Append(column);
            }

            // the reading order of the cards stays the same whatever the number of columns
            miditroubleshooter::OverviewTile const tiles[]
            {
                ServiceTile(), TransportsTile(), RegistryTile(),
                DevicesTile(), SessionsTile(), CrashTile(),
                ApiModeTile(), MmcssTile(), DpcTile(),
            };

            auto const rows = (static_cast<uint32_t>(std::size(tiles)) + columns - 1) / columns;

            for (uint32_t i = 0; i < rows; i++)
            {
                controls::RowDefinition row{};
                row.Height(xaml::GridLengthHelper::Auto());
                grid.RowDefinitions().Append(row);
            }

            for (uint32_t i = 0; i < std::size(tiles); i++)
            {
                controls::Grid::SetRow(tiles[i], static_cast<int32_t>(i / columns));
                controls::Grid::SetColumn(tiles[i], static_cast<int32_t>(i % columns));
            }
        }
        MIDI_TSHOOT_CATCH_AND_LOG(L"Unable to lay out the overview cards.")
    }

    _Use_decl_annotations_
    void OverviewPanel::Apply(OverviewState const& state) noexcept
    {
        try
        {
            std::vector<Issue> issues{};

            ApplyService(state, issues);
            ApplyTransports(state, issues);
            ApplyRegistry(state, issues);
            ApplyDevices(state, issues);
            ApplySessions(state);
            ApplyCrashes(state, issues);
            ApplyApiModeAndChecks(state);

            auto const allKnown =
                state.MidiService.Known &&
                state.TransportCounts.Known &&
                state.RegistryCounts.Known &&
                state.DeviceCounts.Known &&
                state.CrashCounts.Known;

            ApplyIssues(issues, allKnown);
        }
        MIDI_TSHOOT_CATCH_AND_LOG(L"Unable to show the overview.")
    }

    _Use_decl_annotations_
    void OverviewPanel::ApplyService(OverviewState const& state, std::vector<Issue>& issues) noexcept
    {
        try
        {
            auto const tile = ServiceTile();
            auto const& service = state.MidiService;

            if (!service.Known)
            {
                ShowChecking(tile);
                return;
            }

            tile.Value(service.StateText);
            tile.ValueSuffix({});

            if (!service.Installed)
            {
                tile.Severity(SeverityError);
                SetPill(tile, SeverityError, L"OverviewPillProblem");
                tile.Detail(res::GetString(L"OverviewServiceMissingDetail"));

                issues.push_back({ SeverityError, res::GetString(L"OverviewIssuePillFix"),
                    res::GetString(L"OverviewIssueServiceMissing"), res::GetString(L"OverviewIssueLinkService"), L"service" });
                return;
            }

            if (service.Disabled)
            {
                tile.Severity(SeverityError);
                SetPill(tile, SeverityError, L"OverviewPillProblem");
                tile.Detail(res::GetString(L"OverviewServiceDisabledDetail"));

                issues.push_back({ SeverityError, res::GetString(L"OverviewIssuePillFix"),
                    res::GetString(L"OverviewIssueServiceDisabled"), res::GetString(L"OverviewIssueLinkService"), L"service" });
                return;
            }

            auto const startMode = res::GetString(service.StartsWithWindows ?
                L"OverviewServiceAutoStart" : L"OverviewServiceDemandStart");

            if (service.Running)
            {
                tile.Severity(SeverityOk);
                SetPill(tile, SeverityOk, L"OverviewPillRunning");
                tile.Detail(service.StartedText.empty() ?
                    startMode :
                    res::FormatString(L"OverviewServiceStartedFormat", service.StartedText, startMode));
            }
            else
            {
                // On demand is the default, so a stopped service is normal until an app asks for it.
                tile.Severity(SeverityInfo);
                SetPill(tile, SeverityNeutral, L"OverviewPillIdle");
                tile.Detail(startMode);
            }
        }
        MIDI_TSHOOT_CATCH_AND_LOG(L"Unable to show the service tile.")
    }

    _Use_decl_annotations_
    void OverviewPanel::ApplyTransports(OverviewState const& state, std::vector<Issue>& issues) noexcept
    {
        try
        {
            auto const tile = TransportsTile();
            auto const& transports = state.TransportCounts;

            if (!transports.Known)
            {
                ShowChecking(tile);
                TransportsInboxLegend().Text({});
                TransportsPreviewLegend().Text({});
                return;
            }

            TransportsBarInboxColumn().Width(Share(transports.InWindows));
            TransportsBarPreviewColumn().Width(Share(transports.Preview));
            TransportsInboxLegend().Text(res::FormatString(L"OverviewTransportsInboxLegendFormat", transports.InWindows));
            TransportsPreviewLegend().Text(res::FormatString(L"OverviewTransportsPreviewLegendFormat", transports.Preview));

            tile.Detail(transports.TurnedOff == 0 ?
                winrt::hstring{} :
                res::FormatString(L"OverviewTransportsTurnedOffFormat", transports.TurnedOff));

            if (!transports.ServiceRunning)
            {
                tile.Severity(SeverityInfo);
                SetPill(tile, SeverityNeutral, L"OverviewPillServiceNotRunning");
                tile.Value(winrt::to_hstring(transports.Total));
                tile.ValueSuffix(res::GetString(L"OverviewTransportsRegisteredSuffix"));
                return;
            }

            tile.Value(winrt::to_hstring(transports.Total - transports.Problems));
            tile.ValueSuffix(res::GetString(L"OverviewTransportsLoadedSuffix"));

            if (transports.Problems > 0 && !transports.AwaitingRestart)
            {
                tile.Severity(SeverityError);
                tile.PillSeverity(SeverityError);
                tile.PillText(res::FormatString(L"OverviewPillNotLoadedFormat", transports.Problems));

                issues.push_back({ SeverityError, res::GetString(L"OverviewIssuePillFix"),
                    res::FormatString(L"OverviewIssueTransportsFormat", transports.Problems),
                    res::GetString(L"OverviewIssueLinkTransports"), L"transports" });
            }
            else if (transports.AwaitingRestart)
            {
                tile.Severity(SeverityWarning);
                SetPill(tile, SeverityWarning, L"OverviewPillRestartNeeded");

                issues.push_back({ SeverityWarning, res::GetString(L"OverviewIssuePillReview"),
                    res::GetString(L"OverviewIssueTransportsRestart"),
                    res::GetString(L"OverviewIssueLinkTransports"), L"transports" });
            }
            else
            {
                tile.Severity(SeverityOk);
                SetPill(tile, SeverityOk, L"OverviewPillAllLoaded");
            }
        }
        MIDI_TSHOOT_CATCH_AND_LOG(L"Unable to show the transports tile.")
    }

    _Use_decl_annotations_
    void OverviewPanel::ApplyRegistry(OverviewState const& state, std::vector<Issue>& issues) noexcept
    {
        try
        {
            auto const tile = RegistryTile();
            auto const mmcss = MmcssTile();
            auto const& registry = state.RegistryCounts;

            if (!registry.Known)
            {
                ShowChecking(tile);
                ShowChecking(mmcss);
                return;
            }

            tile.Value(winrt::to_hstring(registry.ToRepair));
            tile.ValueSuffix(registry.ToReview == 0 ?
                res::GetString(L"OverviewRegistryRepairSuffix") :
                res::FormatString(L"OverviewRegistryRepairReviewSuffixFormat", registry.ToReview));
            tile.Detail(res::FormatString(L"OverviewRegistryCheckedFormat", registry.Checked));

            if (registry.LegacyMode)
            {
                tile.Severity(SeverityInfo);
                SetPill(tile, SeverityNeutral, L"OverviewPillLegacyMode");
                tile.Detail(res::GetString(L"OverviewRegistryLegacyDetail"));
            }
            else if (registry.ToRepair > 0)
            {
                tile.Severity(SeverityError);
                SetPill(tile, SeverityError, L"OverviewPillNeedsRepair");

                issues.push_back({ SeverityError, res::GetString(L"OverviewIssuePillFix"),
                    res::FormatString(L"OverviewIssueRegistryRepairFormat", registry.ToRepair),
                    res::GetString(L"OverviewIssueLinkRegistry"), L"registry" });
            }
            else if (registry.ToReview > 0)
            {
                tile.Severity(SeverityWarning);
                SetPill(tile, SeverityWarning, L"OverviewPillReview");
            }
            else
            {
                tile.Severity(SeverityOk);
                SetPill(tile, SeverityOk, L"OverviewPillAllCorrect");
            }

            if (registry.MmcssErrors > 0)
            {
                mmcss.Severity(SeverityError);
                SetPill(mmcss, SeverityError, L"OverviewPillProblem");
                mmcss.Value(res::GetString(L"OverviewMmcssBrokenValue"));
                mmcss.Detail(res::GetString(L"OverviewMmcssBrokenDetail"));

                issues.push_back({ SeverityError, res::GetString(L"OverviewIssuePillFix"),
                    res::GetString(L"OverviewIssueMmcss"), res::GetString(L"OverviewIssueLinkRegistry"), L"registry" });
            }
            else if (registry.MmcssWarnings > 0)
            {
                mmcss.Severity(SeverityWarning);
                SetPill(mmcss, SeverityWarning, L"OverviewPillChanged");
                mmcss.Value(res::GetString(L"OverviewMmcssChangedValue"));
                mmcss.Detail(res::GetString(L"OverviewMmcssChangedDetail"));
            }
            else
            {
                mmcss.Severity(SeverityOk);
                SetPill(mmcss, SeverityOk, L"OverviewPillAllCorrect");
                mmcss.Value(res::GetString(L"OverviewMmcssOkValue"));
                mmcss.Detail(res::GetString(L"OverviewMmcssOkDetail"));
            }

            mmcss.ValueSuffix({});
        }
        MIDI_TSHOOT_CATCH_AND_LOG(L"Unable to show the registry tile.")
    }

    _Use_decl_annotations_
    void OverviewPanel::ApplyDevices(OverviewState const& state, std::vector<Issue>& issues) noexcept
    {
        try
        {
            auto const tile = DevicesTile();
            auto const& devices = state.DeviceCounts;

            if (!devices.Known)
            {
                ShowChecking(tile);
                DevicesUmpLegend().Text({});
                DevicesClassicLegend().Text({});
                DevicesOtherLegendPanel().Visibility(xaml::Visibility::Collapsed);
                return;
            }

            tile.Value(winrt::to_hstring(devices.Total));
            tile.ValueSuffix(res::GetString(devices.Total == 1 ? L"OverviewDevicesSuffixOne" : L"OverviewDevicesSuffix"));

            DevicesBarUmpColumn().Width(Share(devices.NewDriver));
            DevicesBarClassicColumn().Width(Share(devices.ClassicDriver));
            DevicesBarOtherColumn().Width(Share(devices.OtherDriver));
            DevicesUmpLegend().Text(res::FormatString(L"OverviewDevicesNewLegendFormat", devices.NewDriver));
            DevicesClassicLegend().Text(res::FormatString(L"OverviewDevicesClassicLegendFormat", devices.ClassicDriver));
            DevicesOtherLegend().Text(res::FormatString(L"OverviewDevicesOtherLegendFormat", devices.OtherDriver));
            DevicesOtherLegendPanel().Visibility(devices.OtherDriver == 0 ? xaml::Visibility::Collapsed : xaml::Visibility::Visible);

            uint32_t toReview{ 0 };

            if (devices.KorgUsbInstalled)
            {
                toReview++;
                issues.push_back({ SeverityWarning, res::GetString(L"OverviewIssuePillReview"),
                    res::GetString(L"OverviewIssueKorgUsb"), res::GetString(L"OverviewIssueLinkDrivers"), L"drivers" });
            }

            if (devices.KorgBleInstalled)
            {
                toReview++;
                issues.push_back({ SeverityWarning, res::GetString(L"OverviewIssuePillReview"),
                    res::GetString(L"OverviewIssueKorgBle"), res::GetString(L"OverviewIssueLinkDrivers"), L"drivers" });
            }

            if (devices.WithProblems > 0)
            {
                toReview++;
                issues.push_back({ SeverityWarning, res::GetString(L"OverviewIssuePillReview"),
                    res::FormatString(L"OverviewIssueDeviceProblemsFormat", devices.WithProblems),
                    res::GetString(L"OverviewIssueLinkDrivers"), L"drivers" });
            }

            if (toReview > 0)
            {
                tile.Severity(SeverityWarning);
                tile.PillSeverity(SeverityWarning);
                tile.PillText(res::FormatString(L"OverviewPillToReviewFormat", toReview));
                tile.Detail({});
            }
            else
            {
                tile.Severity(devices.Total == 0 ? SeverityInfo : SeverityOk);
                SetPill(tile, SeverityOk, devices.Total == 0 ? L"" : L"OverviewPillOk");
                tile.Detail(devices.Total == 0 ? res::GetString(L"OverviewDevicesNoneDetail") : winrt::hstring{});
            }
        }
        MIDI_TSHOOT_CATCH_AND_LOG(L"Unable to show the devices tile.")
    }

    _Use_decl_annotations_
    void OverviewPanel::ApplySessions(OverviewState const& state) noexcept
    {
        try
        {
            auto const tile = SessionsTile();
            auto const& sessions = state.SessionCounts;

            if (!sessions.Known)
            {
                ShowChecking(tile);
                return;
            }

            tile.Severity(SeverityInfo);
            tile.PillText({});
            tile.Value(winrt::to_hstring(sessions.Apps));
            tile.ValueSuffix(res::GetString(sessions.Apps == 1 ? L"OverviewSessionsSuffixOne" : L"OverviewSessionsSuffix"));

            if (!sessions.ServiceRunning)
            {
                tile.Detail(res::GetString(L"OverviewSessionsServiceNotRunning"));
            }
            else if (sessions.Apps == 0)
            {
                tile.Detail(res::GetString(L"OverviewSessionsNone"));
            }
            else
            {
                tile.Detail(res::FormatString(L"OverviewSessionsDetailFormat", sessions.Connections, sessions.Names));
            }
        }
        MIDI_TSHOOT_CATCH_AND_LOG(L"Unable to show the sessions tile.")
    }

    _Use_decl_annotations_
    void OverviewPanel::ApplyCrashes(OverviewState const& state, std::vector<Issue>& issues) noexcept
    {
        try
        {
            auto const tile = CrashTile();
            auto const& crashes = state.CrashCounts;

            if (!crashes.Known)
            {
                ShowChecking(tile);
                return;
            }

            tile.Value(winrt::to_hstring(crashes.RecentCount));
            tile.ValueSuffix(res::GetString(L"OverviewCrashSuffix"));

            if (crashes.RecentCount > 0)
            {
                tile.Severity(SeverityWarning);
                SetPill(tile, SeverityWarning, L"OverviewPillRecent");
                tile.Detail(crashes.LatestText);

                issues.push_back({ SeverityWarning, res::GetString(L"OverviewIssuePillReview"),
                    res::GetString(L"OverviewIssueCrash"), res::GetString(L"OverviewIssueLinkCrash"), L"crashdumps" });
            }
            else
            {
                tile.Severity(SeverityOk);
                SetPill(tile, SeverityOk, L"OverviewPillNone");
                tile.Detail(res::GetString(L"OverviewCrashNoneDetail"));
            }
        }
        MIDI_TSHOOT_CATCH_AND_LOG(L"Unable to show the crash dumps tile.")
    }

    _Use_decl_annotations_
    void OverviewPanel::ApplyApiModeAndChecks(OverviewState const& state) noexcept
    {
        try
        {
            auto const api = ApiModeTile();

            api.Severity(SeverityInfo);
            api.PillText({});
            api.Value(state.ApiModeName.empty() ? res::GetString(L"OverviewChecking") : state.ApiModeName);
            api.ValueSuffix({});
            api.Detail(state.ApiModeDetail);

            // The DPC check takes a minute and records a trace, so it never runs by itself.
            auto const dpc = DpcTile();

            dpc.Severity(SeverityNotChecked);
            SetPill(dpc, SeverityNeutral, L"OverviewPillNotChecked");
            dpc.Value(res::GetString(L"OverviewDpcValue"));
            dpc.ValueSuffix({});
            dpc.Detail(res::GetString(L"OverviewDpcDetail"));
        }
        MIDI_TSHOOT_CATCH_AND_LOG(L"Unable to show the API mode tile.")
    }

    _Use_decl_annotations_
    void OverviewPanel::ApplyIssues(std::vector<Issue> const& issues, bool const allKnown) noexcept
    {
        try
        {
            auto const banner = HeroBanner();
            auto const panel = IssuesPanel();

            // The service is polled every few seconds; rebuilding identical rows would flicker
            // and take keyboard focus away from a link.
            std::wstring signature{ allKnown ? L"1" : L"0" };

            for (auto const& issue : issues)
            {
                signature += L'|';
                signature += std::to_wstring(issue.Severity);
                signature += issue.Message;
            }

            if (signature == m_issuesSignature)
            {
                return;
            }

            m_issuesSignature = signature;

            panel.Children().Clear();

            if (issues.empty())
            {
                banner.Severity(allKnown ? SeverityOk : SeverityNeutral);
                banner.Title(res::GetString(allKnown ? L"OverviewHeroAllGood" : L"OverviewHeroChecking"));
                banner.Message(res::GetString(allKnown ? L"OverviewHeroAllGoodMessage" : L"OverviewHeroCheckingMessage"));
                banner.Chips(nullptr);
                return;
            }

            auto const worst = std::any_of(issues.begin(), issues.end(),
                [](Issue const& issue) { return issue.Severity == SeverityError; }) ? SeverityError : SeverityWarning;

            banner.Severity(worst);
            banner.Title(issues.size() == 1 ?
                res::GetString(L"OverviewHeroOneIssue") :
                res::FormatString(L"OverviewHeroIssuesFormat", static_cast<uint32_t>(issues.size())));
            banner.Message({});

            for (auto const& issue : issues)
            {
                // tag, message, link: the message takes what's left and wraps
                controls::Grid row{};
                row.ColumnSpacing(10);

                for (auto const width : { xaml::GridLengthHelper::Auto(), xaml::GridLength{ 1, xaml::GridUnitType::Star }, xaml::GridLengthHelper::Auto() })
                {
                    controls::ColumnDefinition column{};
                    column.Width(width);
                    row.ColumnDefinitions().Append(column);
                }

                miditroubleshooter::StatusPill pill{};
                pill.Severity(issue.Severity);
                pill.Text(issue.PillText);
                pill.VerticalAlignment(xaml::VerticalAlignment::Top);
                row.Children().Append(pill);

                controls::TextBlock message{};
                message.Text(issue.Message);
                message.VerticalAlignment(xaml::VerticalAlignment::Center);
                message.TextWrapping(xaml::TextWrapping::Wrap);
                controls::Grid::SetColumn(message, 1);
                row.Children().Append(message);

                controls::HyperlinkButton link{};
                link.Content(winrt::box_value(issue.LinkText));
                link.Padding(xaml::Thickness{ 0, 0, 0, 0 });
                link.VerticalAlignment(xaml::VerticalAlignment::Top);
                controls::Grid::SetColumn(link, 2);

                link.Click([weak = get_weak(), tag = issue.Tag](auto&&, auto&&)
                    {
                        if (auto strong = weak.get(); strong && strong->m_host.Navigate)
                        {
                            strong->m_host.Navigate(tag);
                        }
                    });

                row.Children().Append(link);
                panel.Children().Append(row);
            }

            banner.Chips(panel);
        }
        MIDI_TSHOOT_CATCH_AND_LOG(L"Unable to show the overview issues.")
    }

    _Use_decl_annotations_
    void OverviewPanel::OnRefreshClick(foundation::IInspectable const&, xaml::RoutedEventArgs const&)
    {
        if (m_host.Refresh)
        {
            m_host.Refresh();
        }
    }

    _Use_decl_annotations_
    void OverviewPanel::OnRunDiagnosticsClick(foundation::IInspectable const&, xaml::RoutedEventArgs const&)
    {
        if (m_host.RunDiagnostics)
        {
            m_host.RunDiagnostics();
        }
    }

    _Use_decl_annotations_
    void OverviewPanel::OnCaptureClick(foundation::IInspectable const&, xaml::RoutedEventArgs const&)
    {
        if (m_host.CaptureLog)
        {
            m_host.CaptureLog();
        }
    }

    _Use_decl_annotations_
    void OverviewPanel::OnTileLinkClick(miditroubleshooter::OverviewTile const& sender, foundation::IInspectable const&)
    {
        try
        {
            auto const tag = winrt::unbox_value_or<winrt::hstring>(sender.Tag(), winrt::hstring{});

            if (!tag.empty() && m_host.Navigate)
            {
                m_host.Navigate(tag);
            }
        }
        MIDI_TSHOOT_CATCH_AND_LOG(L"Unable to open a page from the overview.")
    }
}
