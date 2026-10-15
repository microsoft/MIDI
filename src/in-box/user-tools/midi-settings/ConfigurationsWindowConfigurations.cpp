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

// The Configurations page: the one in use, the others on this PC, and switching between them.

#include "pch.h"
#include "ConfigurationsWindow.xaml.h"

#include "BackgroundWork.h"
#include "StringResources.h"

namespace native = ::midisettings;
namespace res = ::midisettings::resources;
namespace cfg = ::midisettings::configurations;
namespace engine = ::midisettings::configengine;
namespace documents = ::winrt::Microsoft::UI::Xaml::Documents;
namespace automation = ::winrt::Microsoft::UI::Xaml::Automation;

namespace winrt::midisettings::implementation
{
    namespace
    {
        constexpr wchar_t GlyphConfiguration{ 0xE81E };
        constexpr wchar_t GlyphDeviceNames{ 0xE8EC };
        constexpr wchar_t GlyphPictures{ 0xE8B9 };
        constexpr wchar_t GlyphLoopbacks{ 0xE8EE };
        constexpr wchar_t GlyphNetwork{ 0xE839 };
        constexpr wchar_t GlyphRtpMidi{ 0xE71B };
        constexpr wchar_t GlyphBluetooth{ 0xE702 };
        constexpr wchar_t GlyphSynthesizer{ 0xE8D6 };
        constexpr wchar_t GlyphOtherSettings{ 0xE713 };
        constexpr wchar_t GlyphMore{ 0xE712 };
        constexpr wchar_t GlyphRename{ 0xE8AC };
        constexpr wchar_t GlyphDuplicate{ 0xE8C8 };
        constexpr wchar_t GlyphBackUp{ 0xE81C };
        constexpr wchar_t GlyphShare{ 0xE72D };
        constexpr wchar_t GlyphDelete{ 0xE74D };

        // A name this long would push the row's buttons away, so it's trimmed instead.
        constexpr double MaximumRowNameWidth{ 520 };

        // A file written within this long of being created hasn't been changed since.
        constexpr uint64_t CreatedWindow{ 2ull * 10'000'000ull };

        // A configuration imported as a new one was written within this long of the import.
        constexpr uint64_t ImportedWindow{ 120ull * 10'000'000ull };

        struct ContentsPart
        {
            wchar_t Glyph{ 0 };
            winrt::hstring Text{};
        };

        winrt::hstring HostsAndConnections(_In_ size_t const hosts, _In_ size_t const connections)
        {
            std::vector<winrt::hstring> parts{};

            if (hosts > 0)
            {
                parts.push_back(cfg::CountText(L"CfgCountHosts", hosts));
            }

            if (connections > 0)
            {
                parts.push_back(cfg::CountText(L"CfgCountConnections", connections));
            }

            return cfg::ListText(parts);
        }

        // What a configuration has, in the order the comps show it.
        std::vector<ContentsPart> ContentsParts(_In_ engine::ConfigInventory const& inventory)
        {
            std::vector<ContentsPart> parts{};

            if (!inventory.DeviceNames.empty())
            {
                parts.push_back({ GlyphDeviceNames, cfg::CountText(L"CfgCountDeviceNames", inventory.DeviceNames.size()) });
            }

            if (!inventory.Pictures.empty())
            {
                parts.push_back({ GlyphPictures, cfg::CountText(L"CfgCountPictures", inventory.Pictures.size()) });
            }

            if (!inventory.Loopbacks.empty())
            {
                parts.push_back({ GlyphLoopbacks, cfg::CountText(L"CfgCountLoopbacks", inventory.Loopbacks.size()) });
            }

            auto const networkHosts = inventory.HostCount(engine::NetworkKind::NetworkMidi2);
            auto const networkClients = inventory.ClientCount(engine::NetworkKind::NetworkMidi2);

            if (networkHosts + networkClients > 0)
            {
                parts.push_back({ GlyphNetwork, res::FormatString(L"CfgContentsNetworkFormat", HostsAndConnections(networkHosts, networkClients)) });
            }

            auto const rtpHosts = inventory.HostCount(engine::NetworkKind::RtpMidi);
            auto const rtpClients = inventory.ClientCount(engine::NetworkKind::RtpMidi);

            if (rtpHosts + rtpClients > 0)
            {
                parts.push_back({ GlyphRtpMidi, res::FormatString(L"CfgContentsRtpFormat", HostsAndConnections(rtpHosts, rtpClients)) });
            }

            if (!inventory.BluetoothDevices.empty())
            {
                parts.push_back({ GlyphBluetooth, cfg::CountText(L"CfgCountBluetoothDevices", inventory.BluetoothDevices.size()) });
            }

            if (inventory.HasSynthesizer)
            {
                parts.push_back({ GlyphSynthesizer, res::GetString(L"CfgContentsSynthesizer") });
            }

            if (!inventory.OtherSections.empty())
            {
                parts.push_back({ GlyphOtherSettings, cfg::CountText(L"CfgCountOtherTransports", inventory.OtherSections.size()) });
            }

            return parts;
        }

