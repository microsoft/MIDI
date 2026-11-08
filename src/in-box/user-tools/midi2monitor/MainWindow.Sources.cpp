// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================
//
// Advanced mode's Sources list: one row for each endpoint and one for each of its groups, each
// with a check box, an activity light, and the color its messages get.
//
// Built in code rather than from a template, the way the rest of this window's dynamic parts
// are, because each row keeps references to its own check box and light so the lights can be
// updated four times a second without walking the visual tree.
//

#include "pch.h"
#include "MainWindow.xaml.h"

#include "MonitorPalette.h"
#include "SourceTable.h"
#include "StringResources.h"

#include <set>

namespace native = ::midi2monitor;
namespace res = ::midi2monitor::resources;
namespace automation = ::winrt::Microsoft::UI::Xaml::Automation;

namespace winrt::midi2monitor::implementation
{
    namespace
    {
        constexpr double EndpointRowHeight = 32.0;
        constexpr double GroupRowHeight = 28.0;
        constexpr double LightSize = 7.0;
        constexpr double SwatchSize = 10.0;
        constexpr double RowFontSize = 13.0;
        constexpr double DetailFontSize = 11.0;

        // The chevron's column. A group's check box lines up with its endpoint's picture, under
        // the endpoint's check box text, so the groups read as belonging to it.
        constexpr double ExpanderWidth = 24.0;
        constexpr double CheckBoxBoxWidth = 28.0;
        constexpr double GroupIndent = ExpanderWidth + CheckBoxBoxWidth;

        constexpr double ThumbnailWidth = 26.0;
        constexpr double ThumbnailHeight = 18.0;

        constexpr wchar_t GlyphCollapsed[] = L"\uE76C";
        constexpr wchar_t GlyphExpanded[] = L"\uE70D";

        media::Brush Lookup(_In_ wchar_t const* key)
        {
            return xaml::Application::Current().Resources().Lookup(winrt::box_value(key)).as<media::Brush>();
        }

        // The endpoint's picture from MIDI Settings, or an icon for the kind of endpoint, in a
        // small frame, as the comps draw it.
        xaml::FrameworkElement MakeThumbnail(_In_ winrt::hstring const& imagePath, _In_ winrt::hstring const& transportCode)
        {
            controls::Border frame{};
            frame.Width(ThumbnailWidth);
            frame.Height(ThumbnailHeight);
            frame.CornerRadius(xaml::CornerRadiusHelper::FromUniformRadius(3));
            frame.BorderThickness(xaml::ThicknessHelper::FromUniformLength(1));
            frame.BorderBrush(Lookup(L"CardStrokeColorDefaultBrush"));
            frame.Background(Lookup(L"ControlFillColorSecondaryBrush"));
            frame.VerticalAlignment(xaml::VerticalAlignment::Center);
            frame.Margin(xaml::ThicknessHelper::FromLengths(0, 0, 8, 0));

            if (!imagePath.empty())
            {
                try
                {
                    media::Imaging::BitmapImage bitmap{};
                    bitmap.UriSource(foundation::Uri{ L"file:///" + imagePath });

                    controls::Image image{};
                    image.Source(bitmap);
                    image.Stretch(media::Stretch::Uniform);
                    frame.Child(image);

                    return frame;
                }
                catch (...)
                {
                    // a bad picture falls back to the icon
                }
            }

            std::wstring const code{ transportCode };

            controls::FontIcon icon{};
            icon.FontSize(11);
            icon.Foreground(Lookup(L"TextFillColorSecondaryBrush"));
            icon.Glyph(
                (code == L"LOOP" || code == L"APP") ? L"\uE895" :
                (code.starts_with(L"NET") || code == L"RTP") ? L"\uE839" :
                (code == L"BLE" || code == L"BLE1") ? L"\uE702" :
                L"\uE8D6");

            frame.Child(icon);

            return frame;
        }

