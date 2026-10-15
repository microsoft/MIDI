// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================
//
// UNSUPPORTED - DO NOT COPY THIS APPROACH. This window reads and writes the Windows MIDI Services
// configuration file directly, through the configuration engine. Microsoft does not support direct
// manipulation of that file by anything other than the in-box MIDI tools. Its format, its name, its
// location and the registry value that selects it are implementation details and can change in any
// release without notice. Applications and third-party tools must use the Windows MIDI Services API
// (Windows.Devices.Midi2.ServiceConfig) instead.
// ============================================================================

// The Import page: where a package comes in, carrying out the plan, and what happened. What will
// happen is in ConfigurationsWindowImportReview.cpp.

#include "pch.h"
#include "ConfigurationsWindow.xaml.h"

#include "AppSettings.h"
#include "BackgroundWork.h"
#include "StringResources.h"

namespace native = ::midisettings;
namespace res = ::midisettings::resources;
namespace cfg = ::midisettings::configurations;
namespace engine = ::midisettings::configengine;
namespace datatransfer = ::winrt::Windows::ApplicationModel::DataTransfer;
namespace storage = ::winrt::Windows::Storage;

namespace winrt::midisettings::implementation
{
    namespace
    {
        constexpr wchar_t GlyphDone{ 0xE73E };
        constexpr wchar_t GlyphWarning{ 0xE7BA };
        constexpr wchar_t GlyphWaiting{ 0xE946 };
        constexpr wchar_t GlyphLeftOut{ 0xE738 };

        // The network kind picks which of a pair of strings to use.
        std::wstring_view NetworkKey(
            _In_ engine::NetworkKind const kind,
            _In_ std::wstring_view const network,
            _In_ std::wstring_view const rtp) noexcept
        {
            return kind == engine::NetworkKind::RtpMidi ? rtp : network;
        }
    }

    winrt::hstring ConfigurationsWindow::ImportPackageName() const
    {
        auto const& plan = m_importPlan;

        if (!plan.Source.ConfigName.empty())
        {
            return winrt::hstring{ plan.Source.ConfigName };
        }

        if (!plan.Package.ConfigName.empty())
        {
            return winrt::hstring{ plan.Package.ConfigName };
        }

        auto name = m_importPath;
        auto const separator = name.find_last_of(L"\\/");

        if (separator != std::wstring::npos)
        {
            name = name.substr(separator + 1);
        }

        for (auto const suffix : { engine::PackageFileSuffix, engine::ConfigFileSuffix })
        {
            if (engine::EndsWithText(name, suffix))
            {
                name.resize(name.size() - suffix.size());
                break;
            }
        }

        return winrt::hstring{ name };
    }

    _Use_decl_annotations_
    void ConfigurationsWindow::ShowImportPanel(ImportPanel const panel) noexcept
    {
        try
        {
            m_importPanel = panel;

            ImportStartPanel().Visibility(panel == ImportPanel::Start ? xaml::Visibility::Visible : xaml::Visibility::Collapsed);
            ImportReviewPanel().Visibility(panel == ImportPanel::Review ? xaml::Visibility::Visible : xaml::Visibility::Collapsed);
            ImportDonePanel().Visibility(panel == ImportPanel::Done ? xaml::Visibility::Visible : xaml::Visibility::Collapsed);

            if (panel == ImportPanel::Review)
            {
                ImportReviewScroller().ChangeView(nullptr, 0.0, nullptr, true);
            }

            ApplyWorkState();
        }
        MIDI_SETTINGS_CATCH_AND_LOG(L"Unable to change the Import page.")
    }

    void ConfigurationsWindow::ResetImport() noexcept
    {
        try
        {
            m_importPath.clear();
            m_importContents = {};
            m_importPlan = {};
            m_importChoices = {};
            m_liveEndpoints.clear();
            m_expandedImportParts.clear();
            m_showAllImportGroups.clear();
            m_importFocusTargets.clear();
            m_importResult.reset();
            m_importUndone = false;

            ImportPartsPanel().Children().Clear();
            ImportResultsPanel().Children().Clear();
            ImportReviewStatusBar().IsOpen(false);
            ImportDoneStatusBar().IsOpen(false);
            ImportRestartBar().IsOpen(false);
        }
        MIDI_SETTINGS_CATCH_AND_LOG(L"Unable to put the Import page back.")
    }