        uint64_t Distance(_In_ FILETIME const& left, _In_ FILETIME const& right) noexcept
        {
            auto const a = engine::FileTimeValue(left);
            auto const b = engine::FileTimeValue(right);

            return a > b ? a - b : b - a;
        }

        winrt::hstring DayPhrase(
            _In_ FILETIME const& time,
            _In_ std::wstring_view const todayKey,
            _In_ std::wstring_view const yesterdayKey,
            _In_ std::wstring_view const dateKey)
        {
            auto const days = cfg::DaysAgo(time);

            if (days == 0)
            {
                return res::GetString(todayKey);
            }

            if (days == 1)
            {
                return res::GetString(yesterdayKey);
            }

            return res::FormatString(dateKey, cfg::DateText(time, L"CfgDatePictureShort"));
        }
    }

    void ConfigurationsWindow::RenderConfigurationsPage() noexcept
    {
        try
        {
            auto const switchPending = (m_pending.Kinds & cfg::restartkinds::Switch) != 0;
            auto const portNamesPending = (m_pending.Kinds & cfg::restartkinds::PortNames) != 0;
            auto const restartMessage = res::GetString(portNamesPending ? L"CfgRestartMessagePorts" : L"CfgRestartMessage");

            if (switchPending)
            {
                auto const* next = FindConfiguration(m_currentFileName);
                auto const nextName = next != nullptr ? cfg::ConfigDisplayName(*next) : m_currentFileName;

                SwitchPendingBar().Title(res::FormatString(L"CfgSwitchPendingTitleFormat", nextName));
                SwitchPendingBar().Message(restartMessage);
            }

            SwitchPendingBar().IsOpen(switchPending);

            ChangesPendingBar().Message(restartMessage);
            ChangesPendingBar().IsOpen(!switchPending &&
                (m_pending.Kinds & (cfg::restartkinds::Changes | cfg::restartkinds::PortNames)) != 0);

            auto const* inUse = FindConfiguration(RunningFileName());

            InUseCard().Visibility(inUse != nullptr ? xaml::Visibility::Visible : xaml::Visibility::Collapsed);
            NoConfigurationCard().Visibility(inUse == nullptr ? xaml::Visibility::Visible : xaml::Visibility::Collapsed);

            if (inUse != nullptr)
            {
                RenderInUseCard(*inUse);

                NewCopyItem().Text(res::FormatString(L"CfgNewCopyOfFormat", cfg::ConfigDisplayName(*inUse)));
                NewCopyItem().Visibility(xaml::Visibility::Visible);
            }
            else
            {
                NewCopyItem().Visibility(xaml::Visibility::Collapsed);
            }

            OtherConfigurationsPanel().Children().Clear();

            size_t others{ 0 };

            for (auto const& entry : m_configurations)
            {
                if (inUse != nullptr && engine::SameText(entry.FileName, inUse->FileName))
                {
                    continue;
                }

                OtherConfigurationsPanel().Children().Append(MakeConfigurationRow(entry));
                others++;
            }

            NoOtherConfigurationsText().Visibility(others == 0 && inUse != nullptr ? xaml::Visibility::Visible : xaml::Visibility::Collapsed);
        }
        MIDI_SETTINGS_CATCH_AND_LOG(L"Unable to show the configurations.")
    }

