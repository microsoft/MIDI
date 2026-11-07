// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

// The Overview page. It owns no checks of its own: each page's check reports into
// m_overview, and this file turns that into the dashboard and the navigation dots.

#include "pch.h"
#include "MainWindow.xaml.h"

#include "BackgroundWork.h"
#include "CrashDumps.h"
#include "StringResources.h"

namespace native = ::miditroubleshooter;
namespace dumps = ::miditroubleshooter::crashdumps;
namespace res = ::miditroubleshooter::resources;

namespace winrt::miditroubleshooter::implementation
{
    namespace
    {
        // FILETIME ticks in seven days
        constexpr uint64_t RecentCrashWindow = 7ull * 24 * 60 * 60 * 10'000'000ull;

        winrt::hstring FormatFileTime(_In_ uint64_t const fileTime) noexcept
        {
            try
            {
                if (fileTime == 0)
                {
                    return {};
                }

                winrt::Windows::Globalization::DateTimeFormatting::DateTimeFormatter const formatter{ L"shortdate shorttime" };

                return formatter.Format(winrt::clock::from_file_time(winrt::file_time{ fileTime }));
            }
            catch (...)
            {
                return {};
            }
        }

        // An InfoBadge dot next to a page that needs attention. Severity: 0 none, 1 caution, 2 critical.
        void SetNavigationDot(_In_ controls::NavigationViewItem const& item, _In_ int32_t const severity) noexcept
        {
            try
            {
                if (item == nullptr)
                {
                    return;
                }

                if (severity == 0)
                {
                    item.InfoBadge(nullptr);
                    return;
                }

                controls::InfoBadge badge{};

                auto const styleKey = winrt::box_value(severity >= 2 ?
                    L"CriticalDotInfoBadgeStyle" : L"CautionDotInfoBadgeStyle");

                if (auto const style = xaml::Application::Current().Resources().TryLookup(styleKey))
                {
                    badge.Style(style.as<xaml::Style>());
                }

                xaml::Automation::AutomationProperties::SetName(badge,
                    res::GetString(severity >= 2 ? L"NavigationBadgeProblem" : L"NavigationBadgeReview"));

                item.InfoBadge(badge);
            }
            MIDI_TSHOOT_CATCH_AND_LOG(L"Unable to set a navigation dot.")
        }
    }

    void MainWindow::InitializeOverviewPage() noexcept
    {
        try
        {
            OverviewPanel::Host host{};

            host.Navigate = [weak = get_weak()](winrt::hstring const& tag)
                {
                    if (auto strong = weak.get())
                    {
                        strong->NavigateToTag(tag);
                    }
                };

            host.Refresh = [weak = get_weak()]()
                {
                    if (auto strong = weak.get())
                    {
                        strong->RunStartupChecks();
                    }
                };

            host.RunDiagnostics = [weak = get_weak()]()
                {
                    if (auto strong = weak.get())
                    {
                        strong->NavigateToTag(L"diagnostics");
                        strong->OnRunMidiDiagClick(nullptr, nullptr);
                    }
                };

            host.CaptureLog = [weak = get_weak()]()
                {
                    if (auto strong = weak.get())
                    {
                        strong->NavigateToTag(L"capture");
                    }
                };

            winrt::get_self<OverviewPanel>(OverviewPagePanel())->Initialize(std::move(host));
        }
        MIDI_TSHOOT_CATCH_AND_LOG(L"Unable to set up the overview page.")
    }

    void MainWindow::RunStartupChecks() noexcept
    {
        try
        {
            if (m_closing)
            {
                return;
            }

            // Each of these reports back on its own, so the cards fill in as they finish.
            SummarizeApiModeForOverview(native::GetCurrentApiMode());
            RequestServiceRefreshAsync();
            OnRefreshRegistryClick(nullptr, nullptr);
            OnRefreshDriversClick(nullptr, nullptr);
            RefreshCrashSummaryAsync();
        }
        MIDI_TSHOOT_CATCH_AND_LOG(L"Unable to start the overview checks.")
    }