    foundation::IAsyncAction ConfigurationsWindow::OpenPackageAsync(std::wstring path)
    {
        auto lifetime = get_strong();

        try
        {
            if (!BeginWork())
            {
                ShowStatus(ImportStartStatusBar(), controls::InfoBarSeverity::Warning, res::GetString(L"CfgImportBusyTitle"), {});
                co_return;
            }

            ImportStartStatusBar().IsOpen(false);

            auto const environment = m_environment;

            engine::PackageContents contents{};
            engine::ImportPlan plan{};
            engine::ConfigResult read{};
            engine::ConfigResult planned{};
            std::vector<cfg::LiveEndpoint> live{};

            co_await native::RunOnBackgroundAsync([&]()
                {
                    read = engine::ReadPackage(path, contents);

                    if (!read.Succeeded())
                    {
                        return;
                    }

                    planned = engine::MakeImportPlan(environment, contents, cfg::IsRtpMidiInstalled(), plan);

                    if (!planned.Succeeded())
                    {
                        return;
                    }

                    live = cfg::LiveEndpoints();
                });

            if (m_closing)
            {
                co_return;
            }

            auto const separator = path.find_last_of(L"\\/");
            auto const fileName = separator == std::wstring::npos ? path : path.substr(separator + 1);

            if (!read.Succeeded() || !planned.Succeeded())
            {
                // whatever was showing stays, so a wrong pick doesn't lose a review
                auto const bar = m_importPanel == ImportPanel::Review ? ImportReviewStatusBar() : ImportStartStatusBar();

                ShowFailure(bar, res::FormatString(L"CfgOpenFailedFormat", fileName), read.Succeeded() ? planned : read);

                EndWork();
                co_return;
            }

            ResetImport();

            m_importPath = path;
            m_importContents = std::move(contents);
            m_importPlan = std::move(plan);
            m_liveEndpoints = std::move(live);

            // With nothing in use, the only place it can go is a configuration of its own.
            if (!m_importPlan.HasTarget)
            {
                m_importChoices.Target = engine::ImportTarget::NewConfiguration;
            }

            // the parts the comps show open
            m_expandedImportParts = { L"devices", L"loopbacks", L"network" };

            native::AppSettings::Current().LastPackageFolder(cfg::ParentFolder(path));

            RenderImportReview();
            ShowImportPanel(ImportPanel::Review);

            EndWork();
        }
        catch (...)
        {
            EndWork();
            MIDI_SETTINGS_LOG_GENERAL_EXCEPTION(L"Unable to open a configuration package.");
        }
    }

    winrt::fire_and_forget ConfigurationsWindow::ChoosePackage() noexcept
    {
        auto lifetime = get_strong();

        try
        {
            if (m_busy)
            {
                co_return;
            }

            auto const path = cfg::ShowOpenPackageDialog(WindowHandle(), native::AppSettings::Current().LastPackageFolder());

            if (path.empty() || m_closing)
            {
                co_return;
            }

            co_await OpenPackageAsync(path);
        }
        MIDI_SETTINGS_CATCH_AND_LOG(L"Unable to choose a package.")
    }

    _Use_decl_annotations_
    void ConfigurationsWindow::OnChoosePackageClick(foundation::IInspectable const&, xaml::RoutedEventArgs const&)
    {
        ChoosePackage();
    }

    _Use_decl_annotations_
    void ConfigurationsWindow::OnImportDragOver(foundation::IInspectable const&, xaml::DragEventArgs const& args)
    {
        try
        {
            if (!m_busy && args.DataView().Contains(datatransfer::StandardDataFormats::StorageItems()))
            {
                args.AcceptedOperation(datatransfer::DataPackageOperation::Copy);
                args.DragUIOverride().Caption(res::GetString(L"CfgDropCaption"));
            }
            else
            {
                args.AcceptedOperation(datatransfer::DataPackageOperation::None);
            }
        }
        MIDI_SETTINGS_CATCH_AND_LOG(L"Unable to accept a dropped file.")
    }

