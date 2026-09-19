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

using namespace winrt::Microsoft::UI::Xaml;

namespace patchbay = ::midipatchbay;
namespace resources = ::midipatchbay::resources;

namespace winrt::midipatchbay::implementation
{
    void MainWindow::UpdatePatchHeader() noexcept
    {
        try
        {
            auto const* patch = CurrentPatch();

            if (patch == nullptr)
            {
                PatchNameText().Text(L"");
                PatchDescriptionText().Text(L"");
                PatchDescriptionText().Visibility(xaml::Visibility::Collapsed);
                StateChip().Visibility(xaml::Visibility::Collapsed);
                RoutingToggle().IsEnabled(false);
                SavePatchButton().IsEnabled(false);
                PatchMenuButton().IsEnabled(false);
                AddEndpointButton().IsEnabled(false);
                CreateLoopbackButton().IsEnabled(false);
                return;
            }

            PatchNameText().Text(winrt::hstring{ patch->Name });
            PatchDescriptionText().Text(winrt::hstring{ patch->Description });
            PatchDescriptionText().Visibility(patch->Description.empty()
                ? xaml::Visibility::Collapsed : xaml::Visibility::Visible);

            StateChip().Visibility(xaml::Visibility::Visible);
            StateChipText().Text(patch->FilePath.empty()
                ? resources::GetString(L"ChipTemporary")
                : (m_unsavedPatchKeys.count(PatchKey(*patch)) != 0
                    ? resources::GetString(L"ChipUnsaved")
                    : resources::GetString(L"ChipSaved")));

            auto const routing = m_routingPatchKeys.count(PatchKey(*patch)) != 0;

            RoutingToggle().IsEnabled(true);
            RoutingToggle().IsChecked(routing);
            RoutingToggleText().Text(routing
                ? resources::GetString(L"ChipRouting")
                : resources::GetString(L"ChipNotRouting"));

            SavePatchButton().IsEnabled(true);
            PatchMenuButton().IsEnabled(true);
            AddEndpointButton().IsEnabled(true);
            CreateLoopbackButton().IsEnabled(true);
        }
        MIDI_PATCHBAY_CATCH_AND_LOG(L"Unable to update the patch header.")
    }

    _Use_decl_annotations_
    void MainWindow::OnRoutingToggleClick(foundation::IInspectable const& sender, xaml::RoutedEventArgs const& args)
    {
        UNREFERENCED_PARAMETER(sender);
        UNREFERENCED_PARAMETER(args);

        try
        {
            auto const* patch = CurrentPatch();

            if (patch == nullptr)
            {
                return;
            }

            auto const isChecked = RoutingToggle().IsChecked();

            SetPatchRouting(PatchKey(*patch), isChecked && isChecked.Value());
            UpdatePatchHeader();
        }
        MIDI_PATCHBAY_CATCH_AND_LOG(L"Unable to change whether the patch is routing.")
    }

    void MainWindow::MarkDirty() noexcept
    {
        try
        {
            if (auto const* patch = CurrentPatch())
            {
                m_unsavedPatchKeys.insert(PatchKey(*patch));
                m_lastChangeTime = std::chrono::steady_clock::now();
            }
        }
        MIDI_PATCHBAY_CATCH_AND_LOG(L"Unable to mark the patch as changed.")

        UpdatePatchHeader();
    }