        bool ContainsText(_In_ std::wstring_view haystack, _In_ std::wstring_view needle) noexcept
        {
            if (needle.empty())
            {
                return true;
            }

            return ::FindNLSStringEx(LOCALE_NAME_USER_DEFAULT, FIND_FROMSTART | LINGUISTIC_IGNORECASE,
                haystack.data(), static_cast<int>(haystack.size()),
                needle.data(), static_cast<int>(needle.size()),
                nullptr, nullptr, nullptr, 0) >= 0;
        }

        controls::TextBlock MakeLabel(_In_ winrt::hstring const& text, _In_ winrt::hstring const& detail)
        {
            controls::TextBlock label{};
            label.FontSize(RowFontSize);
            label.TextTrimming(xaml::TextTrimming::CharacterEllipsis);
            label.TextWrapping(xaml::TextWrapping::NoWrap);
            label.VerticalAlignment(xaml::VerticalAlignment::Center);

            xaml::Documents::Run main{};
            main.Text(text);
            label.Inlines().Append(main);

            if (!detail.empty())
            {
                xaml::Documents::Run extra{};
                extra.Text(L"  " + detail);
                extra.FontSize(DetailFontSize);
                extra.Foreground(Lookup(L"TextFillColorTertiaryBrush"));
                label.Inlines().Append(extra);
            }

            return label;
        }

        // WinUI's check box is sized for a form. The list's are smaller, the way the comps draw them.
        void MakeCompact(_Inout_ controls::CheckBox const& check)
        {
            check.MinWidth(0);
            check.MinHeight(0);
            check.Padding(xaml::ThicknessHelper::FromLengths(8, 0, 0, 0));
            check.HorizontalAlignment(xaml::HorizontalAlignment::Stretch);
            check.HorizontalContentAlignment(xaml::HorizontalAlignment::Stretch);
            check.VerticalAlignment(xaml::VerticalAlignment::Center);
        }

        xaml::Shapes::Ellipse MakeLight()
        {
            xaml::Shapes::Ellipse light{};
            light.Width(LightSize);
            light.Height(LightSize);
            light.VerticalAlignment(xaml::VerticalAlignment::Center);
            light.Margin(xaml::ThicknessHelper::FromLengths(6, 0, 6, 0));
            light.Fill(xaml::Application::Current().Resources()
                .Lookup(winrt::box_value(L"ControlStrongFillColorDisabledBrush")).as<media::Brush>());
            return light;
        }
    }

    void MainWindow::RefreshSourcesList() noexcept
    {
        try
        {
            if (!m_advancedMode)
            {
                return;
            }

            // what was open stays open across a rebuild, which the watcher does on any change
            std::set<std::wstring> expanded{};

            for (auto const& row : m_sourceRows)
            {
                if (row.Expanded)
                {
                    expanded.insert(row.EndpointDeviceId);
                }
            }

            m_sourceRows.clear();
            SourcesListPanel().Children().Clear();

            for (auto const& device : m_endpointDevices)
            {
                SourceRow row{};

                row.EndpointDeviceId = std::wstring{ device.EndpointDeviceId() };
                row.Name = device.Name();
                row.Declared = midiapp::DeclaredGroups(device);
                row.Slot = native::SourceTable::SlotFor(row.EndpointDeviceId, row.Name);

                try
                {
                    if (auto const userInfo = device.GetUserSuppliedInfo())
                    {
                        row.ImagePath = midiapp::ResolveEndpointImagePath(userInfo.ImageFileName());
                    }

                    row.TransportCode = device.GetTransportSuppliedInfo().TransportCode();
                }
                catch (...)
                {
                    // no picture: the row shows an icon for the kind of device instead
                }

                // checked endpoints start open, so their groups show without a click
                row.Expanded = expanded.contains(row.EndpointDeviceId) || GroupMaskFor(row.EndpointDeviceId) != 0;

                for (uint8_t groupIndex = 0; groupIndex < 16; groupIndex++)
                {
                    if (!row.Declared[groupIndex] || row.Slot == native::NoSourceSlot)
                    {
                        continue;
                    }

                    native::SourceTable::GroupName(row.Slot, static_cast<uint8_t>(groupIndex + 1),
                        winrt::hstring{ midiapp::DescribeGroup(device, groupIndex) });
                }

                m_sourceRows.push_back(std::move(row));
            }

            for (auto& row : m_sourceRows)
            {
                BuildSourceRow(row);
            }

            // say which endpoint another app asked for, now that its name is known
            if (!m_usingSavedSources && !m_sourceChoices.empty())
            {
                auto const& wanted = m_sourceChoices.begin()->second.EndpointDeviceId;
                auto name = winrt::hstring{ wanted };

                for (auto const& row : m_sourceRows)
                {
                    if (::CompareStringOrdinal(row.EndpointDeviceId.c_str(), -1, wanted.c_str(), -1, TRUE) == CSTR_EQUAL)
                    {
                        name = row.Name;
                        break;
                    }
                }

                SessionSourcesInfoBar().Message(res::FormatString(L"SessionSourcesInfoFormat", std::wstring{ name }));
            }

            PushSlotMasks();
            UpdateDesiredConnections();
            UpdateSourceRowStates();
            ApplySourcesSearchAndActivity();
            UpdateSourcesSummary();
            RebuildChips();
            UpdateCommandStates();

            // an endpoint on the command line means "watch this now"
            if (m_startupMonitorPending && !m_monitoring && CheckedGroupCount() > 0)
            {
                m_startupMonitorPending = false;
                StartMonitoring();
            }
        }
        MIDI_MONITOR_CATCH_AND_LOG(L"Unable to build the sources list.")
    }

