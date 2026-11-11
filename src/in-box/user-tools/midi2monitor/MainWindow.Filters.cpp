// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================
//
// Advanced mode's filter: the same choices as a Windows MIDI Patchbay filter step, from the
// same shared MessageFilter, so a box means the same thing in both apps. Filters hide rows;
// they never stop the capture, so loosening one brings back what it hid.
//
// The Clock and Active Sensing boxes are the status bar's two buttons, not part of the
// MessageFilter, so turning one off in either place shows in both.
//

#include "pch.h"
#include "MainWindow.xaml.h"

#include "MonitorPalette.h"
#include "PopupBackdrop.h"
#include "SourceTable.h"
#include "StringResources.h"

namespace native = ::midi2monitor;
namespace res = ::midi2monitor::resources;
namespace automation = ::winrt::Microsoft::UI::Xaml::Automation;
namespace json = ::winrt::Windows::Data::Json;

namespace winrt::midi2monitor::implementation
{
    namespace
    {
        // Three columns of check boxes, the longest "RPN and NRPN (MIDI 2.0)", each on one line.
        constexpr double FilterFlyoutWidth = 700.0;

        // The UMP message types the filter offers, in the order it shows them, with the
        // resource key for each.
        struct TypeChoice
        {
            ::midiapp::UmpMessageType Type;
            wchar_t const* Key;
        };

        constexpr TypeChoice TypeChoices[] =
        {
            { ::midiapp::UmpMessageType::Utility, L"FilterTypeUtility" },
            { ::midiapp::UmpMessageType::System, L"FilterTypeSystem" },
            { ::midiapp::UmpMessageType::Midi1ChannelVoice, L"FilterTypeMidi1" },
            { ::midiapp::UmpMessageType::Midi2ChannelVoice, L"FilterTypeMidi2" },
            { ::midiapp::UmpMessageType::DataSysEx7, L"FilterTypeSysEx7" },
            { ::midiapp::UmpMessageType::Data128, L"FilterTypeData128" },
            { ::midiapp::UmpMessageType::FlexData, L"FilterTypeFlexData" },
            { ::midiapp::UmpMessageType::Stream, L"FilterTypeStream" },
        };

        // Channel voice choices. A box can stand for several statuses: the MIDI 2.0 per-note
        // messages are one box, and so are the four RPN and NRPN messages.
        struct VoiceChoice
        {
            std::array<uint8_t, 4> Statuses;
            uint8_t Count;
            wchar_t const* Key;
        };

        constexpr VoiceChoice VoiceChoices[] =
        {
            { { 0x9 }, 1, L"FilterVoiceNoteOn" },
            { { 0x8 }, 1, L"FilterVoiceNoteOff" },
            { { 0xA }, 1, L"FilterVoicePolyPressure" },
            { { 0xB }, 1, L"FilterVoiceControlChange" },
            { { 0xC }, 1, L"FilterVoiceProgramChange" },
            { { 0xD }, 1, L"FilterVoiceChannelPressure" },
            { { 0xE }, 1, L"FilterVoicePitchBend" },
            { { 0x0, 0x1, 0x6, 0xF }, 4, L"FilterVoicePerNote" },
            { { 0x2, 0x3, 0x4, 0x5 }, 4, L"FilterVoiceRpnNrpn" },
        };

        // System choices that are the filter's. Clock and Active Sensing are the status bar's.
        struct SystemChoice
        {
            std::array<uint8_t, 3> Statuses;
            uint8_t Count;
            wchar_t const* Key;
        };

        constexpr SystemChoice SystemChoices[] =
        {
            { { 0xF1 }, 1, L"FilterSystemTimeCode" },
            { { 0xF2 }, 1, L"FilterSystemSongPosition" },
            { { 0xF3 }, 1, L"FilterSystemSongSelect" },
            { { 0xF6 }, 1, L"FilterSystemTuneRequest" },
            { { 0xFA, 0xFB, 0xFC }, 3, L"FilterSystemTransport" },
            { { 0xFF }, 1, L"FilterSystemReset" },
        };

