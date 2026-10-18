// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

// What Windows MIDI Services says about each device, shown where the device is: its picture and
// description from MIDI Settings, its groups from its function blocks (or group terminal blocks),
// and the offset and automation interval the sequencer sends it by (design section 25).

#include "pch.h"
#include "MainWindow.xaml.h"

#include "BackgroundWork.h"
#include "DeviceProperties.h"
#include "DialogParts.h"
#include "StringResources.h"

namespace res = ::midisequencer::resources;
namespace documents = ::winrt::Microsoft::UI::Xaml::Documents;

namespace winrt::midisequencer::implementation
{
    using namespace dialogparts;

    namespace
    {
        constexpr double DetailsWidth = 380.0;
        constexpr double DetailsPictureSize = 48.0;

        // Without its own setting, a ramp goes to a device no more often than this (section 7).
        constexpr uint32_t DefaultAutomationIntervalMilliseconds = ::midisequencer::DefaultLaneIntervalMilliseconds;

        // "4" or "4.5", for a time in microseconds.
        std::wstring MillisecondsText(uint32_t const microseconds)
        {
            return std::format(L"{:g}", std::round(static_cast<double>(microseconds) / 100.0) / 10.0);
        }

        std::wstring GroupsText(uint8_t const firstGroup, uint8_t const groupCount)
        {
            auto const first = static_cast<uint32_t>(firstGroup) + 1;

            if (groupCount <= 1)
            {
                return std::wstring{ res::FormatString(L"GroupChoiceFormat", first) };
            }

            return std::wstring{ res::FormatString(L"GroupsRangeFormat", first, first + groupCount - 1) };
        }

        std::wstring HexBytes(uint8_t const* bytes, size_t const count)
        {
            std::wstring text{};

            for (size_t i = 0; i < count; ++i)
            {
                if (i > 0)
                {
                    text += L' ';
                }

                text += std::format(L"{:02X}", bytes[i]);
            }

            return text;
        }

        wchar_t const* DirectionKey(::midisequencer::BlockDirection const direction) noexcept
        {
            switch (direction)
            {
            case ::midisequencer::BlockDirection::Destination: return L"DeviceBlockReceives";
            case ::midisequencer::BlockDirection::Source: return L"DeviceBlockSends";
            default: return L"DeviceBlockSendsAndReceives";
            }
        }

        wchar_t const* ProtocolKey(::midisequencer::BlockProtocol const protocol) noexcept
        {
            switch (protocol)
            {
            case ::midisequencer::BlockProtocol::Midi1: return L"ProtocolMidi1";
            case ::midisequencer::BlockProtocol::Midi1CableSpeed: return L"ProtocolMidi1Cable";
            case ::midisequencer::BlockProtocol::Midi2: return L"ProtocolMidi2";
            default: return nullptr;
            }
        }

        // A label and its value, as one more row of a two-column grid. Nothing when there's no value.
        void AddFact(controls::Grid const& grid, winrt::hstring const& label, std::wstring const& value, media::Brush const& labelBrush)
        {
            if (value.empty())
            {
                return;
            }

            controls::RowDefinition row{};
            row.Height(xaml::GridLength{ 1, xaml::GridUnitType::Auto });
            grid.RowDefinitions().Append(row);

            auto const index = static_cast<int32_t>(grid.RowDefinitions().Size()) - 1;

            controls::TextBlock name{};
            name.Text(label);
            name.FontSize(12);
            name.Foreground(labelBrush);
            controls::Grid::SetRow(name, index);
            grid.Children().Append(name);

            controls::TextBlock text{};
            text.Text(winrt::hstring{ value });
            text.FontSize(12);
            text.TextWrapping(xaml::TextWrapping::Wrap);
            text.IsTextSelectionEnabled(true);
            controls::Grid::SetRow(text, index);
            controls::Grid::SetColumn(text, 1);
            grid.Children().Append(text);
        }
    }