    _Use_decl_annotations_
    void MainWindow::BuildSourceRow(SourceRow& row) noexcept
    {
        try
        {
            auto const id = row.EndpointDeviceId;
            auto const weak = get_weak();

            // ---------------- the endpoint ----------------

            controls::Grid grid{};
            grid.Height(EndpointRowHeight);
            grid.Padding(xaml::ThicknessHelper::FromLengths(4, 0, 6, 0));
            grid.ColumnDefinitions().Append(controls::ColumnDefinition{});
            grid.ColumnDefinitions().Append(controls::ColumnDefinition{});
            grid.ColumnDefinitions().Append(controls::ColumnDefinition{});
            grid.ColumnDefinitions().Append(controls::ColumnDefinition{});
            grid.ColumnDefinitions().GetAt(0).Width(xaml::GridLengthHelper::FromPixels(ExpanderWidth));
            grid.ColumnDefinitions().GetAt(1).Width(xaml::GridLengthHelper::FromValueAndType(1, xaml::GridUnitType::Star));
            grid.ColumnDefinitions().GetAt(2).Width(xaml::GridLengthHelper::Auto());

            // the same width as a group's color square, so every light lines up
            grid.ColumnDefinitions().GetAt(3).Width(xaml::GridLengthHelper::FromPixels(SwatchSize + 4));

            controls::Button expander{};
            expander.Width(ExpanderWidth);
            expander.Height(ExpanderWidth);
            expander.Padding(xaml::ThicknessHelper::FromUniformLength(0));
            expander.MinWidth(0);
            expander.MinHeight(0);
            expander.VerticalAlignment(xaml::VerticalAlignment::Center);
            expander.BorderThickness(xaml::ThicknessHelper::FromUniformLength(0));
            expander.Background(media::SolidColorBrush{ winrt::Windows::UI::Colors::Transparent() });

            controls::FontIcon chevron{};
            chevron.FontSize(9);
            chevron.Glyph(row.Expanded ? GlyphExpanded : GlyphCollapsed);
            expander.Content(chevron);

            automation::AutomationProperties::SetName(expander, res::FormatString(
                row.Expanded ? L"SourcesHideGroupsFormat" : L"SourcesShowGroupsFormat", std::wstring{ row.Name }));

            expander.Click([weak, id](auto&&, auto&&)
                {
                    if (auto strong = weak.get())
                    {
                        strong->ToggleSourceExpanded(id);
                    }
                });

            // an endpoint that doesn't say what its groups are has nothing to open
            auto const hasGroups = std::any_of(row.Declared.begin(), row.Declared.end(), [](bool value) { return value; });
            expander.Visibility(hasGroups ? xaml::Visibility::Visible : xaml::Visibility::Collapsed);

            // the picture and the name are the check box's own content, so a click on either
            // checks it
            controls::Grid content{};
            content.ColumnDefinitions().Append(controls::ColumnDefinition{});
            content.ColumnDefinitions().Append(controls::ColumnDefinition{});
            content.ColumnDefinitions().GetAt(0).Width(xaml::GridLengthHelper::Auto());
            content.ColumnDefinitions().GetAt(1).Width(xaml::GridLengthHelper::FromValueAndType(1, xaml::GridUnitType::Star));

            auto const thumbnail = MakeThumbnail(row.ImagePath, row.TransportCode);
            auto const name = MakeLabel(row.Name, {});
            controls::Grid::SetColumn(name, 1);

            content.Children().Append(thumbnail);
            content.Children().Append(name);

            controls::CheckBox check{};
            MakeCompact(check);
            check.Content(content);
            automation::AutomationProperties::SetName(check, row.Name);
            controls::Grid::SetColumn(check, 1);

            check.Click([weak, id](foundation::IInspectable const& sender, auto&&)
                {
                    auto strong = weak.get();
                    auto box = sender.try_as<controls::CheckBox>();

                    if (strong == nullptr || box == nullptr)
                    {
                        return;
                    }

                    auto const value = box.IsChecked();
                    strong->OnSourceCheckChanged(id, 0, value != nullptr && value.Value());
                });

            auto const light = MakeLight();
            controls::Grid::SetColumn(light, 2);

            grid.Children().Append(expander);
            grid.Children().Append(check);
            grid.Children().Append(light);

            row.Row = grid;
            row.Check = check;
            row.Expander = expander;
            row.Light = light;

            SourcesListPanel().Children().Append(grid);

            // ---------------- its groups ----------------

            row.Groups.clear();

            for (uint8_t groupIndex = 0; groupIndex < 16; groupIndex++)
            {
                if (!row.Declared[groupIndex])
                {
                    continue;
                }

                auto const groupNumber = static_cast<uint8_t>(groupIndex + 1);
                auto const groupName = native::SourceTable::GroupName(row.Slot, groupNumber);

                controls::Grid groupGrid{};
                groupGrid.Height(GroupRowHeight);
                groupGrid.Padding(xaml::ThicknessHelper::FromLengths(4 + GroupIndent, 0, 6, 0));
                groupGrid.ColumnDefinitions().Append(controls::ColumnDefinition{});
                groupGrid.ColumnDefinitions().Append(controls::ColumnDefinition{});
                groupGrid.ColumnDefinitions().Append(controls::ColumnDefinition{});
                groupGrid.ColumnDefinitions().GetAt(0).Width(xaml::GridLengthHelper::FromValueAndType(1, xaml::GridUnitType::Star));
                groupGrid.ColumnDefinitions().GetAt(1).Width(xaml::GridLengthHelper::Auto());
                groupGrid.ColumnDefinitions().GetAt(2).Width(xaml::GridLengthHelper::FromPixels(SwatchSize + 4));

                auto const groupLabel = res::FormatString(L"GroupChoiceFormat", groupNumber);

                controls::CheckBox groupCheck{};
                MakeCompact(groupCheck);
                groupCheck.Content(MakeLabel(groupLabel, groupName));
                automation::AutomationProperties::SetName(groupCheck, groupName.empty()
                    ? res::FormatString(L"SourcesGroupAccessibleFormat", std::wstring{ row.Name }, std::wstring{ groupLabel })
                    : res::FormatString(L"SourcesNamedGroupAccessibleFormat", std::wstring{ row.Name }, std::wstring{ groupLabel }, std::wstring{ groupName }));

                groupCheck.Click([weak, id, groupNumber](foundation::IInspectable const& sender, auto&&)
                    {
                        auto strong = weak.get();
                        auto box = sender.try_as<controls::CheckBox>();

                        if (strong == nullptr || box == nullptr)
                        {
                            return;
                        }

                        auto const value = box.IsChecked();
                        strong->OnSourceCheckChanged(id, groupNumber, value != nullptr && value.Value());
                    });

                auto const groupLight = MakeLight();
                controls::Grid::SetColumn(groupLight, 1);

                xaml::Shapes::Rectangle swatch{};
                swatch.Width(SwatchSize);
                swatch.Height(SwatchSize);
                swatch.RadiusX(3);
                swatch.RadiusY(3);
                swatch.VerticalAlignment(xaml::VerticalAlignment::Center);
                swatch.HorizontalAlignment(xaml::HorizontalAlignment::Right);
                controls::Grid::SetColumn(swatch, 2);

                groupGrid.Children().Append(groupCheck);
                groupGrid.Children().Append(groupLight);
                groupGrid.Children().Append(swatch);

                SourceRowGroup group{};
                group.GroupNumber = groupNumber;
                group.Row = groupGrid;
                group.Check = groupCheck;
                group.Light = groupLight;
                group.Swatch = swatch;

                row.Groups.push_back(group);

                SourcesListPanel().Children().Append(groupGrid);
            }
        }
        MIDI_MONITOR_CATCH_AND_LOG(L"Unable to build a row in the sources list.")
    }

