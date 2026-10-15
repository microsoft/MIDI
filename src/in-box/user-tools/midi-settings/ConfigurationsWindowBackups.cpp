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

// The Backups page: every backup, newest first, what's in the one picked, and putting it back.

#include "pch.h"
#include "ConfigurationsWindow.xaml.h"

#include "AppSettings.h"
#include "BackgroundWork.h"
#include "ConfigShare.h"
#include "ConfigWrite.h"
#include "StringResources.h"

namespace native = ::midisettings;
namespace res = ::midisettings::resources;
namespace cfg = ::midisettings::configurations;
namespace engine = ::midisettings::configengine;
namespace automation = ::winrt::Microsoft::UI::Xaml::Automation;

namespace winrt::midisettings::implementation
{
    namespace
    {
        constexpr wchar_t GlyphBackUp{ 0xE81C };
        constexpr wchar_t GlyphYouBackedItUp{ 0xE77B };
        constexpr wchar_t GlyphBeforeImport{ 0xE8B5 };
        constexpr wchar_t GlyphBeforeRestore{ 0xE777 };
        constexpr wchar_t GlyphBeforeDelete{ 0xE74D };
        constexpr wchar_t GlyphDaily{ 0xE823 };

        // Backups older than this are behind Show them, so the list starts with the ones people
        // are most likely to want.
        constexpr uint64_t RecentDays{ 30 };
        constexpr uint64_t FileTimeTicksPerDay{ 24ull * 60 * 60 * 10'000'000ull };

        // Delete old backups: how far back to keep, and how many of each configuration.
        constexpr uint32_t KeepDays[]{ 7, 30, 90, 365 };
        constexpr int32_t DefaultKeepIndex{ 1 };
        constexpr uint32_t KeepNewestOfEach{ 5 };

        constexpr double TableValueWidth{ 92 };
        constexpr double TableNowWidth{ 76 };
        constexpr double TableRowHeight{ 30 };

        wchar_t BackupGlyph(_In_ engine::BackupEntry const& backup) noexcept
        {
            if (backup.Kind == engine::BackupKind::Daily)
            {
                return GlyphDaily;
            }

            switch (backup.Reason)
            {
            case engine::BackupReason::YouBackedItUp: return GlyphYouBackedItUp;
            case engine::BackupReason::BeforeImport: return GlyphBeforeImport;
            case engine::BackupReason::BeforeRestore: return GlyphBeforeRestore;
            case engine::BackupReason::BeforeDelete: return GlyphBeforeDelete;
            default: return GlyphBackUp;
            }
        }

        std::wstring BackupName(_In_ engine::BackupEntry const& backup)
        {
            if (!backup.ConfigName.empty())
            {
                return backup.ConfigName;
            }

            auto name = backup.ConfigFileName;

            if (engine::EndsWithText(name, engine::ConfigFileSuffix))
            {
                name.resize(name.size() - engine::ConfigFileSuffix.size());
            }

            return name;
        }

        FILETIME DaysBefore(_In_ FILETIME const& time, _In_ uint64_t const days) noexcept
        {
            auto const value = engine::FileTimeValue(time);
            auto const span = days * FileTimeTicksPerDay;
            auto const result = value > span ? value - span : 0;

            FILETIME before{};
            before.dwLowDateTime = static_cast<DWORD>(result & 0xFFFFFFFF);
            before.dwHighDateTime = static_cast<DWORD>(result >> 32);

            return before;
        }
    }

    _Use_decl_annotations_
    winrt::hstring ConfigurationsWindow::BackupReasonText(engine::BackupEntry const& backup, bool const sentence) const
    {
        if (backup.Kind == engine::BackupKind::Daily)
        {
            return res::GetString(sentence ? L"CfgReasonDailySentence" : L"CfgReasonDaily");
        }

        if (backup.Kind == engine::BackupKind::Earlier)
        {
            return res::GetString(sentence ? L"CfgReasonEarlierSentence" : L"CfgReasonEarlier");
        }

        switch (backup.Reason)
        {
        case engine::BackupReason::YouBackedItUp:
            return res::GetString(sentence ? L"CfgReasonYouSentence" : L"CfgReasonYou");

        case engine::BackupReason::BeforeImport:
            if (backup.ReasonComputerName.empty())
            {
                return res::GetString(sentence ? L"CfgReasonImportSentence" : L"CfgReasonImport");
            }

            return res::FormatString(sentence ? L"CfgReasonImportFromSentenceFormat" : L"CfgReasonImportFromFormat", backup.ReasonComputerName);

        case engine::BackupReason::BeforeRestore:
            return res::GetString(sentence ? L"CfgReasonRestoreSentence" : L"CfgReasonRestore");

        case engine::BackupReason::BeforeDelete:
            return res::GetString(sentence ? L"CfgReasonDeleteSentence" : L"CfgReasonDelete");

        default:
            return res::GetString(sentence ? L"CfgReasonOtherSentence" : L"CfgReasonOther");
        }
    }