    _Use_decl_annotations_
    winrt::fire_and_forget ConfigurationsWindow::OnImportDrop(foundation::IInspectable const&, xaml::DragEventArgs const& args)
    {
        auto lifetime = get_strong();

        try
        {
            if (!args.DataView().Contains(datatransfer::StandardDataFormats::StorageItems()))
            {
                co_return;
            }

            auto deferral = args.GetDeferral();
            auto const items = co_await args.DataView().GetStorageItemsAsync();
            deferral.Complete();

            if (m_closing)
            {
                co_return;
            }

            // only the first one: a review is about one package
            for (auto const& item : items)
            {
                if (item.IsOfType(storage::StorageItemTypes::File))
                {
                    std::wstring const path{ item.Path() };

                    if (!path.empty())
                    {
                        co_await OpenPackageAsync(path);
                    }

                    break;
                }
            }
        }
        MIDI_SETTINGS_CATCH_AND_LOG(L"Unable to open a dropped package.")
    }

    _Use_decl_annotations_
    void ConfigurationsWindow::OnImportCancelClick(foundation::IInspectable const&, xaml::RoutedEventArgs const&)
    {
        try
        {
            if (m_busy)
            {
                return;
            }

            ResetImport();
            ShowImportPanel(ImportPanel::Start);
        }
        MIDI_SETTINGS_CATCH_AND_LOG(L"Unable to cancel an import.")
    }

    _Use_decl_annotations_
    winrt::fire_and_forget ConfigurationsWindow::OnImportClick(foundation::IInspectable const&, xaml::RoutedEventArgs const&)
    {
        auto lifetime = get_strong();

        try
        {
            if (!m_elevated || m_importPath.empty() || !BeginWork())
            {
                co_return;
            }

            ImportReviewStatusBar().IsOpen(false);

            auto const environment = m_environment;

            m_pendingBeforeImport = m_pending;
            m_importServiceProcessId = m_serviceProcessId;
            m_portNamingBeforeImport = m_importPlan.HerePortNaming;

            engine::ImportResult result{};

            co_await native::RunOnBackgroundAsync([this, &result, &environment]()
                {
                    result = engine::ApplyImport(environment, m_importContents, m_importPlan, m_importChoices);
                });

            if (m_closing)
            {
                co_return;
            }

            if (!result.Succeeded())
            {
                if (result.Error == engine::ConfigError::ChangedSincePlanned)
                {
                    // Someone saved in between. It's planned again against what's there now.
                    auto const path = m_importPath;

                    EndWork();

                    co_await OpenPackageAsync(path);

                    ShowStatus(ImportReviewStatusBar(), controls::InfoBarSeverity::Warning, res::GetString(L"CfgImportChangedTitle"), res::GetString(L"CfgImportChangedMessage"));
                    co_return;
                }

                ShowFailure(ImportReviewStatusBar(), res::GetString(L"CfgImportFailedTitle"), result);

                EndWork();
                co_return;
            }

            uint32_t kinds{ 0 };

            if (result.Target == engine::ImportTarget::InUse)
            {
                kinds |= cfg::restartkinds::Changes;
            }

            if (result.PortNamingChanged)
            {
                kinds |= cfg::restartkinds::PortNames;
            }

            if (kinds != 0)
            {
                RecordPendingRestart(kinds, {});
            }

            if (result.Target == engine::ImportTarget::NewConfiguration)
            {
                cfg::RememberImported(result.ConfigFileName, m_importPlan.Source.ComputerName);
            }

            m_importResult = std::move(result);
            m_importUndone = false;

            RenderImportDone();
            ShowImportPanel(ImportPanel::Done);

            co_await RefreshAsync();

            EndWork();
        }
        catch (...)
        {
            EndWork();
            MIDI_SETTINGS_LOG_GENERAL_EXCEPTION(L"Unable to import a package.");
        }
    }