    _Use_decl_annotations_
    std::wstring MainWindow::EndpointKeyOf(seq::EndpointRef const& endpoint) const
    {
        // The endpoint that's here when there is one, so a device that moved to another port is
        // still the same device.
        auto key = m_directory->ResolveId(endpoint);

        if (key.empty())
        {
            key = endpoint.Id;
        }

        if (key.empty())
        {
            return L"name:" + endpoint.Name;
        }

        std::transform(key.begin(), key.end(), key.begin(), [](wchar_t const ch) { return static_cast<wchar_t>(::towlower(ch)); });
        return key;
    }

    _Use_decl_annotations_
    controls::Image MainWindow::DevicePictureFor(seq::EndpointRef const& endpoint, double size) const
    {
        if (endpoint.IsEmpty())
        {
            return nullptr;
        }

        auto const summary = m_directory->Resolve(endpoint);

        if (!summary.has_value() || summary->PicturePath.empty())
        {
            return nullptr;
        }

        auto const source = seq::DevicePicture(summary->PicturePath, static_cast<int32_t>(std::ceil(size)));

        if (source == nullptr)
        {
            return nullptr;
        }

        controls::Image image{};
        image.Source(source);
        image.Width(size);
        image.Height(size);
        image.Stretch(media::Stretch::Uniform);
        image.VerticalAlignment(xaml::VerticalAlignment::Center);

        // The device's name is always next to it, so the picture says nothing more.
        automation::AutomationProperties::SetAccessibilityView(image, automation::Peers::AccessibilityView::Raw);
        return image;
    }

    _Use_decl_annotations_
    void MainWindow::SetDeviceToolTip(xaml::DependencyObject const& element, seq::EndpointRef const& endpoint) const
    {
        // What the device is, in the customer's own words from MIDI Settings. Nothing when they
        // didn't say, so no empty tooltip shows.
        std::wstring description{};

        if (!endpoint.IsEmpty())
        {
            if (auto const summary = m_directory->Resolve(endpoint); summary.has_value())
            {
                description = summary->Live.Description;
            }
        }

        controls::ToolTipService::SetToolTip(element, description.empty() ? nullptr : winrt::box_value(winrt::hstring{ description }));
    }

    _Use_decl_annotations_
    winrt::hstring MainWindow::DestinationToolTip(seq::Track const& track) const
    {
        // Where it plays, in full where a narrow strip cuts it short, and what that device is.
        std::wstring text{ DescribeDestination(track) };

        if (auto const summary = m_directory->Resolve(track.Destination.Endpoint); summary.has_value() && !summary->Live.Description.empty())
        {
            text += L"\n" + summary->Live.Description;
        }

        if (DestinationTurnedOff(track.Destination))
        {
            text += L"\n" + std::wstring{ res::GetString(L"DestinationTurnedOffNote") };
        }

        return winrt::hstring{ text };
    }

    _Use_decl_annotations_
    winrt::hstring MainWindow::LaneIntervalToolTip(seq::Track const& track) const
    {
        if (track.Destination.Endpoint.IsEmpty())
        {
            return {};
        }

        auto const resolved = m_directory->Resolve(track.Destination.Endpoint);
        auto name = resolved.has_value() ? resolved->Live.Name : track.Destination.Endpoint.Name;

        if (name.empty())
        {
            name = std::wstring{ res::GetString(L"UnknownEndpoint") };
        }

        auto const interval = m_directory->Lookup(track.Destination.Endpoint, track.Destination.Group).AutomationIntervalMilliseconds;

        return interval > 0
            ? res::FormatString(L"LaneIntervalToolTipFormat", name, interval)
            : res::FormatString(L"LaneIntervalDefaultToolTipFormat", name, DefaultAutomationIntervalMilliseconds);
    }