    void ConfigurationsWindow::RenderBackupsPage() noexcept
    {
        try
        {
            auto const* inUse = FindConfiguration(RunningFileName());

            auto const backUpText = inUse != nullptr ?
                res::FormatString(L"CfgBackUpNamedNowFormat", cfg::ConfigDisplayName(*inUse)) :
                res::GetString(L"CfgBackUpNow");

            BackUpNowText().Text(backUpText);
            automation::AutomationProperties::SetName(BackUpNowButton(), backUpText);

            // One filter entry per configuration that has backups, by the name the newest one has.
            std::vector<std::pair<std::wstring, std::wstring>> configurations{};

            for (auto const& backup : m_backups)
            {
                auto const known = std::any_of(configurations.begin(), configurations.end(),
                    [&backup](auto const& entry) { return engine::SameText(entry.first, backup.ConfigFileName); });

                if (!known)
                {
                    configurations.emplace_back(backup.ConfigFileName, BackupName(backup));
                }
            }

            std::sort(configurations.begin(), configurations.end(), [](auto const& left, auto const& right)
                {
                    return ::CompareStringEx(LOCALE_NAME_USER_DEFAULT, LINGUISTIC_IGNORECASE,
                        left.second.c_str(), static_cast<int>(left.second.size()),
                        right.second.c_str(), static_cast<int>(right.second.size()),
                        nullptr, nullptr, 0) == CSTR_LESS_THAN;
                });

            m_suppressBackupHandling = true;

            auto const combo = BackupFilterComboBox();
            combo.Items().Clear();

            controls::ComboBoxItem all{};
            all.Content(winrt::box_value(res::GetString(L"CfgAllConfigurations")));
            all.Tag(winrt::box_value(winrt::hstring{}));
            combo.Items().Append(all);

            int32_t selected{ 0 };

            for (auto const& [fileName, name] : configurations)
            {
                controls::ComboBoxItem item{};
                item.Content(winrt::box_value(winrt::hstring{ name }));
                item.Tag(winrt::box_value(winrt::hstring{ fileName }));

                if (engine::SameText(fileName, m_backupFilter))
                {
                    selected = static_cast<int32_t>(combo.Items().Size());
                }

                combo.Items().Append(item);
            }

            if (selected == 0)
            {
                m_backupFilter.clear();
            }

            combo.SelectedIndex(selected);

            m_suppressBackupHandling = false;

            NoBackupsText().Visibility(m_backups.empty() ? xaml::Visibility::Visible : xaml::Visibility::Collapsed);

            RenderBackupList();
        }
        catch (...)
        {
            m_suppressBackupHandling = false;
            MIDI_SETTINGS_LOG_GENERAL_EXCEPTION(L"Unable to show the backups.");
        }
    }

