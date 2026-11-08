// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#include "pch.h"
#include "MainWindow.xaml.h"

#include "BackgroundWork.h"
#include "StringResources.h"

namespace native = ::miditroubleshooter;
namespace res = ::miditroubleshooter::resources;

namespace winrt::miditroubleshooter::implementation
{
    namespace
    {
        // how many entries in a group are fine, worth a look, or wrong
        struct GroupCounts
        {
            uint32_t Ok{ 0 };
            uint32_t Warnings{ 0 };
            uint32_t Errors{ 0 };

            void Add(_In_ native::EntrySeverity const severity) noexcept
            {
                switch (severity)
                {
                case native::EntrySeverity::Ok:      Ok++; break;
                case native::EntrySeverity::Warning: Warnings++; break;
                default:                             Errors++; break;
                }
            }

            void Add(_In_ std::vector<native::RegistryEntryInfo> const& entries) noexcept
            {
                for (auto const& entry : entries)
                {
                    Add(entry.Severity);
                }
            }

            uint32_t Total() const noexcept { return Ok + Warnings + Errors; }
        };

        // A group header's pill, and whether the group is open. Groups with something to fix or
        // review open by themselves; healthy ones start closed.
        void ApplyGroup(
            _In_ GroupCounts const& counts,
            _In_ miditroubleshooter::StatusPill const& pill,
            _In_ controls::Expander const& expander) noexcept
        {
            try
            {
                if (counts.Errors > 0)
                {
                    pill.Severity(2);
                    pill.Text(res::FormatString(L"RegistryGroupProblemsFormat", counts.Errors));
                }
                else if (counts.Warnings > 0)
                {
                    pill.Severity(1);
                    pill.Text(res::FormatString(L"RegistryGroupReviewFormat", counts.Warnings));
                }
                else
                {
                    pill.Severity(0);
                    pill.Text(counts.Total() == 1 ?
                        res::GetString(L"RegistryGroupOneCorrect") :
                        res::FormatString(L"RegistryGroupAllCorrectFormat", counts.Total()));
                }

                // never closed from here, so a refresh doesn't fold away a group the customer opened
                if (counts.Errors > 0 || counts.Warnings > 0)
                {
                    expander.IsExpanded(true);
                }
            }
            MIDI_TSHOOT_CATCH_AND_LOG(L"Unable to show a registry group.")
        }

        void FillEntries(
            _In_ collections::IObservableVector<miditroubleshooter::RegistryEntryItem> const& target,
            _In_ std::vector<native::RegistryEntryInfo> const& entries) noexcept
        {
            try
            {
                target.Clear();

                for (auto const& entry : entries)
                {
                    auto item = winrt::make<RegistryEntryItem>();

                    winrt::get_self<RegistryEntryItem>(item)->Initialize(
                        winrt::hstring{ entry.Name },
                        winrt::hstring{ entry.Value },
                        winrt::hstring{ entry.Comment },
                        static_cast<uint32_t>(entry.Severity));

                    target.Append(item);
                }
            }
            MIDI_TSHOOT_CATCH_AND_LOG(L"Unable to fill a registry list.")
        }
    }

    _Use_decl_annotations_
    void MainWindow::ApplyRegistryScan(native::RegistryScan const& scan) noexcept
    {
        try
        {
            m_registryScan = scan;

            FillEntries(m_drivers32Entries, scan.Drivers32Entries);
            FillEntries(m_drivers32WowEntries, scan.Drivers32WowEntries);
            FillEntries(m_serviceRootEntries, scan.ServiceRootEntries);
            FillEntries(m_mmcssTaskEntries, scan.MmcssTaskEntries);

            // Transports are shown here as plain entries: this page is about what the registry
            // says, while the service health page is about what actually loaded.
            m_transportRegistryEntries.Clear();

            for (auto const& transport : scan.Transports)
            {
                auto item = winrt::make<RegistryEntryItem>();

                auto const severity =
                    !transport.ModuleRegistered ? 2u :
                    !transport.ModuleFileFound ? 2u :
                    !transport.Enabled ? 1u : 0u;

                auto const comment =
                    !transport.ModuleRegistered ? res::GetString(L"RegistryCommentTransportNotRegistered") :
                    !transport.ModuleFileFound ? res::GetString(L"RegistryCommentTransportFileMissing") :
                    !transport.Enabled ? res::GetString(L"RegistryCommentTransportDisabled") :
                    transport.BuiltIn ? res::GetString(L"RegistryCommentTransportBuiltIn") :
                    res::GetString(L"RegistryCommentTransportOk");

                winrt::get_self<RegistryEntryItem>(item)->Initialize(
                    winrt::hstring{ transport.Name },
                    winrt::hstring{ transport.ClassId },
                    comment,
                    severity,
                    winrt::hstring{ transport.ModulePath });

                auto const preview = native::IsPreviewModulePath(transport.ModulePath);

                winrt::get_self<RegistryEntryItem>(item)->SetTag(
                    res::GetString(preview ? L"RegistryTagPreview" : L"RegistryTagInWindows"),
                    preview ? 5 : 3);

                m_transportRegistryEntries.Append(item);
            }

            ApplyRegistrySummary(scan);

            // the summary card says it all now; this line is for scans and repairs in progress
            RegistryStatusText().Text({});

            SummarizeRegistryForOverview(scan);
            UpdateOverview();
        }
        MIDI_TSHOOT_CATCH_AND_LOG(L"Unable to show the registry scan.")
    }