    _Use_decl_annotations_
    std::vector<std::wstring> MainWindow::DeviceUses(seq::EndpointRef const& endpoint) const
    {
        std::vector<std::wstring> lines{};

        if (endpoint.IsEmpty())
        {
            return lines;
        }

        auto const key = EndpointKeyOf(endpoint);
        auto const same = [this, &key](seq::EndpointRef const& other)
        {
            return !other.IsEmpty() && EndpointKeyOf(other) == key;
        };

        std::vector<std::wstring> plays{};
        std::vector<std::wstring> records{};

        seq::ForEachTrack(m_doc, [&](seq::Track const& track, size_t)
        {
            if (!track.IsFolder)
            {
                if (same(track.Destination.Endpoint))
                {
                    plays.push_back(track.Name);
                }

                if (same(track.Source.Endpoint))
                {
                    records.push_back(track.Name);
                }
            }

            return true;
        });

        if (!plays.empty())
        {
            lines.emplace_back(res::FormatString(L"DeviceUsesPlaysFormat", JoinNames(plays)));
        }

        if (!records.empty())
        {
            lines.emplace_back(res::FormatString(L"DeviceUsesRecordsFormat", JoinNames(records)));
        }

        auto const& settings = seq::AppSettings::Current();

        seq::EndpointRef metronome{ settings.MetronomeEndpointName(), settings.MetronomeEndpointId() };

        if (metronome.IsEmpty())
        {
            metronome.Id = m_directory->SynthEndpointId();
        }

        if (settings.MetronomeEnabled() && same(metronome))
        {
            lines.emplace_back(res::GetString(L"DeviceUsesMetronome"));
        }

        for (auto const& output : settings.ClockOutputs())
        {
            if (!same(seq::EndpointRef{ output.EndpointName, output.EndpointId }))
            {
                continue;
            }

            std::vector<std::wstring> parts{};

            if (output.Clock) { parts.emplace_back(res::GetString(L"ClockColumnClock")); }
            if (output.StartStop) { parts.emplace_back(res::GetString(L"ClockColumnStartStop")); }
            if (output.SongPosition) { parts.emplace_back(res::GetString(L"ClockColumnSongPosition")); }
            if (output.Tempo) { parts.emplace_back(res::GetString(L"StudioColumnTempo")); }

            if (!parts.empty())
            {
                lines.emplace_back(res::FormatString(L"DeviceUsesSyncFormat", static_cast<uint32_t>(output.Group) + 1, JoinNames(parts)));
            }
        }

        if (auto const& timeCode = settings.TimeCode(); timeCode.IsOn() && same(seq::EndpointRef{ timeCode.EndpointName, timeCode.EndpointId }))
        {
            lines.emplace_back(res::FormatString(L"DeviceUsesTimeCodeFormat", static_cast<uint32_t>(timeCode.Group) + 1));
        }

        if (auto const& follow = settings.FollowClock(); follow.IsOn() && same(seq::EndpointRef{ follow.EndpointName, follow.EndpointId }))
        {
            lines.emplace_back(res::FormatString(L"DeviceUsesFollowFormat", static_cast<uint32_t>(follow.Group) + 1));
        }

        if (lines.empty())
        {
            lines.emplace_back(res::GetString(L"DeviceUsesNothing"));
        }

        // What Studio setup turned off.
        auto const groupList = [](uint16_t const groups)
        {
            std::vector<std::wstring> names{};

            for (uint32_t group = 0; group < 16; ++group)
            {
                if ((groups & (1u << group)) != 0)
                {
                    names.emplace_back(res::FormatString(L"GroupChoiceFormat", group + 1));
                }
            }

            return JoinNames(names);
        };

        if (auto const off = GroupsTurnedOff(endpoint, true); off != 0)
        {
            lines.emplace_back(res::FormatString(L"DeviceUsesPlayToOffFormat", groupList(off)));
        }

        if (auto const off = GroupsTurnedOff(endpoint, false); off != 0)
        {
            lines.emplace_back(res::FormatString(L"DeviceUsesRecordFromOffFormat", groupList(off)));
        }

        return lines;
    }