    void ConfigurationsWindow::RenderBackupList() noexcept
    {
        try
        {
            m_suppressBackupHandling = true;

            auto const list = BackupList();
            list.Items().Clear();
            m_backupListIndexes.clear();

            auto const now = engine::CurrentFileTime();
            auto const recentLimit = engine::FileTimeValue(DaysBefore(now, RecentDays));

            int64_t lastDay{ -1 };
            size_t older{ 0 };
            std::optional<int32_t> selectedIndex{};
            std::optional<int32_t> firstRowIndex{};

            for (size_t index = 0; index < m_backups.size(); ++index)
            {
                auto const& backup = m_backups[index];

                if (!m_backupFilter.empty() && !engine::SameText(backup.ConfigFileName, m_backupFilter))
                {
                    continue;
                }

                if (!m_showOlderBackups && engine::FileTimeValue(backup.MadeAt) < recentLimit)
                {
                    older++;
                    continue;
                }

                auto const days = cfg::DaysAgo(backup.MadeAt);

                if (days != lastDay)
                {
                    lastDay = days;

                    auto const label = days == 0 ? res::GetString(L"CfgToday") :
                        days == 1 ? res::GetString(L"CfgYesterday") :
                        cfg::DateText(backup.MadeAt, L"CfgDatePictureDay");

                    // a heading, not something to pick
                    controls::ListViewItem heading{};
                    heading.Style(StyleNamed(L"CfgBackupHeadingItemStyle"));
                    heading.Content(MakeText(label, L"CfgGroupLabelStyle"));

                    list.Items().Append(heading);
                    m_backupListIndexes.push_back(std::nullopt);
                }

                auto const listIndex = static_cast<int32_t>(list.Items().Size());

                list.Items().Append(MakeBackupRow(backup));
                m_backupListIndexes.push_back(index);

                if (!firstRowIndex.has_value())
                {
                    firstRowIndex = listIndex;
                }

                if (m_selectedBackup.has_value() && engine::SameText(m_selectedBackup->Path, backup.Path))
                {
                    selectedIndex = listIndex;
                }
            }

            OlderBackupsRow().Visibility(older > 0 ? xaml::Visibility::Visible : xaml::Visibility::Collapsed);

            if (older > 0)
            {
                OlderBackupsTitle().Text(cfg::CountText(L"CfgCountOlderBackups", older));
                OlderBackupsCaption().Text(res::FormatString(L"CfgOlderFromBeforeFormat", cfg::DateText(DaysBefore(now, RecentDays), L"CfgDatePictureLong")));
            }

            m_suppressBackupHandling = false;

            // The newest is picked to start with, so there's always something to compare.
            if (!selectedIndex.has_value())
            {
                selectedIndex = firstRowIndex;
            }

            if (selectedIndex.has_value())
            {
                list.SelectedIndex(*selectedIndex);
            }
            else
            {
                m_selectedBackup.reset();
                RenderBackupDetail();
            }
        }
        catch (...)
        {
            m_suppressBackupHandling = false;
            MIDI_SETTINGS_LOG_GENERAL_EXCEPTION(L"Unable to list the backups.");
        }
    }

    _Use_decl_annotations_
    controls::ListViewItem ConfigurationsWindow::MakeBackupRow(engine::BackupEntry const& backup)
    {
        auto const name = winrt::hstring{ BackupName(backup) };
        auto const reason = BackupReasonText(backup, false);
        auto const time = cfg::TimeText(backup.MadeAt);

        auto const subtitle = backup.IsConfigurationOnly() ?
            res::FormatString(L"CfgBackupRowConfigOnlyFormat", time, reason) :
            res::FormatString(L"CfgBackupRowFormat", time, reason);

        controls::ListViewItem item{};
        item.Style(StyleNamed(L"CfgBackupRowItemStyle"));

        controls::Grid grid{};
        grid.ColumnSpacing(14);

        controls::ColumnDefinition iconColumn{};
        iconColumn.Width(xaml::GridLengthHelper::Auto());
        grid.ColumnDefinitions().Append(iconColumn);

        controls::ColumnDefinition textColumn{};
        textColumn.Width(xaml::GridLengthHelper::FromValueAndType(1, xaml::GridUnitType::Star));
        grid.ColumnDefinitions().Append(textColumn);

        grid.Children().Append(MakeGlyph(BackupGlyph(backup), L"CfgRowGlyphStyle"));

        controls::StackPanel text{};
        text.Spacing(2);
        text.VerticalAlignment(xaml::VerticalAlignment::Center);
        controls::Grid::SetColumn(text, 1);

        text.Children().Append(MakeText(name, L"CfgRowTitleStyle"));
        text.Children().Append(MakeText(subtitle, L"CfgRowSubtitleStyle"));

        grid.Children().Append(text);

        item.Content(grid);

        // The heading above it isn't read with it, so the day is part of its name.
        automation::AutomationProperties::SetName(item, res::FormatString(
            L"CfgBackupRowAccessibleFormat",
            name,
            cfg::WhenText(backup.MadeAt, false),
            backup.IsConfigurationOnly() ? res::FormatString(L"CfgReasonConfigOnlyFormat", reason) : reason));

        return item;
    }

    _Use_decl_annotations_
    void ConfigurationsWindow::OnBackupFilterChanged(foundation::IInspectable const&, controls::SelectionChangedEventArgs const&)
    {
        try
        {
            if (m_suppressBackupHandling)
            {
                return;
            }

            auto const item = BackupFilterComboBox().SelectedItem().try_as<controls::ComboBoxItem>();

            m_backupFilter = item != nullptr ? std::wstring{ winrt::unbox_value_or<winrt::hstring>(item.Tag(), winrt::hstring{}) } : std::wstring{};

            RenderBackupList();
        }
        MIDI_SETTINGS_CATCH_AND_LOG(L"Unable to filter the backups.")
    }