    void MainWindow::AutoSaveIfDue() noexcept
    {
        try
        {
            if (m_unsavedPatchKeys.empty() || m_lastChangeTime.time_since_epoch().count() == 0)
            {
                return;
            }

            // Debounced: dragging a node raises a change per drop, and a patch with a dozen
            // endpoints would otherwise rewrite its file a dozen times in a few seconds.
            constexpr auto quietPeriod = std::chrono::milliseconds{ 1500 };

            if (std::chrono::steady_clock::now() - m_lastChangeTime < quietPeriod)
            {
                return;
            }

            bool changed{ false };

            for (auto& patch : m_patches)
            {
                auto const key = PatchKey(patch);

                if (patch.FilePath.empty() || m_unsavedPatchKeys.count(key) == 0)
                {
                    continue;
                }

                if (patchbay::PatchStore::Current().Save(patch))
                {
                    m_unsavedPatchKeys.erase(key);
                    changed = true;
                }
                else
                {
                    // Reported once rather than every tick, by clearing the flag either way.
                    m_unsavedPatchKeys.erase(key);
                    ShowStatus(patchbay::PatchStore::Current().LastErrorMessage(),
                        controls::InfoBarSeverity::Error);
                }
            }

            if (changed)
            {
                UpdatePatchHeader();
            }
        }
        MIDI_PATCHBAY_CATCH_AND_LOG(L"Unable to save the patch automatically.")
    }

    void MainWindow::RefreshAnalysis() noexcept
    {
        try
        {
            auto const* patch = CurrentPatch();

            if (patch == nullptr)
            {
                m_analysis = {};
                return;
            }

            m_analysis = patchbay::AnalyzePatch(*patch, m_liveEndpoints);
        }
        MIDI_PATCHBAY_CATCH_AND_LOG(L"Unable to analyze the patch.")
    }

    void MainWindow::ApplyRouting() noexcept
    {
        try
        {
            std::vector<patchbay::RoutePlanEntry> plan{};

            auto& catalog = patchbay::EndpointCatalog::Current();

            for (auto const& patch : m_patches)
            {
                if (m_routingPatchKeys.count(PatchKey(patch)) == 0)
                {
                    continue;
                }

                // Loops are only muted for the patch on screen, because that is the only one the
                // analysis was run against. Every routing patch gets its own pass here.
                auto const analysis = patchbay::AnalyzePatch(patch, m_liveEndpoints);

                for (auto const& connection : patch.Connections)
                {
                    if (connection.Muted || analysis.LoopMutedConnectionIds.count(connection.Id) != 0)
                    {
                        continue;
                    }

                    auto const* source = patch.FindEndpoint(connection.SourceEndpointId);
                    auto const* destination = patch.FindEndpoint(connection.DestinationEndpointId);

                    if (source == nullptr || destination == nullptr)
                    {
                        continue;
                    }

                    auto const liveSource = catalog.Resolve(*source);
                    auto const liveDestination = catalog.Resolve(*destination);

                    // A connection whose endpoints are not both here is not an error; it simply
                    // waits, and is wired up by the next pass when the device comes back.
                    if (!liveSource.has_value() || !liveDestination.has_value())
                    {
                        continue;
                    }

                    patchbay::RoutePlanEntry entry{};

                    entry.ConnectionId = connection.Id;
                    entry.SourceEndpointDeviceId = liveSource->EndpointDeviceId;
                    entry.DestinationEndpointDeviceId = liveDestination->EndpointDeviceId;
                    entry.SourceGroupIndex = connection.SourceGroupIndex;
                    entry.DestinationGroupIndex = connection.DestinationGroupIndex;

                    plan.push_back(std::move(entry));
                }
            }

            // Everything below blocks on the service, so it never runs on the XAML thread.
            patchbay::RunOnBackgroundAsync([plan = std::move(plan)]() mutable
                {
                    patchbay::RouteEngine::Current().Apply(std::move(plan));
                });
        }
        MIDI_PATCHBAY_CATCH_AND_LOG(L"Unable to apply the routing.")
    }

    void MainWindow::OnRefreshTimerTick() noexcept
    {
        try
        {
            if (m_closing)
            {
                return;
            }

            m_routeStats = patchbay::RouteEngine::Current().Stats();

            m_canvas.RefreshStatus(m_routeStats);

            UpdateInspectorActivity();
            UpdateStatusStrip();
            UpdateTray();
            AutoSaveIfDue();

            auto const error = patchbay::RouteEngine::Current().LastErrorMessage();

            if (!error.empty() && !ServiceBar().IsOpen())
            {
                ShowStatus(error, controls::InfoBarSeverity::Error);
            }

            ServiceBar().IsOpen(!patchbay::EndpointCatalog::Current().IsServiceAvailable());
        }
        MIDI_PATCHBAY_CATCH_AND_LOG(L"The refresh timer failed.")
    }

