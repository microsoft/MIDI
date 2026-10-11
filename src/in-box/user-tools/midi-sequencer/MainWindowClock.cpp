// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#include "pch.h"
#include "MainWindow.xaml.h"

#include "MidiTimeCode.h"
#include "StringResources.h"

namespace res = ::midisequencer::resources;

namespace winrt::midisequencer::implementation
{
    namespace
    {
        // The clock table's columns, the same in its heading and in every row (comp 6).
        constexpr std::array<double, 7> ClockColumns{ 220, 200, 56, 100, 100, 112, 36 };
        constexpr double ClockColumnSpacing{ 8 };

        // The notes wrap to the table's width: the dialog scrolls sideways, so nothing else stops them.
        constexpr double ClockTableWidth()
        {
            auto width = ClockColumnSpacing * static_cast<double>(ClockColumns.size() - 1);

            for (auto const column : ClockColumns)
            {
                width += column;
            }

            return width;
        }

        // midiapp::MidiTimeCodeFrameRate values, in the order the list shows them.
        constexpr std::array<wchar_t const*, 4> RateNames{ L"TimeCodeRate24", L"TimeCodeRate25", L"TimeCodeRate2997", L"TimeCodeRate30" };

        controls::Grid MakeClockRowGrid()
        {
            controls::Grid grid{};
            grid.ColumnSpacing(ClockColumnSpacing);

            for (auto const width : ClockColumns)
            {
                controls::ColumnDefinition column{};
                column.Width(xaml::GridLength{ width, xaml::GridUnitType::Pixel });
                grid.ColumnDefinitions().Append(column);
            }

            return grid;
        }

        void PlaceIn(controls::Grid const& grid, xaml::FrameworkElement const& element, int32_t column)
        {
            controls::Grid::SetColumn(element, column);
            grid.Children().Append(element);
        }

        controls::TextBlock Heading(winrt::hstring const& text)
        {
            controls::TextBlock heading{};
            heading.Text(text);
            heading.FontWeight(winrt::Microsoft::UI::Text::FontWeights::SemiBold());
            heading.Margin(xaml::Thickness{ 0, 12, 0, 2 });
            automation::AutomationProperties::SetHeadingLevel(heading, automation::Peers::AutomationHeadingLevel::Level2);
            return heading;
        }

        controls::TextBlock Note(winrt::hstring const& text, media::Brush const& brush)
        {
            controls::TextBlock note{};
            note.Text(text);
            note.TextWrapping(xaml::TextWrapping::Wrap);
            note.FontSize(12);
            note.Foreground(brush);
            return note;
        }

        controls::CheckBox TableCheck(bool value)
        {
            controls::CheckBox check{};
            check.IsChecked(value);
            check.MinWidth(0);
            check.Padding(xaml::Thickness{ 0, 0, 0, 0 });
            check.HorizontalAlignment(xaml::HorizontalAlignment::Center);
            check.VerticalAlignment(xaml::VerticalAlignment::Center);
            return check;
        }

        bool IsChecked(controls::CheckBox const& check)
        {
            return check.IsChecked() != nullptr && check.IsChecked().GetBoolean();
        }

        // One row of the clock table. The device a row started with is kept as it was saved
        // until another is picked, so a device that isn't plugged in keeps its name.
        struct ClockRow
        {
            seq::ClockOutputSetting Original{};
            bool DeviceChanged{ false };
            controls::Grid Grid{ nullptr };
            controls::ComboBox Device{ nullptr };
            controls::ComboBox Group{ nullptr };
            controls::CheckBox Clock{ nullptr };
            controls::CheckBox StartStop{ nullptr };
            controls::CheckBox SongPosition{ nullptr };
            controls::NumberBox Offset{ nullptr };
            controls::Button Remove{ nullptr };
        };

        using ClockRows = std::vector<std::shared_ptr<ClockRow>>;