    _Use_decl_annotations_
    void ConfigurationsWindow::OnShowOlderBackupsClick(foundation::IInspectable const&, xaml::RoutedEventArgs const&)
    {
        try
        {
            m_showOlderBackups = true;

            RenderBackupList();
        }
        MIDI_SETTINGS_CATCH_AND_LOG(L"Unable to show the older backups.")
    }

    _Use_decl_annotations_
    void ConfigurationsWindow::OnBackupSelectionChanged(foundation::IInspectable const&, controls::SelectionChangedEventArgs const& args)
    {
        try
        {
            if (m_suppressBackupHandling)
            {
                return;
            }

            auto const index = BackupList().SelectedIndex();

            if (index < 0 || static_cast<size_t>(index) >= m_backupListIndexes.size())
            {
                m_selectedBackup.reset();
                RenderBackupDetail();
                return;
            }

            auto const& backupIndex = m_backupListIndexes[static_cast<size_t>(index)];

            // A heading can't be picked. The keyboard goes on to the row in the direction it was going.
            if (!backupIndex.has_value())
            {
                auto goingUp = false;

                if (args != nullptr && args.RemovedItems().Size() > 0)
                {
                    uint32_t removedIndex{ 0 };

                    if (BackupList().Items().IndexOf(args.RemovedItems().GetAt(0), removedIndex))
                    {
                        goingUp = static_cast<int32_t>(removedIndex) > index;
                    }
                }

                auto const step = goingUp ? -1 : 1;

                for (auto next = index + step; next >= 0 && static_cast<size_t>(next) < m_backupListIndexes.size(); next += step)
                {
                    if (m_backupListIndexes[static_cast<size_t>(next)].has_value())
                    {
                        BackupList().SelectedIndex(next);
                        return;
                    }
                }

                return;
            }

            auto const& backup = m_backups[*backupIndex];

            if (m_selectedBackup.has_value() && engine::SameText(m_selectedBackup->Path, backup.Path) && m_selectedBackupInventory.has_value())
            {
                RenderBackupDetail();
                return;
            }

            m_selectedBackup = backup;

            LoadBackupDetailAsync(backup);
        }
        MIDI_SETTINGS_CATCH_AND_LOG(L"Unable to show a backup.")
    }

    _Use_decl_annotations_
    winrt::fire_and_forget ConfigurationsWindow::LoadBackupDetailAsync(engine::BackupEntry backup)
    {
        auto lifetime = get_strong();

        try
        {
            auto const generation = ++m_backupDetailGeneration;

            m_selectedBackupInventory.reset();
            m_selectedBackupPictures = 0;
            m_selectedBackupReadFailed = false;

            RenderBackupDetail();

            engine::ConfigInventory inventory{};
            uint32_t pictures{ 0 };
            bool read{ false };

            co_await native::RunOnBackgroundAsync([&inventory, &pictures, &read, &backup]()
                {
                    if (backup.Kind == engine::BackupKind::Full)
                    {
                        engine::PackageContents contents{};

                        if (engine::ReadPackage(backup.Path, contents).Succeeded())
                        {
                            inventory = engine::TakeInventory(std::string_view{ contents.ConfigurationBytes });
                            pictures = static_cast<uint32_t>(contents.Pictures.size());
                            read = inventory.IsConfiguration;
                        }
                    }
                    else
                    {
                        std::string bytes{};

                        if (engine::ReadWholeFile(backup.Path, engine::MaximumConfigBytes, bytes).Succeeded())
                        {
                            inventory = engine::TakeInventory(std::string_view{ bytes });
                            read = inventory.IsConfiguration;
                        }
                    }
                });

            // another one was picked while this one was read
            if (m_closing || generation != m_backupDetailGeneration)
            {
                co_return;
            }

            if (read)
            {
                m_selectedBackupInventory = std::move(inventory);
                m_selectedBackupPictures = pictures;
            }
            else
            {
                m_selectedBackupReadFailed = true;
            }

            RenderBackupDetail();
        }
        MIDI_SETTINGS_CATCH_AND_LOG(L"Unable to read a backup.")
    }