    _Use_decl_annotations_
    void MainWindow::ApplyRegistrySummary(native::RegistryScan const& scan) noexcept
    {
        try
        {
            GroupCounts drivers64{};
            GroupCounts drivers32{};
            GroupCounts serviceRoot{};
            GroupCounts transports{};
            GroupCounts mmcss{};

            drivers64.Add(scan.Drivers32Entries);
            drivers32.Add(scan.Drivers32WowEntries);
            serviceRoot.Add(scan.ServiceRootEntries);
            mmcss.Add(scan.MmcssTaskEntries);

            for (auto const& transport : scan.Transports)
            {
                transports.Add(
                    !transport.ModuleRegistered || !transport.ModuleFileFound ? native::EntrySeverity::Error :
                    !transport.Enabled ? native::EntrySeverity::Warning : native::EntrySeverity::Ok);
            }

            ApplyGroup(drivers64, Drivers64Pill(), Drivers64Expander());
            ApplyGroup(drivers32, Drivers32Pill(), Drivers32Expander());
            ApplyGroup(serviceRoot, ServiceRootPill(), ServiceRootExpander());
            ApplyGroup(transports, RegistryTransportsPill(), RegistryTransportsExpander());
            ApplyGroup(mmcss, MmcssPill(), MmcssExpander());

            auto const ok = drivers64.Ok + drivers32.Ok + serviceRoot.Ok + transports.Ok + mmcss.Ok;
            auto const warnings = drivers64.Warnings + drivers32.Warnings + serviceRoot.Warnings + transports.Warnings + mmcss.Warnings;
            auto const errors = drivers64.Errors + drivers32.Errors + serviceRoot.Errors + transports.Errors + mmcss.Errors;
            auto const changeCount = static_cast<uint32_t>(scan.Plan.Descriptions.size());

            auto const banner = RegistrySummary();

            if (scan.LegacyMode)
            {
                banner.Severity(3);
                banner.Title(res::GetString(L"RegistrySummaryLegacyTitle"));
                banner.Message(res::GetString(L"RegistryLegacyModeNoChanges"));
            }
            else if (changeCount > 0)
            {
                banner.Severity(2);
                banner.Title(res::FormatString(L"RegistrySummaryRepairFormat", changeCount));
                banner.Message(res::GetString(L"RegistrySummaryRepairMessage"));
            }
            else if (errors > 0)
            {
                banner.Severity(2);
                banner.Title(res::GetString(L"RegistrySummaryProblemsTitle"));
                banner.Message(res::GetString(L"RegistrySummaryProblemsMessage"));
            }
            else if (warnings > 0)
            {
                banner.Severity(1);
                banner.Title(res::FormatString(L"RegistrySummaryReviewFormat", warnings));
                banner.Message(res::GetString(L"RegistrySummaryReviewMessage"));
            }
            else
            {
                banner.Severity(0);
                banner.Title(res::GetString(L"RegistryHealthy"));
                banner.Message({});
            }

            RegistryOkPill().Text(res::FormatString(L"RegistryOkPillFormat", ok));
            RegistryRepairPill().Text(res::FormatString(L"RegistryProblemsPillFormat", errors));
            RegistryRepairPill().Visibility(errors == 0 ? xaml::Visibility::Collapsed : xaml::Visibility::Visible);
            RegistryReviewPill().Text(res::FormatString(L"RegistryReviewPillFormat", warnings));
            RegistryReviewPill().Visibility(warnings == 0 ? xaml::Visibility::Collapsed : xaml::Visibility::Visible);

            // Repair is offered only when there is something it can change.
            auto const canRepair = scan.Plan.AnyChanges() && !scan.LegacyMode;

            RepairRegistryButton().Visibility(canRepair ? xaml::Visibility::Visible : xaml::Visibility::Collapsed);
            RepairRegistryButton().IsEnabled(canRepair && m_elevated);
            RepairRegistryButtonText().Text(res::FormatString(L"RepairRegistryButtonTextFormat", changeCount));
        }
        MIDI_TSHOOT_CATCH_AND_LOG(L"Unable to show the registry summary.")
    }