    _Use_decl_annotations_
    void ConfigurationsWindow::RenderInUseCard(engine::ConfigFileEntry const& entry) noexcept
    {
        try
        {
            auto const name = cfg::ConfigDisplayName(entry);
            auto const switchPending = (m_pending.Kinds & cfg::restartkinds::Switch) != 0;

            InUseNameText().Text(name);
            controls::ToolTipService::SetToolTip(InUseNameText(), winrt::box_value(winrt::hstring{ name }));

            InUseChip().Visibility(switchPending ? xaml::Visibility::Collapsed : xaml::Visibility::Visible);
            InUseUntilRestartChip().Visibility(switchPending ? xaml::Visibility::Visible : xaml::Visibility::Collapsed);

            // Keep using undoes a switch that hasn't happened yet. Backing up waits until it has.
            KeepUsingButton().Content(winrt::box_value(res::FormatString(L"CfgKeepUsingFormat", name)));
            KeepUsingButton().Visibility(switchPending ? xaml::Visibility::Visible : xaml::Visibility::Collapsed);
            BackUpInUseButton().Visibility(switchPending ? xaml::Visibility::Collapsed : xaml::Visibility::Visible);

            auto const renameName = res::FormatString(L"CfgRenameNamedFormat", name);
            automation::AutomationProperties::SetName(RenameInUseButton(), renameName);
            controls::ToolTipService::SetToolTip(RenameInUseButton(), winrt::box_value(renameName));

            if (!entry.Readable)
            {
                InUseChangedText().Text(res::GetString(L"CfgUnreadableConfiguration"));
                InUseContentsText().Blocks().Clear();
                return;
            }

            InUseChangedText().Text(ChangedText(entry, true));
            FillContentsText(InUseContentsText(), entry.Inventory);
        }
        MIDI_SETTINGS_CATCH_AND_LOG(L"Unable to show the configuration in use.")
    }

    _Use_decl_annotations_
    void ConfigurationsWindow::FillContentsText(controls::RichTextBlock const& block, engine::ConfigInventory const& inventory)
    {
        block.Blocks().Clear();

        documents::Paragraph paragraph{};

        auto const parts = ContentsParts(inventory);

        std::vector<winrt::hstring> names{};

        if (parts.empty())
        {
            documents::Run run{};
            run.Text(res::GetString(L"CfgNothingSetUp"));
            paragraph.Inlines().Append(run);

            names.push_back(run.Text());
        }

        for (auto const& part : parts)
        {
            // Each count is one piece, so a line only ever breaks between them.
            controls::StackPanel item{};
            item.Orientation(controls::Orientation::Horizontal);
            item.Spacing(8);
            item.Margin(xaml::Thickness{ 0, 0, 14, 0 });

            item.Children().Append(MakeGlyph(part.Glyph, L"CfgContentsGlyphStyle"));
            item.Children().Append(MakeText(part.Text, L"CfgContentsTextStyle"));

            documents::InlineUIContainer container{};
            container.Child(item);

            paragraph.Inlines().Append(container);

            documents::Run space{};
            space.Text(L" ");
            paragraph.Inlines().Append(space);

            names.push_back(part.Text);
        }

        block.Blocks().Append(paragraph);

        automation::AutomationProperties::SetName(block, cfg::ListText(names));
    }

