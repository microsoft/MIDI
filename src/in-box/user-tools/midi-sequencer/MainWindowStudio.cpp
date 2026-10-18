// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

// Studio setup: each device and group the sequence plays to or records from, and what the
// sequencer does with it. Whether it plays to it or records from it at all, and what else it
// sends: the clock, start and stop, song position and tempo. Many DAWs make you set up the studio
// before you start. This lists what the sequence already uses instead, so it can be done
// afterward. The choices belong to the PC, so every sequence that uses a device uses them
// (design section 25).

#include "pch.h"
#include "MainWindow.xaml.h"

#include "DialogParts.h"
#include "StringResources.h"
#include "StudioSetup.h"

namespace res = ::midisequencer::resources;

namespace winrt::midisequencer::implementation
{
    using namespace dialogparts;

    namespace
    {
        // device, used by, play to, record from, clock, start and stop, song position, tempo,
        // offset, monitor, details, remove
        constexpr std::array<double, 12> StudioColumns{ 216, 124, 52, 60, 52, 64, 64, 52, 96, 32, 32, 32 };
        constexpr double StudioColumnSpacing{ 8 };
        constexpr double StudioPictureSize{ 28 };
        constexpr double OtherPictureSize{ 24 };

        constexpr int32_t DeviceColumn{ 0 };
        constexpr int32_t UsedByColumn{ 1 };
        constexpr int32_t PlayToColumn{ 2 };
        constexpr int32_t RecordFromColumn{ 3 };
        constexpr int32_t ClockColumn{ 4 };
        constexpr int32_t StartStopColumn{ 5 };
        constexpr int32_t SongPositionColumn{ 6 };
        constexpr int32_t TempoColumn{ 7 };
        constexpr int32_t OffsetColumn{ 8 };
        constexpr int32_t MonitorColumn{ 9 };
        constexpr int32_t DetailsColumn{ 10 };
        constexpr int32_t RemoveColumn{ 11 };

        // Enough for any studio. It stops "Add a device" growing the dialog without end.
        constexpr size_t MaximumStudioRows{ 64 };

        constexpr double StudioTableWidth()
        {
            auto width = StudioColumnSpacing * static_cast<double>(StudioColumns.size() - 1);

            for (auto const column : StudioColumns)
            {
                width += column;
            }

            return width;
        }

        controls::Grid MakeStudioRowGrid()
        {
            controls::Grid grid{};
            grid.ColumnSpacing(StudioColumnSpacing);

            for (auto const width : StudioColumns)
            {
                controls::ColumnDefinition column{};
                column.Width(xaml::GridLength{ width, xaml::GridUnitType::Pixel });
                grid.ColumnDefinitions().Append(column);
            }

            return grid;
        }

        controls::Button IconButton(wchar_t const* glyph)
        {
            controls::Button button{};
            button.Width(32);
            button.Height(32);
            button.MinWidth(0);
            button.MinHeight(0);
            button.Padding(xaml::Thickness{ 0, 0, 0, 0 });
            button.Background(media::SolidColorBrush{ winrt::Microsoft::UI::Colors::Transparent() });
            button.BorderThickness(xaml::Thickness{ 0, 0, 0, 0 });
            button.VerticalAlignment(xaml::VerticalAlignment::Center);

            controls::FontIcon icon{};
            icon.Glyph(glyph);
            icon.FontSize(13);
            button.Content(icon);

            return button;
        }

        // One row of the table. A row added here chooses its device and group from lists; the
        // others are for what the sequence already uses, or what the PC's settings name.
        struct StudioRowView
        {
            ::midisequencer::StudioRow Row{};
            bool Added{ false };

            // False when the group is known to speak MIDI 1.0, which has no tempo message.
            bool TempoPossible{ true };

            controls::Grid Grid{ nullptr };
            controls::ComboBox Device{ nullptr };
            controls::ComboBox Group{ nullptr };

            // Only for the ways the group goes, so either can be missing.
            controls::CheckBox PlayTo{ nullptr };
            controls::CheckBox RecordFrom{ nullptr };

            // Only for a group the sequencer can send to.
            controls::CheckBox Clock{ nullptr };
            controls::CheckBox StartStop{ nullptr };
            controls::CheckBox SongPosition{ nullptr };
            controls::CheckBox Tempo{ nullptr };
            controls::NumberBox Offset{ nullptr };

            controls::Button Monitor{ nullptr };
            controls::Button Details{ nullptr };
            controls::Button Remove{ nullptr };
        };

        using StudioRowViews = std::vector<std::shared_ptr<StudioRowView>>;

        // The device and group a row is for now, and what's ticked on it. What a row doesn't show
        // stays as it was saved.
        ::midisequencer::StudioRow CurrentRow(StudioRowView const& view)
        {
            auto row = view.Row;

            if (view.Added)
            {
                row.Endpoint = ChosenEndpoint(view.Device, {}, true);
                row.Group = ChosenGroup(view.Group, 0);
            }

            if (view.PlayTo != nullptr)
            {
                row.PlayToOff = !IsChecked(view.PlayTo);
            }

            if (view.RecordFrom != nullptr)
            {
                row.RecordFromOff = !IsChecked(view.RecordFrom);
            }

            if (view.Clock != nullptr)
            {
                row.Sync.Clock = IsChecked(view.Clock);
                row.Sync.StartStop = IsChecked(view.StartStop);
                row.Sync.SongPosition = IsChecked(view.SongPosition);
                row.Sync.Tempo = view.TempoPossible && IsChecked(view.Tempo);

                constexpr long Limit = ::midisequencer::AppSettings::MaximumClockOffsetMilliseconds;
                auto const offset = view.Offset.Value();
                row.Sync.OffsetMilliseconds = std::isnan(offset) ? 0 : static_cast<int32_t>(std::clamp(std::lround(offset), -Limit, Limit));
            }

            return row;
        }