        bool SystemAllowed(_In_ ::midiapp::MessageFilter const& filter, _In_ uint8_t status) noexcept
        {
            auto const index = ::midiapp::IndexOfSystemMessage(status);
            return index >= ::midiapp::SystemMessageCount || filter.SystemMessages[index];
        }

        void SystemAllowed(_Inout_ ::midiapp::MessageFilter& filter, _In_ uint8_t status, _In_ bool value) noexcept
        {
            auto const index = ::midiapp::IndexOfSystemMessage(status);

            if (index < ::midiapp::SystemMessageCount)
            {
                filter.SystemMessages[index] = value;
            }
        }

        // The system messages the filter owns, which leaves out Clock and Active Sensing.
        bool FilterSystemAllShown(_In_ ::midiapp::MessageFilter const& filter) noexcept
        {
            for (auto const& choice : SystemChoices)
            {
                for (uint8_t i = 0; i < choice.Count; i++)
                {
                    if (!SystemAllowed(filter, choice.Statuses[i]))
                    {
                        return false;
                    }
                }
            }

            return true;
        }

        std::wstring NoteName(_In_ uint8_t note)
        {
            auto const name = midi2msg::MidiMessageHelper::GetNoteDisplayNameFromNoteIndex(note);
            auto const octave = midi2msg::MidiMessageHelper::GetNoteOctaveFromNoteIndex(note);

            return std::format(L"{}{} ({})", name.c_str(), octave, note);
        }

        // "1 to 4, 10" from the channels let through, counted from 1 the way people count them
        std::wstring ChannelRuns(_In_ ::midiapp::MessageFilter const& filter)
        {
            std::wstring text{};
            int32_t runStart{ -1 };

            for (int32_t channel = 0; channel <= 16; channel++)
            {
                auto const on = channel < 16 && filter.Channels[static_cast<size_t>(channel)];

                if (on && runStart < 0)
                {
                    runStart = channel;
                }
                else if (!on && runStart >= 0)
                {
                    if (!text.empty())
                    {
                        text += L", ";
                    }

                    text += (channel - 1 == runStart)
                        ? std::format(L"{}", runStart + 1)
                        : std::wstring{ res::FormatString(L"FilterChannelRunFormat", runStart + 1, channel) };

                    runStart = -1;
                }
            }

            return text;
        }

        controls::TextBlock SectionTitle(_In_ wchar_t const* key)
        {
            controls::TextBlock title{};
            title.Text(res::GetString(key));
            title.Style(xaml::Application::Current().Resources()
                .Lookup(winrt::box_value(L"CaptionTextBlockStyle")).as<xaml::Style>());
            title.Foreground(xaml::Application::Current().Resources()
                .Lookup(winrt::box_value(L"TextFillColorSecondaryBrush")).as<media::Brush>());
            title.Margin(xaml::ThicknessHelper::FromLengths(0, 6, 0, 2));
            return title;
        }

        controls::Grid ColumnsGrid(_In_ int32_t columns)
        {
            controls::Grid grid{};
            grid.ColumnSpacing(12);

            for (int32_t i = 0; i < columns; i++)
            {
                controls::ColumnDefinition column{};
                column.Width(xaml::GridLengthHelper::FromValueAndType(1, xaml::GridUnitType::Star));
                grid.ColumnDefinitions().Append(column);
            }

            return grid;
        }

        void PlaceInGrid(_Inout_ controls::Grid const& grid, _In_ xaml::FrameworkElement const& element, _In_ int32_t index, _In_ int32_t columns)
        {
            auto const row = index / columns;

            while (static_cast<int32_t>(grid.RowDefinitions().Size()) <= row)
            {
                grid.RowDefinitions().Append(controls::RowDefinition{});
            }

            controls::Grid::SetRow(element, row);
            controls::Grid::SetColumn(element, index % columns);
            grid.Children().Append(element);
        }
    }

    // ------------------------------------------------------------------------------------
    // Keeping the filter
    // ------------------------------------------------------------------------------------

    void MainWindow::LoadMessageFilter() noexcept
    {
        try
        {
            m_messageFilter = ::midiapp::MessageFilter{};

            auto const text = native::AppSettings::Current().AdvancedFilter();

            if (text.empty())
            {
                return;
            }

            json::JsonObject object{ nullptr };

            // written by this app, but in the registry where anything can change it
            if (json::JsonObject::TryParse(text, object) && object != nullptr)
            {
                m_messageFilter = ::midiapp::FilterFromJson(object);
            }
        }
        MIDI_MONITOR_CATCH_AND_LOG(L"Unable to read the saved filter.")
    }