    _Use_decl_annotations_
    winrt::hstring ConfigurationsWindow::ChangedText(engine::ConfigFileEntry const& entry, bool const withTime) const
    {
        // Imported as a new configuration, and not changed since.
        std::wstring computer{};
        FILETIME importedAt{};

        if (!withTime && cfg::TryGetImported(entry.FileName, computer, importedAt) &&
            Distance(importedAt, entry.LastWritten) <= ImportedWindow)
        {
            if (computer.empty())
            {
                return DayPhrase(importedAt, L"CfgImportedToday", L"CfgImportedYesterday", L"CfgImportedOnFormat");
            }

            auto const days = cfg::DaysAgo(importedAt);

            if (days == 0)
            {
                return res::FormatString(L"CfgImportedFromTodayFormat", computer);
            }

            if (days == 1)
            {
                return res::FormatString(L"CfgImportedFromYesterdayFormat", computer);
            }

            return res::FormatString(L"CfgImportedFromOnFormat", computer, cfg::DateText(importedAt, L"CfgDatePictureShort"));
        }

        if (!withTime)
        {
            if (Distance(entry.Created, entry.LastWritten) <= CreatedWindow)
            {
                return DayPhrase(entry.LastWritten, L"CfgCreatedToday", L"CfgCreatedYesterday", L"CfgCreatedOnFormat");
            }

            return DayPhrase(entry.LastWritten, L"CfgChangedToday", L"CfgChangedYesterday", L"CfgChangedOnFormat");
        }

        winrt::hstring changed{};

        switch (cfg::DaysAgo(entry.LastWritten))
        {
        case 0:
            changed = res::FormatString(L"CfgChangedTodayAtFormat", cfg::TimeText(entry.LastWritten));
            break;

        case 1:
            changed = res::FormatString(L"CfgChangedYesterdayAtFormat", cfg::TimeText(entry.LastWritten));
            break;

        default:
            changed = res::FormatString(L"CfgChangedOnFormat", cfg::DateText(entry.LastWritten, L"CfgDatePictureShort"));
            break;
        }

        // When the last change was an import or a restore, it says so.
        auto const backup = engine::BackupBeforeLastChange(entry.FileName, entry.LastWritten, m_backups);

        if (backup.has_value())
        {
            if (backup->Reason == engine::BackupReason::BeforeImport)
            {
                auto const what = backup->ReasonConfigName.empty() ? std::wstring{ res::GetString(L"CfgAPackage") } : backup->ReasonConfigName;

                return backup->ReasonComputerName.empty() ?
                    res::FormatString(L"CfgChangedWhenImportedFormat", changed, what) :
                    res::FormatString(L"CfgChangedWhenImportedFromFormat", changed, what, backup->ReasonComputerName);
            }

            if (backup->Reason == engine::BackupReason::BeforeRestore)
            {
                return res::FormatString(L"CfgChangedWhenRestoredFormat", changed);
            }
        }

        return changed;
    }