    void MainWindow::UpdateSourceRowStates() noexcept
    {
        try
        {
            m_suppressSourceEvents = true;

            auto const showLights = m_advancedListening && native::AppSettings::Current().ShowEndpointActivity();
            auto const lightVisibility = showLights ? xaml::Visibility::Visible : xaml::Visibility::Collapsed;

            for (auto& row : m_sourceRows)
            {
                if (row.Check == nullptr)
                {
                    continue;
                }

                auto const all = AllGroupsMask(row);
                auto const mask = GroupMaskFor(row.EndpointDeviceId) & all;

                if (mask == 0)
                {
                    row.Check.IsChecked(false);
                }
                else if (mask == all)
                {
                    row.Check.IsChecked(true);
                }
                else
                {
                    // some of its groups
                    row.Check.IsChecked(nullptr);
                }

                row.Light.Visibility(lightVisibility);

                if (auto const chevron = row.Expander.Content().try_as<controls::FontIcon>())
                {
                    chevron.Glyph(row.Expanded ? GlyphExpanded : GlyphCollapsed);
                }

                for (auto& group : row.Groups)
                {
                    auto const isChecked = (mask & (1u << (group.GroupNumber - 1))) != 0;

                    group.Check.IsChecked(isChecked);
                    group.Light.Visibility(lightVisibility);

                    if (isChecked && row.Slot != native::NoSourceSlot)
                    {
                        group.Swatch.Fill(native::MonitorPalette::SourceBrush(native::SourceTable::ColorIndex(row.Slot, group.GroupNumber)));
                        group.Swatch.Visibility(xaml::Visibility::Visible);
                    }
                    else
                    {
                        group.Swatch.Visibility(xaml::Visibility::Collapsed);
                    }
                }
            }

            auto const canFilterBySending = native::AppSettings::Current().ShowEndpointActivity();
            OnlySendingToggle().Visibility(canFilterBySending ? xaml::Visibility::Visible : xaml::Visibility::Collapsed);

            m_suppressSourceEvents = false;
        }
        catch (...)
        {
            m_suppressSourceEvents = false;
            MIDI_MONITOR_LOG_GENERAL_EXCEPTION(L"Unable to update the sources list.");
        }
    }