    _Use_decl_annotations_
    void MainWindow::ShowDeviceDetails(seq::EndpointRef const& endpoint, xaml::FrameworkElement const& anchor) noexcept
    {
        try
        {
            if (endpoint.IsEmpty() || anchor == nullptr)
            {
                return;
            }

            auto const secondary = BrushFor(m_palette.Text2);
            auto const tertiary = BrushFor(m_palette.Text3);
            auto const summary = m_directory->Resolve(endpoint);

            std::wstring name{ summary.has_value() ? summary->Live.Name : endpoint.Name };

            if (name.empty())
            {
                name = std::wstring{ res::GetString(L"UnknownEndpoint") };
            }

            controls::StackPanel panel{};
            panel.Spacing(4);
            panel.Width(DetailsWidth);
            panel.Padding(xaml::Thickness{ 0, 0, 12, 0 });

            // ---- the device: what you see first, from what's already known ----

            controls::Grid header{};
            header.ColumnSpacing(12);

            {
                controls::ColumnDefinition picture{};
                picture.Width(xaml::GridLength{ 1, xaml::GridUnitType::Auto });
                header.ColumnDefinitions().Append(picture);

                controls::ColumnDefinition words{};
                words.Width(xaml::GridLength{ 1, xaml::GridUnitType::Star });
                header.ColumnDefinitions().Append(words);
            }

            if (auto const picture = DevicePictureFor(endpoint, DetailsPictureSize); picture != nullptr)
            {
                picture.VerticalAlignment(xaml::VerticalAlignment::Top);
                PlaceIn(header, picture, 0);
            }

            controls::StackPanel says{};
            says.Spacing(2);

            controls::TextBlock title{};
            title.Text(winrt::hstring{ name });
            title.FontSize(15);
            title.FontWeight(winrt::Microsoft::UI::Text::FontWeights::SemiBold());
            title.TextWrapping(xaml::TextWrapping::Wrap);
            automation::AutomationProperties::SetHeadingLevel(title, automation::Peers::AutomationHeadingLevel::Level1);
            says.Children().Append(title);

            if (summary.has_value() && !summary->Live.Description.empty())
            {
                says.Children().Append(Note(winrt::hstring{ summary->Live.Description }, secondary));
            }

            // How it's connected and what it speaks, once that's been read.
            auto const kind = Note(L"", tertiary);
            says.Children().Append(kind);

            if (!summary.has_value())
            {
                kind.Text(res::GetString(L"DeviceNotConnected"));
                kind.Foreground(ThemeBrush(L"SeqWarnTextBrush"));
            }

            PlaceIn(header, says, 1);
            panel.Children().Append(header);

            // ---- what uses it ----

            panel.Children().Append(Heading(res::GetString(L"DeviceUsesHeading")));

            for (auto const& line : DeviceUses(endpoint))
            {
                panel.Children().Append(Note(winrt::hstring{ line }, secondary));
            }

            controls::StackPanel more{};
            more.Spacing(4);
            panel.Children().Append(more);

            controls::ScrollViewer scroller{};
            scroller.Content(panel);
            scroller.MaxHeight(560);
            scroller.VerticalScrollBarVisibility(controls::ScrollBarVisibility::Auto);
            automation::AutomationProperties::SetName(scroller, res::FormatString(L"DeviceDetailsNameFormat", name));

            controls::Flyout flyout{};
            flyout.Content(scroller);
            flyout.ShowAt(anchor);

            if (!summary.has_value())
            {
                return;
            }

            more.Children().Append(Note(res::GetString(L"DeviceDetailsReading"), tertiary));

            // Everything else asks the service, so it's read off the UI thread and added when it's
            // back. The two controls are held, not looked up again: neither holds this.
            [](winrt::com_ptr<MainWindow> strong, std::wstring id, controls::StackPanel host, controls::TextBlock kind) -> winrt::fire_and_forget
            {
                try
                {
                    seq::DeviceProperties properties{};

                    co_await ::midisequencer::RunOnBackgroundAsync([&properties, &id]()
                    {
                        properties = seq::ReadDeviceProperties(id);
                    });

                    if (strong->m_closing)
                    {
                        co_return;
                    }

                    host.Children().Clear();
                    strong->AddDeviceProperties(host, kind, properties);
                }
                MIDI_SEQUENCER_CATCH_AND_LOG(L"Unable to read a device's details.")
            }(get_strong(), summary->Live.EndpointDeviceId, more, kind);
        }
        MIDI_SEQUENCER_CATCH_AND_LOG(L"Unable to show a device's details.")
    }