    void ConfigurationsWindow::RenderImportDone() noexcept
    {
        try
        {
            if (!m_importResult.has_value())
            {
                return;
            }

            auto const& result = *m_importResult;
            auto const intoNew = result.Target == engine::ImportTarget::NewConfiguration;

            ImportDoneTitleText().Text(intoNew ?
                res::FormatString(L"CfgDoneNewTitleFormat", result.ConfigName) :
                res::FormatString(L"CfgDoneIntoTitleFormat", ImportPackageName(), result.ConfigName));

            ImportDoneDetailText().Text(res::GetString(intoNew ? L"CfgDoneNewDetail" : L"CfgDoneIntoDetail"));

            auto const panel = ImportResultsPanel();
            panel.Children().Clear();

            for (size_t index = 0; index < result.Lines.size(); ++index)
            {
                auto const& line = result.Lines[index];

                controls::Border row{};
                row.Padding(xaml::Thickness{ 0, 8, 0, 8 });

                if (index + 1 < result.Lines.size())
                {
                    row.Style(StyleNamed(L"CfgShareRowStyle"));
                    row.MinHeight(0);
                }

                controls::Grid grid{};
                grid.ColumnSpacing(12);

                controls::ColumnDefinition glyphColumn{};
                glyphColumn.Width(xaml::GridLengthHelper::Auto());
                grid.ColumnDefinitions().Append(glyphColumn);

                controls::ColumnDefinition textColumn{};
                textColumn.Width(xaml::GridLengthHelper::FromValueAndType(1, xaml::GridUnitType::Star));
                grid.ColumnDefinitions().Append(textColumn);

                wchar_t glyph{ GlyphDone };
                std::wstring_view style{ L"CfgDoneGlyphStyle" };

                switch (line.Tone)
                {
                case engine::ResultTone::Warning: glyph = GlyphWarning; style = L"CfgWarningGlyphStyle"; break;
                case engine::ResultTone::Waiting: glyph = GlyphWaiting; style = L"CfgWaitingGlyphStyle"; break;
                case engine::ResultTone::Neutral: glyph = GlyphLeftOut; style = L"CfgSameGlyphStyle"; break;
                default: break;
                }

                auto icon = MakeGlyph(glyph, style);
                icon.FontSize(16);
                icon.VerticalAlignment(xaml::VerticalAlignment::Top);
                icon.Margin(xaml::Thickness{ 0, 2, 0, 0 });
                grid.Children().Append(icon);

                auto text = MakeText(ResultLineText(line), L"CfgResultTextStyle");
                controls::Grid::SetColumn(text, 1);
                grid.Children().Append(text);

                row.Child(grid);
                panel.Children().Append(row);
            }

            auto const needsRestart = !m_importUndone && (!intoNew || result.PortNamingChanged);

            ImportRestartBar().Message(res::GetString(result.PortNamingChanged ? L"CfgRestartMessagePorts" : L"CfgRestartMessage"));
            ImportRestartBar().IsOpen(needsRestart);

            UndoImportButton().Visibility(m_importUndone ? xaml::Visibility::Collapsed : xaml::Visibility::Visible);
        }
        MIDI_SETTINGS_CATCH_AND_LOG(L"Unable to show what the import did.")
    }