    void MainWindow::UpdateStatusStrip() noexcept
    {
        try
        {
            auto const* patch = CurrentPatch();

            auto const endpointCount = patch == nullptr ? 0 : patch->Endpoints.size();
            auto const connectionCount = patch == nullptr ? 0 : patch->Connections.size();

            StatusEndpointsText().Text(endpointCount == 1
                ? resources::GetString(L"StatusEndpointsOne")
                : resources::FormatString(L"StatusEndpointsFormat", endpointCount));

            StatusConnectionsText().Text(connectionCount == 1
                ? resources::GetString(L"StatusConnectionsOne")
                : resources::FormatString(L"StatusConnectionsFormat", connectionCount));

            uint64_t total{ 0 };

            for (auto const& [id, stats] : m_routeStats)
            {
                UNREFERENCED_PARAMETER(id);
                total += stats.MessagesForwarded;
            }

            auto const now = std::chrono::steady_clock::now();

            if (m_lastRateSample.time_since_epoch().count() != 0)
            {
                auto const elapsed = std::chrono::duration<double>(now - m_lastRateSample).count();

                if (elapsed > 0.05 && total >= m_lastTotalMessages)
                {
                    auto const rate = static_cast<uint64_t>((total - m_lastTotalMessages) / elapsed);

                    StatusRateText().Text(resources::FormatString(L"StatusRateFormat", rate));
                }
            }

            m_lastRateSample = now;
            m_lastTotalMessages = total;

            if (m_analysis.HasCertainLoop())
            {
                auto const muted = m_analysis.LoopMutedConnectionIds.size();

                StatusLoopText().Text(muted == 1
                    ? resources::GetString(L"StatusLoopMutedOne")
                    : resources::FormatString(L"StatusLoopMutedFormat", muted));
            }
            else
            {
                // "detected", not "none": a DIN cable or another router can close a circle this
                // app cannot see.
                StatusLoopText().Text(resources::GetString(L"StatusNoLoopsDetected"));
            }
        }
        MIDI_PATCHBAY_CATCH_AND_LOG(L"Unable to update the status strip.")
    }

    void MainWindow::UpdateMessages() noexcept
    {
        try
        {
            auto const* patch = CurrentPatch();

            // ---------------------------------------------------------- loops
            auto const certain = std::find_if(m_analysis.Loops.begin(), m_analysis.Loops.end(),
                [](patchbay::LoopFinding const& f) { return f.Severity == patchbay::LoopSeverity::Certain; });

            auto const possible = std::find_if(m_analysis.Loops.begin(), m_analysis.Loops.end(),
                [](patchbay::LoopFinding const& f) { return f.Severity == patchbay::LoopSeverity::Possible; });

            auto const joinNames = [](std::vector<std::wstring> const& names) -> std::wstring
                {
                    std::wstring text{};

                    for (size_t i = 0; i < names.size(); i++)
                    {
                        if (i > 0)
                        {
                            text += L" \u2192 ";
                        }

                        text += names[i];
                    }

                    return text;
                };

            if (certain != m_analysis.Loops.end())
            {
                LoopBar().Severity(controls::InfoBarSeverity::Error);
                LoopBar().Title(resources::GetString(L"LoopCertainTitle"));
                LoopBar().Message(resources::FormatString(
                    L"LoopCertainMessageFormat", joinNames(certain->EndpointNames)));
                LoopBar().IsOpen(true);
            }
            else if (possible != m_analysis.Loops.end() && patchbay::AppSettings::Current().WarnAboutLoops())
            {
                LoopBar().Severity(controls::InfoBarSeverity::Warning);
                LoopBar().Title(resources::GetString(L"LoopPossibleTitle"));
                LoopBar().Message(resources::FormatString(
                    L"LoopPossibleMessageFormat",
                    joinNames(possible->EndpointNames),
                    joinNames(possible->AssumedEchoNames)));
                LoopBar().IsOpen(true);
            }
            else
            {
                LoopBar().IsOpen(false);
            }

            // ------------------------------------------------------- offline
            if (patch == nullptr)
            {
                OfflineBar().IsOpen(false);
                return;
            }

            std::vector<std::wstring> missing{};

            for (auto const& endpoint : patch->Endpoints)
            {
                if (!patchbay::EndpointCatalog::Current().Resolve(endpoint).has_value())
                {
                    missing.push_back(endpoint.DisplayName);
                }
            }

            if (missing.empty())
            {
                OfflineBar().IsOpen(false);
                return;
            }

            std::wstring names{};

            for (size_t i = 0; i < missing.size(); i++)
            {
                if (i > 0)
                {
                    names += resources::GetString(L"ListSeparator");
                }

                names += missing[i];
            }

            OfflineBar().Title(missing.size() == 1
                ? resources::GetString(L"OfflineTitleOne")
                : resources::FormatString(L"OfflineTitleFormat", missing.size()));
            OfflineBar().Message(resources::FormatString(L"OfflineMessageFormat", names));
            OfflineBar().IsOpen(true);
        }
        MIDI_PATCHBAY_CATCH_AND_LOG(L"Unable to update the messages.")
    }