    void MainWindow::ApplyMessageFilter() noexcept
    {
        try
        {
            // a filter that lets everything through costs one check per message
            m_messageFilter.IsActive = true;
            m_messageFilter.IsActive = !m_messageFilter.PassesEverything();

            native::AppSettings::Current().AdvancedFilter(
                std::wstring{ ::midiapp::FilterToJson(m_messageFilter).Stringify() });

            if (m_advancedMode)
            {
                m_pipeline.PostMessageFilter(m_messageFilter);
            }

            RebuildChips();
        }
        MIDI_MONITOR_CATCH_AND_LOG(L"Unable to apply the filter.")
    }

    // ------------------------------------------------------------------------------------
    // The filter panel
    // ------------------------------------------------------------------------------------

    _Use_decl_annotations_
    void MainWindow::OnFilterButtonClick(foundation::IInspectable const&, xaml::RoutedEventArgs const&)
    {
        try
        {
            BuildFilterFlyout();

            if (m_filterFlyout != nullptr)
            {
                m_filterFlyout.ShowAt(FilterButton());
            }
        }
        MIDI_MONITOR_CATCH_AND_LOG(L"Unable to open the filter.")
    }

    // Built each time it opens, so it always shows the filter as it is now, including changes
    // made with the chips or the status bar buttons.
    void MainWindow::BuildFilterFlyout() noexcept
    {
        try
        {
            auto const weak = get_weak();
            auto const& settings = native::AppSettings::Current();

            controls::StackPanel panel{};
            panel.Width(FilterFlyoutWidth);
            panel.Spacing(4);

            // ---------------- heading ----------------

            controls::Grid heading{};
            heading.ColumnDefinitions().Append(controls::ColumnDefinition{});
            heading.ColumnDefinitions().Append(controls::ColumnDefinition{});
            heading.ColumnDefinitions().GetAt(1).Width(xaml::GridLengthHelper::Auto());

            controls::TextBlock title{};
            title.Text(res::GetString(L"FilterTitle"));
            title.VerticalAlignment(xaml::VerticalAlignment::Center);
            title.Style(xaml::Application::Current().Resources()
                .Lookup(winrt::box_value(L"BodyStrongTextBlockStyle")).as<xaml::Style>());

            controls::Button showEverything{};
            showEverything.Content(winrt::box_value(res::GetString(L"FilterShowEverything")));
            controls::Grid::SetColumn(showEverything, 1);

            showEverything.Click([weak](auto&&, auto&&)
                {
                    auto strong = weak.get();

                    if (strong == nullptr)
                    {
                        return;
                    }

                    try
                    {
                        strong->m_messageFilter.Reset();
                        native::AppSettings::Current().FoldRepeats(false);
                        strong->m_pipeline.PostFoldRepeats(false);
                        strong->ShowClockToggle().IsChecked(true);
                        strong->ShowActiveSenseToggle().IsChecked(true);
                        strong->ApplyMessageFilter();

                        if (strong->m_filterFlyout != nullptr)
                        {
                            strong->m_filterFlyout.Hide();
                        }
                    }
                    MIDI_MONITOR_CATCH_AND_LOG(L"Unable to show everything.")
                });

            heading.Children().Append(title);
            heading.Children().Append(showEverything);
            // the heading stays in place above the part that scrolls
            controls::StackPanel top{};
            top.Width(FilterFlyoutWidth);
            top.Spacing(4);
            top.Margin(xaml::ThicknessHelper::FromLengths(0, 0, 12, 8));
            top.Children().Append(heading);

            controls::TextBlock help{};
            help.Text(res::GetString(L"FilterHelp"));
            help.TextWrapping(xaml::TextWrapping::Wrap);
            help.Style(xaml::Application::Current().Resources()
                .Lookup(winrt::box_value(L"CaptionTextBlockStyle")).as<xaml::Style>());
            help.Foreground(xaml::Application::Current().Resources()
                .Lookup(winrt::box_value(L"TextFillColorSecondaryBrush")).as<media::Brush>());
            top.Children().Append(help);

            // a check box whose change runs the given update, then applies the filter
            auto const makeCheck = [weak](wchar_t const* key, bool isChecked, std::function<void(MainWindow&, bool)> update)
                {
                    controls::CheckBox box{};
                    box.Content(winrt::box_value(res::GetString(key)));
                    box.IsChecked(isChecked);
                    box.MinWidth(0);

                    box.Click([weak, update](foundation::IInspectable const& sender, auto&&)
                        {
                            auto strong = weak.get();
                            auto checkBox = sender.try_as<controls::CheckBox>();

                            if (strong == nullptr || checkBox == nullptr)
                            {
                                return;
                            }

                            try
                            {
                                auto const value = checkBox.IsChecked();
                                update(*strong, value != nullptr && value.Value());
                                strong->ApplyMessageFilter();
                            }
                            MIDI_MONITOR_CATCH_AND_LOG(L"Unable to change the filter.")
                        });

                    return box;
                };

            // ---------------- message types ----------------

            panel.Children().Append(SectionTitle(L"FilterMessageTypesHeader"));

            auto types = ColumnsGrid(2);
            int32_t index{ 0 };

            for (auto const& choice : TypeChoices)
            {
                auto const type = static_cast<size_t>(choice.Type);

                PlaceInGrid(types, makeCheck(choice.Key, m_messageFilter.MessageTypes[type],
                    [type](MainWindow& window, bool value) { window.m_messageFilter.MessageTypes[type] = value; }),
                    index++, 2);
            }

            panel.Children().Append(types);

            // ---------------- channels ----------------

            controls::Grid channelsHeading{};
            channelsHeading.ColumnDefinitions().Append(controls::ColumnDefinition{});
            channelsHeading.ColumnDefinitions().Append(controls::ColumnDefinition{});
            channelsHeading.ColumnDefinitions().GetAt(1).Width(xaml::GridLengthHelper::Auto());
            channelsHeading.Children().Append(SectionTitle(L"FilterChannelsHeader"));

            controls::HyperlinkButton allChannels{};
            allChannels.Content(winrt::box_value(res::GetString(L"FilterChannelsAll")));
            allChannels.Padding(xaml::ThicknessHelper::FromLengths(4, 0, 4, 0));
            controls::Grid::SetColumn(allChannels, 1);
            channelsHeading.Children().Append(allChannels);
            panel.Children().Append(channelsHeading);

            controls::Grid channels{};
            channels.ColumnSpacing(4);

            std::vector<controls::Primitives::ToggleButton> channelButtons{};

            for (int32_t channel = 0; channel < 16; channel++)
            {
                controls::ColumnDefinition column{};
                column.Width(xaml::GridLengthHelper::FromValueAndType(1, xaml::GridUnitType::Star));
                channels.ColumnDefinitions().Append(column);

                controls::Primitives::ToggleButton button{};
                button.Content(winrt::box_value(std::format(L"{}", channel + 1)));
                button.IsChecked(m_messageFilter.Channels[static_cast<size_t>(channel)]);
                button.MinWidth(0);
                button.Padding(xaml::ThicknessHelper::FromLengths(0, 4, 0, 4));
                button.HorizontalAlignment(xaml::HorizontalAlignment::Stretch);
                automation::AutomationProperties::SetName(button, res::FormatString(L"FilterChannelAccessibleFormat", channel + 1));
                controls::Grid::SetColumn(button, channel);

                button.Click([weak, channel](foundation::IInspectable const& sender, auto&&)
                    {
                        auto strong = weak.get();
                        auto toggle = sender.try_as<controls::Primitives::ToggleButton>();

                        if (strong == nullptr || toggle == nullptr)
                        {
                            return;
                        }

                        auto const value = toggle.IsChecked();
                        strong->m_messageFilter.Channels[static_cast<size_t>(channel)] = value != nullptr && value.Value();
                        strong->ApplyMessageFilter();
                    });

                channels.Children().Append(button);
                channelButtons.push_back(button);
            }

            allChannels.Click([weak, channelButtons](auto&&, auto&&)
                {
                    auto strong = weak.get();

                    if (strong == nullptr)
                    {
                        return;
                    }

                    strong->m_messageFilter.Channels.fill(true);

                    for (auto const& button : channelButtons)
                    {
                        button.IsChecked(true);
                    }

                    strong->ApplyMessageFilter();
                });

            panel.Children().Append(channels);

            // ---------------- channel voice messages ----------------

            panel.Children().Append(SectionTitle(L"FilterVoiceHeader"));

            auto voice = ColumnsGrid(3);
            index = 0;

            for (auto const& choice : VoiceChoices)
            {
                bool allowed{ true };

                for (uint8_t i = 0; i < choice.Count; i++)
                {
                    allowed = allowed && m_messageFilter.ChannelVoiceStatuses[choice.Statuses[i]];
                }

                PlaceInGrid(voice, makeCheck(choice.Key, allowed,
                    [choice](MainWindow& window, bool value)
                    {
                        for (uint8_t i = 0; i < choice.Count; i++)
                        {
                            window.m_messageFilter.ChannelVoiceStatuses[choice.Statuses[i]] = value;
                        }
                    }),
                    index++, 3);
            }

            panel.Children().Append(voice);

            // ---------------- system messages ----------------

            panel.Children().Append(SectionTitle(L"FilterSystemHeader"));

            auto system = ColumnsGrid(3);
            index = 0;

            for (auto const& choice : SystemChoices)
            {
                bool allowed{ true };

                for (uint8_t i = 0; i < choice.Count; i++)
                {
                    allowed = allowed && SystemAllowed(m_messageFilter, choice.Statuses[i]);
                }

                PlaceInGrid(system, makeCheck(choice.Key, allowed,
                    [choice](MainWindow& window, bool value)
                    {
                        for (uint8_t i = 0; i < choice.Count; i++)
                        {
                            SystemAllowed(window.m_messageFilter, choice.Statuses[i], value);
                        }
                    }),
                    index++, 3);
            }

            // the status bar's two buttons, shown here too
            PlaceInGrid(system, makeCheck(L"FilterSystemClock", settings.ShowClockMessages(),
                [](MainWindow& window, bool value) { window.ShowClockToggle().IsChecked(value); }),
                index++, 3);

            PlaceInGrid(system, makeCheck(L"FilterSystemActiveSensing", settings.ShowActiveSenseMessages(),
                [](MainWindow& window, bool value) { window.ShowActiveSenseToggle().IsChecked(value); }),
                index++, 3);

            panel.Children().Append(system);

            // ---------------- notes ----------------

            panel.Children().Append(SectionTitle(L"FilterNotesHeader"));

            controls::StackPanel notes{};
            notes.Orientation(controls::Orientation::Horizontal);
            notes.Spacing(8);

            auto limit = makeCheck(L"FilterNotesLimit", m_messageFilter.LimitNoteRange,
                [](MainWindow& window, bool value) { window.m_messageFilter.LimitNoteRange = value; });

            controls::ComboBox lowest{};
            controls::ComboBox highest{};

            for (uint8_t note = 0; note <= 127; note++)
            {
                auto const name = winrt::box_value(winrt::hstring{ NoteName(note) });
                lowest.Items().Append(name);
                highest.Items().Append(winrt::box_value(winrt::hstring{ NoteName(note) }));
            }

            lowest.SelectedIndex(m_messageFilter.LowestAllowedNote);
            highest.SelectedIndex(m_messageFilter.HighestAllowedNote);
            lowest.MinWidth(120);
            highest.MinWidth(120);
            automation::AutomationProperties::SetName(lowest, res::GetString(L"FilterNotesLowest"));
            automation::AutomationProperties::SetName(highest, res::GetString(L"FilterNotesHighest"));

            // weak: each box owns this handler, so a strong reference back would keep them alive
            auto const onNoteChanged = [weak,
                weakLowest = winrt::make_weak(lowest),
                weakHighest = winrt::make_weak(highest),
                weakLimit = winrt::make_weak(limit)](auto&&, auto&&)
                {
                    auto strong = weak.get();
                    auto lowest = weakLowest.get();
                    auto highest = weakHighest.get();
                    auto limit = weakLimit.get();

                    if (strong == nullptr || lowest == nullptr || highest == nullptr || limit == nullptr ||
                        lowest.SelectedIndex() < 0 || highest.SelectedIndex() < 0)
                    {
                        return;
                    }

                    // picking a range turns the range on
                    auto low = static_cast<uint8_t>(lowest.SelectedIndex());
                    auto high = static_cast<uint8_t>(highest.SelectedIndex());

                    if (low > high)
                    {
                        std::swap(low, high);
                    }

                    strong->m_messageFilter.LowestAllowedNote = low;
                    strong->m_messageFilter.HighestAllowedNote = high;
                    strong->m_messageFilter.LimitNoteRange = true;
                    limit.IsChecked(true);
                    strong->ApplyMessageFilter();
                };

            lowest.SelectionChanged(onNoteChanged);
            highest.SelectionChanged(onNoteChanged);

            controls::TextBlock to{};
            to.Text(res::GetString(L"FilterNotesTo"));
            to.VerticalAlignment(xaml::VerticalAlignment::Center);

            notes.Children().Append(limit);
            notes.Children().Append(lowest);
            notes.Children().Append(to);
            notes.Children().Append(highest);
            panel.Children().Append(notes);

            // ---------------- folding repeats, which is the Monitor's own ----------------

            controls::Border line{};
            line.Height(1);
            line.Margin(xaml::ThicknessHelper::FromLengths(0, 10, 0, 6));
            line.Background(xaml::Application::Current().Resources()
                .Lookup(winrt::box_value(L"DividerStrokeColorDefaultBrush")).as<media::Brush>());
            panel.Children().Append(line);

            panel.Children().Append(makeCheck(L"FilterFoldRepeats", settings.FoldRepeats(),
                [](MainWindow& window, bool value)
                {
                    native::AppSettings::Current().FoldRepeats(value);
                    window.m_pipeline.PostFoldRepeats(value);
                }));

            // Short enough to fit in the window below the toolbar, so nothing is cut off at the
            // bottom; the part under the heading scrolls when it can't all show.
            auto const windowHeight = RootGrid().ActualHeight();
            auto const scrollHeight = (std::max)(200.0, (windowHeight > 0 ? windowHeight - 220.0 : 620.0));

            controls::ScrollViewer scroller{};
            scroller.Content(panel);
            scroller.MaxHeight((std::min)(620.0, scrollHeight));
            scroller.VerticalScrollBarVisibility(controls::ScrollBarVisibility::Auto);
            scroller.Padding(xaml::ThicknessHelper::FromLengths(0, 0, 12, 0));

            controls::Grid layout{};
            layout.RowDefinitions().Append(controls::RowDefinition{});
            layout.RowDefinitions().Append(controls::RowDefinition{});
            layout.RowDefinitions().GetAt(0).Height(xaml::GridLengthHelper::Auto());
            controls::Grid::SetRow(scroller, 1);
            layout.Children().Append(top);
            layout.Children().Append(scroller);

            controls::Flyout flyout{};
            flyout.Content(layout);
            flyout.Placement(controls::Primitives::FlyoutPlacementMode::BottomEdgeAlignedLeft);

            // the flyout's width comes from its presenter's MaxWidth, which is narrower than this
            xaml::Style presenterStyle{ winrt::xaml_typename<controls::FlyoutPresenter>() };
            presenterStyle.BasedOn(xaml::Application::Current().Resources()
                .Lookup(winrt::box_value(L"DefaultFlyoutPresenterStyle")).as<xaml::Style>());
            presenterStyle.Setters().Append(xaml::Setter{ xaml::FrameworkElement::MaxWidthProperty(), winrt::box_value(FilterFlyoutWidth + 64) });

            // its own scrolling would move the heading away with the rest
            presenterStyle.Setters().Append(xaml::Setter{ controls::ScrollViewer::VerticalScrollModeProperty(), winrt::box_value(controls::ScrollMode::Disabled) });
            presenterStyle.Setters().Append(xaml::Setter{ controls::ScrollViewer::VerticalScrollBarVisibilityProperty(), winrt::box_value(controls::ScrollBarVisibility::Disabled) });
            flyout.FlyoutPresenterStyle(presenterStyle);

            midiapp::UseMenuBackdrop(flyout);

            m_filterFlyout = flyout;
        }
        MIDI_MONITOR_CATCH_AND_LOG(L"Unable to build the filter.")
    }