    _Use_decl_annotations_
    void MainWindow::SummarizeRegistryForOverview(native::RegistryScan const& scan) noexcept
    {
        auto& summary = m_overview.RegistryCounts;

        summary = {};
        summary.Known = true;
        summary.LegacyMode = scan.LegacyMode;
        summary.ToRepair = scan.LegacyMode ? 0 : static_cast<uint32_t>(scan.Plan.Descriptions.size());

        auto const review = [&summary](std::vector<native::RegistryEntryInfo> const& entries)
            {
                for (auto const& entry : entries)
                {
                    summary.Checked++;

                    // errors the repair plan does not cover still deserve a look
                    if (entry.Severity != native::EntrySeverity::Ok)
                    {
                        summary.ToReview++;
                    }
                }
            };

        review(scan.Drivers32Entries);
        review(scan.Drivers32WowEntries);
        review(scan.ServiceRootEntries);

        // the entries Repair will fix are counted once, as repairs
        summary.ToReview = summary.ToReview > summary.ToRepair ? summary.ToReview - summary.ToRepair : 0;

        for (auto const& transport : scan.Transports)
        {
            summary.Checked++;

            if (!transport.ModuleRegistered || !transport.ModuleFileFound)
            {
                summary.ToReview++;
            }
        }

        for (auto const& entry : scan.MmcssTaskEntries)
        {
            summary.Checked++;

            if (entry.Severity == native::EntrySeverity::Error)
            {
                summary.MmcssErrors++;
            }
            else if (entry.Severity == native::EntrySeverity::Warning)
            {
                summary.MmcssWarnings++;
            }
        }
    }

    _Use_decl_annotations_
    winrt::fire_and_forget MainWindow::OnRefreshRegistryClick(foundation::IInspectable const&, xaml::RoutedEventArgs const&)
    {
        auto lifetime = get_strong();

        try
        {
            RefreshRegistryButton().IsEnabled(false);
            RegistryStatusText().Text(res::GetString(L"RegistryScanning"));

            // Runs on the closing and the exception paths too, so a scan that goes wrong cannot
            // leave the button dead for the rest of the session.
            auto const clearBusy = wil::scope_exit([this]() noexcept
                {
                    try
                    {
                        if (!m_closing)
                        {
                            RefreshRegistryButton().IsEnabled(true);
                        }
                    }
                    catch (...)
                    {
                    }
                });

            native::RegistryScan scan{};

            co_await native::RunOnBackgroundAsync([&scan]()
                {
                    scan = native::ScanRegistry();
                });

            if (m_closing)
            {
                co_return;
            }

            ApplyRegistryScan(scan);

            // the API mode lives in the same key, so a scan is a good moment to re-read it
            LoadApiMode();
        }
        MIDI_TSHOOT_CATCH_AND_LOG(L"Unable to scan the registry.")
    }

    _Use_decl_annotations_
    winrt::fire_and_forget MainWindow::OnRepairRegistryClick(foundation::IInspectable const&, xaml::RoutedEventArgs const&)
    {
        auto lifetime = get_strong();

        try
        {
            if (!RequireElevation())
            {
                co_return;
            }

            auto const plan = m_registryScan.Plan;

            if (!plan.AnyChanges())
            {
                co_return;
            }

            std::wstring message{ res::GetString(L"RegistryRepairConfirmIntro") };

            message += L"\r\n";

            for (auto const& description : plan.Descriptions)
            {
                message += L"\r\n\x2022 ";
                message += description;
            }

            message += L"\r\n\r\n";
            message += res::GetString(L"RegistryRepairConfirmOutro");

            auto const confirmed = co_await ConfirmAsync(
                res::GetString(L"RegistryRepairConfirmTitle"), winrt::hstring{ message });

            if (!confirmed)
            {
                co_return;
            }

            RepairRegistryButton().IsEnabled(false);
            RegistryStatusText().Text(res::GetString(L"RegistryRepairing"));

            // The rescan below decides whether there is anything left to repair, so the button
            // is restored from the scan rather than simply switched back on.
            auto const clearBusy = wil::scope_exit([this]() noexcept
                {
                    try
                    {
                        if (!m_closing)
                        {
                            RepairRegistryButton().IsEnabled(m_registryScan.Plan.AnyChanges() && m_elevated);
                        }
                    }
                    catch (...)
                    {
                    }
                });

            native::RepairResult result{};

            co_await native::RunOnBackgroundAsync([&result, &plan]()
                {
                    result = native::ApplyRegistryRepair(plan);
                });

            if (m_closing)
            {
                co_return;
            }

            native::RegistryScan scan{};

            co_await native::RunOnBackgroundAsync([&scan]()
                {
                    scan = native::ScanRegistry();
                });

            if (m_closing)
            {
                co_return;
            }

            ApplyRegistryScan(scan);

            RegistryStatusText().Text(result.Succeeded ?
                res::GetString(L"RegistryRepairSucceeded") :
                res::GetString(L"RegistryRepairPartlyFailed"));

            // The audio stack caches these at start, so nothing changes until a restart. The
            // console tool asks for a reboot for the same reason rather than restarting
            // services underneath running applications.
            if (result.Succeeded)
            {
                co_await OfferRestartAsync(res::GetString(L"RegistryRepairRestartMessage"));
            }
        }
        MIDI_TSHOOT_CATCH_AND_LOG(L"Unable to repair the registry.")
    }
}