    void ConfigurationsWindow::RenderBackupDetail() noexcept
    {
        try
        {
            ApplyWorkState();

            if (!m_selectedBackup.has_value())
            {
                BackupDetailCard().Visibility(xaml::Visibility::Collapsed);
                return;
            }

            auto const& backup = *m_selectedBackup;

            BackupDetailCard().Visibility(xaml::Visibility::Visible);
            BackupDetailName().Text(BackupName(backup));
            BackupDetailWhen().Text(res::FormatString(L"CfgBackupWhenFormat", cfg::WhenText(backup.MadeAt, true), BackupReasonText(backup, true)));

            auto const table = BackupDetailTable();
            table.Children().Clear();
            table.RowDefinitions().Clear();
            table.ColumnDefinitions().Clear();

            if (!m_selectedBackupInventory.has_value())
            {
                BackupDetailKind().Text(res::GetString(m_selectedBackupReadFailed ? L"CfgBackupUnreadable" : L"CfgBackupReading"));
                return;
            }

            for (auto const width : {
                xaml::GridLengthHelper::FromValueAndType(1, xaml::GridUnitType::Star),
                xaml::GridLengthHelper::FromPixels(TableValueWidth),
                xaml::GridLengthHelper::FromPixels(TableNowWidth) })
            {
                controls::ColumnDefinition column{};
                column.Width(width);
                table.ColumnDefinitions().Append(column);
            }

            auto const& then = *m_selectedBackupInventory;
            auto const* now = FindConfiguration(backup.ConfigFileName);
            auto const noValue = res::GetString(L"CfgNoValue");

            struct Row
            {
                std::wstring_view LabelKey;
                winrt::hstring Then;
                winrt::hstring Now;
            };

            auto const count = [](size_t const value) { return winrt::to_hstring(value); };
            auto const nowCount = [now, &noValue](auto const& select) { return now != nullptr && now->Readable ? winrt::to_hstring(select(now->Inventory)) : noValue; };

            std::vector<Row> const rows
            {
                { L"CfgTableDeviceNames", count(then.DeviceNames.size()), nowCount([](auto const& i) { return i.DeviceNames.size(); }) },
                { L"CfgTablePictures", count(then.Pictures.size()), nowCount([](auto const& i) { return i.Pictures.size(); }) },
                { L"CfgTableLoopbacks", count(then.Loopbacks.size()), nowCount([](auto const& i) { return i.Loopbacks.size(); }) },
                { L"CfgTableNetworkHosts", count(then.HostCount(engine::NetworkKind::NetworkMidi2)), nowCount([](auto const& i) { return i.HostCount(engine::NetworkKind::NetworkMidi2); }) },
                { L"CfgTableNetworkConnections", count(then.ClientCount(engine::NetworkKind::NetworkMidi2)), nowCount([](auto const& i) { return i.ClientCount(engine::NetworkKind::NetworkMidi2); }) },
                { L"CfgTableRtpHosts", count(then.HostCount(engine::NetworkKind::RtpMidi)), nowCount([](auto const& i) { return i.HostCount(engine::NetworkKind::RtpMidi); }) },
                { L"CfgTableBluetoothDevices", count(then.BluetoothDevices.size()), nowCount([](auto const& i) { return i.BluetoothDevices.size(); }) },
                { L"CfgTableSynthMode", cfg::SynthModeText(then.SynthMode), now != nullptr && now->Readable ? cfg::SynthModeText(now->Inventory.SynthMode) : noValue },
            };

            auto const addRow = [this, &table](winrt::hstring const& label, winrt::hstring const& thenText, winrt::hstring const& nowText, bool const heading, bool const last)
                {
                    auto const row = static_cast<int32_t>(table.RowDefinitions().Size());

                    controls::RowDefinition definition{};
                    definition.Height(xaml::GridLengthHelper::Auto());
                    definition.MinHeight(heading ? 22 : TableRowHeight);
                    table.RowDefinitions().Append(definition);

                    auto labelText = MakeText(label, heading ? L"CfgTableHeaderStyle" : L"CfgTableKeyStyle");
                    auto thenBlock = MakeText(thenText, heading ? L"CfgTableHeaderStyle" : L"CfgTableValueStyle");
                    auto nowBlock = MakeText(nowText, heading ? L"CfgTableHeaderStyle" : L"CfgTableNowStyle");

                    thenBlock.HorizontalAlignment(xaml::HorizontalAlignment::Right);
                    nowBlock.HorizontalAlignment(xaml::HorizontalAlignment::Right);

                    controls::Grid::SetRow(labelText, row);
                    controls::Grid::SetRow(thenBlock, row);
                    controls::Grid::SetRow(nowBlock, row);
                    controls::Grid::SetColumn(thenBlock, 1);
                    controls::Grid::SetColumn(nowBlock, 2);

                    if (!last)
                    {
                        xaml::Shapes::Rectangle divider{};
                        divider.Style(StyleNamed(L"CfgDividerStyle"));
                        divider.VerticalAlignment(xaml::VerticalAlignment::Bottom);
                        controls::Grid::SetRow(divider, row);
                        controls::Grid::SetColumnSpan(divider, 3);
                        table.Children().Append(divider);
                    }

                    table.Children().Append(labelText);
                    table.Children().Append(thenBlock);
                    table.Children().Append(nowBlock);
                };

            addRow(res::GetString(L"CfgTableWhatsInIt"), res::GetString(L"CfgTableThisBackup"), res::GetString(L"CfgTableNow"), true, false);

            for (size_t index = 0; index < rows.size(); ++index)
            {
                addRow(res::GetString(rows[index].LabelKey), rows[index].Then, rows[index].Now, false, index + 1 == rows.size());
            }

            if (backup.IsConfigurationOnly())
            {
                BackupDetailKind().Text(res::GetString(L"CfgBackupConfigOnly"));
            }
            else if (m_selectedBackupPictures == 0)
            {
                BackupDetailKind().Text(res::GetString(L"CfgBackupFullNoPictures"));
            }
            else
            {
                BackupDetailKind().Text(cfg::CountText(L"CfgBackupFullPictures", m_selectedBackupPictures));
            }
        }
        MIDI_SETTINGS_CATCH_AND_LOG(L"Unable to show what's in a backup.")
    }