    _Use_decl_annotations_
    winrt::hstring ConfigurationsWindow::ResultLineText(engine::ResultLine const& line) const
    {
        switch (line.Code)
        {
        case engine::ResultCode::DeviceNames:
        {
            std::vector<std::wstring> sentences{};

            if (line.Count > 0 && line.Count2 > 0)
            {
                sentences.push_back(std::wstring{ res::FormatString(L"CfgResultTwoAddedFormat",
                    cfg::CountText(L"CfgCountDeviceNames", line.Count), cfg::CountText(L"CfgCountPictures", line.Count2)) });
            }
            else if (line.Count > 0)
            {
                sentences.push_back(std::wstring{ res::FormatString(L"CfgResultAddedFormat", cfg::CountText(L"CfgCountDeviceNames", line.Count)) });
            }
            else if (line.Count2 > 0)
            {
                sentences.push_back(std::wstring{ res::FormatString(L"CfgResultAddedFormat", cfg::CountText(L"CfgCountPictures", line.Count2)) });
            }
            else if (line.Count4 == 0)
            {
                sentences.push_back(std::wstring{ res::GetString(L"CfgResultNoNewNames") });
            }

            if (line.Count3 > 0)
            {
                sentences.push_back(std::wstring{ cfg::CountText(L"CfgResultNamesKept", line.Count3) });
            }

            if (line.Count4 > 0)
            {
                sentences.push_back(std::wstring{ cfg::CountText(L"CfgResultNamesReplaced", line.Count4) });
            }

            if (line.Count5 > 0)
            {
                sentences.push_back(std::wstring{ cfg::CountText(L"CfgResultNamesRemoved", line.Count5) });
            }

            std::wstring text{};

            for (auto const& sentence : sentences)
            {
                text = text.empty() ? sentence : std::wstring{ res::FormatString(L"CfgTwoSentencesFormat", text, sentence) };
            }

            return winrt::hstring{ text };
        }

        case engine::ResultCode::DeviceNamesNotImported:
            return res::GetString(L"CfgResultNamesNotImported");

        case engine::ResultCode::LoopbackAdded:
            return res::FormatString(L"CfgResultLoopbackAddedFormat", line.Name);

        case engine::ResultCode::LoopbackAddedAs:
            return res::FormatString(L"CfgResultLoopbackAddedAsFormat", line.Name, line.NewName);

        case engine::ResultCode::LoopbackLeftOut:
            return res::FormatString(L"CfgResultLoopbackLeftOutFormat", line.Name);

        case engine::ResultCode::LoopbacksNotImported:
            return res::GetString(L"CfgResultLoopbacksNotImported");

        case engine::ResultCode::HostAdded:
            return res::FormatString(NetworkKey(line.Network, L"CfgResultHostAddedFormat", L"CfgResultRtpHostAddedFormat"), line.Name);

        case engine::ResultCode::HostAddedAsThisPc:
        case engine::ResultCode::HostAddedAs:
            return res::FormatString(NetworkKey(line.Network, L"CfgResultHostAddedAsFormat", L"CfgResultRtpHostAddedAsFormat"), line.Name, line.NewName);

        case engine::ResultCode::HostKeptOtherPcName:
            return res::FormatString(NetworkKey(line.Network, L"CfgResultHostKeptNameFormat", L"CfgResultRtpHostKeptNameFormat"), line.Name);

        case engine::ResultCode::HostLeftOut:
            return res::FormatString(NetworkKey(line.Network, L"CfgResultHostLeftOutFormat", L"CfgResultRtpHostLeftOutFormat"), line.Name);

        case engine::ResultCode::ClientAdded:
            return res::FormatString(NetworkKey(line.Network, L"CfgResultClientAddedFormat", L"CfgResultRtpClientAddedFormat"), line.Name);

        case engine::ResultCode::NetworkNotImported:
            return res::GetString(NetworkKey(line.Network, L"CfgResultNetworkNotImported", L"CfgResultRtpNotImported"));

        case engine::ResultCode::RtpSavedForLater:
        {
            // what was saved, from the plan, because the line only has the total
            size_t hosts{ 0 };
            size_t clients{ 0 };

            for (auto const& item : m_importPlan.Items)
            {
                if (item.Network == engine::NetworkKind::RtpMidi && engine::IsImported(engine::OutcomeFor(item, m_importChoices)))
                {
                    if (item.Kind == engine::ItemKind::Host)
                    {
                        hosts++;
                    }
                    else if (item.Kind == engine::ItemKind::Client)
                    {
                        clients++;
                    }
                }
            }

            if (hosts == 1 && clients == 0)
            {
                return res::GetString(L"CfgResultRtpHostSaved");
            }

            if (hosts == 0 && clients == 1)
            {
                return res::GetString(L"CfgResultRtpClientSaved");
            }

            std::vector<winrt::hstring> parts{};

            if (hosts > 0)
            {
                parts.push_back(cfg::CountText(L"CfgCountHosts", hosts));
            }

            if (clients > 0)
            {
                parts.push_back(cfg::CountText(L"CfgCountConnections", clients));
            }

            return res::FormatString(L"CfgResultRtpSavedFormat", cfg::ListText(parts));
        }

        case engine::ResultCode::BluetoothAdded:
            return cfg::CountText(L"CfgResultBluetoothAdded", line.Count);

        case engine::ResultCode::BluetoothNotImported:
            return res::GetString(L"CfgResultBluetoothNotImported");

        case engine::ResultCode::SynthModeSet:
            return res::FormatString(L"CfgResultSynthModeSetFormat", cfg::SynthModeText(line.Name));

        case engine::ResultCode::PortNamingSet:
            return res::GetString(L"CfgResultPortNamingSet");

        case engine::ResultCode::PortNamingNotUsable:
            return res::GetString(L"CfgResultPortNamingNotUsable");

        case engine::ResultCode::PortNamingFailed:
            return res::GetString(L"CfgResultPortNamingFailed");

        case engine::ResultCode::OtherSettingsKept:
            return res::GetString(L"CfgResultOtherSettingsKept");

        case engine::ResultCode::OtherSectionAdded:
            return res::GetString(L"CfgResultOtherSectionAdded");

        case engine::ResultCode::PicturesFailed:
            return cfg::CountText(L"CfgResultPicturesFailed", line.Count);

        default:
            return {};
        }
    }