    winrt::fire_and_forget MainWindow::RefreshCrashSummaryAsync() noexcept
    {
        auto lifetime = get_strong();

        try
        {
            std::vector<dumps::DumpFile> files{};

            co_await native::RunOnBackgroundAsync([&files]()
                {
                    files = dumps::ListDumps();
                });

            if (m_closing)
            {
                co_return;
            }

            FILETIME now{};
            ::GetSystemTimeAsFileTime(&now);

            auto const nowTicks = (static_cast<uint64_t>(now.dwHighDateTime) << 32) | now.dwLowDateTime;
            auto const cutoff = nowTicks > RecentCrashWindow ? nowTicks - RecentCrashWindow : 0;

            uint32_t recent{ 0 };
            dumps::DumpFile const* latest{ nullptr };

            for (auto const& file : files)
            {
                if (file.WriteTime < cutoff)
                {
                    continue;
                }

                recent++;

                if (latest == nullptr || file.WriteTime > latest->WriteTime)
                {
                    latest = &file;
                }
            }

            auto& crashes = m_overview.CrashCounts;

            crashes.Known = true;
            crashes.RecentCount = recent;
            crashes.LatestText = latest == nullptr ?
                winrt::hstring{} :
                res::FormatString(L"OverviewCrashLatestFormat",
                    latest->IsServiceDump ?
                        res::GetString(L"OverviewCrashServiceName") :
                        winrt::hstring{ latest->FolderName },
                    FormatFileTime(latest->WriteTime));

            UpdateOverview();
        }
        MIDI_TSHOOT_CATCH_AND_LOG(L"Unable to summarize the crash dumps.")
    }

    void MainWindow::UpdateOverview() noexcept
    {
        try
        {
            if (m_closing)
            {
                return;
            }

            winrt::get_self<OverviewPanel>(OverviewPagePanel())->Apply(m_overview);

            UpdateNavigationBadges();
        }
        MIDI_TSHOOT_CATCH_AND_LOG(L"Unable to update the overview.")
    }

    void MainWindow::UpdateNavigationBadges() noexcept
    {
        try
        {
            auto const& service = m_overview.MidiService;
            auto const& transports = m_overview.TransportCounts;
            auto const& registry = m_overview.RegistryCounts;
            auto const& devices = m_overview.DeviceCounts;
            auto const& crashes = m_overview.CrashCounts;

            SetNavigationDot(ServiceNavigationItem(),
                service.Known && (!service.Installed || service.Disabled) ? 2 : 0);

            SetNavigationDot(TransportsNavigationItem(),
                !transports.Known ? 0 :
                transports.AwaitingRestart ? 1 :
                transports.Problems > 0 ? 2 : 0);

            SetNavigationDot(RegistryNavigationItem(),
                !registry.Known || registry.LegacyMode ? 0 :
                registry.ToRepair > 0 || registry.MmcssErrors > 0 ? 2 :
                registry.ToReview > 0 || registry.MmcssWarnings > 0 ? 1 : 0);

            SetNavigationDot(DriversNavigationItem(),
                devices.Known && (devices.KorgUsbInstalled || devices.KorgBleInstalled || devices.WithProblems > 0) ? 1 : 0);

            SetNavigationDot(CrashDumpsNavigationItem(),
                crashes.Known && crashes.RecentCount > 0 ? 1 : 0);
        }
        MIDI_TSHOOT_CATCH_AND_LOG(L"Unable to update the navigation dots.")
    }

    _Use_decl_annotations_
    void MainWindow::NavigateToTag(winrt::hstring const& tag) noexcept
    {
        try
        {
            auto const pageIndex = PageIndexForTag(tag);

            if (auto const item = NavigationItemForPage(pageIndex))
            {
                // raises the selection changed event, which shows the page and remembers it
                MainNavigation().SelectedItem(item);
            }
        }
        MIDI_TSHOOT_CATCH_AND_LOG(L"Unable to open a page.")
    }
}