    void MainWindow::ApplySourcesSearchAndActivity() noexcept
    {
        try
        {
            auto const search = std::wstring{ SourcesSearchBox().Text() };

            // only while listening is there anything to say about who is sending
            auto const onlySending = OnlySendingToggle().IsOn() &&
                m_advancedListening &&
                native::AppSettings::Current().ShowEndpointActivity();

            for (auto& row : m_sourceRows)
            {
                if (row.Row == nullptr)
                {
                    continue;
                }

                auto const mask = GroupMaskFor(row.EndpointDeviceId);
                auto const nameMatches = ContainsText(row.Name, search);

                std::vector<bool> groupMatches(row.Groups.size(), false);
                bool anyGroupMatches{ false };

                for (size_t i = 0; i < row.Groups.size(); i++)
                {
                    auto const label = res::FormatString(L"GroupChoiceFormat", row.Groups[i].GroupNumber);
                    auto const name = native::SourceTable::GroupName(row.Slot, row.Groups[i].GroupNumber);

                    groupMatches[i] = ContainsText(label, search) || ContainsText(name, search);
                    anyGroupMatches = anyGroupMatches || groupMatches[i];
                }

                // a checked endpoint always stays, so it can be unchecked
                auto const showRow = (nameMatches || anyGroupMatches) && (!onlySending || row.Sending || mask != 0);

                row.Row.Visibility(showRow ? xaml::Visibility::Visible : xaml::Visibility::Collapsed);

                for (size_t i = 0; i < row.Groups.size(); i++)
                {
                    // a search that matches a group shows it without opening the endpoint
                    auto const showGroup = showRow &&
                        (row.Expanded || (!search.empty() && groupMatches[i] && !nameMatches));

                    row.Groups[i].Row.Visibility(showGroup ? xaml::Visibility::Visible : xaml::Visibility::Collapsed);
                }
            }
        }
        MIDI_MONITOR_CATCH_AND_LOG(L"Unable to filter the sources list.")
    }

