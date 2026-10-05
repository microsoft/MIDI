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
    namespace
    {
        // Notices the customer closed, by patch, until the app closes. Shared by every editor, so
        // closing a patch and opening it again doesn't bring a notice back.
        std::unordered_set<std::wstring> g_autoStartNoticeDismissed{};
        std::unordered_set<std::wstring> g_conversionNoticeDismissed{};
    }

    void MainWindow::UpdatePatchHeader() noexcept
    {
        try
        {
            auto const* patch = CurrentPatch();
            auto& library = patchbay::PatchLibrary::Current();

            if (patch == nullptr)
            {
                PatchNameText().Text(L"");
                PatchDescriptionText().Text(L"");
                PatchDescriptionText().Visibility(xaml::Visibility::Collapsed);
                StateChip().Visibility(xaml::Visibility::Collapsed);
                RoutingToggle().IsEnabled(false);
                AutoStartSwitch().IsEnabled(false);
                SavePatchButton().IsEnabled(false);
                PatchMenuButton().IsEnabled(false);
                AddEndpointButton().IsEnabled(false);
                CreateLoopbackButton().IsEnabled(false);
                NotRoutingBar().IsOpen(false);
                AutoStartBar().IsOpen(false);
                return;
            }

            PatchNameText().Text(winrt::hstring{ patch->Name });
            PatchDescriptionText().Text(winrt::hstring{ patch->Description });
            PatchDescriptionText().Visibility(patch->Description.empty()
                ? xaml::Visibility::Collapsed : xaml::Visibility::Visible);

            StateChip().Visibility(xaml::Visibility::Visible);
            StateChipText().Text(patch->FilePath.empty()
                ? resources::GetString(L"ChipTemporary")
                : (library.IsUnsaved(m_patchKey)
                    ? resources::GetString(L"ChipUnsaved")
                    : resources::GetString(L"ChipSaved")));

            auto const routing = library.IsRouting(m_patchKey);

            RoutingToggle().IsEnabled(true);
            RoutingToggle().IsChecked(routing);
            RoutingToggleText().Text(routing
                ? resources::GetString(L"ChipRouting")
                : resources::GetString(L"ChipNotRouting"));

            NotRoutingBar().IsOpen(!routing);

            // Only a patch on disk can come back by itself when the app starts.
            auto const saved = !patch->FilePath.empty();
            auto const savedPatchesStart = patchbay::AppSettings::Current().ActivateSavedPatchesAtStartup();

            AutoStartSwitch().IsEnabled(true);
            AutoStartSwitch().IsOn(saved && patch->ActivateAtStartup);

            if (saved && !(patch->ActivateAtStartup && savedPatchesStart) &&
                g_autoStartNoticeDismissed.count(m_patchKey) == 0)
            {
                AutoStartBar().Message(savedPatchesStart
                    ? resources::GetString(L"AutoStartBarMessage")
                    : resources::FormatString(L"AutoStartBarSettingOffMessageFormat",
                        std::wstring{ resources::GetString(L"SettingActivateAtStartup") }));
                AutoStartBar().IsOpen(true);
            }
            else
            {
                AutoStartBar().IsOpen(false);
            }

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
            if (CurrentPatch() == nullptr)
            {
                return;
            }

            auto const isChecked = RoutingToggle().IsChecked();

            patchbay::PatchLibrary::Current().SetRouting(m_patchKey, isChecked && isChecked.Value());
            UpdatePatchHeader();
        }
        MIDI_PATCHBAY_CATCH_AND_LOG(L"Unable to change whether the patch is routing.")
    }

    _Use_decl_annotations_
    void MainWindow::OnNotRoutingStartClick(foundation::IInspectable const& sender, xaml::RoutedEventArgs const& args)
    {
        UNREFERENCED_PARAMETER(sender);
        UNREFERENCED_PARAMETER(args);

        patchbay::PatchLibrary::Current().SetRouting(m_patchKey, true);
    }

    _Use_decl_annotations_
    void MainWindow::OnAutoStartToggled(foundation::IInspectable const& sender, xaml::RoutedEventArgs const& args)
    {
        UNREFERENCED_PARAMETER(sender);
        UNREFERENCED_PARAMETER(args);

        try
        {
            auto* patch = CurrentPatch();

            if (patch == nullptr)
            {
                return;
            }

            auto const on = AutoStartSwitch().IsOn();
            auto const saved = !patch->FilePath.empty();

            // UpdatePatchHeader setting the switch raises this too.
            if (on == (saved && patch->ActivateAtStartup))
            {
                return;
            }

            if (!saved)
            {
                ShowSavePatchDialogAsync(true);
                return;
            }

            patch->ActivateAtStartup = on;

            // A setting of the patch rather than an edit, so it is saved but not undone.
            m_committing = true;
            auto const reset = wil::scope_exit([this]() { m_committing = false; });

            patchbay::PatchLibrary::Current().Changed(m_patchKey, false);
            UpdatePatchHeader();
        }
        MIDI_PATCHBAY_CATCH_AND_LOG(L"Unable to change whether the patch starts automatically.")
    }

    _Use_decl_annotations_
    void MainWindow::OnAutoStartBarCloseClick(controls::InfoBar const& sender, foundation::IInspectable const& args)
    {
        UNREFERENCED_PARAMETER(sender);
        UNREFERENCED_PARAMETER(args);

        try
        {
            g_autoStartNoticeDismissed.insert(m_patchKey);
        }
        MIDI_PATCHBAY_CATCH_AND_LOG(L"Unable to close the automatic start notice.")
    }

    _Use_decl_annotations_
    void MainWindow::OnConversionBarCloseClick(controls::InfoBar const& sender, foundation::IInspectable const& args)
    {
        UNREFERENCED_PARAMETER(sender);
        UNREFERENCED_PARAMETER(args);

        try
        {
            g_conversionNoticeDismissed.insert(m_patchKey);
        }
        MIDI_PATCHBAY_CATCH_AND_LOG(L"Unable to close the conversion notice.")
    }

    _Use_decl_annotations_
    void MainWindow::OnShowEarlierVersionClick(foundation::IInspectable const& sender, xaml::RoutedEventArgs const& args)
    {
        UNREFERENCED_PARAMETER(sender);
        UNREFERENCED_PARAMETER(args);

        try
        {
            auto const* patch = CurrentPatch();

            if (patch == nullptr || patch->EarlierVersionPath.empty())
            {
                return;
            }

            // Explorer opens on the folder with the file picked out, rather than opening the file.
            wil::unique_any<PIDLIST_ABSOLUTE, decltype(&::ILFree), ::ILFree> item{ ::ILCreateFromPathW(patch->EarlierVersionPath.c_str()) };

            if (item)
            {
                LOG_IF_FAILED(::SHOpenFolderAndSelectItems(item.get(), 0, nullptr, 0));
            }
        }
        MIDI_PATCHBAY_CATCH_AND_LOG(L"Unable to show the earlier version of the patch.")
    }

    void MainWindow::UpdateConversionNotice() noexcept
    {
        try
        {
            auto const* patch = CurrentPatch();

            if (patch == nullptr ||
                patch->LoadedFileVersion >= patchbay::CurrentPatchFileVersion ||
                g_conversionNoticeDismissed.count(m_patchKey) != 0)
            {
                ConversionBar().IsOpen(false);
                return;
            }

            std::wstring message{ resources::GetString(patch->EarlierVersionPath.empty()
                ? L"ConversionMessageNoCopy"
                : L"ConversionMessage") };

            for (auto const& issue : patch->ConversionIssues)
            {
                message += L"\n";

                if (issue.Kind == patchbay::ConversionIssueKind::TooLargeToConvert)
                {
                    message += resources::GetString(L"ConversionIssueTooLarge");
                    continue;
                }

                auto const* block = patch->FindBlock(issue.BlockId);

                message += resources::FormatString(L"ConversionIssueNoteFormat",
                    patchbay::DescribeNote(issue.FromNote),
                    patchbay::DescribeNote(issue.ToNote),
                    block == nullptr ? patchbay::BlockKindName(patchbay::BlockKind::NoteMap) : patchbay::BlockDisplayName(*block));
            }

            ConversionBar().Title(resources::GetString(L"ConversionTitle"));
            ConversionBar().Message(winrt::hstring{ message });
            ConversionBar().Severity(patch->ConversionIssues.empty()
                ? controls::InfoBarSeverity::Informational
                : controls::InfoBarSeverity::Warning);

            ConversionShowButton().Visibility(patch->EarlierVersionPath.empty()
                ? xaml::Visibility::Collapsed
                : xaml::Visibility::Visible);

            ConversionBar().IsOpen(true);
        }
        MIDI_PATCHBAY_CATCH_AND_LOG(L"Unable to show the conversion notice.")
    }

    void MainWindow::OnActivity() noexcept
    {
        try
        {
            if (m_closing || m_patchKey.empty())
            {
                return;
            }

            m_activity = patchbay::PatchLibrary::Current().Activity(m_patchKey);

            m_canvas.RefreshStatus(m_activity);

            UpdateInspectorActivity();
            UpdateStatusStrip();

            ServiceBar().IsOpen(!patchbay::EndpointCatalog::Current().IsServiceAvailable());
        }
        MIDI_PATCHBAY_CATCH_AND_LOG(L"Unable to show the latest activity.")
    }

    void MainWindow::UpdateStatusStrip() noexcept
    {
        try
        {
            auto const* patch = CurrentPatch();

            auto const endpointCount = patch == nullptr ? 0 : patch->Endpoints.size();
            auto const stepCount = patch == nullptr ? 0 : patch->StepCount();
            auto const connectionCount = patch == nullptr ? 0 : patch->Connections.size();

            StatusEndpointsText().Text(endpointCount == 1
                ? resources::GetString(L"StatusEndpointsOne")
                : resources::FormatString(L"StatusEndpointsFormat", endpointCount));

            StatusStepsText().Text(stepCount == 1
                ? resources::GetString(L"StatusStepsOne")
                : resources::FormatString(L"StatusStepsFormat", stepCount));

            StatusConnectionsText().Text(connectionCount == 1
                ? resources::GetString(L"StatusConnectionsOne")
                : resources::FormatString(L"StatusConnectionsFormat", connectionCount));

            // What reached a destination. A message that goes through three steps on its way is
            // still one message.
            uint64_t total{ 0 };

            if (patch != nullptr)
            {
                for (auto const& link : patch->Connections)
                {
                    if (patch->FindEndpoint(link.DestinationId) == nullptr)
                    {
                        continue;
                    }

                    if (auto const found = m_activity.find(link.Id); found != m_activity.end())
                    {
                        total += found->second.MessagesForwarded;
                    }
                }
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

            auto const& analysis = Analysis();

            if (analysis.HasCertainLoop())
            {
                auto const muted = analysis.LoopMutedConnectionIds.size();

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
            auto const& analysis = Analysis();

            // ---------------------------------------------------------- loops
            auto const certain = std::find_if(analysis.Loops.begin(), analysis.Loops.end(),
                [](patchbay::LoopFinding const& f) { return f.Severity == patchbay::LoopSeverity::Certain; });

            auto const possible = std::find_if(analysis.Loops.begin(), analysis.Loops.end(),
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

            // Routing turned down for the whole patch is worse than a loop that is held muted,
            // so it takes the same bar first.
            auto const problem = patchbay::PatchLibrary::Current().Problem(m_patchKey);

            if (problem.has_value())
            {
                LoopBar().Severity(controls::InfoBarSeverity::Error);
                LoopBar().Title(resources::GetString(L"RouteProblemTitle"));
                LoopBar().Message(resources::GetString(problem.value() == patchbay::RouteProblemKind::LoopBetweenBlocks
                    ? L"RouteProblemStepLoop"
                    : L"RouteProblemTooComplex"));
                LoopShowButton().Visibility(xaml::Visibility::Collapsed);
                LoopBar().IsOpen(true);
            }
            else if (certain != analysis.Loops.end())
            {
                LoopBar().Severity(controls::InfoBarSeverity::Error);
                LoopBar().Title(resources::GetString(L"LoopCertainTitle"));
                LoopBar().Message(resources::FormatString(
                    L"LoopCertainMessageFormat", joinNames(certain->EndpointNames)));
                LoopShowButton().Visibility(xaml::Visibility::Visible);
                LoopBar().IsOpen(true);
            }
            else if (possible != analysis.Loops.end() && patchbay::AppSettings::Current().WarnAboutLoops())
            {
                LoopBar().Severity(controls::InfoBarSeverity::Warning);
                LoopBar().Title(resources::GetString(L"LoopPossibleTitle"));
                LoopBar().Message(resources::FormatString(
                    L"LoopPossibleMessageFormat",
                    joinNames(possible->EndpointNames),
                    joinNames(possible->AssumedEchoNames)));
                LoopShowButton().Visibility(xaml::Visibility::Visible);
                LoopBar().IsOpen(true);
            }
            else
            {
                LoopBar().IsOpen(false);
            }

            ServiceBar().IsOpen(!patchbay::EndpointCatalog::Current().IsServiceAvailable());

            // ------------------------------------------------------- offline
            if (patch == nullptr)
            {
                OfflineBar().IsOpen(false);
                return;
            }

            std::vector<std::wstring> missing{};

            for (auto const& endpoint : patch->Endpoints)
            {
                if (!patchbay::ResolveEndpoint(endpoint).has_value())
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