    _Use_decl_annotations_
    xaml::UIElement ConfigurationsWindow::MakeConfigurationRow(engine::ConfigFileEntry const& entry)
    {
        auto const name = cfg::ConfigDisplayName(entry);
        auto const fileName = entry.FileName;
        auto const switchPending = (m_pending.Kinds & cfg::restartkinds::Switch) != 0;
        auto const startsAfterRestart = switchPending && engine::SameText(entry.FileName, m_currentFileName);
        auto const isCurrent = engine::SameText(entry.FileName, m_currentFileName) || engine::SameText(entry.FileName, RunningFileName());

        controls::Border card{};
        card.Style(StyleNamed(L"CfgRowCardStyle"));

        controls::Grid grid{};
        grid.ColumnSpacing(14);

        for (auto const width : { xaml::GridLengthHelper::Auto(), xaml::GridLengthHelper::FromValueAndType(1, xaml::GridUnitType::Star), xaml::GridLengthHelper::Auto() })
        {
            controls::ColumnDefinition column{};
            column.Width(width);
            grid.ColumnDefinitions().Append(column);
        }

        grid.Children().Append(MakeGlyph(GlyphConfiguration, L"CfgRowGlyphStyle"));

        controls::StackPanel text{};
        text.Spacing(2);
        text.VerticalAlignment(xaml::VerticalAlignment::Center);
        controls::Grid::SetColumn(text, 1);

        controls::StackPanel titleLine{};
        titleLine.Orientation(controls::Orientation::Horizontal);
        titleLine.Spacing(8);

        auto title = MakeText(winrt::hstring{ name }, L"CfgRowTitleStyle");
        title.MaxWidth(MaximumRowNameWidth);
        controls::ToolTipService::SetToolTip(title, winrt::box_value(winrt::hstring{ name }));
        titleLine.Children().Append(title);

        if (startsAfterRestart)
        {
            titleLine.Children().Append(MakeChip(res::GetString(L"CfgStartsAfterRestartChip"), L"Caution"));
        }

        text.Children().Append(titleLine);

        auto const subtitle = entry.Readable ?
            res::FormatString(L"CfgRowSubtitleFormat", ChangedText(entry, false), cfg::ShortContentsText(entry.Inventory)) :
            res::GetString(L"CfgUnreadableConfiguration");

        auto subtitleText = MakeText(subtitle, L"CfgRowSubtitleStyle");
        controls::ToolTipService::SetToolTip(subtitleText, winrt::box_value(subtitle));
        text.Children().Append(subtitleText);

        grid.Children().Append(text);

        controls::StackPanel actions{};
        actions.Orientation(controls::Orientation::Horizontal);
        actions.Spacing(8);
        actions.VerticalAlignment(xaml::VerticalAlignment::Center);
        controls::Grid::SetColumn(actions, 2);

        auto weak = get_weak();

        // The one that starts after the restart is already chosen.
        if (!startsAfterRestart)
        {
            controls::Button use{};
            use.Content(winrt::box_value(res::GetString(L"CfgUseThisOne")));
            use.IsEnabled(m_elevated && entry.Readable);
            automation::AutomationProperties::SetName(use, res::FormatString(L"CfgUseThisOneNamedFormat", name));

            use.Click([weak, fileName](auto&&, auto&&)
                {
                    if (auto strong = weak.get())
                    {
                        strong->SwitchToAsync(fileName);
                    }
                });

            actions.Children().Append(use);
        }

        controls::Button more{};
        more.Style(StyleNamed(L"CfgSubtleIconButtonStyle"));

        controls::FontIcon moreGlyph{};
        moreGlyph.Glyph(winrt::hstring{ std::wstring(1, GlyphMore) });
        moreGlyph.FontSize(16);
        more.Content(moreGlyph);

        auto const moreName = res::FormatString(L"CfgMoreOptionsFormat", name);
        automation::AutomationProperties::SetName(more, moreName);
        controls::ToolTipService::SetToolTip(more, winrt::box_value(res::GetString(L"CfgMoreOptions")));

        controls::MenuFlyout menu{};
        menu.Placement(controls::Primitives::FlyoutPlacementMode::BottomEdgeAlignedRight);

        auto const addItem = [&menu](winrt::hstring const& label, wchar_t const glyph, bool const enabled, auto&& handler)
            {
                controls::MenuFlyoutItem item{};
                item.Text(label);

                controls::FontIcon icon{};
                icon.Glyph(winrt::hstring{ std::wstring(1, glyph) });
                item.Icon(icon);

                item.IsEnabled(enabled);
                item.Click(handler);

                menu.Items().Append(item);
            };

        addItem(res::GetString(L"CfgMenuRename"), GlyphRename, m_elevated && entry.Readable,
            [weak, fileName](auto&&, auto&&) { if (auto strong = weak.get()) { strong->RenameAsync(fileName); } });

        addItem(res::GetString(L"CfgMenuDuplicate"), GlyphDuplicate, m_elevated && entry.Readable,
            [weak, fileName](auto&&, auto&&) { if (auto strong = weak.get()) { strong->CreateAsync(fileName); } });

        addItem(res::GetString(L"CfgMenuBackUp"), GlyphBackUp, m_elevated && entry.Readable,
            [weak, fileName](auto&&, auto&&) { if (auto strong = weak.get()) { strong->BackUpAsync(fileName, strong->ConfigurationsStatusBar()); } });

        addItem(res::GetString(L"CfgMenuShare"), GlyphShare, entry.Readable,
            [weak, fileName](auto&&, auto&&) { if (auto strong = weak.get()) { strong->ShareConfiguration(fileName); } });

        menu.Items().Append(controls::MenuFlyoutSeparator{});

        // The one in use, and the one about to be, can't be deleted.
        addItem(res::GetString(L"CfgMenuDelete"), GlyphDelete, m_elevated && !isCurrent,
            [weak, fileName](auto&&, auto&&) { if (auto strong = weak.get()) { strong->DeleteConfigurationAsync(fileName); } });

        more.Flyout(menu);
        actions.Children().Append(more);

        grid.Children().Append(actions);

        card.Child(grid);

        return card;
    }