    // ------------------------------------------------------------------------------------
    // Chips: one for each rule, and in a narrow window one for each source
    // ------------------------------------------------------------------------------------

    void MainWindow::RebuildChips() noexcept
    {
        try
        {
            auto const weak = get_weak();
            auto panel = ChipsPanel();
            panel.Children().Clear();

            if (!m_advancedMode)
            {
                return;
            }

            auto const resources = xaml::Application::Current().Resources();

            auto const makeChip = [&resources](winrt::hstring const& text, media::Brush const& swatch, std::function<void()> remove)
                {
                    controls::Border chip{};
                    chip.Height(28);
                    chip.CornerRadius(xaml::CornerRadiusHelper::FromUniformRadius(14));
                    chip.Padding(xaml::ThicknessHelper::FromLengths(10, 0, 2, 0));
                    chip.BorderThickness(xaml::ThicknessHelper::FromUniformLength(1));
                    chip.Background(resources.Lookup(winrt::box_value(L"ControlFillColorSecondaryBrush")).as<media::Brush>());
                    chip.BorderBrush(resources.Lookup(winrt::box_value(L"ControlStrokeColorDefaultBrush")).as<media::Brush>());
                    chip.VerticalAlignment(xaml::VerticalAlignment::Center);

                    controls::StackPanel content{};
                    content.Orientation(controls::Orientation::Horizontal);
                    content.Spacing(6);

                    if (swatch != nullptr)
                    {
                        xaml::Shapes::Ellipse dot{};
                        dot.Width(8);
                        dot.Height(8);
                        dot.Fill(swatch);
                        dot.VerticalAlignment(xaml::VerticalAlignment::Center);
                        content.Children().Append(dot);
                    }

                    controls::TextBlock label{};
                    label.Text(text);
                    label.FontSize(12);
                    label.VerticalAlignment(xaml::VerticalAlignment::Center);
                    content.Children().Append(label);

                    controls::Button close{};
                    close.Width(22);
                    close.Height(22);
                    close.MinWidth(0);
                    close.MinHeight(0);
                    close.Padding(xaml::ThicknessHelper::FromUniformLength(0));
                    close.BorderThickness(xaml::ThicknessHelper::FromUniformLength(0));
                    close.Background(media::SolidColorBrush{ winrt::Windows::UI::Colors::Transparent() });
                    close.VerticalAlignment(xaml::VerticalAlignment::Center);

                    controls::FontIcon glyph{};
                    glyph.Glyph(L"\uE711");
                    glyph.FontSize(9);
                    close.Content(glyph);

                    automation::AutomationProperties::SetName(close, res::FormatString(L"ChipRemoveFormat", std::wstring{ text }));
                    controls::ToolTipService::SetToolTip(close, winrt::box_value(res::FormatString(L"ChipRemoveFormat", std::wstring{ text })));

                    close.Click([remove](auto&&, auto&&)
                        {
                            try
                            {
                                remove();
                            }
                            MIDI_MONITOR_CATCH_AND_LOG(L"Unable to remove a chip.")
                        });

                    content.Children().Append(close);
                    chip.Child(content);

                    return chip;
                };

            // ---------------- sources, when the list is folded away ----------------

            if (m_narrowLayout)
            {
                for (auto const& row : m_sourceRows)
                {
                    auto const mask = GroupMaskFor(row.EndpointDeviceId) & AllGroupsMask(row);

                    for (uint8_t group = 1; group <= 16 && row.Slot != native::NoSourceSlot; group++)
                    {
                        if ((mask & (1u << (group - 1))) == 0)
                        {
                            continue;
                        }

                        auto const id = row.EndpointDeviceId;
                        auto const text = native::SourceTable::Describe(row.Slot, group);
                        auto const swatch = native::MonitorPalette::SourceBrush(native::SourceTable::ColorIndex(row.Slot, group));

                        panel.Children().Append(makeChip(text, swatch, [weak, id, group]()
                            {
                                if (auto strong = weak.get())
                                {
                                    strong->OnSourceCheckChanged(id, group, false);
                                }
                            }));
                    }
                }

                controls::Button add{};
                add.Height(28);
                add.Padding(xaml::ThicknessHelper::FromLengths(10, 0, 10, 0));
                add.CornerRadius(xaml::CornerRadiusHelper::FromUniformRadius(14));
                add.Content(winrt::box_value(res::GetString(L"ChipAddSource")));
                add.Click([weak](auto&&, auto&&)
                    {
                        if (auto strong = weak.get())
                        {
                            strong->SourcesPaneToggle().IsChecked(true);
                        }
                    });

                panel.Children().Append(add);
            }

            // ---------------- the filter's rules ----------------

            auto const& settings = native::AppSettings::Current();
            uint32_t ruleCount{ 0 };

            auto const addRule = [&](winrt::hstring const& text, std::function<void(MainWindow&)> remove)
                {
                    ruleCount++;

                    panel.Children().Append(makeChip(text, nullptr, [weak, remove]()
                        {
                            if (auto strong = weak.get())
                            {
                                remove(*strong);
                                strong->ApplyMessageFilter();
                            }
                        }));
                };

            if (!settings.ShowClockMessages())
            {
                addRule(res::GetString(L"ChipClockHidden"), [](MainWindow& window) { window.ShowClockToggle().IsChecked(true); });
            }

            if (!settings.ShowActiveSenseMessages())
            {
                addRule(res::GetString(L"ChipActiveSensingHidden"), [](MainWindow& window) { window.ShowActiveSenseToggle().IsChecked(true); });
            }

            if (m_messageFilter.IsActive)
            {
                if (!::midiapp::AllTrue(m_messageFilter.MessageTypes.data(), m_messageFilter.MessageTypes.size()))
                {
                    addRule(res::GetString(L"ChipMessageTypesHidden"), [](MainWindow& window) { window.m_messageFilter.MessageTypes.fill(true); });
                }

                if (!::midiapp::AllTrue(m_messageFilter.Channels.data(), m_messageFilter.Channels.size()))
                {
                    auto const runs = ChannelRuns(m_messageFilter);

                    addRule(runs.empty()
                        ? res::GetString(L"ChipNoChannels")
                        : res::FormatString(L"ChipChannelsFormat", runs),
                        [](MainWindow& window) { window.m_messageFilter.Channels.fill(true); });
                }

                if (!::midiapp::AllTrue(m_messageFilter.ChannelVoiceStatuses.data(), m_messageFilter.ChannelVoiceStatuses.size()))
                {
                    addRule(res::GetString(L"ChipVoiceHidden"), [](MainWindow& window) { window.m_messageFilter.ChannelVoiceStatuses.fill(true); });
                }

                if (!FilterSystemAllShown(m_messageFilter))
                {
                    addRule(res::GetString(L"ChipSystemHidden"), [](MainWindow& window)
                        {
                            for (auto const& choice : SystemChoices)
                            {
                                for (uint8_t i = 0; i < choice.Count; i++)
                                {
                                    SystemAllowed(window.m_messageFilter, choice.Statuses[i], true);
                                }
                            }
                        });
                }

                if (m_messageFilter.LimitNoteRange)
                {
                    addRule(res::FormatString(L"ChipNotesFormat",
                        NoteName(m_messageFilter.LowestAllowedNote), NoteName(m_messageFilter.HighestAllowedNote)),
                        [](MainWindow& window) { window.m_messageFilter.LimitNoteRange = false; });
                }
            }

            if (settings.FoldRepeats())
            {
                addRule(res::GetString(L"ChipRepeatsFolded"), [](MainWindow& window)
                    {
                        native::AppSettings::Current().FoldRepeats(false);
                        window.m_pipeline.PostFoldRepeats(false);
                    });
            }

            FilterCountText().Text(std::format(L"{}", ruleCount));
            FilterCountBadge().Visibility(ruleCount > 0 ? xaml::Visibility::Visible : xaml::Visibility::Collapsed);
        }
        MIDI_MONITOR_CATCH_AND_LOG(L"Unable to show the filter chips.")
    }
}