    _Use_decl_annotations_
    void ConfigurationsWindow::OnImportDoneClick(foundation::IInspectable const&, xaml::RoutedEventArgs const&)
    {
        try
        {
            if (m_busy)
            {
                return;
            }

            ResetImport();
            ShowImportPanel(ImportPanel::Start);

            // where the restart is offered until it happens
            SelectPage(L"configurations");
        }
        MIDI_SETTINGS_CATCH_AND_LOG(L"Unable to finish an import.")
    }

    _Use_decl_annotations_
    winrt::fire_and_forget ConfigurationsWindow::OnUndoImportClick(foundation::IInspectable const&, xaml::RoutedEventArgs const&)
    {
        auto lifetime = get_strong();

        try
        {
            if (!m_importResult.has_value() || m_importUndone || !m_elevated || !BeginWork())
            {
                co_return;
            }

            auto const result = *m_importResult;
            auto const environment = m_environment;
            auto const portNamingBefore = m_portNamingBeforeImport;

            engine::ConfigResult undone{};

            co_await native::RunOnBackgroundAsync([&undone, &result, &environment, portNamingBefore]()
                {
                    if (result.Target == engine::ImportTarget::InUse)
                    {
                        if (!result.Backup.has_value())
                        {
                            undone.Error = engine::ConfigError::NotFound;
                            return;
                        }

                        // The backup made just before the import, put back without another backup
                        // of what the import made.
                        undone = engine::RestoreBackup(environment, *result.Backup, false);
                        return;
                    }

                    // a configuration of its own goes, backed up first like any other delete
                    undone = engine::DeleteConfiguration(environment, result.ConfigFileName);

                    if (undone.Succeeded() && result.PortNamingChanged && portNamingBefore != engine::PortNaming::None &&
                        environment.WritePortNaming && !environment.WritePortNaming(portNamingBefore))
                    {
                        undone.Error = engine::ConfigError::RegistryFailed;
                    }
                });

            if (m_closing)
            {
                co_return;
            }

            if (undone.Succeeded())
            {
                m_importUndone = true;

                // While the service still runs what it ran before the import, nothing waits for it
                // that didn't before. After a restart, it runs the import, so undoing needs one too.
                if (cfg::ServiceProcessId() == m_importServiceProcessId)
                {
                    cfg::WritePendingRestart(m_importServiceProcessId, m_pendingBeforeImport);
                    m_pending = m_pendingBeforeImport;
                }
                else
                {
                    RecordPendingRestart(
                        (result.Target == engine::ImportTarget::InUse ? cfg::restartkinds::Changes : 0) |
                        (result.PortNamingChanged ? cfg::restartkinds::PortNames : 0),
                        {});
                }

                ShowStatus(
                    ImportDoneStatusBar(),
                    controls::InfoBarSeverity::Success,
                    res::GetString(L"CfgUndoneTitle"),
                    result.Target == engine::ImportTarget::InUse ?
                        res::FormatString(L"CfgUndoneIntoMessageFormat", result.ConfigName) :
                        res::FormatString(L"CfgUndoneNewMessageFormat", result.ConfigName));

                RenderImportDone();
            }
            else
            {
                ShowFailure(ImportDoneStatusBar(), res::GetString(L"CfgUndoFailedTitle"), undone);
            }

            co_await RefreshAsync();

            EndWork();
        }
        catch (...)
        {
            EndWork();
            MIDI_SETTINGS_LOG_GENERAL_EXCEPTION(L"Unable to undo an import.");
        }
    }
}