    _Use_decl_annotations_
    winrt::fire_and_forget ConfigurationsWindow::SwitchToAsync(std::wstring fileName)
    {
        auto lifetime = get_strong();

        try
        {
            if (!BeginWork())
            {
                co_return;
            }

            auto const* target = FindConfiguration(fileName);

            if (target == nullptr)
            {
                EndWork();
                co_return;
            }

            auto const targetName = cfg::ConfigDisplayName(*target);
            auto const running = RunningFileName();
            auto const* runningEntry = FindConfiguration(running);

            auto dialog = MakeDialog(
                res::FormatString(L"CfgSwitchTitleFormat", targetName),
                res::GetString(L"CfgRestartServiceCommand"),
                res::GetString(L"CfgRestartLater"),
                res::GetString(L"CfgCancel"));

            controls::StackPanel panel{};
            panel.Spacing(12);
            panel.Children().Append(MakeDialogText(res::FormatString(L"CfgSwitchTextFormat", targetName), false));

            if (runningEntry != nullptr)
            {
                panel.Children().Append(MakeDialogText(res::FormatString(L"CfgSwitchCaptionFormat", cfg::ConfigDisplayName(*runningEntry)), true));
            }

            dialog.Content(panel);

            auto const choice = co_await dialog.ShowAsync();

            if (choice == controls::ContentDialogResult::None || m_closing)
            {
                EndWork();
                co_return;
            }

            auto const environment = m_environment;
            engine::ConfigResult switched{};

            co_await native::RunOnBackgroundAsync([&switched, &environment, &fileName]()
                {
                    switched = engine::SwitchConfiguration(environment, fileName);
                });

            if (m_closing)
            {
                co_return;
            }

            if (!switched.Succeeded())
            {
                ShowFailure(ConfigurationsStatusBar(), res::FormatString(L"CfgSwitchFailedFormat", targetName), switched);
                EndWork();
                co_return;
            }

            m_currentFileName = fileName;

            // Going back to the one the service still runs leaves nothing to wait for.
            if (engine::SameText(fileName, running) && (m_pending.Kinds & cfg::restartkinds::Switch) != 0)
            {
                auto pending = m_pending;
                pending.Kinds &= ~cfg::restartkinds::Switch;
                pending.RunningFileName.clear();

                cfg::WritePendingRestart(m_serviceProcessId, pending);
                m_pending = pending;
            }
            else
            {
                RecordPendingRestart(cfg::restartkinds::Switch, running);
            }

            ConfigurationsStatusBar().IsOpen(false);

            if (choice == controls::ContentDialogResult::Primary)
            {
                co_await RestartServiceAsync(ConfigurationsStatusBar());
            }
            else
            {
                co_await RefreshAsync();
            }

            EndWork();
        }
        catch (...)
        {
            EndWork();
            MIDI_SETTINGS_LOG_GENERAL_EXCEPTION(L"Unable to switch configurations.");
        }
    }

    _Use_decl_annotations_
    winrt::fire_and_forget ConfigurationsWindow::OnKeepUsingClick(foundation::IInspectable const&, xaml::RoutedEventArgs const&)
    {
        auto lifetime = get_strong();

        try
        {
            if ((m_pending.Kinds & cfg::restartkinds::Switch) == 0 || m_pending.RunningFileName.empty() || !BeginWork())
            {
                co_return;
            }

            auto const running = m_pending.RunningFileName;
            auto const environment = m_environment;

            engine::ConfigResult switched{};

            co_await native::RunOnBackgroundAsync([&switched, &environment, &running]()
                {
                    switched = engine::SwitchConfiguration(environment, running);
                });

            if (m_closing)
            {
                co_return;
            }

            if (switched.Succeeded())
            {
                auto pending = m_pending;
                pending.Kinds &= ~cfg::restartkinds::Switch;
                pending.RunningFileName.clear();

                cfg::WritePendingRestart(m_serviceProcessId, pending);

                m_pending = pending;
                m_currentFileName = running;

                ConfigurationsStatusBar().IsOpen(false);
            }
            else
            {
                auto const* entry = FindConfiguration(running);

                ShowFailure(
                    ConfigurationsStatusBar(),
                    res::FormatString(L"CfgSwitchFailedFormat", entry != nullptr ? cfg::ConfigDisplayName(*entry) : running),
                    switched);
            }

            co_await RefreshAsync();

            EndWork();
        }
        catch (...)
        {
            EndWork();
            MIDI_SETTINGS_LOG_GENERAL_EXCEPTION(L"Unable to keep the configuration in use.");
        }
    }

    _Use_decl_annotations_
    winrt::fire_and_forget ConfigurationsWindow::OnRenameInUseClick(foundation::IInspectable const&, xaml::RoutedEventArgs const&)
    {
        RenameAsync(RunningFileName());
        co_return;
    }

