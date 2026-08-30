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
                    transport.ModulePath.empty() ?
                        comment :
                        res::FormatString(L"RegistryTransportCommentFormat", comment, winrt::hstring{ transport.ModulePath }),
                    severity);

                m_transportRegistryEntries.Append(item);
            }

            LegacyModeBar().IsOpen(scan.LegacyMode);

            auto const changeCount = static_cast<uint32_t>(scan.Plan.Descriptions.size());

            RepairRegistryButton().IsEnabled(scan.Plan.AnyChanges() && m_elevated);

            if (scan.LegacyMode)
            {
                RegistryStatusText().Text(res::GetString(L"RegistryLegacyModeNoChanges"));
            }
            else if (changeCount == 0)
            {
                RegistryStatusText().Text(res::GetString(L"RegistryHealthy"));
            }
            else
            {
                RegistryStatusText().Text(res::FormatString(L"RegistryProblemsFoundFormat", changeCount));
            }
        }
        MIDI_TSHOOT_CATCH_AND_LOG(L"Unable to show the registry scan.")
    }

    _Use_decl_annotations_
    winrt::fire_and_forget MainWindow::OnRefreshRegistryClick(foundation::IInspectable const&, xaml::RoutedEventArgs const&)
    {
        auto lifetime = get_strong();

        try
        {
            RefreshRegistryButton().IsEnabled(false);
            RegistryStatusText().Text(res::GetString(L"RegistryScanning"));

            native::RegistryScan scan{};

            co_await native::RunOnBackgroundAsync([&scan]()
                {
                    scan = native::ScanRegistry();
                });

            if (m_closing)
            {
                co_return;
            }

            RefreshRegistryButton().IsEnabled(true);

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