    _Use_decl_annotations_
    winrt::fire_and_forget ConfigurationsWindow::OnBackUpNowClick(foundation::IInspectable const&, xaml::RoutedEventArgs const&)
    {
        BackUpAsync(RunningFileName(), BackupsStatusBar());
        co_return;
    }

    _Use_decl_annotations_
    winrt::fire_and_forget ConfigurationsWindow::OnRestoreBackupClick(foundation::IInspectable const&, xaml::RoutedEventArgs const&)
    {
        auto lifetime = get_strong();

        try
        {
            if (!m_selectedBackup.has_value() || !m_elevated || !BeginWork())
            {
                co_return;
            }

            auto const backup = *m_selectedBackup;
            auto const name = BackupName(backup);
            auto const exists = FindConfiguration(backup.ConfigFileName) != nullptr;
            auto const inUse = engine::SameText(backup.ConfigFileName, RunningFileName()) || engine::SameText(backup.ConfigFileName, m_currentFileName);
            auto const when = cfg::WhenText(backup.MadeAt, false);

            // the import is named the way the comps name it here
            auto const reason = backup.Reason == engine::BackupReason::BeforeImport && !backup.ReasonComputerName.empty() && !backup.IsConfigurationOnly() ?
                res::FormatString(L"CfgRestoreReasonImportFormat", backup.ReasonComputerName) :
                BackupReasonText(backup, true);

            winrt::hstring text{};

            if (!exists)
            {
                text = res::FormatString(L"CfgRestoreTextMissingFormat", name, when, reason);
            }
            else if (backup.IsConfigurationOnly())
            {
                text = res::FormatString(L"CfgRestoreTextFormat", name, when, reason);
            }
            else
            {
                text = res::FormatString(L"CfgRestoreTextFullFormat", name, when, reason);
            }

            auto dialog = inUse ?
                MakeDialog(res::GetString(L"CfgRestoreTitle"), res::GetString(L"CfgRestartServiceCommand"), res::GetString(L"CfgRestartLater"), res::GetString(L"CfgCancel")) :
                MakeDialog(res::GetString(L"CfgRestoreTitle"), res::GetString(L"CfgRestoreButton"), {}, res::GetString(L"CfgCancel"));

            controls::StackPanel panel{};
            panel.Spacing(12);
            panel.Children().Append(MakeDialogText(text, false));
            panel.Children().Append(MakeDialogText(res::FormatString(inUse ? L"CfgRestoreInUseCaptionFormat" : L"CfgRestoreNotInUseCaptionFormat", name), true));

            dialog.Content(panel);

            auto const choice = co_await dialog.ShowAsync();

            if (m_closing)
            {
                co_return;
            }

            if (choice == controls::ContentDialogResult::None)
            {
                EndWork();
                co_return;
            }

            auto const environment = m_environment;
            engine::RestoreResult restored{};

            co_await native::RunOnBackgroundAsync([&restored, &environment, &backup]()
                {
                    restored = engine::RestoreBackup(environment, backup, true);
                });

            if (m_closing)
            {
                co_return;
            }

            if (!restored.Succeeded())
            {
                ShowFailure(BackupsStatusBar(), res::FormatString(L"CfgRestoreFailedFormat", name), restored);

                co_await RefreshAsync();
                EndWork();
                co_return;
            }

            uint32_t kinds{ 0 };

            if (inUse && !restored.Unchanged)
            {
                kinds |= cfg::restartkinds::Changes;
            }

            if (restored.PortNamingChanged)
            {
                kinds |= cfg::restartkinds::PortNames;
            }

            if (kinds != 0)
            {
                RecordPendingRestart(kinds, {});
            }

            auto message = res::GetString(inUse ? L"CfgRestoredInUseMessage" : L"CfgRestoredMessage");

            if (restored.PicturesFailed > 0)
            {
                message = res::FormatString(L"CfgTwoSentencesFormat", message, cfg::CountText(L"CfgResultPicturesFailed", restored.PicturesFailed));
            }

            ShowStatus(
                BackupsStatusBar(),
                restored.PicturesFailed > 0 ? controls::InfoBarSeverity::Warning : controls::InfoBarSeverity::Success,
                res::FormatString(L"CfgRestoredTitleFormat", name),
                message);

            if (inUse && choice == controls::ContentDialogResult::Primary)
            {
                co_await RestartServiceAsync(BackupsStatusBar());
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
            MIDI_SETTINGS_LOG_GENERAL_EXCEPTION(L"Unable to restore a backup.");
        }
    }

    _Use_decl_annotations_
    winrt::fire_and_forget ConfigurationsWindow::OnShareBackupClick(foundation::IInspectable const&, xaml::RoutedEventArgs const&)
    {
        auto lifetime = get_strong();

        try
        {
            if (!m_selectedBackup.has_value() || !BeginWork())
            {
                co_return;
            }

            auto const backup = *m_selectedBackup;

            auto const path = cfg::ShowSavePackageDialog(
                WindowHandle(),
                cfg::SafeFileName(BackupName(backup)) + std::wstring{ engine::PackageFileSuffix },
                native::AppSettings::Current().LastPackageFolder());

            if (path.empty() || m_closing)
            {
                EndWork();
                co_return;
            }

            auto const environment = m_environment;
            engine::ShareResult shared{};

            co_await native::RunOnBackgroundAsync([&shared, &environment, &backup, &path]()
                {
                    shared = engine::ShareBackup(environment, backup, path);
                });

            if (m_closing)
            {
                co_return;
            }

            if (shared.Succeeded())
            {
                native::AppSettings::Current().LastPackageFolder(cfg::ParentFolder(path));
                ShowSaved(BackupsStatusBar(), path);
            }
            else
            {
                ShowFailure(BackupsStatusBar(), res::GetString(L"CfgShareFailedTitle"), shared);
            }

            EndWork();
        }
        catch (...)
        {
            EndWork();
            MIDI_SETTINGS_LOG_GENERAL_EXCEPTION(L"Unable to share a backup.");
        }
    }

    _Use_decl_annotations_
    winrt::fire_and_forget ConfigurationsWindow::OnDeleteBackupClick(foundation::IInspectable const&, xaml::RoutedEventArgs const&)
    {
        auto lifetime = get_strong();

        try
        {
            if (!m_selectedBackup.has_value() || !m_elevated || !BeginWork())
            {
                co_return;
            }

            auto const backup = *m_selectedBackup;

            auto dialog = MakeDialog(res::GetString(L"CfgDeleteBackupTitle"), res::GetString(L"CfgDeleteButton"), {}, res::GetString(L"CfgCancel"));
            dialog.Content(MakeDialogText(res::FormatString(L"CfgDeleteBackupTextFormat", BackupName(backup), cfg::WhenText(backup.MadeAt, false)), false));

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

            engine::ConfigResult deleted{};

            co_await native::RunOnBackgroundAsync([&deleted, &backup]()
                {
                    deleted = engine::DeleteBackup(backup);
                });

            if (m_closing)
            {
                co_return;
            }

            if (deleted.Succeeded())
            {
                m_selectedBackup.reset();
                ShowStatus(BackupsStatusBar(), controls::InfoBarSeverity::Success, res::GetString(L"CfgBackupDeletedTitle"), {});
            }
            else
            {
                ShowFailure(BackupsStatusBar(), res::GetString(L"CfgDeleteBackupFailedTitle"), deleted);
            }

            co_await RefreshAsync();

            EndWork();
        }
        catch (...)
        {
            EndWork();
            MIDI_SETTINGS_LOG_GENERAL_EXCEPTION(L"Unable to delete a backup.");
        }
    }

    _Use_decl_annotations_
    winrt::fire_and_forget ConfigurationsWindow::OnDeleteOldBackupsClick(foundation::IInspectable const&, xaml::RoutedEventArgs const&)
    {
        auto lifetime = get_strong();

        try
        {
            if (m_backups.empty() || !m_elevated || !BeginWork())
            {
                co_return;
            }

            auto const backups = m_backups;
            auto const now = engine::CurrentFileTime();

            auto dialog = MakeDialog(res::GetString(L"CfgCleanupTitle"), res::GetString(L"CfgCleanupButtonNone"), {}, res::GetString(L"CfgCancel"));

            controls::StackPanel panel{};
            panel.Spacing(12);

            panel.Children().Append(MakeDialogText(res::GetString(L"CfgCleanupText"), false));

            controls::ComboBox keep{};
            keep.Header(winrt::box_value(res::GetString(L"CfgCleanupKeepHeader")));
            keep.HorizontalAlignment(xaml::HorizontalAlignment::Stretch);

            for (auto const key : { L"CfgCleanupKeep7", L"CfgCleanupKeep30", L"CfgCleanupKeep90", L"CfgCleanupKeepYear" })
            {
                keep.Items().Append(winrt::box_value(res::GetString(key)));
            }

            keep.SelectedIndex(DefaultKeepIndex);
            panel.Children().Append(keep);

            controls::CheckBox newest{};
            newest.Content(winrt::box_value(cfg::CountText(L"CfgCleanupKeepNewest", KeepNewestOfEach)));
            newest.IsChecked(true);
            panel.Children().Append(newest);

            auto countText = MakeDialogText({}, false);
            countText.FontWeight(winrt::Microsoft::UI::Text::FontWeights::SemiBold());
            panel.Children().Append(countText);

            dialog.Content(panel);

            auto const chosen = [keep, newest, backups, now]()
                {
                    auto const index = std::clamp(keep.SelectedIndex(), 0, static_cast<int32_t>(std::size(KeepDays)) - 1);
                    auto const keepNewest = newest.IsChecked() != nullptr && newest.IsChecked().GetBoolean();

                    return engine::OldBackups(backups, now, KeepDays[index], keepNewest ? KeepNewestOfEach : 0);
                };

            auto const update = [dialog, countText, chosen]()
                {
                    try
                    {
                        auto const old = chosen();

                        countText.Text(old.empty() ?
                            res::GetString(L"CfgCleanupNone") :
                            cfg::CountText(L"CfgCleanupCount", old.size()));

                        dialog.PrimaryButtonText(old.empty() ?
                            res::GetString(L"CfgCleanupButtonNone") :
                            cfg::CountText(L"CfgCleanupButton", old.size()));

                        dialog.IsPrimaryButtonEnabled(!old.empty());
                    }
                    catch (...)
                    {
                    }
                };

            keep.SelectionChanged([update](auto&&, auto&&) { update(); });
            newest.Click([update](auto&&, auto&&) { update(); });

            update();

            auto const choice = co_await dialog.ShowAsync();

            if (m_closing)
            {
                co_return;
            }

            auto const old = chosen();

            if (choice != controls::ContentDialogResult::Primary || old.empty())
            {
                EndWork();
                co_return;
            }

            engine::DeleteBackupsResult deleted{};

            co_await native::RunOnBackgroundAsync([&deleted, &old]()
                {
                    deleted = engine::DeleteBackups(old);
                });

            if (m_closing)
            {
                co_return;
            }

            if (deleted.Failed == 0)
            {
                ShowStatus(BackupsStatusBar(), controls::InfoBarSeverity::Success, cfg::CountText(L"CfgCleanupDone", deleted.Deleted), {});
            }
            else
            {
                ShowStatus(
                    BackupsStatusBar(),
                    controls::InfoBarSeverity::Warning,
                    cfg::CountText(L"CfgCleanupDone", deleted.Deleted),
                    cfg::CountText(L"CfgCleanupFailed", deleted.Failed));
            }

            if (m_selectedBackup.has_value() &&
                std::any_of(old.begin(), old.end(), [this](auto const& entry) { return engine::SameText(entry.Path, m_selectedBackup->Path); }))
            {
                m_selectedBackup.reset();
            }

            co_await RefreshAsync();

            EndWork();
        }
        catch (...)
        {
            EndWork();
            MIDI_SETTINGS_LOG_GENERAL_EXCEPTION(L"Unable to delete old backups.");
        }
    }
}