    _Use_decl_annotations_
    winrt::fire_and_forget ConfigurationsWindow::OnBackUpInUseClick(foundation::IInspectable const&, xaml::RoutedEventArgs const&)
    {
        BackUpAsync(RunningFileName(), ConfigurationsStatusBar());
        co_return;
    }

    _Use_decl_annotations_
    void ConfigurationsWindow::OnShareInUseClick(foundation::IInspectable const&, xaml::RoutedEventArgs const&)
    {
        ShareConfiguration(RunningFileName());
    }

    _Use_decl_annotations_
    winrt::fire_and_forget ConfigurationsWindow::OnNewEmptyClick(foundation::IInspectable const&, xaml::RoutedEventArgs const&)
    {
        CreateAsync({});
        co_return;
    }

    _Use_decl_annotations_
    winrt::fire_and_forget ConfigurationsWindow::OnNewCopyClick(foundation::IInspectable const&, xaml::RoutedEventArgs const&)
    {
        CreateAsync(RunningFileName());
        co_return;
    }

    _Use_decl_annotations_
    winrt::fire_and_forget ConfigurationsWindow::RenameAsync(std::wstring fileName)
    {
        auto lifetime = get_strong();

        try
        {
            auto const* entry = FindConfiguration(fileName);

            if (entry == nullptr || !BeginWork())
            {
                co_return;
            }

            auto const oldName = cfg::ConfigDisplayName(*entry);

            auto const name = co_await AskForNameAsync(
                res::FormatString(L"CfgRenameTitleFormat", oldName),
                res::GetString(L"CfgRenameButton"),
                winrt::hstring{ oldName },
                fileName);

            if (m_closing)
            {
                co_return;
            }

            if (name.empty() || std::wstring_view{ name } == oldName)
            {
                EndWork();
                co_return;
            }

            auto const environment = m_environment;
            std::wstring const newName{ name };

            engine::ConfigResult renamed{};

            co_await native::RunOnBackgroundAsync([&renamed, &environment, &fileName, &newName]()
                {
                    renamed = engine::RenameConfiguration(environment, fileName, newName);
                });

            if (m_closing)
            {
                co_return;
            }

            if (renamed.Succeeded())
            {
                ConfigurationsStatusBar().IsOpen(false);
            }
            else
            {
                ShowFailure(ConfigurationsStatusBar(), res::FormatString(L"CfgRenameFailedFormat", oldName), renamed);
            }

            co_await RefreshAsync();

            EndWork();
        }
        catch (...)
        {
            EndWork();
            MIDI_SETTINGS_LOG_GENERAL_EXCEPTION(L"Unable to rename a configuration.");
        }
    }

    _Use_decl_annotations_
    winrt::fire_and_forget ConfigurationsWindow::CreateAsync(std::wstring copyOfFileName)
    {
        auto lifetime = get_strong();

        try
        {
            if (!BeginWork())
            {
                co_return;
            }

            winrt::hstring initialName{};

            if (!copyOfFileName.empty())
            {
                auto const* source = FindConfiguration(copyOfFileName);

                if (source == nullptr)
                {
                    EndWork();
                    co_return;
                }

                initialName = res::FormatString(L"CfgCopyOfFormat", cfg::ConfigDisplayName(*source));
            }

            auto const name = co_await AskForNameAsync(
                res::GetString(copyOfFileName.empty() ? L"CfgNewConfigurationTitle" : L"CfgDuplicateTitle"),
                res::GetString(L"CfgCreateButton"),
                initialName,
                {});

            if (m_closing)
            {
                co_return;
            }

            if (name.empty())
            {
                EndWork();
                co_return;
            }

            auto const environment = m_environment;
            std::wstring const newName{ name };

            engine::CreateConfigResult created{};

            co_await native::RunOnBackgroundAsync([&created, &environment, &newName, &copyOfFileName]()
                {
                    created = engine::CreateConfiguration(environment, newName, copyOfFileName);
                });

            if (m_closing)
            {
                co_return;
            }

            if (created.Succeeded())
            {
                ShowStatus(
                    ConfigurationsStatusBar(),
                    controls::InfoBarSeverity::Success,
                    res::FormatString(L"CfgCreatedTitleFormat", newName),
                    res::GetString(L"CfgCreatedMessage"));
            }
            else
            {
                ShowFailure(ConfigurationsStatusBar(), res::FormatString(L"CfgCreateFailedFormat", newName), created);
            }

            co_await RefreshAsync();

            EndWork();
        }
        catch (...)
        {
            EndWork();
            MIDI_SETTINGS_LOG_GENERAL_EXCEPTION(L"Unable to create a configuration.");
        }
    }