        // The endpoint a list's choice means: the one saved, until the person picks another.
        seq::EndpointRef ChosenEndpoint(controls::ComboBox const& combo, seq::EndpointRef const& original, bool changed)
        {
            if (!changed)
            {
                return original;
            }

            auto const choice = combo.SelectedItem().try_as<appshared::EndpointChoice>();

            if (choice == nullptr || choice.EndpointDeviceId().empty())
            {
                return {};
            }

            return seq::EndpointRef{ std::wstring{ choice.DisplayName() }, std::wstring{ choice.EndpointDeviceId() } };
        }

        uint8_t ChosenGroup(controls::ComboBox const& combo, uint8_t fallback)
        {
            auto const choice = combo.SelectedItem().try_as<appshared::NamedChoice>();
            return choice != nullptr ? static_cast<uint8_t>(std::clamp(choice.Value(), 0, 15)) : fallback;
        }
    }

    void MainWindow::ShowClockDialog()
    {
        [](winrt::com_ptr<MainWindow> strong) -> winrt::fire_and_forget
        {
            try
            {
                auto& settings = seq::AppSettings::Current();
                auto const weak = strong->get_weak();

                auto const secondary = strong->BrushFor(strong->m_palette.Text2);
                auto const endpointTemplate = strong->RootGrid().Resources().Lookup(winrt::box_value(L"EndpointChoiceTemplate")).as<xaml::DataTemplate>();
                auto const namedTemplate = strong->RootGrid().Resources().Lookup(winrt::box_value(L"NamedChoiceTemplate")).as<xaml::DataTemplate>();

                controls::StackPanel panel{};
                panel.Spacing(8);
                panel.Padding(xaml::Thickness{ 0, 0, 16, 0 });
                panel.MaxWidth(ClockTableWidth() + 16);

                panel.Children().Append(Note(res::GetString(L"ClockDialogIntro"), secondary));

                // ---- where the tempo comes from ----

                panel.Children().Append(Heading(res::GetString(L"ClockTempoFromHeading")));

                auto const firstTempo = strong->m_doc.Tempo.empty() ? 120.0 : strong->m_doc.Tempo.front().BeatsPerMinute;

                auto const makeRadio = [&secondary](winrt::hstring const& title, winrt::hstring const& detail)
                {
                    controls::StackPanel content{};
                    content.Spacing(2);

                    controls::TextBlock titleText{};
                    titleText.Text(title);
                    content.Children().Append(titleText);
                    content.Children().Append(Note(detail, secondary));

                    controls::RadioButton radio{};
                    radio.GroupName(L"ClockTempoSource");
                    radio.Content(content);
                    automation::AutomationProperties::SetName(radio, title);
                    return radio;
                };

                auto const follow = settings.FollowClock();
                seq::EndpointRef const followEndpoint{ follow.EndpointName, follow.EndpointId };

                auto const fromSequence = makeRadio(res::GetString(L"ClockTempoThisSequence"),
                    res::FormatString(L"ClockTempoThisSequenceFormat", std::format(L"{:g}", std::round(firstTempo * 100.0) / 100.0)));
                fromSequence.IsChecked(!follow.Enabled);
                panel.Children().Append(fromSequence);

                auto const fromDevice = makeRadio(res::GetString(L"ClockTempoAnotherDevice"), res::GetString(L"ClockTempoAnotherDeviceHelp"));
                fromDevice.IsChecked(follow.Enabled);
                panel.Children().Append(fromDevice);

                // The device to follow, under its choice.
                controls::StackPanel followRow{};
                followRow.Orientation(controls::Orientation::Horizontal);
                followRow.Spacing(8);
                followRow.Margin(xaml::Thickness{ 28, 0, 0, 0 });

                controls::ComboBox followDevice{};
                followDevice.Width(ClockColumns[0]);
                followDevice.ItemTemplate(endpointTemplate);
                strong->FillEndpointCombo(followDevice, true, followEndpoint);
                automation::AutomationProperties::SetName(followDevice, res::GetString(L"FollowDeviceName"));
                followRow.Children().Append(followDevice);

                controls::ComboBox followGroup{};
                followGroup.Width(ClockColumns[1]);
                followGroup.ItemTemplate(namedTemplate);
                strong->FillGroupCombo(followGroup, true, followEndpoint, follow.Group, false);
                automation::AutomationProperties::SetName(followGroup, res::GetString(L"FollowGroupName"));
                followRow.Children().Append(followGroup);

                panel.Children().Append(followRow);

                auto const followError = Note(L"", strong->ThemeBrush(L"SeqWarnTextBrush"));
                followError.Visibility(xaml::Visibility::Collapsed);
                followError.Margin(xaml::Thickness{ 28, 0, 0, 0 });
                panel.Children().Append(followError);

                auto const followChanged = std::make_shared<bool>(false);

                followDevice.SelectionChanged([weak, weakDevice = winrt::make_weak(followDevice), weakGroup = winrt::make_weak(followGroup), followChanged](auto&&, auto&&)
                {
                    auto const window = weak.get();
                    auto const device = weakDevice.get();
                    auto const group = weakGroup.get();

                    if (window && device && group)
                    {
                        *followChanged = true;
                        window->FillGroupCombo(group, true, ChosenEndpoint(device, {}, true), 0, false);
                    }
                });

                // The lists only matter while following is chosen.
                auto const enableFollowRow = [weakDevice = winrt::make_weak(followDevice), weakGroup = winrt::make_weak(followGroup), weakRadio = winrt::make_weak(fromDevice)]()
                {
                    auto const device = weakDevice.get();
                    auto const group = weakGroup.get();
                    auto const radio = weakRadio.get();

                    if (device && group && radio)
                    {
                        auto const on = radio.IsChecked() != nullptr && radio.IsChecked().GetBoolean();
                        device.IsEnabled(on);
                        group.IsEnabled(on);
                    }
                };

                enableFollowRow();
                fromDevice.Checked([enableFollowRow](auto&&, auto&&) { enableFollowRow(); });
                fromDevice.Unchecked([enableFollowRow](auto&&, auto&&) { enableFollowRow(); });

                // ---- devices that get the clock ----

                panel.Children().Append(Heading(res::GetString(L"ClockSendHeading")));

                auto const header = MakeClockRowGrid();

                for (auto const& [key, column] : std::array<std::pair<wchar_t const*, int32_t>, 6>{ {
                    { L"ClockColumnDevice", 0 }, { L"ClockColumnGroup", 1 }, { L"ClockColumnClock", 2 },
                    { L"ClockColumnStartStop", 3 }, { L"ClockColumnSongPosition", 4 }, { L"ClockColumnOffset", 5 } } })
                {
                    controls::TextBlock text{};
                    text.Text(res::GetString(key));
                    text.FontSize(12);
                    text.Foreground(secondary);
                    text.TextWrapping(xaml::TextWrapping::Wrap);
                    text.HorizontalAlignment(column >= 2 && column <= 4 ? xaml::HorizontalAlignment::Center : xaml::HorizontalAlignment::Left);
                    text.TextAlignment(column >= 2 && column <= 4 ? xaml::TextAlignment::Center : xaml::TextAlignment::Left);

                    if (column == 5)
                    {
                        controls::ToolTipService::SetToolTip(text, winrt::box_value(res::GetString(L"ClockOffsetToolTip")));
                    }

                    PlaceIn(header, text, column);
                }

                panel.Children().Append(header);

                controls::StackPanel rowsPanel{};
                rowsPanel.Spacing(6);
                panel.Children().Append(rowsPanel);

                auto const empty = Note(res::GetString(L"ClockNoDevices"), secondary);
                panel.Children().Append(empty);

                auto const rows = std::make_shared<ClockRows>();
                std::weak_ptr<ClockRows> const weakRows{ rows };

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

                    controls::TextBlock label{};
                    label.Text(res::GetString(L"ClockAddDevice"));
                    content.Children().Append(label);

                    add.Content(content);
                    automation::AutomationProperties::SetName(add, res::GetString(L"ClockAddDevice"));
                }

                // Handlers hold weak references: a control that held its own panel would never be freed.
                auto const refresh = [weakEmpty = winrt::make_weak(empty), weakAdd = winrt::make_weak(add), weakRows]()
                {
                    auto const list = weakRows.lock();
                    auto const emptyText = weakEmpty.get();
                    auto const addButton = weakAdd.get();

                    if (list && emptyText && addButton)
                    {
                        emptyText.Visibility(list->empty() ? xaml::Visibility::Visible : xaml::Visibility::Collapsed);
                        addButton.IsEnabled(list->size() < seq::AppSettings::MaximumClockOutputs);
                    }
                };

                // Each control in a row says which device it's for, or a screen reader hears the same
                // names on every row.
                auto const nameRow = [](ClockRow const& row)
                {
                    auto const choice = row.Device.SelectedItem().try_as<appshared::EndpointChoice>();
                    auto const device = choice != nullptr && !choice.EndpointDeviceId().empty()
                        ? std::wstring{ choice.DisplayName() }
                        : std::wstring{ res::GetString(L"ClockRowNoDevice") };

                    automation::AutomationProperties::SetName(row.Device, res::FormatString(L"ClockRowDeviceFormat", device));
                    automation::AutomationProperties::SetName(row.Group, res::FormatString(L"ClockRowGroupFormat", device));
                    automation::AutomationProperties::SetName(row.Clock, res::FormatString(L"ClockRowClockFormat", device));
                    automation::AutomationProperties::SetName(row.StartStop, res::FormatString(L"ClockRowStartStopFormat", device));
                    automation::AutomationProperties::SetName(row.SongPosition, res::FormatString(L"ClockRowSongPositionFormat", device));
                    automation::AutomationProperties::SetName(row.Offset, res::FormatString(L"ClockRowOffsetFormat", device));
                    automation::AutomationProperties::SetName(row.Remove, res::FormatString(L"ClockRowRemoveFormat", device));
                    controls::ToolTipService::SetToolTip(row.Remove, winrt::box_value(res::FormatString(L"ClockRowRemoveFormat", device)));
                };

                auto const addRow = [weak, weakRows, weakPanel = winrt::make_weak(rowsPanel), refresh, nameRow, endpointTemplate, namedTemplate](seq::ClockOutputSetting const& setting)
                {
                    auto window = weak.get();
                    auto list = weakRows.lock();
                    auto rowsHost = weakPanel.get();

                    if (!window || !list || !rowsHost)
                    {
                        return;
                    }

                    auto row = std::make_shared<ClockRow>();
                    row->Original = setting;
                    row->Grid = MakeClockRowGrid();

                    seq::EndpointRef const endpoint{ setting.EndpointName, setting.EndpointId };

                    row->Device = controls::ComboBox{};
                    row->Device.HorizontalAlignment(xaml::HorizontalAlignment::Stretch);
                    row->Device.ItemTemplate(endpointTemplate);
                    window->FillEndpointCombo(row->Device, false, endpoint);
                    PlaceIn(row->Grid, row->Device, 0);

                    row->Group = controls::ComboBox{};
                    row->Group.HorizontalAlignment(xaml::HorizontalAlignment::Stretch);
                    row->Group.ItemTemplate(namedTemplate);
                    window->FillGroupCombo(row->Group, false, endpoint, setting.Group, false);
                    PlaceIn(row->Grid, row->Group, 1);

                    row->Clock = TableCheck(setting.Clock);
                    PlaceIn(row->Grid, row->Clock, 2);

                    row->StartStop = TableCheck(setting.StartStop);
                    PlaceIn(row->Grid, row->StartStop, 3);

                    row->SongPosition = TableCheck(setting.SongPosition);
                    PlaceIn(row->Grid, row->SongPosition, 4);

                    row->Offset = controls::NumberBox{};
                    row->Offset.Minimum(-seq::AppSettings::MaximumClockOffsetMilliseconds);
                    row->Offset.Maximum(seq::AppSettings::MaximumClockOffsetMilliseconds);
                    row->Offset.Value(setting.OffsetMilliseconds);
                    row->Offset.SmallChange(1);
                    row->Offset.LargeChange(10);
                    row->Offset.SpinButtonPlacementMode(controls::NumberBoxSpinButtonPlacementMode::Compact);
                    row->Offset.VerticalAlignment(xaml::VerticalAlignment::Center);
                    PlaceIn(row->Grid, row->Offset, 5);

                    row->Remove = controls::Button{};
                    row->Remove.Width(32);
                    row->Remove.Height(32);
                    row->Remove.Padding(xaml::Thickness{ 0, 0, 0, 0 });
                    row->Remove.Background(window->BrushFor(winrt::Microsoft::UI::Colors::Transparent()));
                    row->Remove.BorderThickness(xaml::Thickness{ 0, 0, 0, 0 });

                    controls::FontIcon bin{};
                    bin.Glyph(L"\uE74D");
                    bin.FontSize(13);
                    row->Remove.Content(bin);
                    PlaceIn(row->Grid, row->Remove, 6);

                    nameRow(*row);

                    std::weak_ptr<ClockRow> const weakRow{ row };

                    row->Device.SelectionChanged([weak, weakRow, nameRow](auto&&, auto&&)
                    {
                        auto const window = weak.get();
                        auto const current = weakRow.lock();

                        if (!window || !current)
                        {
                            return;
                        }

                        current->DeviceChanged = true;
                        window->FillGroupCombo(current->Group, false, ChosenEndpoint(current->Device, {}, true), 0, false);
                        nameRow(*current);
                    });

                    row->Remove.Click([weakRow, weakRows, weakPanel = winrt::make_weak(rowsHost), refresh](auto&&, auto&&)
                    {
                        auto const current = weakRow.lock();
                        auto const list = weakRows.lock();
                        auto const host = weakPanel.get();

                        if (!current || !list || !host)
                        {
                            return;
                        }

                        uint32_t index{ 0 };

                        if (host.Children().IndexOf(current->Grid, index))
                        {
                            host.Children().RemoveAt(index);
                        }

                        std::erase(*list, current);
                        refresh();
                    });

                    rowsHost.Children().Append(row->Grid);
                    list->push_back(std::move(row));
                    refresh();
                };

                for (auto const& output : settings.ClockOutputs())
                {
                    addRow(output);
                }

                refresh();

                add.Click([addRow, weakRows](auto&&, auto&&)
                {
                    auto const list = weakRows.lock();

                    if (list && list->size() < seq::AppSettings::MaximumClockOutputs)
                    {
                        addRow(seq::ClockOutputSetting{});

                        // Straight to the new row's device list.
                        if (!list->empty())
                        {
                            list->back()->Device.Focus(xaml::FocusState::Programmatic);
                        }
                    }
                });

                controls::Grid actions{};
                actions.ColumnSpacing(16);

                {
                    controls::ColumnDefinition first{};
                    first.Width(xaml::GridLength{ 1, xaml::GridUnitType::Auto });
                    actions.ColumnDefinitions().Append(first);

                    controls::ColumnDefinition second{};
                    second.Width(xaml::GridLength{ 1, xaml::GridUnitType::Star });
                    actions.ColumnDefinitions().Append(second);
                }

                add.VerticalAlignment(xaml::VerticalAlignment::Top);
                PlaceIn(actions, add, 0);

                controls::CheckBox sendTempo{};
                sendTempo.IsChecked(settings.ClockSendsTempo());
                automation::AutomationProperties::SetName(sendTempo, res::GetString(L"ClockSendTempo"));

                {
                    controls::StackPanel content{};
                    content.Spacing(2);

                    controls::TextBlock title{};
                    title.Text(res::GetString(L"ClockSendTempo"));
                    title.TextWrapping(xaml::TextWrapping::Wrap);
                    content.Children().Append(title);
                    content.Children().Append(Note(res::GetString(L"ClockSendTempoHelp"), secondary));
                    sendTempo.Content(content);
                }

                PlaceIn(actions, sendTempo, 1);
                panel.Children().Append(actions);

                // ---- MIDI Time Code ----

                panel.Children().Append(Heading(res::GetString(L"TimeCodeHeading")));

                auto const timeCode = settings.TimeCode();
                seq::EndpointRef const timeCodeEndpoint{ timeCode.EndpointName, timeCode.EndpointId };

                controls::StackPanel timeCodeRow{};
                timeCodeRow.Orientation(controls::Orientation::Horizontal);
                timeCodeRow.Spacing(8);

                controls::ToggleSwitch timeCodeOn{};
                timeCodeOn.OnContent(winrt::box_value(L""));
                timeCodeOn.OffContent(winrt::box_value(L""));
                timeCodeOn.MinWidth(0);
                timeCodeOn.Width(44);
                timeCodeOn.IsOn(timeCode.Enabled);
                timeCodeOn.VerticalAlignment(xaml::VerticalAlignment::Center);
                automation::AutomationProperties::SetName(timeCodeOn, res::GetString(L"TimeCodeSwitchName"));
                timeCodeRow.Children().Append(timeCodeOn);

                controls::TextBlock sendTo{};
                sendTo.Text(res::GetString(L"TimeCodeSendTo"));
                sendTo.VerticalAlignment(xaml::VerticalAlignment::Center);
                timeCodeRow.Children().Append(sendTo);

                controls::ComboBox timeCodeDevice{};
                timeCodeDevice.Width(ClockColumns[0]);
                timeCodeDevice.ItemTemplate(endpointTemplate);
                strong->FillEndpointCombo(timeCodeDevice, false, timeCodeEndpoint);
                automation::AutomationProperties::SetName(timeCodeDevice, res::GetString(L"TimeCodeDeviceName"));
                timeCodeRow.Children().Append(timeCodeDevice);

                controls::ComboBox timeCodeGroup{};
                timeCodeGroup.Width(ClockColumns[1]);
                timeCodeGroup.ItemTemplate(namedTemplate);
                strong->FillGroupCombo(timeCodeGroup, false, timeCodeEndpoint, timeCode.Group, false);
                automation::AutomationProperties::SetName(timeCodeGroup, res::GetString(L"TimeCodeGroupName"));
                timeCodeRow.Children().Append(timeCodeGroup);

                panel.Children().Append(timeCodeRow);

                controls::StackPanel rateRow{};
                rateRow.Orientation(controls::Orientation::Horizontal);
                rateRow.Spacing(8);
                rateRow.Margin(xaml::Thickness{ 52, 0, 0, 0 });

                controls::ComboBox rate{};
                rate.Width(190);

                for (auto const key : RateNames)
                {
                    rate.Items().Append(winrt::box_value(res::GetString(key)));
                }

                rate.SelectedIndex(std::clamp(timeCode.Rate, 0, 3));
                automation::AutomationProperties::SetName(rate, res::GetString(L"TimeCodeRateName"));
                rateRow.Children().Append(rate);

                controls::TextBlock startingAt{};
                startingAt.Text(res::GetString(L"TimeCodeStartingAt"));
                startingAt.VerticalAlignment(xaml::VerticalAlignment::Center);
                rateRow.Children().Append(startingAt);

                controls::TextBox start{};
                start.Width(120);
                start.Text(winrt::hstring{ timeCode.Start });
                start.FontFamily(media::FontFamily{ L"Cascadia Mono, Consolas" });
                automation::AutomationProperties::SetName(start, res::GetString(L"TimeCodeStartName"));
                rateRow.Children().Append(start);

                panel.Children().Append(rateRow);

                auto const timeCodeError = Note(L"", strong->ThemeBrush(L"SeqWarnTextBrush"));
                timeCodeError.Visibility(xaml::Visibility::Collapsed);
                timeCodeError.Margin(xaml::Thickness{ 52, 0, 0, 0 });
                panel.Children().Append(timeCodeError);

                auto const timeCodeHelp = Note(res::GetString(L"TimeCodeHelp"), secondary);
                timeCodeHelp.Margin(xaml::Thickness{ 52, 0, 0, 0 });
                panel.Children().Append(timeCodeHelp);

                auto const timeCodeChanged = std::make_shared<bool>(false);

                timeCodeDevice.SelectionChanged([weak, weakDevice = winrt::make_weak(timeCodeDevice), weakGroup = winrt::make_weak(timeCodeGroup), timeCodeChanged](auto&&, auto&&)
                {
                    auto const window = weak.get();
                    auto const device = weakDevice.get();
                    auto const group = weakGroup.get();

                    if (window && device && group)
                    {
                        *timeCodeChanged = true;
                        window->FillGroupCombo(group, false, ChosenEndpoint(device, {}, true), 0, false);
                    }
                });

                // ---- timing ----

                panel.Children().Append(Heading(res::GetString(L"TimingTitle")));

                controls::CheckBox offsets{};
                offsets.Content(winrt::box_value(res::GetString(L"ApplyOffsetsCheck")));
                offsets.IsChecked(settings.ApplyDeviceOffsets());
                panel.Children().Append(offsets);
                panel.Children().Append(Note(res::GetString(L"ApplyOffsetsHelp"), secondary));
                panel.Children().Append(Note(res::GetString(L"ApplyOffsetsWhenOff"), secondary));

                controls::ScrollViewer scroller{};
                scroller.Content(panel);
                scroller.HorizontalScrollBarVisibility(controls::ScrollBarVisibility::Auto);
                scroller.HorizontalScrollMode(controls::ScrollMode::Auto);
                scroller.VerticalScrollBarVisibility(controls::ScrollBarVisibility::Auto);

                controls::ContentDialog dialog{};
                dialog.XamlRoot(strong->Content().XamlRoot());
                dialog.Resources().Insert(winrt::box_value(L"ContentDialogMaxWidth"), winrt::box_value(940.0));
                dialog.Title(winrt::box_value(res::GetString(L"ClockDialogTitle")));
                dialog.Content(scroller);
                dialog.PrimaryButtonText(res::GetString(L"DialogDone"));
                dialog.CloseButtonText(res::GetString(L"DialogCancel"));
                dialog.DefaultButton(controls::ContentDialogButton::Primary);
                dialog.RequestedTheme(strong->RootGrid().ActualTheme());

                // Following needs a device; time code that's on needs a device and a start it can
                // read. The dialog stays open to say so.
                dialog.PrimaryButtonClick([fromDevice, followDevice, followEndpoint, followChanged, followError, timeCodeOn, timeCodeDevice, rate, start, timeCodeError, timeCodeChanged, timeCodeEndpoint](auto&&, controls::ContentDialogButtonClickEventArgs const& args)
                {
                    if (fromDevice.IsChecked() != nullptr && fromDevice.IsChecked().GetBoolean() &&
                        ChosenEndpoint(followDevice, followEndpoint, *followChanged).IsEmpty())
                    {
                        followError.Text(res::GetString(L"FollowNoDevice"));
                        followError.Visibility(xaml::Visibility::Visible);
                        args.Cancel(true);
                        return;
                    }

                    if (!timeCodeOn.IsOn())
                    {
                        return;
                    }

                    if (ChosenEndpoint(timeCodeDevice, timeCodeEndpoint, *timeCodeChanged).IsEmpty())
                    {
                        timeCodeError.Text(res::GetString(L"TimeCodeNoDevice"));
                        timeCodeError.Visibility(xaml::Visibility::Visible);
                        args.Cancel(true);
                        return;
                    }

                    midiapp::MidiTimeCodePosition position{};

                    if (!midiapp::TryParsePosition(std::wstring{ start.Text() }, midiapp::FrameRateFromValue(std::clamp(rate.SelectedIndex(), 0, 3)), position))
                    {
                        timeCodeError.Text(res::GetString(L"TimeCodeBadStart"));
                        timeCodeError.Visibility(xaml::Visibility::Visible);
                        args.Cancel(true);
                    }
                });

                auto const result = co_await dialog.ShowAsync();

                if (result == controls::ContentDialogResult::Primary && !strong->m_closing)
                {
                    std::vector<seq::ClockOutputSetting> outputs{};

                    for (auto const& row : *rows)
                    {
                        seq::EndpointRef const original{ row->Original.EndpointName, row->Original.EndpointId };
                        auto const endpoint = ChosenEndpoint(row->Device, original, row->DeviceChanged);

                        if (endpoint.IsEmpty())
                        {
                            continue;
                        }

                        auto const offset = row->Offset.Value();

                        seq::ClockOutputSetting output{};
                        output.EndpointName = endpoint.Name;
                        output.EndpointId = endpoint.Id;
                        output.Group = ChosenGroup(row->Group, row->Original.Group);
                        output.Clock = IsChecked(row->Clock);
                        output.StartStop = IsChecked(row->StartStop);
                        output.SongPosition = IsChecked(row->SongPosition);
                        output.OffsetMilliseconds = std::isnan(offset) ? 0 : static_cast<int32_t>(std::clamp(std::lround(offset),
                            -static_cast<long>(seq::AppSettings::MaximumClockOffsetMilliseconds), static_cast<long>(seq::AppSettings::MaximumClockOffsetMilliseconds)));
                        outputs.push_back(std::move(output));
                    }

                    settings.ClockOutputs(outputs);
                    settings.ClockSendsTempo(IsChecked(sendTempo));

                    auto updated = timeCode;
                    auto const endpoint = ChosenEndpoint(timeCodeDevice, timeCodeEndpoint, *timeCodeChanged);
                    updated.Enabled = timeCodeOn.IsOn() && !endpoint.IsEmpty();
                    updated.EndpointName = endpoint.Name;
                    updated.EndpointId = endpoint.Id;
                    updated.Group = ChosenGroup(timeCodeGroup, timeCode.Group);
                    updated.Rate = std::clamp(rate.SelectedIndex(), 0, 3);

                    midiapp::MidiTimeCodePosition position{};
                    auto const frameRate = midiapp::FrameRateFromValue(updated.Rate);

                    if (midiapp::TryParsePosition(std::wstring{ start.Text() }, frameRate, position))
                    {
                        updated.Start = midiapp::FormatPosition(midiapp::ClampPosition(position, frameRate), frameRate);
                    }

                    settings.TimeCode(updated);

                    auto followUpdated = follow;
                    auto const followChosen = ChosenEndpoint(followDevice, followEndpoint, *followChanged);
                    followUpdated.Enabled = fromDevice.IsChecked() != nullptr && fromDevice.IsChecked().GetBoolean() && !followChosen.IsEmpty();

                    if (!followChosen.IsEmpty())
                    {
                        followUpdated.EndpointName = followChosen.Name;
                        followUpdated.EndpointId = followChosen.Id;
                        followUpdated.Group = ChosenGroup(followGroup, follow.Group);
                    }

                    settings.FollowClock(followUpdated);

                    auto const applyOffsets = IsChecked(offsets);
                    settings.ApplyDeviceOffsets(applyOffsets);
                    strong->m_directory->ApplyOffsets(applyOffsets);

                    strong->ApplyEngineSettings();
                    strong->PrepareConnectionsAsync();
                    strong->UpdateProtocolNote();
                    strong->UpdateStatusBar();
                }

                // The rows' controls refer back to the list; let them go with the dialog.
                rows->clear();
            }
            MIDI_SEQUENCER_CATCH_AND_LOG(L"Unable to show the clock settings.")
        }(get_strong());
    }
}