    _Use_decl_annotations_
    void MainWindow::AddDeviceProperties(controls::StackPanel const& host, controls::TextBlock const& kind, seq::DeviceProperties const& properties)
    {
        auto const secondary = BrushFor(m_palette.Text2);
        auto const tertiary = BrushFor(m_palette.Text3);

        if (!properties.Found)
        {
            host.Children().Append(Note(res::GetString(L"DeviceDetailsUnavailable"), secondary));
            return;
        }

        if (kind != nullptr)
        {
            std::wstring text{ properties.TransportName.empty() ? properties.TransportCode : properties.TransportName };
            std::wstring const protocol{ res::GetString(properties.SpeaksMidi2 ? L"ProtocolMidi2" : L"ProtocolMidi1") };

            kind.Text(winrt::hstring{ text.empty() ? protocol : text + L" \u00B7 " + protocol });
        }

        // ---- its groups ----

        host.Children().Append(Heading(res::GetString(L"DeviceGroupsHeading")));

        if (properties.Blocks.empty())
        {
            host.Children().Append(Note(res::GetString(L"DeviceGroupsNone"), secondary));
        }
        else
        {
            for (auto const& block : properties.Blocks)
            {
                controls::TextBlock line{};
                line.FontSize(12);
                line.TextWrapping(xaml::TextWrapping::Wrap);

                documents::Run blockName{};
                blockName.Text(block.Name.empty() ? res::GetString(L"DeviceBlockUnnamed") : winrt::hstring{ block.Name });
                blockName.FontWeight(winrt::Microsoft::UI::Text::FontWeights::SemiBold());
                line.Inlines().Append(blockName);

                std::vector<std::wstring> facts{ GroupsText(block.FirstGroup, block.GroupCount), std::wstring{ res::GetString(DirectionKey(block.Direction)) } };

                if (auto const protocol = ProtocolKey(block.Protocol); protocol != nullptr)
                {
                    facts.emplace_back(res::GetString(protocol));
                }

                if (!block.Active)
                {
                    facts.emplace_back(res::GetString(L"DeviceBlockInactive"));
                }

                std::wstring rest{};

                for (auto const& fact : facts)
                {
                    rest += L" \u00B7 " + fact;
                }

                documents::Run detail{};
                detail.Text(winrt::hstring{ rest });
                detail.Foreground(secondary);
                line.Inlines().Append(detail);

                host.Children().Append(line);
            }

            host.Children().Append(Note(res::GetString(properties.FunctionBlocks ? L"DeviceGroupsFromFunctionBlocks" : L"DeviceGroupsFromTerminalBlocks"), tertiary));
        }

        // ---- how the sequencer sends to it ----

        host.Children().Append(Heading(res::GetString(L"DeviceSendingHeading")));

        if (properties.Offset.Microseconds == 0)
        {
            host.Children().Append(Note(res::GetString(L"DeviceOffsetNone"), secondary));
        }
        else if (!m_directory->ApplyOffsets())
        {
            host.Children().Append(Note(res::FormatString(L"DeviceOffsetOffFormat", MillisecondsText(properties.Offset.Microseconds)), secondary));
        }
        else
        {
            auto const key = properties.Offset.ChosenInMidiSettings ? L"DeviceOffsetSettingsFormat" : L"DeviceOffsetTransportFormat";
            host.Children().Append(Note(res::FormatString(key, MillisecondsText(properties.Offset.Microseconds)), secondary));
        }

        if (properties.AutomationIntervalMilliseconds > 0)
        {
            host.Children().Append(Note(res::FormatString(L"DeviceIntervalFormat", properties.AutomationIntervalMilliseconds), secondary));
        }
        else
        {
            host.Children().Append(Note(res::FormatString(L"DeviceIntervalNoneFormat", DefaultAutomationIntervalMilliseconds), secondary));
        }

        if (properties.SupportsMpe)
        {
            host.Children().Append(Note(res::GetString(L"DeviceMpe"), secondary));
        }

        if (properties.RequiresNoteOffTranslation)
        {
            host.Children().Append(Note(res::GetString(L"DeviceNoteOff"), secondary));
        }

        // ---- what it is ----

        controls::Grid facts{};
        facts.ColumnSpacing(12);
        facts.RowSpacing(2);

        {
            controls::ColumnDefinition label{};
            label.Width(xaml::GridLength{ 1, xaml::GridUnitType::Auto });
            facts.ColumnDefinitions().Append(label);

            controls::ColumnDefinition value{};
            value.Width(xaml::GridLength{ 1, xaml::GridUnitType::Star });
            facts.ColumnDefinitions().Append(value);
        }

        AddFact(facts, res::GetString(L"DeviceFactManufacturer"), properties.Manufacturer, tertiary);

        if (properties.ParentName != properties.Name)
        {
            AddFact(facts, res::GetString(L"DeviceFactConnectedThrough"), properties.ParentName, tertiary);
        }

        AddFact(facts, res::GetString(L"DeviceFactSerialNumber"), properties.SerialNumber, tertiary);
        AddFact(facts, res::GetString(L"DeviceFactProductInstance"), properties.ProductInstanceId, tertiary);

        if (properties.VendorId != 0 || properties.ProductId != 0)
        {
            AddFact(facts, res::GetString(L"DeviceFactUsbIds"), std::format(L"{:04X}:{:04X}", properties.VendorId, properties.ProductId), tertiary);
        }

        if (properties.HasIdentity)
        {
            AddFact(facts, res::GetString(L"DeviceFactSystemExclusiveId"), HexBytes(properties.SystemExclusiveId.data(), properties.SystemExclusiveId.size()), tertiary);
            AddFact(facts, res::GetString(L"DeviceFactFamilyModel"), std::format(L"{:04X} {:04X}", properties.Family, properties.Model), tertiary);
            AddFact(facts, res::GetString(L"DeviceFactSoftwareRevision"), HexBytes(properties.SoftwareRevision.data(), properties.SoftwareRevision.size()), tertiary);
        }

        if (properties.UmpVersionMajor > 0)
        {
            AddFact(facts, res::GetString(L"DeviceFactUmpVersion"), std::format(L"{}.{}", properties.UmpVersionMajor, properties.UmpVersionMinor), tertiary);
        }

        if (facts.RowDefinitions().Size() > 0)
        {
            host.Children().Append(Heading(res::GetString(L"DeviceAboutHeading")));
            host.Children().Append(facts);
        }

        // ---- where to change it ----

        auto const change = Note(res::GetString(L"DeviceChangeInSettings"), tertiary);
        change.Margin(xaml::Thickness{ 0, 12, 0, 4 });
        host.Children().Append(change);

        auto const buttonStyle = RootGrid().Resources().Lookup(winrt::box_value(L"SmallButtonStyle")).as<xaml::Style>();

        controls::StackPanel buttons{};
        buttons.Orientation(controls::Orientation::Horizontal);
        buttons.Spacing(8);

        controls::Button open{};
        open.Style(buttonStyle);
        open.Content(winrt::box_value(res::GetString(L"OpenMidiSettings")));
        open.Click([weak = get_weak()](auto&&, auto&&)
        {
            if (auto strong = weak.get())
            {
                strong->OpenMidiSettings();
            }
        });

        buttons.Children().Append(open);

        // What a device sends, as it arrives. MIDI Monitor only listens: it sends the device nothing.
        if (auto const summary = m_directory->Resolve(seq::EndpointRef{ properties.Name, properties.Id }); summary.has_value() && summary->Live.SourceGroupCount() > 0)
        {
            controls::Button watch{};
            watch.Style(buttonStyle);
            watch.Content(winrt::box_value(res::GetString(L"WatchInMonitor")));
            watch.Click([weak = get_weak(), id = summary->Live.EndpointDeviceId](auto&&, auto&&)
            {
                if (auto strong = weak.get())
                {
                    strong->LaunchMonitor(id, -1);
                }
            });

            buttons.Children().Append(watch);
        }

        host.Children().Append(buttons);
    }