    _Use_decl_annotations_
    winrt::fire_and_forget ConfigurationsWindow::BackUpAsync(std::wstring fileName, controls::InfoBar statusBar)
    {
        auto lifetime = get_strong();

        try
        {
            auto const* entry = FindConfiguration(fileName);

            if (entry == nullptr || !BeginWork())
            {
                co_return;
            }

            auto const name = cfg::ConfigDisplayName(*entry);
            auto const environment = m_environment;

            engine::BackupResult made{};

            co_await native::RunOnBackgroundAsync([&made, &environment, &fileName]()
                {
                    made = engine::MakeBackup(environment, fileName, engine::BackupReason::YouBackedItUp, {}, {});
                });

            if (m_closing)
            {
                co_return;
            }

            if (made.Succeeded())
            {
                ShowStatus(
                    statusBar,
                    controls::InfoBarSeverity::Success,
                    res::FormatString(L"CfgBackedUpTitleFormat", name),
                    res::GetString(L"CfgBackedUpMessage"));
            }
            else
            {
                ShowFailure(statusBar, res::FormatString(L"CfgBackUpFailedFormat", name), made);
            }

            co_await RefreshAsync();

            EndWork();
        }
        catch (...)
        {
            EndWork();
            MIDI_SETTINGS_LOG_GENERAL_EXCEPTION(L"Unable to back up a configuration.");
        }
    }

    _Use_decl_annotations_
    winrt::fire_and_forget ConfigurationsWindow::DeleteConfigurationAsync(std::wstring fileName)
    {
        auto lifetime = get_strong();

        try
        {
            auto const* entry = FindConfiguration(fileName);

            if (entry == nullptr || !BeginWork())
            {
                co_return;
            }

            auto const name = cfg::ConfigDisplayName(*entry);

            auto dialog = MakeDialog(
                res::FormatString(L"CfgDeleteTitleFormat", name),
                res::GetString(L"CfgDeleteButton"),
                {},
                res::GetString(L"CfgCancel"));

            dialog.Content(MakeDialogText(res::GetString(L"CfgDeleteText"), false));

            auto const choice = co_await dialog.ShowAsync();

            if (m_closing)
            {
                co_return;
            }

            if (choice != controls::ContentDialogResult::Primary)
            {
                EndWork();
                co_return;
            }

            auto const environment = m_environment;
            engine::ConfigResult deleted{};

            co_await native::RunOnBackgroundAsync([&deleted, &environment, &fileName]()
                {
                    deleted = engine::DeleteConfiguration(environment, fileName);
                });

            if (m_closing)
            {
                co_return;
            }

            if (deleted.Succeeded())
            {
                ShowStatus(
                    ConfigurationsStatusBar(),
                    controls::InfoBarSeverity::Success,
                    res::FormatString(L"CfgDeletedTitleFormat", name),
                    res::GetString(L"CfgDeletedMessage"));
            }
            else
            {
                ShowFailure(ConfigurationsStatusBar(), res::FormatString(L"CfgDeleteFailedFormat", name), deleted);
            }

            co_await RefreshAsync();

            EndWork();
        }
        catch (...)
        {
            EndWork();
            MIDI_SETTINGS_LOG_GENERAL_EXCEPTION(L"Unable to delete a configuration.");
        }
    }

    _Use_decl_annotations_
    void ConfigurationsWindow::ShareConfiguration(std::wstring const& fileName) noexcept
    {
        try
        {
            if (FindConfiguration(fileName) == nullptr)
            {
                return;
            }

            m_shareFileName = fileName;

            ShareSavedBar().IsOpen(false);
            ShareStatusBar().IsOpen(false);

            RenderSharePage();
            SelectPage(L"share");
        }
        MIDI_SETTINGS_CATCH_AND_LOG(L"Unable to share a configuration.")
    }
}