        // Nothing goes to a group that isn't played to, so its other choices wait until it is.
        void EnableSync(StudioRowView const& view)
        {
            if (view.Clock == nullptr)
            {
                return;
            }

            auto const on = view.PlayTo == nullptr || IsChecked(view.PlayTo);

            view.Clock.IsEnabled(on);
            view.StartStop.IsEnabled(on);
            view.SongPosition.IsEnabled(on);
            view.Tempo.IsEnabled(on && view.TempoPossible);
            view.Offset.IsEnabled(on);
        }
    }

    void MainWindow::ShowStudioSetup()
    {
        [](winrt::com_ptr<MainWindow> strong) -> winrt::fire_and_forget
        {
            try
            {
                auto& settings = seq::AppSettings::Current();
                auto const weak = strong->get_weak();

                auto const secondary = strong->BrushFor(strong->m_palette.Text2);
                auto const tertiary = strong->BrushFor(strong->m_palette.Text3);
                auto const warning = strong->ThemeBrush(L"SeqWarnTextBrush");
                auto const endpointTemplate = strong->RootGrid().Resources().Lookup(winrt::box_value(L"EndpointChoiceTemplate")).as<xaml::DataTemplate>();
                auto const namedTemplate = strong->RootGrid().Resources().Lookup(winrt::box_value(L"NamedChoiceTemplate")).as<xaml::DataTemplate>();

                // Two saved endpoints are the same device when they mean the same endpoint here.
                seq::EndpointKey const key = [weak](seq::EndpointRef const& endpoint)
                {
                    auto const window = weak.get();
                    return window ? window->EndpointKeyOf(endpoint) : endpoint.Id;
                };

                // ---- what the sequence uses, with what this PC already does with each device ----

                seq::StudioInputs inputs{};

                for (auto const& output : settings.ClockOutputs())
                {
                    seq::DeviceSync sync{};
                    sync.Clock = output.Clock;
                    sync.StartStop = output.StartStop;
                    sync.SongPosition = output.SongPosition;
                    sync.Tempo = output.Tempo;
                    sync.OffsetMilliseconds = output.OffsetMilliseconds;

                    inputs.Sync.push_back(seq::DeviceSyncSetting{ seq::EndpointRef{ output.EndpointName, output.EndpointId }, output.Group, sync });
                }

                inputs.Ports = strong->SavedPortSwitches();

                if (settings.MetronomeEnabled())
                {
                    inputs.Metronome = seq::EndpointRef{ settings.MetronomeEndpointName(), settings.MetronomeEndpointId() };
                    inputs.MetronomeGroup = settings.MetronomeGroup();

                    if (inputs.Metronome.IsEmpty())
                    {
                        inputs.Metronome.Id = strong->m_directory->SynthEndpointId();
                    }
                }

                if (auto const& timeCode = settings.TimeCode(); timeCode.IsOn())
                {
                    inputs.TimeCode = seq::EndpointRef{ timeCode.EndpointName, timeCode.EndpointId };
                    inputs.TimeCodeGroup = timeCode.Group;
                }

                if (auto const& follow = settings.FollowClock(); follow.IsOn())
                {
                    inputs.FollowedClock = seq::EndpointRef{ follow.EndpointName, follow.EndpointId };
                    inputs.FollowedClockGroup = follow.Group;
                }

                // The groups a device sends on, for a track that records from any of them.
                seq::SourceGroupsOf const sourceGroups = [weak](seq::EndpointRef const& endpoint) -> uint16_t
                {
                    auto const window = weak.get();
                    return window ? window->SourceGroupMask(endpoint) : uint16_t{ 0 };
                };

                auto const studioRows = seq::StudioRows(strong->m_doc, inputs, key, sourceGroups);

                controls::StackPanel panel{};
                panel.Spacing(8);
                panel.Padding(xaml::Thickness{ 0, 0, 16, 0 });
                panel.MaxWidth(StudioTableWidth() + 16);

                panel.Children().Append(Note(res::GetString(L"StudioIntro"), secondary));
                panel.Children().Append(Heading(res::GetString(L"StudioDevicesHeading")));

                auto const header = MakeStudioRowGrid();

                for (auto const& [label, column] : std::array<std::pair<wchar_t const*, int32_t>, 9>{ {
                    { L"ClockColumnDevice", DeviceColumn }, { L"StudioColumnUsedBy", UsedByColumn }, { L"StudioColumnPlayTo", PlayToColumn },
                    { L"StudioColumnRecordFrom", RecordFromColumn }, { L"ClockColumnClock", ClockColumn }, { L"ClockColumnStartStop", StartStopColumn },
                    { L"ClockColumnSongPosition", SongPositionColumn }, { L"StudioColumnTempo", TempoColumn }, { L"ClockColumnOffset", OffsetColumn } } })
                {
                    auto const centered = column >= PlayToColumn && column <= TempoColumn;

                    controls::TextBlock text{};
                    text.Text(res::GetString(label));
                    text.FontSize(12);
                    text.Foreground(secondary);
                    text.TextWrapping(xaml::TextWrapping::Wrap);
                    text.VerticalAlignment(xaml::VerticalAlignment::Bottom);
                    text.HorizontalAlignment(centered ? xaml::HorizontalAlignment::Center : xaml::HorizontalAlignment::Left);
                    text.TextAlignment(centered ? xaml::TextAlignment::Center : xaml::TextAlignment::Left);

                    wchar_t const* help{ nullptr };

                    switch (column)
                    {
                    case PlayToColumn: help = L"StudioPlayToHelp"; break;
                    case RecordFromColumn: help = L"StudioRecordFromHelp"; break;
                    case TempoColumn: help = L"StudioTempoHelp"; break;
                    case OffsetColumn: help = L"ClockOffsetToolTip"; break;
                    default: break;
                    }

                    if (help != nullptr)
                    {
                        controls::ToolTipService::SetToolTip(text, winrt::box_value(res::GetString(help)));
                    }

                    PlaceIn(header, text, column);
                }

                // The table keeps its columns and scrolls sideways in a narrow window. The notes
                // around it wrap to the window instead. The bottom padding keeps the scroll bar
                // off the last row.
                controls::StackPanel table{};
                table.Spacing(6);
                table.Padding(xaml::Thickness{ 0, 0, 0, 12 });
                table.Children().Append(header);

                controls::StackPanel rowsPanel{};
                rowsPanel.Spacing(6);
                table.Children().Append(rowsPanel);

                controls::ScrollViewer tableScroller{};
                tableScroller.Content(table);
                tableScroller.HorizontalScrollBarVisibility(controls::ScrollBarVisibility::Auto);
                tableScroller.HorizontalScrollMode(controls::ScrollMode::Auto);
                tableScroller.VerticalScrollBarVisibility(controls::ScrollBarVisibility::Disabled);
                tableScroller.VerticalScrollMode(controls::ScrollMode::Disabled);
                panel.Children().Append(tableScroller);

                auto const empty = Note(res::GetString(L"StudioNoDevices"), secondary);
                panel.Children().Append(empty);

                auto const views = std::make_shared<StudioRowViews>();
                std::weak_ptr<StudioRowViews> const weakViews{ views };

                controls::Button add{};
                add.Style(strong->RootGrid().Resources().Lookup(winrt::box_value(L"SmallButtonStyle")).as<xaml::Style>());

                {
                    controls::StackPanel content{};
                    content.Orientation(controls::Orientation::Horizontal);
                    content.Spacing(6);

                    controls::FontIcon plus{};
                    plus.Glyph(L"\uE710");
                    plus.FontSize(12);
                    content.Children().Append(plus);

                    controls::TextBlock text{};
                    text.Text(res::GetString(L"ClockAddDevice"));
                    content.Children().Append(text);

                    add.Content(content);
                    automation::AutomationProperties::SetName(add, res::GetString(L"ClockAddDevice"));
                    controls::ToolTipService::SetToolTip(add, winrt::box_value(res::GetString(L"StudioAddDeviceHelp")));
                }

                // Handlers hold weak references: a control that held its own panel would never be freed.
                auto const refresh = [weakEmpty = winrt::make_weak(empty), weakAdd = winrt::make_weak(add), weakViews]()
                {
                    auto const list = weakViews.lock();
                    auto const emptyText = weakEmpty.get();
                    auto const addButton = weakAdd.get();

                    if (list && emptyText && addButton)
                    {
                        emptyText.Visibility(list->empty() ? xaml::Visibility::Visible : xaml::Visibility::Collapsed);
                        addButton.IsEnabled(list->size() < MaximumStudioRows);
                    }
                };

                // MIDI 1.0 has no tempo message. A device that isn't here might speak either.
                auto const tempoPossible = [weak](seq::EndpointRef const& endpoint, uint8_t group)
                {
                    auto const window = weak.get();

                    if (!window || endpoint.IsEmpty() || !window->m_directory->Resolve(endpoint).has_value())
                    {
                        return true;
                    }

                    return window->m_directory->Lookup(endpoint, group).SpeaksMidi2;
                };

                // What a screen reader says each control of a row is for, such as "Send clock to Moog One, group 1".
                auto const rowLabel = [weak](StudioRowView const& view) -> std::wstring
                {
                    auto const window = weak.get();
                    auto const row = CurrentRow(view);

                    if (row.Endpoint.IsEmpty() || !window)
                    {
                        return std::wstring{ res::GetString(L"ClockRowNoDevice") };
                    }

                    auto const summary = window->m_directory->Resolve(row.Endpoint);
                    auto name = summary.has_value() ? summary->Live.Name : row.Endpoint.Name;

                    if (name.empty())
                    {
                        name = std::wstring{ res::GetString(L"UnknownEndpoint") };
                    }

                    return std::wstring{ res::FormatString(L"StudioRowLabelFormat", name, static_cast<uint32_t>(row.Group) + 1) };
                };

                auto const nameRow = [rowLabel](StudioRowView const& view)
                {
                    auto const label = rowLabel(view);

                    if (view.Device != nullptr)
                    {
                        automation::AutomationProperties::SetName(view.Device, res::FormatString(L"ClockRowDeviceFormat", label));
                    }

                    if (view.Group != nullptr)
                    {
                        automation::AutomationProperties::SetName(view.Group, res::FormatString(L"ClockRowGroupFormat", label));
                    }

                    if (view.PlayTo != nullptr)
                    {
                        automation::AutomationProperties::SetName(view.PlayTo, res::FormatString(L"StudioRowPlayToFormat", label));
                    }

                    if (view.RecordFrom != nullptr)
                    {
                        automation::AutomationProperties::SetName(view.RecordFrom, res::FormatString(L"StudioRowRecordFromFormat", label));
                    }

                    if (view.Clock != nullptr)
                    {
                        automation::AutomationProperties::SetName(view.Clock, res::FormatString(L"ClockRowClockFormat", label));
                        automation::AutomationProperties::SetName(view.StartStop, res::FormatString(L"ClockRowStartStopFormat", label));
                        automation::AutomationProperties::SetName(view.SongPosition, res::FormatString(L"ClockRowSongPositionFormat", label));
                        automation::AutomationProperties::SetName(view.Tempo, res::FormatString(L"StudioRowTempoFormat", label));
                        automation::AutomationProperties::SetName(view.Offset, res::FormatString(L"ClockRowOffsetFormat", label));
                    }

                    if (view.Monitor != nullptr)
                    {
                        auto const monitor = res::FormatString(L"StudioRowMonitorFormat", label);
                        automation::AutomationProperties::SetName(view.Monitor, monitor);
                        controls::ToolTipService::SetToolTip(view.Monitor, winrt::box_value(monitor));
                    }

                    auto const details = res::FormatString(L"StudioRowDetailsFormat", label);
                    automation::AutomationProperties::SetName(view.Details, details);
                    controls::ToolTipService::SetToolTip(view.Details, winrt::box_value(details));

                    if (view.Remove != nullptr)
                    {
                        auto const remove = res::FormatString(L"ClockRowRemoveFormat", label);
                        automation::AutomationProperties::SetName(view.Remove, remove);
                        controls::ToolTipService::SetToolTip(view.Remove, winrt::box_value(remove));
                    }
                };

                // The device's picture, its name, and its group with the group's name and protocol.
                auto const deviceCell = [weak, tertiary, warning](seq::StudioRow const& row) -> xaml::FrameworkElement
                {
                    controls::Grid cell{};
                    cell.ColumnSpacing(8);
                    cell.VerticalAlignment(xaml::VerticalAlignment::Center);

                    {
                        controls::ColumnDefinition picture{};
                        picture.Width(xaml::GridLength{ 1, xaml::GridUnitType::Auto });
                        cell.ColumnDefinitions().Append(picture);

                        controls::ColumnDefinition words{};
                        words.Width(xaml::GridLength{ 1, xaml::GridUnitType::Star });
                        cell.ColumnDefinitions().Append(words);
                    }

                    auto const window = weak.get();

                    if (!window)
                    {
                        return cell;
                    }

                    auto const summary = window->m_directory->Resolve(row.Endpoint);

                    if (auto const picture = window->DevicePictureFor(row.Endpoint, StudioPictureSize); picture != nullptr)
                    {
                        PlaceIn(cell, picture, 0);
                    }

                    auto name = summary.has_value() ? summary->Live.Name : row.Endpoint.Name;

                    if (name.empty())
                    {
                        name = std::wstring{ res::GetString(L"UnknownEndpoint") };
                    }

                    controls::StackPanel words{};
                    words.VerticalAlignment(xaml::VerticalAlignment::Center);

                    controls::TextBlock title{};
                    title.Text(winrt::hstring{ name });
                    title.FontSize(13);
                    title.FontWeight(winrt::Microsoft::UI::Text::FontWeights::SemiBold());
                    title.TextTrimming(xaml::TextTrimming::CharacterEllipsis);
                    words.Children().Append(title);

                    std::wstring sub{ res::FormatString(L"GroupChoiceFormat", static_cast<uint32_t>(row.Group) + 1) };

                    controls::TextBlock subText{};
                    subText.FontSize(11);
                    subText.TextTrimming(xaml::TextTrimming::CharacterEllipsis);
                    subText.Foreground(tertiary);

                    if (summary.has_value())
                    {
                        // The function block's name, or the group terminal block's, says which
                        // port of an interface this is. What a group speaks matters only for
                        // sending to it.
                        auto const group = static_cast<int32_t>(row.Group & 0x0F);
                        auto const sendsTo = summary->Live.DestinationGroups[group];

                        if (auto const groupName = summary->Live.GroupName(group, !sendsTo); !groupName.empty() && groupName != name)
                        {
                            sub += L" \u00B7 " + groupName;
                        }

                        if (sendsTo)
                        {
                            auto const midi2 = window->m_directory->Lookup(row.Endpoint, row.Group).SpeaksMidi2;
                            sub += L" \u00B7 " + std::wstring{ res::GetString(midi2 ? L"ProtocolMidi2" : L"ProtocolMidi1") };
                        }

                        if (!summary->Live.Description.empty())
                        {
                            controls::ToolTipService::SetToolTip(cell, winrt::box_value(winrt::hstring{ summary->Live.Description }));
                        }
                    }
                    else
                    {
                        sub += L" \u00B7 " + std::wstring{ res::GetString(L"DeviceNotConnectedShort") };
                        subText.Foreground(warning);
                    }

                    subText.Text(winrt::hstring{ sub });
                    words.Children().Append(subText);

                    PlaceIn(cell, words, 1);
                    return cell;
                };

                auto const usedByCell = [](seq::StudioRow const& row) -> xaml::FrameworkElement
                {
                    std::vector<std::wstring> uses{ row.Tracks };

                    if (!row.RecordingTracks.empty())
                    {
                        uses.emplace_back(res::FormatString(L"StudioUsedByRecordingFormat", JoinNames(row.RecordingTracks)));
                    }

                    if (row.Metronome)
                    {
                        uses.emplace_back(res::GetString(L"StudioUsedByMetronome"));
                    }

                    if (row.TimeCode)
                    {
                        uses.emplace_back(res::GetString(L"StudioUsedByTimeCode"));
                    }

                    if (row.SetsTempo)
                    {
                        uses.emplace_back(res::GetString(L"StudioUsedBySetsTempo"));
                    }

                    std::wstring text{};

                    if (!uses.empty())
                    {
                        text = JoinNames(uses);
                    }
                    else
                    {
                        text = std::wstring{ res::GetString(row.Sync.AnyChosen() ? L"StudioUsedByClockOnly" : L"StudioUsedByNotInSequence") };
                    }

                    controls::TextBlock used{};
                    used.Text(winrt::hstring{ text });
                    used.FontSize(12);
                    used.TextWrapping(xaml::TextWrapping::Wrap);
                    used.MaxLines(2);
                    used.TextTrimming(xaml::TextTrimming::CharacterEllipsis);
                    used.VerticalAlignment(xaml::VerticalAlignment::Center);
                    controls::ToolTipService::SetToolTip(used, winrt::box_value(winrt::hstring{ text }));
                    return used;
                };

                // Which ways a row's group goes: what the device says, and whatever uses it, so a
                // device that isn't here still shows what it's used for.
                auto const directions = [weak](seq::StudioRow const& row) -> std::pair<bool, bool>
                {
                    auto playTo = row.PlayedTo() || row.PlayToOff || row.Sync.AnyChosen();
                    auto recordFrom = row.RecordedFrom() || row.RecordFromOff;

                    if (auto const window = weak.get())
                    {
                        if (auto const summary = window->m_directory->Resolve(row.Endpoint); summary.has_value())
                        {
                            playTo = playTo || summary->Live.DestinationGroups[row.Group & 0x0F];
                            recordFrom = recordFrom || summary->Live.SourceGroups[row.Group & 0x0F];
                        }
                    }

                    return { playTo, recordFrom };
                };

                auto const addRow = [weak, weakViews, weakPanel = winrt::make_weak(rowsPanel), refresh, nameRow, tempoPossible, directions, deviceCell, usedByCell, endpointTemplate, namedTemplate](seq::StudioRow const& row, bool added)
                {
                    auto const window = weak.get();
                    auto const list = weakViews.lock();
                    auto const host = weakPanel.get();

                    if (!window || !list || !host)
                    {
                        return;
                    }

                    auto view = std::make_shared<StudioRowView>();
                    view->Row = row;
                    view->Added = added;
                    view->Grid = MakeStudioRowGrid();
                    view->TempoPossible = tempoPossible(row.Endpoint, row.Group);

                    // A row added here is for a device that follows the clock.
                    auto const ways = added ? std::pair<bool, bool>{ true, false } : directions(row);

                    if (added)
                    {
                        view->Device = controls::ComboBox{};
                        view->Device.HorizontalAlignment(xaml::HorizontalAlignment::Stretch);
                        view->Device.VerticalAlignment(xaml::VerticalAlignment::Center);
                        view->Device.ItemTemplate(endpointTemplate);
                        window->FillEndpointCombo(view->Device, false, row.Endpoint);
                        PlaceIn(view->Grid, view->Device, DeviceColumn);

                        view->Group = controls::ComboBox{};
                        view->Group.HorizontalAlignment(xaml::HorizontalAlignment::Stretch);
                        view->Group.VerticalAlignment(xaml::VerticalAlignment::Center);
                        view->Group.ItemTemplate(namedTemplate);
                        window->FillGroupCombo(view->Group, false, row.Endpoint, row.Group, false);
                        PlaceIn(view->Grid, view->Group, UsedByColumn);
                    }
                    else
                    {
                        PlaceIn(view->Grid, deviceCell(row), DeviceColumn);
                        PlaceIn(view->Grid, usedByCell(row), UsedByColumn);

                        if (ways.first)
                        {
                            view->PlayTo = TableCheck(!row.PlayToOff);
                            PlaceIn(view->Grid, view->PlayTo, PlayToColumn);
                        }

                        if (ways.second)
                        {
                            view->RecordFrom = TableCheck(!row.RecordFromOff);
                            PlaceIn(view->Grid, view->RecordFrom, RecordFromColumn);
                        }
                    }

                    if (ways.first)
                    {
                        view->Clock = TableCheck(row.Sync.Clock);
                        PlaceIn(view->Grid, view->Clock, ClockColumn);

                        view->StartStop = TableCheck(row.Sync.StartStop);
                        PlaceIn(view->Grid, view->StartStop, StartStopColumn);

                        view->SongPosition = TableCheck(row.Sync.SongPosition);
                        PlaceIn(view->Grid, view->SongPosition, SongPositionColumn);

                        view->Tempo = TableCheck(row.Sync.Tempo && view->TempoPossible);
                        PlaceIn(view->Grid, view->Tempo, TempoColumn);

                        view->Offset = controls::NumberBox{};
                        view->Offset.Minimum(-seq::AppSettings::MaximumClockOffsetMilliseconds);
                        view->Offset.Maximum(seq::AppSettings::MaximumClockOffsetMilliseconds);
                        view->Offset.Value(row.Sync.OffsetMilliseconds);
                        view->Offset.SmallChange(1);
                        view->Offset.LargeChange(10);
                        view->Offset.SpinButtonPlacementMode(controls::NumberBoxSpinButtonPlacementMode::Compact);
                        view->Offset.VerticalAlignment(xaml::VerticalAlignment::Center);
                        PlaceIn(view->Grid, view->Offset, OffsetColumn);
                    }

                    // What a group sends, as it arrives. MIDI Monitor only listens.
                    if (ways.second && window->m_directory->Resolve(row.Endpoint).has_value())
                    {
                        view->Monitor = IconButton(L"\uE890");
                        PlaceIn(view->Grid, view->Monitor, MonitorColumn);
                    }

                    view->Details = IconButton(L"\uE946");
                    PlaceIn(view->Grid, view->Details, DetailsColumn);

                    // What the sequence uses stays listed. A row only the settings name can go.
                    if (added || row.OnlySaved)
                    {
                        view->Remove = IconButton(L"\uE74D");
                        PlaceIn(view->Grid, view->Remove, RemoveColumn);
                    }

                    EnableSync(*view);
                    nameRow(*view);

                    std::weak_ptr<StudioRowView> const weakRow{ view };

                    view->Details.Click([weak, weakRow](foundation::IInspectable const& sender, auto&&)
                    {
                        auto const current = weakRow.lock();
                        auto const window = weak.get();

                        if (!current || !window)
                        {
                            return;
                        }

                        if (auto const endpoint = CurrentRow(*current).Endpoint; !endpoint.IsEmpty())
                        {
                            window->ShowDeviceDetails(endpoint, sender.as<xaml::FrameworkElement>());
                        }
                    });

                    if (view->Monitor != nullptr)
                    {
                        view->Monitor.Click([weak, weakRow](auto&&, auto&&)
                        {
                            auto const current = weakRow.lock();
                            auto const window = weak.get();

                            if (!current || !window)
                            {
                                return;
                            }

                            auto const now = CurrentRow(*current);

                            if (auto const summary = window->m_directory->Resolve(now.Endpoint); summary.has_value())
                            {
                                window->LaunchMonitor(summary->Live.EndpointDeviceId, now.Group);
                            }
                        });
                    }

                    if (view->PlayTo != nullptr)
                    {
                        auto const toggled = [weakRow](auto&&, auto&&)
                        {
                            if (auto const current = weakRow.lock())
                            {
                                EnableSync(*current);
                            }
                        };

                        view->PlayTo.Checked(toggled);
                        view->PlayTo.Unchecked(toggled);
                    }

                    if (added)
                    {
                        // Another device or group may speak another protocol.
                        auto const recheck = [weakRow, nameRow, tempoPossible]()
                        {
                            if (auto const current = weakRow.lock())
                            {
                                auto const now = CurrentRow(*current);
                                current->TempoPossible = tempoPossible(now.Endpoint, now.Group);
                                EnableSync(*current);
                                nameRow(*current);
                            }
                        };

                        view->Device.SelectionChanged([weak, weakRow, recheck](auto&&, auto&&)
                        {
                            auto const window = weak.get();
                            auto const current = weakRow.lock();

                            if (window && current)
                            {
                                window->FillGroupCombo(current->Group, false, ChosenEndpoint(current->Device, {}, true), 0, false);
                                recheck();
                            }
                        });

                        view->Group.SelectionChanged([recheck](auto&&, auto&&) { recheck(); });
                    }

                    if (view->Remove != nullptr)
                    {
                        view->Remove.Click([weakRow, weakViews, weakHost = winrt::make_weak(host), refresh](auto&&, auto&&)
                        {
                            auto const current = weakRow.lock();
                            auto const list = weakViews.lock();
                            auto const rows = weakHost.get();

                            if (!current || !list || !rows)
                            {
                                return;
                            }

                            uint32_t index{ 0 };

                            if (rows.Children().IndexOf(current->Grid, index))
                            {
                                rows.Children().RemoveAt(index);
                            }

                            std::erase(*list, current);
                            refresh();
                        });
                    }

                    host.Children().Append(view->Grid);
                    list->push_back(std::move(view));
                    refresh();
                };

                for (auto const& row : studioRows)
                {
                    addRow(row, false);
                }

                refresh();

                add.Click([addRow, weakViews](auto&&, auto&&)
                {
                    auto const list = weakViews.lock();

                    if (!list || list->size() >= MaximumStudioRows)
                    {
                        return;
                    }

                    // A device added here is one that follows the clock, so it starts with all of it.
                    seq::StudioRow row{};
                    row.Sync.Clock = true;
                    row.Sync.StartStop = true;
                    row.Sync.SongPosition = true;
                    row.Sync.Tempo = true;
                    addRow(row, true);

                    // Straight to the new row's device list.
                    if (!list->empty() && list->back()->Device != nullptr)
                    {
                        list->back()->Device.Focus(xaml::FocusState::Programmatic);
                    }
                });

                add.HorizontalAlignment(xaml::HorizontalAlignment::Left);
                panel.Children().Append(add);

                auto const error = Note(L"", warning);
                error.Visibility(xaml::Visibility::Collapsed);
                panel.Children().Append(error);

                panel.Children().Append(Note(res::GetString(L"StudioTempoHelp"), secondary));
                panel.Children().Append(Note(res::GetString(L"StudioPortsHelp"), secondary));

                // ---- every other device, so anything on the PC can be looked at ----

                std::set<std::wstring> listed{};

                for (auto const& row : studioRows)
                {
                    listed.insert(key(row.Endpoint));
                }

                controls::StackPanel others{};
                others.Spacing(6);

                for (auto const& summary : strong->m_directory->Snapshot())
                {
                    auto const endpoint = seq::EndpointDirectory::MakeRef(summary.Live);

                    if (listed.contains(key(endpoint)))
                    {
                        continue;
                    }

                    controls::Grid line{};
                    line.ColumnSpacing(8);

                    for (auto const width : { xaml::GridLength{ OtherPictureSize, xaml::GridUnitType::Pixel }, xaml::GridLength{ 1, xaml::GridUnitType::Star },
                        xaml::GridLength{ 32, xaml::GridUnitType::Pixel }, xaml::GridLength{ 32, xaml::GridUnitType::Pixel } })
                    {
                        controls::ColumnDefinition column{};
                        column.Width(width);
                        line.ColumnDefinitions().Append(column);
                    }

                    if (auto const picture = strong->DevicePictureFor(endpoint, OtherPictureSize); picture != nullptr)
                    {
                        PlaceIn(line, picture, 0);
                    }

                    controls::StackPanel words{};
                    words.VerticalAlignment(xaml::VerticalAlignment::Center);

                    controls::TextBlock title{};
                    title.Text(winrt::hstring{ summary.Live.Name });
                    title.FontSize(13);
                    title.TextTrimming(xaml::TextTrimming::CharacterEllipsis);
                    words.Children().Append(title);

                    // What it says about itself, or which way its messages go.
                    auto const sends = summary.Live.SourceGroupCount() > 0;
                    auto const receives = summary.Live.DestinationGroupCount() > 0;
                    auto const kind = sends && receives ? L"DeviceBlockSendsAndReceives" : sends ? L"DeviceBlockSends" : L"DeviceBlockReceives";

                    controls::TextBlock sub{};
                    sub.Text(summary.Live.Description.empty() ? res::GetString(kind) : winrt::hstring{ summary.Live.Description });
                    sub.FontSize(11);
                    sub.TextTrimming(xaml::TextTrimming::CharacterEllipsis);
                    sub.Foreground(tertiary);
                    words.Children().Append(sub);

                    PlaceIn(line, words, 1);

                    if (sends)
                    {
                        auto const monitor = IconButton(L"\uE890");
                        auto const monitorName = res::FormatString(L"StudioRowMonitorFormat", summary.Live.Name);
                        automation::AutomationProperties::SetName(monitor, monitorName);
                        controls::ToolTipService::SetToolTip(monitor, winrt::box_value(monitorName));

                        monitor.Click([weak, id = summary.Live.EndpointDeviceId](auto&&, auto&&)
                        {
                            if (auto const window = weak.get())
                            {
                                window->LaunchMonitor(id, -1);
                            }
                        });

                        PlaceIn(line, monitor, 2);
                    }

                    auto const details = IconButton(L"\uE946");
                    auto const detailsName = res::FormatString(L"StudioRowDetailsFormat", summary.Live.Name);
                    automation::AutomationProperties::SetName(details, detailsName);
                    controls::ToolTipService::SetToolTip(details, winrt::box_value(detailsName));

                    details.Click([weak, endpoint](foundation::IInspectable const& sender, auto&&)
                    {
                        if (auto const window = weak.get())
                        {
                            window->ShowDeviceDetails(endpoint, sender.as<xaml::FrameworkElement>());
                        }
                    });

                    PlaceIn(line, details, 3);
                    others.Children().Append(line);
                }

                if (others.Children().Size() > 0)
                {
                    controls::Expander expander{};
                    expander.Header(winrt::box_value(res::FormatString(L"StudioOtherDevicesFormat", others.Children().Size())));
                    expander.Content(others);
                    expander.HorizontalAlignment(xaml::HorizontalAlignment::Stretch);
                    expander.HorizontalContentAlignment(xaml::HorizontalAlignment::Stretch);
                    expander.Margin(xaml::Thickness{ 0, 12, 0, 0 });
                    panel.Children().Append(expander);
                }

                controls::ScrollViewer scroller{};
                scroller.Content(panel);
                scroller.HorizontalScrollBarVisibility(controls::ScrollBarVisibility::Disabled);
                scroller.HorizontalScrollMode(controls::ScrollMode::Disabled);
                scroller.VerticalScrollBarVisibility(controls::ScrollBarVisibility::Auto);

                controls::ContentDialog dialog{};
                dialog.XamlRoot(strong->Content().XamlRoot());
                dialog.Resources().Insert(winrt::box_value(L"ContentDialogMaxWidth"), winrt::box_value(StudioTableWidth() + 80.0));
                dialog.Title(winrt::box_value(res::GetString(L"StudioDialogTitle")));
                dialog.Content(scroller);
                dialog.PrimaryButtonText(res::GetString(L"DialogDone"));
                dialog.CloseButtonText(res::GetString(L"DialogCancel"));
                dialog.DefaultButton(controls::ContentDialogButton::Primary);
                dialog.RequestedTheme(strong->RootGrid().ActualTheme());

                // Everything ticked, row by row, as the rows are now.
                auto const collect = [weakViews]()
                {
                    std::vector<seq::StudioRow> rows{};

                    if (auto const list = weakViews.lock())
                    {
                        for (auto const& view : *list)
                        {
                            rows.push_back(CurrentRow(*view));
                        }
                    }

                    return rows;
                };

                // The PC keeps a limited number. The dialog stays open to say so.
                dialog.PrimaryButtonClick([collect, key, error](auto&&, controls::ContentDialogButtonClickEventArgs const& args)
                {
                    if (seq::StudioChoices(collect(), key).size() > seq::AppSettings::MaximumClockOutputs)
                    {
                        error.Text(res::FormatString(L"StudioTooManyFormat", seq::AppSettings::MaximumClockOutputs));
                        error.Visibility(xaml::Visibility::Visible);
                        args.Cancel(true);
                    }
                });

                auto const result = co_await dialog.ShowAsync();

                if (result == controls::ContentDialogResult::Primary && !strong->m_closing)
                {
                    auto const rows = collect();

                    // Kept by name too, so a device is found again after it moves to another port.
                    auto const nameOf = [&strong](seq::EndpointRef const& endpoint)
                    {
                        if (!endpoint.Name.empty())
                        {
                            return endpoint.Name;
                        }

                        auto const summary = strong->m_directory->Resolve(endpoint);
                        return summary.has_value() ? summary->Live.Name : std::wstring{};
                    };

                    std::vector<seq::ClockOutputSetting> outputs{};

                    for (auto const& choice : seq::StudioChoices(rows, key))
                    {
                        seq::ClockOutputSetting output{};
                        output.EndpointId = choice.Endpoint.Id;
                        output.EndpointName = nameOf(choice.Endpoint);
                        output.Group = choice.Group;
                        output.Clock = choice.Sync.Clock;
                        output.StartStop = choice.Sync.StartStop;
                        output.SongPosition = choice.Sync.SongPosition;
                        output.Tempo = choice.Sync.Tempo;
                        output.OffsetMilliseconds = choice.Sync.OffsetMilliseconds;
                        outputs.push_back(std::move(output));
                    }

                    settings.ClockOutputs(outputs);

                    std::vector<seq::PortSetting> ports{};

                    for (auto const& port : seq::StudioPorts(rows, key))
                    {
                        seq::PortSetting setting{};
                        setting.EndpointId = port.Endpoint.Id;
                        setting.EndpointName = nameOf(port.Endpoint);
                        setting.Group = port.Group;
                        setting.PlayTo = port.PlayTo;
                        setting.RecordFrom = port.RecordFrom;
                        ports.push_back(std::move(setting));
                    }

                    // A group turned off while the music plays would keep the notes it's holding.
                    // Stopping first ends them while the group can still hear it.
                    auto const before = strong->SavedPortSwitches();
                    auto const turningOff = std::any_of(ports.begin(), ports.end(), [&before, &key](seq::PortSetting const& port)
                    {
                        seq::EndpointRef const endpoint{ port.EndpointName, port.EndpointId };
                        return !port.PlayTo && (seq::GroupsOff(before, endpoint, true, key) & (1u << (port.Group & 0x0F))) == 0;
                    });

                    if (turningOff && strong->m_engine != nullptr && strong->m_engine->IsPlaying())
                    {
                        strong->Stop();
                    }

                    settings.PortsOff(ports);

                    strong->ApplyEngineSettings();

                    // Reads the new devices in full, opens them, and updates the status bar.
                    strong->RefreshEndpoints();
                }

                // The rows' controls refer back to the list; let them go with the dialog.
                views->clear();
            }
            MIDI_SEQUENCER_CATCH_AND_LOG(L"Unable to show Studio setup.")
        }(get_strong());
    }

    _Use_decl_annotations_
    void MainWindow::OnStudioSetupClick(foundation::IInspectable const&, xaml::RoutedEventArgs const&)
    {
        ShowStudioSetup();
    }
}