    void MainWindow::OpenMidiSettings() noexcept
    {
        try
        {
            auto const settingsApp = SiblingToolPath(L"midisettings");

            if (settingsApp.empty())
            {
                ShowMessage(res::GetString(L"MidiSettingsNotFound"));
                return;
            }

            auto const path = settingsApp.wstring();

            SHELLEXECUTEINFOW info{};
            info.cbSize = sizeof(info);
            info.fMask = SEE_MASK_NOASYNC;
            info.lpVerb = L"open";
            info.lpFile = path.c_str();
            info.nShow = SW_SHOWNORMAL;

            if (!::ShellExecuteExW(&info))
            {
                ShowMessage(res::GetString(L"MidiSettingsNotFound"));
            }
        }
        MIDI_SEQUENCER_CATCH_AND_LOG(L"Unable to open MIDI Settings.")
    }

    _Use_decl_annotations_
    void MainWindow::LaunchMonitor(std::wstring const& endpointId, int32_t group) noexcept
    {
        try
        {
            auto const monitor = SiblingToolPath(L"midi2monitor");

            if (monitor.empty())
            {
                ShowMessage(res::GetString(L"MonitorNotFound"));
                return;
            }

            std::wstring arguments{};

            if (!endpointId.empty())
            {
                arguments = L"\"" + endpointId + L"\"";

                if (group >= 0 && group < 16)
                {
                    arguments += L" --group " + std::to_wstring(group + 1);
                }
            }

            auto const path = monitor.wstring();

            SHELLEXECUTEINFOW info{};
            info.cbSize = sizeof(info);
            info.fMask = SEE_MASK_NOASYNC;
            info.lpVerb = L"open";
            info.lpFile = path.c_str();
            info.lpParameters = arguments.empty() ? nullptr : arguments.c_str();
            info.nShow = SW_SHOWNORMAL;

            if (!::ShellExecuteExW(&info))
            {
                ShowMessage(res::GetString(L"MonitorNotFound"));
            }
        }
        MIDI_SEQUENCER_CATCH_AND_LOG(L"Unable to open MIDI Monitor.")
    }