    void MainWindow::UpdateTray() noexcept
    {
        try
        {
            if (!m_tray.IsVisible())
            {
                return;
            }

            std::vector<patchbay::TrayPatchItem> items{};

            size_t routing{ 0 };

            for (auto const& patch : m_patches)
            {
                patchbay::TrayPatchItem item{};

                item.PatchId = PatchKey(patch);
                item.Name = patch.Name;
                item.IsRouting = m_routingPatchKeys.count(item.PatchId) != 0;

                if (item.IsRouting)
                {
                    routing++;
                }

                item.HasWarning = std::any_of(patch.Endpoints.begin(), patch.Endpoints.end(),
                    [](patchbay::PatchEndpoint const& e)
                    { return !patchbay::EndpointCatalog::Current().Resolve(e).has_value(); });

                if (item.HasWarning)
                {
                    item.Detail = std::wstring{ resources::GetString(L"TrayWaiting") };
                }
                else if (!item.IsRouting)
                {
                    item.Detail = std::wstring{ resources::GetString(L"TrayOff") };
                }

                items.push_back(std::move(item));
            }

            m_tray.Update(
                std::wstring{ resources::FormatString(L"TrayTooltipFormat", routing) },
                std::move(items));
        }
        MIDI_PATCHBAY_CATCH_AND_LOG(L"Unable to update the notification area.")
    }

    _Use_decl_annotations_
    void MainWindow::SetPatchRouting(std::wstring const& patchKey, bool routing) noexcept
    {
        try
        {
            if (routing)
            {
                m_routingPatchKeys.insert(patchKey);
            }
            else
            {
                m_routingPatchKeys.erase(patchKey);
            }

            ApplyRouting();
            UpdateTray();
            UpdateStatusStrip();
        }
        MIDI_PATCHBAY_CATCH_AND_LOG(L"Unable to change whether a patch is routing.")
    }

    void MainWindow::StopAllRouting() noexcept
    {
        m_routingPatchKeys.clear();

        ApplyRouting();
        UpdateTray();
        UpdateStatusStrip();
    }

    _Use_decl_annotations_
    void MainWindow::ShowStatus(winrt::hstring const& message, controls::InfoBarSeverity severity) noexcept
    {
        try
        {
            if (message.empty())
            {
                StatusBar().IsOpen(false);
                return;
            }

            StatusBar().Severity(severity);
            StatusBar().Message(message);
            StatusBar().IsOpen(true);
        }
        MIDI_PATCHBAY_CATCH_AND_LOG(L"Unable to show a status message.")
    }
}