    _Use_decl_annotations_
    void MainWindow::OnSourcesSearchChanged(foundation::IInspectable const&, controls::TextChangedEventArgs const&)
    {
        ApplySourcesSearchAndActivity();
    }

    _Use_decl_annotations_
    void MainWindow::OnSourceCheckChanged(std::wstring const& endpointDeviceId, uint8_t groupNumber, bool isChecked) noexcept
    {
        if (m_suppressSourceEvents)
        {
            return;
        }

        try
        {
            auto const found = std::find_if(m_sourceRows.begin(), m_sourceRows.end(),
                [&endpointDeviceId](SourceRow const& row) { return row.EndpointDeviceId == endpointDeviceId; });

            uint32_t const all = found == m_sourceRows.end() ? 0xFFFFu : AllGroupsMask(*found);
            auto mask = GroupMaskFor(endpointDeviceId);

            if (groupNumber == 0)
            {
                mask = isChecked ? all : 0;
            }
            else if (isChecked)
            {
                mask |= (1u << (groupNumber - 1));
            }
            else
            {
                mask &= ~(1u << (groupNumber - 1));
            }

            SetGroupMask(endpointDeviceId, mask);
            PersistSources();

            PushSlotMasks();
            UpdateDesiredConnections();
            UpdateSourceRowStates();
            ApplySourcesSearchAndActivity();
            UpdateSourcesSummary();
            RebuildChips();
            UpdateCommandStates();
        }
        MIDI_MONITOR_CATCH_AND_LOG(L"Unable to change which groups are listened to.")
    }

    _Use_decl_annotations_
    void MainWindow::ToggleSourceExpanded(std::wstring const& endpointDeviceId) noexcept
    {
        try
        {
            for (auto& row : m_sourceRows)
            {
                if (row.EndpointDeviceId != endpointDeviceId)
                {
                    continue;
                }

                row.Expanded = !row.Expanded;

                automation::AutomationProperties::SetName(row.Expander, res::FormatString(
                    row.Expanded ? L"SourcesHideGroupsFormat" : L"SourcesShowGroupsFormat", std::wstring{ row.Name }));
                break;
            }

            UpdateSourceRowStates();
            ApplySourcesSearchAndActivity();
        }
        MIDI_MONITOR_CATCH_AND_LOG(L"Unable to show or hide an endpoint's groups.")
    }

    void MainWindow::UpdateSourcesSummary() noexcept
    {
        try
        {
            size_t endpoints{ 0 };

            for (auto const& row : m_sourceRows)
            {
                if ((GroupMaskFor(row.EndpointDeviceId) & AllGroupsMask(row)) != 0)
                {
                    endpoints++;
                }
            }

            auto const groups = CheckedGroupCount();

            SourcesSummaryText().Text(groups == 0
                ? res::GetString(L"SourcesSummaryNone")
                : res::FormatString(L"SourcesSummaryFormat", groups, endpoints));
        }
        MIDI_MONITOR_CATCH_AND_LOG(L"Unable to update the sources summary.")
    }
}