    void MainWindow::UpdateDeviceDescriptions() noexcept
    {
        try
        {
            auto const track = seq::FindTrack(m_doc, m_inspectorTrackId);

            auto const describe = [this](controls::TextBlock const& text, seq::EndpointRef const& endpoint, bool const off, wchar_t const* offKey)
            {
                std::wstring description{};

                if (!endpoint.IsEmpty())
                {
                    if (auto const summary = m_directory->Resolve(endpoint); summary.has_value())
                    {
                        description = summary->Live.Description;
                    }
                }

                // Turned off in Studio setup, which matters more than what the device is.
                if (off)
                {
                    std::wstring const note{ res::GetString(offKey) };
                    description = description.empty() ? note : note + L"\n" + description;
                }

                text.Text(winrt::hstring{ description });
                text.Visibility(description.empty() ? xaml::Visibility::Collapsed : xaml::Visibility::Visible);
            };

            describe(SourceDescription(), track != nullptr ? track->Source.Endpoint : seq::EndpointRef{},
                track != nullptr && SourceTurnedOff(track->Source), L"SourceTurnedOffNote");
            describe(DestinationDescription(), track != nullptr ? track->Destination.Endpoint : seq::EndpointRef{},
                track != nullptr && DestinationTurnedOff(track->Destination), L"DestinationTurnedOffNote");

            SourceDetailsButton().IsEnabled(track != nullptr && !track->Source.Endpoint.IsEmpty());
            DestinationDetailsButton().IsEnabled(track != nullptr && !track->Destination.Endpoint.IsEmpty());
        }
        MIDI_SEQUENCER_CATCH_AND_LOG(L"Unable to show what the track's devices are.")
    }

    _Use_decl_annotations_
    void MainWindow::OnSourceDetailsClick(foundation::IInspectable const& sender, xaml::RoutedEventArgs const&)
    {
        try
        {
            if (auto const track = seq::FindTrack(m_doc, m_inspectorTrackId); track != nullptr)
            {
                ShowDeviceDetails(track->Source.Endpoint, sender.as<xaml::FrameworkElement>());
            }
        }
        MIDI_SEQUENCER_CATCH_AND_LOG(L"Unable to show the source's details.")
    }

    _Use_decl_annotations_
    void MainWindow::OnDestinationDetailsClick(foundation::IInspectable const& sender, xaml::RoutedEventArgs const&)
    {
        try
        {
            if (auto const track = seq::FindTrack(m_doc, m_inspectorTrackId); track != nullptr)
            {
                ShowDeviceDetails(track->Destination.Endpoint, sender.as<xaml::FrameworkElement>());
            }
        }
        MIDI_SEQUENCER_CATCH_AND_LOG(L"Unable to show the destination's details.")
    }
}
