// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#include "pch.h"
#include "MainWindow.xaml.h"

#include "SequenceRender.h"
#include "StringResources.h"

namespace res = ::midisequencer::resources;

namespace winrt::midisequencer::implementation
{
    namespace
    {
        constexpr double TicksPerFourFourBar = 3840.0;
        constexpr double DragThreshold = 4.0;
        constexpr double LaneTop = 4294967295.0;

        // How near a point the pointer has to be to pick it up, in pixels.
        constexpr float PointReach = 6.0f;

        // With snap off, the pencil leaves a point about this many pixels apart.
        constexpr double PencilPixels = 6.0;

        // Controllers people know by name. The rest are "Controller N".
        constexpr std::array<uint16_t, 18> NamedControllers{ 1, 2, 4, 5, 7, 8, 10, 11, 64, 65, 66, 67, 71, 72, 73, 74, 91, 93 };

        std::wstring ControllerName(uint16_t number)
        {
            if (std::find(NamedControllers.begin(), NamedControllers.end(), number) != NamedControllers.end())
            {
                return std::wstring{ res::GetString(L"ControllerName" + std::to_wstring(number)) };
            }

            return {};
        }

        std::wstring Trim(std::wstring const& text)
        {
            auto const first = text.find_first_not_of(L" \t");
            auto const last = text.find_last_not_of(L" \t");
            return first == std::wstring::npos ? std::wstring{} : text.substr(first, last - first + 1);
        }

        // A number someone typed, allowing a percent sign or a unit after it.
        std::optional<double> ParseNumber(std::wstring const& text)
        {
            try
            {
                auto const trimmed = Trim(text);
                size_t used{ 0 };
                auto const value = std::stod(trimmed, &used);

                if (used == 0 || !std::isfinite(value))
                {
                    return std::nullopt;
                }

                return value;
            }
            catch (...)
            {
                return std::nullopt;
            }
        }

        std::wstring TempoValueText(double bpm)
        {
            return std::format(L"{:g}", std::round(bpm * 100.0) / 100.0);
        }

        // Where a lane's value is shown and typed: percent, plus or minus from the center for pitch
        // bend, and the program number counted from 1.
        double LaneValueToUnits(seq::LaneTarget target, uint32_t value)
        {
            switch (target)
            {
            case seq::LaneTarget::Program:
                return static_cast<double>(seq::ScaleDown(value, 32, 7) + 1);

            case seq::LaneTarget::PitchBend:
                return (static_cast<double>(value) - 2147483648.0) / (value >= 0x80000000u ? 2147483647.0 : 2147483648.0) * 100.0;

            default:
                return static_cast<double>(value) / LaneTop * 100.0;
            }
        }

        std::optional<uint32_t> UnitsToLaneValue(seq::LaneTarget target, double units)
        {
            switch (target)
            {
            case seq::LaneTarget::Program:
                if (units < 1.0 || units > 128.0)
                {
                    return std::nullopt;
                }

                return seq::ScaleUp(static_cast<uint32_t>(std::lround(units)) - 1, 7, 32);

            case seq::LaneTarget::PitchBend:
                if (units < -100.0 || units > 100.0)
                {
                    return std::nullopt;
                }

                return static_cast<uint32_t>(std::clamp<long long>(std::llround(2147483648.0 + units / 100.0 * (units >= 0 ? 2147483647.0 : 2147483648.0)), 0, 0xFFFFFFFFLL));

            default:
                if (units < 0.0 || units > 100.0)
                {
                    return std::nullopt;
                }

                return static_cast<uint32_t>(std::llround(units / 100.0 * LaneTop));
            }
        }

        bool IsMeterDenominator(int32_t value) noexcept
        {
            return value == 1 || value == 2 || value == 4 || value == 8 || value == 16 || value == 32;
        }

        void AddItem(controls::MenuFlyout const& menu, winrt::hstring const& text, wchar_t const* glyph, std::function<void()> action, bool enabled = true)
        {
            controls::MenuFlyoutItem item{};
            item.Text(text);
            item.IsEnabled(enabled);

            if (glyph != nullptr)
            {
                controls::FontIcon icon{};
                icon.Glyph(glyph);
                item.Icon(icon);
            }

            item.Click([action = std::move(action)](auto&&, auto&&) { action(); });
            menu.Items().Append(item);
        }

        void AddToggle(controls::MenuFlyout const& menu, winrt::hstring const& text, bool checked, bool enabled, std::function<void()> action)
        {
            controls::ToggleMenuFlyoutItem item{};
            item.Text(text);
            item.IsChecked(checked);
            item.IsEnabled(enabled);
            item.Click([action = std::move(action)](auto&&, auto&&) { action(); });
            menu.Items().Append(item);
        }
    }

    // ---------------------------------------------------------------- names and values

    _Use_decl_annotations_
    std::vector<seq::AutomationLane>* MainWindow::LanesOwnedBy(std::wstring const& ownerId) noexcept
    {
        if (auto const track = seq::FindTrack(m_doc, ownerId); track != nullptr)
        {
            return track->IsFolder ? nullptr : &track->Lanes;
        }

        auto const clip = seq::FindClip(m_doc, ownerId);
        return clip != nullptr ? &clip->Lanes : nullptr;
    }

    _Use_decl_annotations_
    std::wstring MainWindow::LaneName(seq::AutomationLane const& lane) const
    {
        if (!lane.Name.empty())
        {
            return lane.Name;
        }

        auto const bank = lane.Number >> 7;
        auto const index = lane.Number & 0x7F;

        switch (lane.Target)
        {
        case seq::LaneTarget::Controller:
        {
            auto name = ControllerName(lane.Number);
            return name.empty() ? std::wstring{ res::FormatString(L"LaneControllerFormat", lane.Number) } : name;
        }

        case seq::LaneTarget::PitchBend: return std::wstring{ res::GetString(L"LanePitchBend") };
        case seq::LaneTarget::ChannelPressure: return std::wstring{ res::GetString(L"LaneChannelPressure") };
        case seq::LaneTarget::Program: return std::wstring{ res::GetString(L"LaneProgram") };
        case seq::LaneTarget::Registered: return std::wstring{ res::FormatString(L"LaneRegisteredFormat", bank, index) };
        case seq::LaneTarget::Assignable: return std::wstring{ res::FormatString(L"LaneAssignableFormat", bank, index) };
        default: return {};
        }
    }

    _Use_decl_annotations_
    std::wstring MainWindow::LaneDescription(seq::Track const& track, seq::AutomationLane const& lane) const
    {
        auto const bank = lane.Number >> 7;
        auto const index = lane.Number & 0x7F;

        std::wstring target{};

        switch (lane.Target)
        {
        case seq::LaneTarget::Controller: target = res::FormatString(L"LaneShortControllerFormat", lane.Number); break;
        case seq::LaneTarget::PitchBend: target = res::GetString(L"LanePitchBend"); break;
        case seq::LaneTarget::ChannelPressure: target = res::GetString(L"LaneShortPressure"); break;
        case seq::LaneTarget::Program: target = res::GetString(L"LaneShortProgram"); break;
        case seq::LaneTarget::Registered: target = res::FormatString(L"LaneRegisteredFormat", bank, index); break;
        case seq::LaneTarget::Assignable: target = res::FormatString(L"LaneAssignableFormat", bank, index); break;
        default: break;
        }

        // What the destination gets: everything over MIDI 2.0, 7 or 14 bits over MIDI 1.0.
        auto midi2 = m_directory->Lookup(track.Destination.Endpoint, track.Destination.Group).SpeaksMidi2;

        if (track.Destination.Protocol == seq::ProtocolChoice::Midi1)
        {
            midi2 = false;
        }
        else if (track.Destination.Protocol == seq::ProtocolChoice::Midi2)
        {
            midi2 = true;
        }

        auto const bits = static_cast<int32_t>(seq::LaneResolutionBits(lane.Target, midi2));
        std::wstring text{ res::FormatString(L"LaneSubFormat", target, bits) };

        // A destination that keeps each message's channel plays the lane on the lane's own.
        if (track.Destination.Channel < 0)
        {
            text += L" \u00B7 ";
            text += res::FormatString(L"ChannelShortFormat", lane.Channel + 1);
        }

        // How often a ramp goes to the device, when MIDI Settings says.
        if (auto const interval = m_directory->Lookup(track.Destination.Endpoint, track.Destination.Group).AutomationIntervalMilliseconds; interval > 0)
        {
            text += L" \u00B7 ";
            text += res::FormatString(L"LaneIntervalShortFormat", interval);
        }

        return text;
    }

    _Use_decl_annotations_
    std::wstring MainWindow::LaneValueText(seq::AutomationLane const& lane, uint32_t value) const
    {
        auto const units = LaneValueToUnits(lane.Target, value);

        switch (lane.Target)
        {
        case seq::LaneTarget::Program:
            return std::wstring{ res::FormatString(L"LaneValueProgramFormat", static_cast<int32_t>(units)) };

        case seq::LaneTarget::PitchBend:
        {
            auto const rounded = static_cast<int32_t>(std::lround(units));
            auto const number = rounded == 0 ? std::wstring{ L"0" } : std::format(L"{:+}", rounded);
            return std::wstring{ res::FormatString(L"LaneValuePercentFormat", number) };
        }

        default:
            return std::wstring{ res::FormatString(L"LaneValuePercentFormat", std::to_wstring(std::lround(units))) };
        }
    }

    _Use_decl_annotations_
    std::optional<uint32_t> MainWindow::PlayingLaneValue(seq::Track const& track, seq::AutomationLane const& lane, int64_t tick) const
    {
        // Inside a clip with its own lane for this, the clip's lane is what plays.
        for (auto const& placement : track.Timeline)
        {
            auto const clip = seq::FindClip(m_doc, placement.ClipId);

            if (clip == nullptr || clip->Length <= 0)
            {
                continue;
            }

            auto const length = placement.Length > 0 ? placement.Length : clip->Length;
            auto const end = placement.Tick + (clip->Loop ? length : std::min(length, clip->Length));

            if (tick < placement.Tick || tick >= end)
            {
                continue;
            }

            for (auto const& own : clip->Lanes)
            {
                if (!own.Points.empty() && seq::SameLaneTarget(own, lane))
                {
                    return seq::LaneValueAt(own, (tick - placement.Tick) % clip->Length);
                }
            }
        }

        return seq::LaneValueAt(lane, tick);
    }

    _Use_decl_annotations_
    void MainWindow::UpdateLaneChips(int64_t tick) noexcept
    {
        try
        {
            for (auto const& chip : m_laneChips)
            {
                auto const track = seq::FindTrack(m_doc, chip.TrackId);

                if (track == nullptr || chip.Text == nullptr)
                {
                    continue;
                }

                auto const lane = std::find_if(track->Lanes.begin(), track->Lanes.end(), [&chip](seq::AutomationLane const& candidate) { return candidate.Id == chip.LaneId; });

                if (lane == track->Lanes.end())
                {
                    continue;
                }

                auto const value = PlayingLaneValue(*track, *lane, tick);
                winrt::hstring const text{ value.has_value() ? LaneValueText(*lane, *value) : std::wstring{ L"\u2013" } };

                if (chip.Text.Text() != text)
                {
                    chip.Text.Text(text);
                }
            }
        }
        catch (...)
        {
        }
    }

    // ---------------------------------------------------------------- lanes

    _Use_decl_annotations_
    void MainWindow::ToggleLanes(std::wstring const& trackId) noexcept
    {
        try
        {
            auto const track = seq::FindTrack(m_doc, trackId);

            if (track == nullptr || track->IsFolder)
            {
                return;
            }

            // Kept in the file so a sequence opens the way it was left, but not an edit to undo.
            track->LanesOpen = !track->LanesOpen;
            m_dirty = true;
            ++m_version;

            if (!track->LanesOpen && m_selectedLaneTrackId == trackId)
            {
                ClearAutomationSelection();
            }

            RebuildLayout();
            UpdateTitle();
            UpdateSavedState();

            if (m_autosaveTimer != nullptr && !m_path.empty() && !m_readOnly)
            {
                m_autosaveTimer.Stop();
                m_autosaveTimer.Start();
            }
        }
        MIDI_SEQUENCER_CATCH_AND_LOG(L"Unable to show or hide automation.")
    }

    _Use_decl_annotations_
    void MainWindow::EditLanes(std::wstring const& trackId, std::wstring const& name, std::function<void(std::vector<seq::AutomationLane>&)> const& edit)
    {
        try
        {
            auto const lanes = LanesOwnedBy(trackId);

            if (lanes == nullptr)
            {
                return;
            }

            auto before = *lanes;
            edit(*lanes);

            seq::ChangeList changes{};

            if (seq::FindTrack(m_doc, trackId) != nullptr)
            {
                changes.push_back(seq::MakeTrackLanesChange(trackId, std::move(before), *lanes));
            }
            else
            {
                changes.push_back(seq::MakeClipLanesChange(trackId, std::move(before), *lanes));
            }

            Commit(name, std::move(changes));
        }
        MIDI_SEQUENCER_CATCH_AND_LOG(L"Unable to change the automation.")
    }

    _Use_decl_annotations_
    void MainWindow::RenameLane(std::wstring const& trackId, std::wstring const& laneId)
    {
        auto const lanes = LanesOwnedBy(trackId);
        auto const lane = lanes != nullptr ? seq::FindLane(*lanes, laneId) : nullptr;

        if (lane == nullptr)
        {
            return;
        }

        ShowTextPrompt(res::GetString(L"RenameLaneTitle"), winrt::hstring{ LaneName(*lane) }, [weak = get_weak(), trackId, laneId](std::wstring const& text)
        {
            auto strong = weak.get();

            if (!strong)
            {
                return;
            }

            strong->EditLanes(trackId, std::wstring{ res::GetString(L"UndoRenameLane") }, [&](std::vector<seq::AutomationLane>& lanes)
            {
                if (auto found = seq::FindLane(lanes, laneId); found != nullptr)
                {
                    // Cleared, or left as its target's own name: the target names it.
                    auto unnamed = *found;
                    unnamed.Name.clear();
                    found->Name = text == strong->LaneName(unnamed) ? std::wstring{} : text.substr(0, 128);
                }
            });
        });
    }

    _Use_decl_annotations_
    void MainWindow::ShowAddLaneDialog(std::wstring const& trackId)
    {
        [](winrt::com_ptr<MainWindow> strong, std::wstring trackId) -> winrt::fire_and_forget
        {
            try
            {
                // A track's lanes, or a clip's, which play on the channel its track keeps.
                auto const track = seq::FindTrack(strong->m_doc, trackId);
                auto const clip = track == nullptr ? seq::FindClip(strong->m_doc, trackId) : nullptr;
                auto const player = track != nullptr ? track : strong->EditorTrack();

                if ((track == nullptr || track->IsFolder) && clip == nullptr)
                {
                    co_return;
                }

                auto const keepsChannels = player != nullptr && player->Destination.Channel < 0;
                auto const trackName = track != nullptr ? track->Name : clip->Name;

                controls::StackPanel panel{};
                panel.Spacing(12);
                panel.MinWidth(340);

                controls::ComboBox kind{};
                kind.Header(winrt::box_value(res::GetString(L"AddLaneKind")));
                kind.HorizontalAlignment(xaml::HorizontalAlignment::Stretch);

                for (auto const key : { L"LaneKindController", L"LaneKindPitchBend", L"LaneKindChannelPressure", L"LaneKindProgram", L"LaneKindRegistered", L"LaneKindAssignable" })
                {
                    kind.Items().Append(winrt::box_value(res::GetString(key)));
                }

                kind.SelectedIndex(0);
                panel.Children().Append(kind);

                auto const makeNumber = [](wchar_t const* headerKey, double maximum, double value)
                {
                    controls::NumberBox box{};
                    box.Header(winrt::box_value(res::GetString(headerKey)));
                    box.Minimum(0);
                    box.Maximum(maximum);
                    box.Value(value);
                    box.SpinButtonPlacementMode(controls::NumberBoxSpinButtonPlacementMode::Inline);
                    return box;
                };

                auto const controller = makeNumber(L"AddLaneController", 127, 74);
                panel.Children().Append(controller);

                controls::Grid parameter{};
                parameter.ColumnSpacing(8);
                parameter.Visibility(xaml::Visibility::Collapsed);

                for (int column = 0; column < 2; ++column)
                {
                    controls::ColumnDefinition definition{};
                    definition.Width(xaml::GridLength{ 1, xaml::GridUnitType::Star });
                    parameter.ColumnDefinitions().Append(definition);
                }

                auto const bank = makeNumber(L"AddLaneBank", 127, 0);
                auto const index = makeNumber(L"AddLaneIndex", 127, 0);
                controls::Grid::SetColumn(index, 1);
                parameter.Children().Append(bank);
                parameter.Children().Append(index);
                panel.Children().Append(parameter);

                controls::ComboBox channel{};
                channel.Header(winrt::box_value(res::GetString(L"AddLaneChannel")));
                channel.HorizontalAlignment(xaml::HorizontalAlignment::Stretch);
                channel.ItemTemplate(strong->RootGrid().Resources().Lookup(winrt::box_value(L"NamedChoiceTemplate")).as<xaml::DataTemplate>());
                strong->FillChannelCombo(channel, 0, false);
                channel.Visibility(keepsChannels ? xaml::Visibility::Visible : xaml::Visibility::Collapsed);
                panel.Children().Append(channel);

                controls::TextBox name{};
                name.Header(winrt::box_value(res::GetString(L"AddLaneName")));
                name.PlaceholderText(res::GetString(L"AddLaneNamePlaceholder"));
                panel.Children().Append(name);

                auto const describe = [](controls::NumberBox const& box)
                {
                    auto const value = box.Value();
                    auto const known = std::isnan(value) ? std::wstring{} : ControllerName(static_cast<uint16_t>(std::clamp(std::lround(value), 0L, 127L)));
                    box.Description(known.empty() ? nullptr : winrt::box_value(winrt::hstring{ known }));
                };

                describe(controller);
                controller.ValueChanged([describe](controls::NumberBox const& sender, auto&&) { describe(sender); });

                kind.SelectionChanged([controller, parameter](foundation::IInspectable const& sender, auto&&)
                {
                    auto const choice = sender.as<controls::ComboBox>().SelectedIndex();
                    controller.Visibility(choice == 0 ? xaml::Visibility::Visible : xaml::Visibility::Collapsed);
                    parameter.Visibility(choice == 4 || choice == 5 ? xaml::Visibility::Visible : xaml::Visibility::Collapsed);
                });

                controls::ContentDialog dialog{};
                dialog.XamlRoot(strong->Content().XamlRoot());
                dialog.Title(winrt::box_value(res::FormatString(L"AddLaneTitleFormat", trackName)));
                dialog.Content(panel);
                dialog.PrimaryButtonText(res::GetString(L"AddLaneAdd"));
                dialog.CloseButtonText(res::GetString(L"DialogCancel"));
                dialog.DefaultButton(controls::ContentDialogButton::Primary);
                dialog.RequestedTheme(strong->RootGrid().ActualTheme());

                auto const result = co_await dialog.ShowAsync();

                if (result != controls::ContentDialogResult::Primary)
                {
                    co_return;
                }

                static constexpr std::array<seq::LaneTarget, 6> targets{
                    seq::LaneTarget::Controller, seq::LaneTarget::PitchBend, seq::LaneTarget::ChannelPressure,
                    seq::LaneTarget::Program, seq::LaneTarget::Registered, seq::LaneTarget::Assignable };

                auto const target = targets[static_cast<size_t>(std::clamp(kind.SelectedIndex(), 0, 5))];

                auto const whole = [](controls::NumberBox const& box) -> uint16_t
                {
                    auto const value = box.Value();
                    return std::isnan(value) ? 0 : static_cast<uint16_t>(std::clamp(std::lround(value), 0L, 127L));
                };

                uint16_t number{ 0 };

                if (target == seq::LaneTarget::Controller)
                {
                    number = whole(controller);
                }
                else if (target == seq::LaneTarget::Registered || target == seq::LaneTarget::Assignable)
                {
                    number = static_cast<uint16_t>(whole(bank) * 128 + whole(index));
                }

                uint8_t laneChannel{ 0 };

                if (keepsChannels)
                {
                    if (auto const choice = channel.SelectedItem().try_as<appshared::NamedChoice>(); choice != nullptr)
                    {
                        laneChannel = static_cast<uint8_t>(std::clamp(choice.Value(), 0, 15));
                    }
                }

                auto lane = seq::MakeLane(target, number, laneChannel);
                lane.Name = Trim(std::wstring{ name.Text() }).substr(0, 128);

                // The track or the clip can have gone while the dialog was open.
                auto const lanes = strong->LanesOwnedBy(trackId);

                if (lanes == nullptr)
                {
                    co_return;
                }

                if (auto const now = seq::FindTrack(strong->m_doc, trackId); now != nullptr)
                {
                    now->LanesOpen = true;
                }

                // One lane for each thing: a second would fight the first.
                auto const existing = std::find_if(lanes->begin(), lanes->end(), [&lane](seq::AutomationLane const& candidate)
                {
                    return seq::SameLaneTarget(candidate, lane);
                });

                if (existing != lanes->end())
                {
                    strong->ShowMessage(res::FormatString(L"LaneAlreadyThereFormat", strong->LaneName(*existing)));
                    strong->RebuildLayout();
                    co_return;
                }

                strong->EditLanes(trackId, std::wstring{ res::GetString(L"UndoAddLane") }, [&lane](std::vector<seq::AutomationLane>& lanes)
                {
                    lanes.push_back(lane);
                });
            }
            MIDI_SEQUENCER_CATCH_AND_LOG(L"Unable to add an automation lane.")
        }(get_strong(), trackId);
    }

    _Use_decl_annotations_
    void MainWindow::ShowLaneRowMenu(std::wstring const& trackId, std::wstring const& laneId, std::optional<seq::LanePoint> addAt, xaml::UIElement const& anchor, foundation::Point point)
    {
        auto const lanes = LanesOwnedBy(trackId);
        auto const lane = lanes != nullptr ? seq::FindLane(*lanes, laneId) : nullptr;
        auto const onTrack = seq::FindTrack(m_doc, trackId) != nullptr;

        if (lane == nullptr)
        {
            return;
        }

        controls::MenuFlyout menu{};
        auto weak = get_weak();

        if (addAt.has_value())
        {
            AddItem(menu, res::GetString(L"MenuAddPointHere"), L"\uE710", [weak, trackId, laneId, at = *addAt]()
            {
                if (auto s = weak.get())
                {
                    std::wstring added{};

                    s->EditLanes(trackId, std::wstring{ res::GetString(L"UndoEditAutomation") }, [&](std::vector<seq::AutomationLane>& lanes)
                    {
                        if (auto found = seq::FindLane(lanes, laneId); found != nullptr)
                        {
                            seq::AddLanePoint(*found, at);
                            added = s->LaneName(*found);
                        }
                    });

                    if (!added.empty())
                    {
                        s->ShowMessage(res::FormatString(L"LanePointAddedFormat", added));
                    }
                }
            });
        }

        AddItem(menu, res::GetString(L"MenuRenameLane"), L"\uE8AC", [weak, trackId, laneId]() { if (auto s = weak.get()) { s->RenameLane(trackId, laneId); } });

        AddItem(menu, res::GetString(L"MenuClearLane"), L"\uE894", [weak, trackId, laneId]()
        {
            if (auto s = weak.get())
            {
                s->ClearAutomationSelection();
                s->EditLanes(trackId, std::wstring{ res::GetString(L"UndoClearLane") }, [&](std::vector<seq::AutomationLane>& lanes)
                {
                    if (auto found = seq::FindLane(lanes, laneId); found != nullptr)
                    {
                        found->Points.clear();
                    }
                });
            }
        }, !lane->Points.empty());

        // A clip's lanes show while it's open in the editor; only a track's can be hidden.
        if (onTrack)
        {
            AddItem(menu, res::GetString(L"MenuHideAutomation"), nullptr, [weak, trackId]() { if (auto s = weak.get()) { s->ToggleLanes(trackId); } });
        }

        menu.Items().Append(controls::MenuFlyoutSeparator{});

        AddItem(menu, res::GetString(L"MenuAddLane"), L"\uE9D2", [weak, trackId]() { if (auto s = weak.get()) { s->ShowAddLaneDialog(trackId); } });

        AddItem(menu, res::GetString(L"MenuDeleteLane"), L"\uE74D", [weak, trackId, laneId]()
        {
            if (auto s = weak.get())
            {
                s->ClearAutomationSelection();
                s->m_clipLanes.ClearSelection();
                s->EditLanes(trackId, std::wstring{ res::GetString(L"UndoDeleteLane") }, [&](std::vector<seq::AutomationLane>& lanes)
                {
                    std::erase_if(lanes, [&](seq::AutomationLane const& candidate) { return candidate.Id == laneId; });
                });
            }
        });

        primitives::FlyoutShowOptions options{};
        options.Position(point);
        menu.ShowAt(anchor, options);
    }

    _Use_decl_annotations_
    void MainWindow::ShowLanePointMenu(std::wstring const& trackId, std::wstring const& laneId, size_t index, xaml::UIElement const& anchor, foundation::Point point)
    {
        auto const lanes = LanesOwnedBy(trackId);
        auto const lane = lanes != nullptr ? seq::FindLane(*lanes, laneId) : nullptr;

        if (lane == nullptr || index >= lane->Points.size())
        {
            return;
        }

        auto const target = lane->Target;
        auto const current = lane->Points[index];

        controls::MenuFlyout menu{};
        auto weak = get_weak();

        // Changes the point at this index, if it's still there.
        auto const change = [weak, trackId, laneId, index](std::wstring const& undoName, std::function<void(seq::AutomationLane&, size_t)> edit)
        {
            if (auto s = weak.get())
            {
                s->EditLanes(trackId, undoName, [&](std::vector<seq::AutomationLane>& lanes)
                {
                    if (auto found = seq::FindLane(lanes, laneId); found != nullptr && index < found->Points.size())
                    {
                        edit(*found, index);
                    }
                });
            }
        };

        AddItem(menu, res::GetString(L"MenuSetValue"), L"\uE70F", [weak, change, target, current]()
        {
            auto s = weak.get();

            if (!s)
            {
                return;
            }

            auto const title = res::GetString(
                target == seq::LaneTarget::Program ? L"LaneValuePromptProgram" :
                target == seq::LaneTarget::PitchBend ? L"LaneValuePromptBend" : L"LaneValuePromptPercent");

            auto const units = LaneValueToUnits(target, current.Value);
            auto const initial = target == seq::LaneTarget::Program ? std::format(L"{}", std::lround(units)) : std::format(L"{:g}", std::round(units * 10.0) / 10.0);

            s->ShowTextPrompt(title, winrt::hstring{ initial }, [weak, change, target](std::wstring const& text)
            {
                auto strong = weak.get();
                auto const typed = ParseNumber(text);
                auto const value = typed.has_value() ? UnitsToLaneValue(target, *typed) : std::nullopt;

                if (!strong)
                {
                    return;
                }

                if (!value.has_value())
                {
                    strong->ShowMessage(res::GetString(L"LaneValueOutOfRange"));
                    return;
                }

                change(std::wstring{ res::GetString(L"UndoEditAutomation") }, [&](seq::AutomationLane& lane, size_t at)
                {
                    lane.Points[at].Value = *value;
                });
            });
        });

        AddToggle(menu, res::GetString(L"MenuHoldPoint"), current.Hold || target == seq::LaneTarget::Program, target != seq::LaneTarget::Program, [change]()
        {
            change(std::wstring{ res::GetString(L"UndoEditAutomation") }, [](seq::AutomationLane& lane, size_t at)
            {
                lane.Points[at].Hold = !lane.Points[at].Hold;
            });
        });

        AddItem(menu, res::GetString(L"MenuDeletePoint"), L"\uE74D", [weak, change]()
        {
            if (auto s = weak.get())
            {
                s->ClearAutomationSelection();
                s->m_clipLanes.ClearSelection();
            }

            change(std::wstring{ res::GetString(L"UndoDeletePoint") }, [](seq::AutomationLane& lane, size_t at)
            {
                lane.Points.erase(lane.Points.begin() + static_cast<ptrdiff_t>(at));
            });
        });

        primitives::FlyoutShowOptions options{};
        options.Position(point);
        menu.ShowAt(anchor, options);
    }

    // ---------------------------------------------------------------- tempo and meter

    _Use_decl_annotations_
    void MainWindow::PromptForTempo(int64_t tick, std::optional<size_t> index)
    {
        auto const current = index.has_value() && *index < m_doc.Tempo.size()
            ? m_doc.Tempo[*index].BeatsPerMinute
            : seq::TempoMap{ m_doc.Tempo }.BeatsPerMinuteAtTick(tick);

        auto const bar = seq::BarPositionAtTick(m_doc.Meter, tick).Bar;

        ShowTextPrompt(res::FormatString(L"TempoPromptFormat", bar), winrt::hstring{ TempoValueText(current) }, [weak = get_weak(), tick, index](std::wstring const& text)
        {
            auto strong = weak.get();

            if (!strong)
            {
                return;
            }

            auto const bpm = ParseNumber(text);

            if (!bpm.has_value() || *bpm < seq::MinimumBeatsPerMinute || *bpm > seq::MaximumBeatsPerMinute)
            {
                strong->ShowMessage(res::GetString(L"TempoOutOfRange"));
                return;
            }

            auto before = strong->m_doc.Tempo;

            if (index.has_value() && *index < strong->m_doc.Tempo.size())
            {
                strong->m_doc.Tempo[*index].BeatsPerMinute = *bpm;
            }
            else
            {
                seq::SetTempoAt(strong->m_doc.Tempo, tick, *bpm);
            }

            seq::ChangeList changes{};
            changes.push_back(seq::MakeTempoChange(std::move(before), strong->m_doc.Tempo));
            strong->Commit(std::wstring{ res::GetString(L"UndoChangeTempo") }, std::move(changes));
        });
    }

    _Use_decl_annotations_
    void MainWindow::PromptForMeter(int64_t bar)
    {
        auto const& meter = seq::MeterAtTick(m_doc.Meter, seq::TickAtBar(m_doc.Meter, bar));
        auto const initial = std::format(L"{}/{}", meter.Numerator, meter.Denominator);

        ShowTextPrompt(res::FormatString(L"MeterPromptFormat", bar), winrt::hstring{ initial }, [weak = get_weak(), bar](std::wstring const& text)
        {
            auto strong = weak.get();

            if (!strong)
            {
                return;
            }

            auto const slash = text.find(L'/');
            int32_t numerator{ 0 };
            int32_t denominator{ 0 };

            try
            {
                if (slash != std::wstring::npos)
                {
                    numerator = std::stoi(Trim(text.substr(0, slash)));
                    denominator = std::stoi(Trim(text.substr(slash + 1)));
                }
            }
            catch (...)
            {
                numerator = 0;
            }

            if (numerator < 1 || numerator > 64 || !IsMeterDenominator(denominator))
            {
                strong->ShowMessage(res::GetString(L"MeterInvalid"));
                return;
            }

            auto before = strong->m_doc.Meter;
            seq::SetMeterAt(strong->m_doc.Meter, bar, static_cast<uint8_t>(numerator), static_cast<uint8_t>(denominator));

            seq::ChangeList changes{};
            changes.push_back(seq::MakeMeterChange(std::move(before), strong->m_doc.Meter));
            strong->Commit(std::wstring{ res::GetString(L"UndoChangeMeter") }, std::move(changes));
        });
    }

    _Use_decl_annotations_
    void MainWindow::AddTempoMenuItems(controls::MenuFlyout const& menu, int64_t tick)
    {
        auto weak = get_weak();
        auto const snapped = seq::SnapTick(tick, std::max<int64_t>(SnapGrid(), seq::TicksPerQuarterNote));
        auto const bar = seq::BarPositionAtTick(m_doc.Meter, tick).Bar;
        auto const barTick = seq::TickAtBar(m_doc.Meter, bar);

        menu.Items().Append(controls::MenuFlyoutSeparator{});

        AddItem(menu, res::GetString(L"MenuChangeTempoHere"), L"\uE916", [weak, snapped]()
        {
            if (auto s = weak.get()) { s->PromptForTempo(snapped, std::nullopt); }
        });

        AddItem(menu, res::FormatString(L"MenuChangeMeterFormat", bar), nullptr, [weak, bar]()
        {
            if (auto s = weak.get()) { s->PromptForMeter(bar); }
        });

        // A meter change that starts this bar can go. The first meter can't.
        for (size_t i = 1; i < m_doc.Meter.size(); ++i)
        {
            if (m_doc.Meter[i].Tick != barTick)
            {
                continue;
            }

            AddItem(menu, res::FormatString(L"MenuRemoveMeterFormat", bar), L"\uE74D", [weak, i]()
            {
                if (auto s = weak.get())
                {
                    auto before = s->m_doc.Meter;

                    if (seq::RemoveMeterChange(s->m_doc.Meter, i))
                    {
                        seq::ChangeList changes{};
                        changes.push_back(seq::MakeMeterChange(std::move(before), s->m_doc.Meter));
                        s->Commit(std::wstring{ res::GetString(L"UndoChangeMeter") }, std::move(changes));
                    }
                }
            });

            break;
        }
    }

    _Use_decl_annotations_
    void MainWindow::ShowTempoPointMenu(size_t index, xaml::UIElement const& anchor, foundation::Point point)
    {
        if (index >= m_doc.Tempo.size())
        {
            return;
        }

        auto const tick = m_doc.Tempo[index].Tick;
        auto const last = index + 1 >= m_doc.Tempo.size();

        controls::MenuFlyout menu{};
        auto weak = get_weak();

        // One undo step for each change to the tempo map.
        auto const change = [weak](std::function<void(std::vector<seq::TempoPoint>&)> edit)
        {
            if (auto s = weak.get())
            {
                auto before = s->m_doc.Tempo;
                edit(s->m_doc.Tempo);

                seq::ChangeList changes{};
                changes.push_back(seq::MakeTempoChange(std::move(before), s->m_doc.Tempo));
                s->Commit(std::wstring{ res::GetString(L"UndoChangeTempo") }, std::move(changes));
            }
        };

        AddItem(menu, res::GetString(L"MenuSetTempo"), L"\uE70F", [weak, tick, index]()
        {
            if (auto s = weak.get()) { s->PromptForTempo(tick, index); }
        });

        AddToggle(menu, res::GetString(L"MenuRampToNext"), m_doc.Tempo[index].RampToNext, !last, [change, index]()
        {
            change([index](std::vector<seq::TempoPoint>& tempo)
            {
                if (index + 1 < tempo.size())
                {
                    tempo[index].RampToNext = !tempo[index].RampToNext;
                }
            });
        });

        AddItem(menu, res::GetString(L"MenuDeleteTempoChange"), L"\uE74D", [weak, change, index]()
        {
            if (auto s = weak.get())
            {
                s->ClearAutomationSelection();
            }

            change([index](std::vector<seq::TempoPoint>& tempo) { seq::RemoveTempoPoint(tempo, index); });
        }, index > 0);

        primitives::FlyoutShowOptions options{};
        options.Position(point);
        menu.ShowAt(anchor, options);
    }

    _Use_decl_annotations_
    void MainWindow::OnTempoDisplayClick(foundation::IInspectable const& sender, xaml::RoutedEventArgs const&)
    {
        try
        {
            if (auto const anchor = sender.try_as<xaml::FrameworkElement>(); anchor != nullptr)
            {
                ShowTempoFlyout(anchor);
            }
        }
        MIDI_SEQUENCER_CATCH_AND_LOG(L"Unable to show the tempo settings.")
    }

    _Use_decl_annotations_
    void MainWindow::ShowTempoFlyout(xaml::FrameworkElement const& anchor)
    {
        auto const tick = m_position;
        auto const& tempo = m_doc.Tempo;
        auto const& meter = m_doc.Meter;

        if (tempo.empty() || meter.empty())
        {
            return;
        }

        // The tempo and meter in effect at the playhead, and where each began.
        auto const tempoIndex = static_cast<size_t>(std::max<ptrdiff_t>(0, std::distance(tempo.begin(),
            std::upper_bound(tempo.begin(), tempo.end(), tick, [](int64_t value, seq::TempoPoint const& point) { return value < point.Tick; })) - 1));

        auto const meterIndex = static_cast<size_t>(std::max<ptrdiff_t>(0, std::distance(meter.begin(),
            std::upper_bound(meter.begin(), meter.end(), tick, [](int64_t value, seq::MeterChange const& change) { return value < change.Tick; })) - 1));

        auto const tempoBar = seq::BarPositionAtTick(meter, tempo[tempoIndex].Tick).Bar;
        auto const meterBar = seq::BarPositionAtTick(meter, meter[meterIndex].Tick).Bar;
        auto const playheadBar = seq::BarPositionAtTick(meter, tick).Bar;
        auto const playheadBarTick = seq::TickAtBar(meter, playheadBar);

        controls::StackPanel panel{};
        panel.Spacing(10);
        panel.Width(320);

        controls::TextBlock title{};
        title.Text(res::GetString(L"TempoFlyoutTitle"));
        title.FontWeight(winrt::Microsoft::UI::Text::FontWeights::SemiBold());
        panel.Children().Append(title);

        controls::NumberBox bpm{};
        bpm.Header(winrt::box_value(res::FormatString(L"TempoFromBarFormat", tempoBar)));
        bpm.Minimum(seq::MinimumBeatsPerMinute);
        bpm.Maximum(seq::MaximumBeatsPerMinute);
        bpm.SmallChange(1);
        bpm.LargeChange(10);
        bpm.Value(std::round(tempo[tempoIndex].BeatsPerMinute * 100.0) / 100.0);
        bpm.SpinButtonPlacementMode(controls::NumberBoxSpinButtonPlacementMode::Inline);
        panel.Children().Append(bpm);

        // Laid out like the meter is written, 4 / 4, under one heading.
        controls::StackPanel meterGroup{};
        meterGroup.Spacing(4);

        controls::TextBlock meterHeader{};
        meterHeader.Text(res::FormatString(L"MeterFromBarFormat", meterBar));
        meterGroup.Children().Append(meterHeader);

        controls::Grid meterRow{};
        meterRow.ColumnSpacing(8);

        for (auto const star : { true, false, true })
        {
            controls::ColumnDefinition definition{};
            definition.Width(star ? xaml::GridLength{ 1, xaml::GridUnitType::Star } : xaml::GridLength{ 0, xaml::GridUnitType::Auto });
            meterRow.ColumnDefinitions().Append(definition);
        }

        controls::NumberBox numerator{};
        automation::AutomationProperties::SetName(numerator, res::GetString(L"MeterBeatsPerBarName"));
        numerator.Minimum(1);
        numerator.Maximum(64);
        numerator.Value(meter[meterIndex].Numerator);
        numerator.SpinButtonPlacementMode(controls::NumberBoxSpinButtonPlacementMode::Inline);
        meterRow.Children().Append(numerator);

        controls::TextBlock slash{};
        slash.Text(res::GetString(L"MeterSeparator"));
        slash.VerticalAlignment(xaml::VerticalAlignment::Center);
        automation::AutomationProperties::SetAccessibilityView(slash, automation::Peers::AccessibilityView::Raw);
        controls::Grid::SetColumn(slash, 1);
        meterRow.Children().Append(slash);

        controls::ComboBox denominator{};
        automation::AutomationProperties::SetName(denominator, res::GetString(L"MeterBeatNoteName"));
        denominator.HorizontalAlignment(xaml::HorizontalAlignment::Stretch);
        controls::Grid::SetColumn(denominator, 2);

        static constexpr std::array<uint8_t, 6> denominators{ 1, 2, 4, 8, 16, 32 };

        for (size_t i = 0; i < denominators.size(); ++i)
        {
            denominator.Items().Append(winrt::box_value(res::GetString(L"MeterBeatNote" + std::to_wstring(static_cast<int>(denominators[i])))));

            if (denominators[i] == meter[meterIndex].Denominator)
            {
                denominator.SelectedIndex(static_cast<int32_t>(i));
            }
        }

        meterRow.Children().Append(denominator);
        meterGroup.Children().Append(meterRow);
        panel.Children().Append(meterGroup);

        controls::Flyout flyout{};
        auto weak = get_weak();
        auto const weakFlyout = winrt::make_weak(flyout);

        // Changes from a bar the playhead is in, when it isn't already where they start.
        if (tempo[tempoIndex].Tick != playheadBarTick || meter[meterIndex].Tick != playheadBarTick)
        {
            controls::StackPanel buttons{};
            buttons.Spacing(6);

            if (tempo[tempoIndex].Tick != playheadBarTick)
            {
                controls::Button addTempo{};
                addTempo.Content(winrt::box_value(res::FormatString(L"AddTempoChangeAtBarFormat", playheadBar)));
                addTempo.Style(RootGrid().Resources().Lookup(winrt::box_value(L"SmallButtonStyle")).as<xaml::Style>());
                addTempo.Click([weak, weakFlyout, playheadBarTick](auto&&, auto&&)
                {
                    if (auto f = weakFlyout.get()) { f.Hide(); }
                    if (auto s = weak.get()) { s->PromptForTempo(playheadBarTick, std::nullopt); }
                });
                buttons.Children().Append(addTempo);
            }

            if (meter[meterIndex].Tick != playheadBarTick)
            {
                controls::Button addMeter{};
                addMeter.Content(winrt::box_value(res::FormatString(L"AddMeterChangeAtBarFormat", playheadBar)));
                addMeter.Style(RootGrid().Resources().Lookup(winrt::box_value(L"SmallButtonStyle")).as<xaml::Style>());
                addMeter.Click([weak, weakFlyout, playheadBar](auto&&, auto&&)
                {
                    if (auto f = weakFlyout.get()) { f.Hide(); }
                    if (auto s = weak.get()) { s->PromptForMeter(playheadBar); }
                });
                buttons.Children().Append(addMeter);
            }

            panel.Children().Append(buttons);
        }

        bpm.ValueChanged([weak, tempoIndex](controls::NumberBox const& sender, auto&&)
        {
            auto strong = weak.get();
            auto const value = sender.Value();

            if (!strong || std::isnan(value) || tempoIndex >= strong->m_doc.Tempo.size())
            {
                return;
            }

            auto const bpmValue = std::clamp(value, seq::MinimumBeatsPerMinute, seq::MaximumBeatsPerMinute);

            if (std::abs(strong->m_doc.Tempo[tempoIndex].BeatsPerMinute - bpmValue) < 0.0001)
            {
                return;
            }

            auto before = strong->m_doc.Tempo;
            strong->m_doc.Tempo[tempoIndex].BeatsPerMinute = bpmValue;

            seq::ChangeList changes{};
            changes.push_back(seq::MakeTempoChange(std::move(before), strong->m_doc.Tempo));
            strong->Commit(std::wstring{ res::GetString(L"UndoChangeTempo") }, std::move(changes));
        });

        // The two halves of the meter are read together, whichever changed.
        auto const applyMeter = [weak, meterBar, weakNumerator = winrt::make_weak(numerator), weakDenominator = winrt::make_weak(denominator)]()
        {
            auto strong = weak.get();
            auto top = weakNumerator.get();
            auto bottom = weakDenominator.get();

            if (!strong || top == nullptr || bottom == nullptr || std::isnan(top.Value()) || bottom.SelectedIndex() < 0)
            {
                return;
            }

            auto const n = static_cast<uint8_t>(std::clamp(std::lround(top.Value()), 1L, 64L));
            auto const d = denominators[static_cast<size_t>(std::min<int32_t>(bottom.SelectedIndex(), 5))];
            auto const& current = seq::MeterAtTick(strong->m_doc.Meter, seq::TickAtBar(strong->m_doc.Meter, meterBar));

            if (current.Numerator == n && current.Denominator == d)
            {
                return;
            }

            auto before = strong->m_doc.Meter;
            seq::SetMeterAt(strong->m_doc.Meter, meterBar, n, d);

            seq::ChangeList changes{};
            changes.push_back(seq::MakeMeterChange(std::move(before), strong->m_doc.Meter));
            strong->Commit(std::wstring{ res::GetString(L"UndoChangeMeter") }, std::move(changes));
        };

        numerator.ValueChanged([applyMeter](auto&&, auto&&) { applyMeter(); });
        denominator.SelectionChanged([applyMeter](auto&&, auto&&) { applyMeter(); });

        flyout.Content(panel);
        flyout.ShowAt(anchor);
    }

    // ---------------------------------------------------------------- selection

    void MainWindow::ClearAutomationSelection() noexcept
    {
        m_selectedLaneTrackId.clear();
        m_selectedLaneId.clear();
        m_selectedLanePoint = SIZE_MAX;
        m_selectedTempoPoint = SIZE_MAX;
    }

    void MainWindow::DeleteSelectedAutomationPoint() noexcept
    {
        try
        {
            if (m_selectedTempoPoint != SIZE_MAX)
            {
                auto const index = m_selectedTempoPoint;
                m_selectedTempoPoint = SIZE_MAX;

                auto before = m_doc.Tempo;

                if (seq::RemoveTempoPoint(m_doc.Tempo, index))
                {
                    seq::ChangeList changes{};
                    changes.push_back(seq::MakeTempoChange(std::move(before), m_doc.Tempo));
                    Commit(std::wstring{ res::GetString(L"UndoChangeTempo") }, std::move(changes));
                }
                else
                {
                    InvalidateArrange();
                }

                return;
            }

            auto const trackId = m_selectedLaneTrackId;
            auto const laneId = m_selectedLaneId;
            auto const index = m_selectedLanePoint;
            ClearAutomationSelection();

            auto const track = seq::FindTrack(m_doc, trackId);
            auto const lane = track != nullptr ? seq::FindLane(track->Lanes, laneId) : nullptr;

            if (lane == nullptr || index >= lane->Points.size())
            {
                InvalidateArrange();
                return;
            }

            EditLanes(trackId, std::wstring{ res::GetString(L"UndoDeletePoint") }, [&](std::vector<seq::AutomationLane>& lanes)
            {
                if (auto found = seq::FindLane(lanes, laneId); found != nullptr && index < found->Points.size())
                {
                    found->Points.erase(found->Points.begin() + static_cast<ptrdiff_t>(index));
                }
            });
        }
        MIDI_SEQUENCER_CATCH_AND_LOG(L"Unable to delete a point.")
    }

    // ---------------------------------------------------------------- pointing at points

    _Use_decl_annotations_
    int64_t MainWindow::TickAtLaneX(double x) const noexcept
    {
        auto const pixelsPerTick = m_barWidth / TicksPerFourFourBar;
        return std::max<int64_t>(0, static_cast<int64_t>(std::floor((x + m_scrollX) / pixelsPerTick)));
    }

    _Use_decl_annotations_
    size_t MainWindow::HitTestLanePoint(seq::AutomationLane const& lane, float rowTop, float rowHeight, foundation::Point point) const noexcept
    {
        auto const pixelsPerTick = m_barWidth / TicksPerFourFourBar;
        auto const from = TickAtLaneX(static_cast<double>(point.X) - PointReach);
        auto const to = TickAtLaneX(static_cast<double>(point.X) + PointReach) + 1;
        auto const& points = lane.Points;

        auto const first = std::lower_bound(points.begin(), points.end(), from, [](seq::LanePoint const& candidate, int64_t tick) { return candidate.Tick < tick; });

        size_t best{ SIZE_MAX };
        auto bestDistance = PointReach * PointReach + 0.01f;

        for (auto it = first; it != points.end() && it->Tick <= to; ++it)
        {
            auto const dx = static_cast<float>(static_cast<double>(it->Tick) * pixelsPerTick - m_scrollX) - point.X;
            auto const dy = seq::LaneValueY(it->Value, rowTop, rowHeight) - point.Y;
            auto const distance = dx * dx + dy * dy;

            // At a jump, the later point is the one on top.
            if (distance <= bestDistance)
            {
                best = static_cast<size_t>(std::distance(points.begin(), it));
                bestDistance = distance;
            }
        }

        return best;
    }

    _Use_decl_annotations_
    size_t MainWindow::HitTestTempoPoint(float rowTop, float rowHeight, foundation::Point point) const noexcept
    {
        auto const pixelsPerTick = m_barWidth / TicksPerFourFourBar;
        auto const range = seq::TempoRowRange(m_doc.Tempo);

        size_t best{ SIZE_MAX };
        auto bestDistance = PointReach * PointReach + 0.01f;

        for (size_t i = 0; i < m_doc.Tempo.size(); ++i)
        {
            auto const dx = static_cast<float>(static_cast<double>(m_doc.Tempo[i].Tick) * pixelsPerTick - m_scrollX) - point.X;
            auto const dy = seq::TempoY(m_doc.Tempo[i].BeatsPerMinute, range, rowTop, rowHeight) - point.Y;
            auto const distance = dx * dx + dy * dy;

            if (distance <= bestDistance)
            {
                best = i;
                bestDistance = distance;
            }
        }

        return best;
    }

    _Use_decl_annotations_
    bool MainWindow::OnAutomationPressed(bool pinned, seq::ArrangeRow const& row, input::PointerRoutedEventArgs const& args)
    {
        using winrt::Windows::System::VirtualKeyModifiers;

        auto const trackId = row.TrackId;
        auto const laneId = row.LaneId;
        auto const top = static_cast<float>(row.Top - (pinned ? 0.0 : m_scrollY));
        auto const height = static_cast<float>(row.Height);

        auto const track = seq::FindTrack(m_doc, trackId);
        auto const lane = track != nullptr ? seq::FindLane(track->Lanes, laneId) : nullptr;

        if (lane == nullptr)
        {
            return false;
        }

        auto const canvas = pinned ? m_pinnedLaneCanvas : m_scrollLaneCanvas;
        auto const position = args.GetCurrentPoint(canvas).Position();
        auto const pencil = (args.KeyModifiers() & VirtualKeyModifiers::Control) == VirtualKeyModifiers::Control;

        SelectTrack(trackId);
        m_selectedPlacement = SIZE_MAX;
        m_selectedTempoPoint = SIZE_MAX;

        PointDrag drag{};
        drag.Pinned = pinned;
        drag.TrackId = trackId;
        drag.LaneId = laneId;
        drag.Start = position;
        drag.RowTop = top;
        drag.RowHeight = height;
        drag.LanesBefore = track->Lanes;

        auto const hitPoint = pencil ? SIZE_MAX : HitTestLanePoint(*lane, top, height, position);

        if (pencil)
        {
            drag.What = PointDrag::Kind::Pencil;
            drag.Moved = true;
        }
        else if (hitPoint != SIZE_MAX)
        {
            drag.What = PointDrag::Kind::LanePoint;
            drag.Point = hitPoint;
        }
        else
        {
            // Anywhere else in the row adds a point there, already picked up.
            seq::LanePoint point{};
            point.Tick = seq::SnapTick(TickAtLaneX(position.X), SnapGrid());
            point.Value = seq::LaneValueAtY(position.Y, top, height);
            point.Hold = lane->Target == seq::LaneTarget::Program;

            if (lane->Target == seq::LaneTarget::Program)
            {
                point.Value = seq::ScaleUp(seq::ScaleDown(point.Value, 32, 7), 7, 32);
            }

            drag.What = PointDrag::Kind::LanePoint;
            drag.Point = seq::AddLanePoint(*lane, point);
            drag.Moved = true;
            m_pointAddedAt = std::chrono::steady_clock::now();
        }

        m_pointDrag = std::move(drag);

        m_selectedLaneTrackId = trackId;
        m_selectedLaneId = laneId;
        m_selectedLanePoint = m_pointDrag.What == PointDrag::Kind::LanePoint ? m_pointDrag.Point : SIZE_MAX;

        canvas.CapturePointer(args.Pointer());

        if (m_pointDrag.What == PointDrag::Kind::Pencil)
        {
            OnPointDragMoved(args);
        }

        InvalidateArrange();
        args.Handled(true);
        return true;
    }

    _Use_decl_annotations_
    bool MainWindow::OnTempoPressed(bool pinned, seq::ArrangeRow const& row, input::PointerRoutedEventArgs const& args)
    {
        auto const canvas = pinned ? m_pinnedLaneCanvas : m_scrollLaneCanvas;
        auto const position = args.GetCurrentPoint(canvas).Position();
        auto const top = static_cast<float>(row.Top - (pinned ? 0.0 : m_scrollY));
        auto const height = static_cast<float>(row.Height);
        auto index = HitTestTempoPoint(top, height, position);

        // The scale stays as it was when the press began, so the line doesn't slide away.
        auto const range = seq::TempoRowRange(m_doc.Tempo);
        auto before = m_doc.Tempo;
        auto added{ false };

        if (index == SIZE_MAX)
        {
            // Anywhere else on the line adds a tempo change there, already picked up, the same as
            // a point on an automation lane. Tenths of a beat per minute, and sixteenth notes
            // unless snap is finer.
            auto const bpm = std::clamp(std::round(seq::TempoAtY(position.Y, range, top, height) * 10.0) / 10.0, seq::MinimumBeatsPerMinute, seq::MaximumBeatsPerMinute);
            auto const grid = SnapGrid() > 0 ? SnapGrid() : seq::TicksPerQuarterNote / 4;
            auto const tick = seq::SnapTick(TickAtLaneX(position.X), grid);

            seq::SetTempoAt(m_doc.Tempo, tick, bpm);

            for (size_t i = 0; i < m_doc.Tempo.size(); ++i)
            {
                if (m_doc.Tempo[i].Tick == tick)
                {
                    index = i;
                    break;
                }
            }

            if (index == SIZE_MAX)
            {
                m_doc.Tempo = std::move(before);
                return false;
            }

            added = true;
            m_pointAddedAt = std::chrono::steady_clock::now();
            UpdateDisplays(m_position);
        }

        ClearAutomationSelection();
        m_selectedTempoPoint = index;
        m_selectedPlacement = SIZE_MAX;

        PointDrag drag{};
        drag.What = PointDrag::Kind::TempoPoint;
        drag.Pinned = pinned;
        drag.Point = index;
        drag.Start = position;
        drag.RowTop = top;
        drag.RowHeight = height;
        drag.Range = range;
        drag.TempoBefore = std::move(before);
        drag.Moved = added;
        m_pointDrag = std::move(drag);

        canvas.CapturePointer(args.Pointer());
        InvalidateArrange();
        args.Handled(true);
        return true;
    }

    _Use_decl_annotations_
    void MainWindow::OnPointDragMoved(input::PointerRoutedEventArgs const& args)
    {
        auto const canvas = m_pointDrag.Pinned ? m_pinnedLaneCanvas : m_scrollLaneCanvas;
        auto const position = args.GetCurrentPoint(canvas).Position();

        if (!m_pointDrag.Moved)
        {
            auto const dx = static_cast<double>(position.X) - static_cast<double>(m_pointDrag.Start.X);
            auto const dy = static_cast<double>(position.Y) - static_cast<double>(m_pointDrag.Start.Y);

            if (std::hypot(dx, dy) < DragThreshold)
            {
                return;
            }

            m_pointDrag.Moved = true;
        }

        if (m_pointDrag.What == PointDrag::Kind::TempoPoint)
        {
            auto const index = m_pointDrag.Point;

            if (index >= m_doc.Tempo.size())
            {
                return;
            }

            // Tenths of a beat per minute, and sixteenth notes unless snap is finer.
            auto const bpm = std::round(seq::TempoAtY(position.Y, m_pointDrag.Range, m_pointDrag.RowTop, m_pointDrag.RowHeight) * 10.0) / 10.0;
            auto const grid = SnapGrid() > 0 ? SnapGrid() : seq::TicksPerQuarterNote / 4;
            seq::MoveTempoPoint(m_doc.Tempo, index, seq::SnapTick(TickAtLaneX(position.X), grid), bpm);

            UpdateDisplays(m_position);
            InvalidateArrange();
            return;
        }

        auto const track = seq::FindTrack(m_doc, m_pointDrag.TrackId);
        auto const lane = track != nullptr ? seq::FindLane(track->Lanes, m_pointDrag.LaneId) : nullptr;

        if (lane == nullptr)
        {
            return;
        }

        auto value = seq::LaneValueAtY(position.Y, m_pointDrag.RowTop, m_pointDrag.RowHeight);

        if (lane->Target == seq::LaneTarget::Program)
        {
            value = seq::ScaleUp(seq::ScaleDown(value, 32, 7), 7, 32);
        }

        if (m_pointDrag.What == PointDrag::Kind::LanePoint)
        {
            seq::MoveLanePoint(*lane, m_pointDrag.Point, seq::SnapTick(TickAtLaneX(position.X), SnapGrid()), value);
            InvalidateArrange();
            return;
        }

        // The pencil leaves a point on each step of the grid it crosses, filling in any a quick
        // stroke skipped, and replaces what was under the stroke.
        auto const pixelsPerTick = m_barWidth / TicksPerFourFourBar;
        auto const grid = SnapGrid() > 0 ? SnapGrid() : std::max<int64_t>(1, std::llround(PencilPixels / pixelsPerTick));
        auto const tick = (TickAtLaneX(position.X) / grid) * grid;
        auto& drawn = m_pointDrag.Drawn;

        if (m_pointDrag.LastTick >= 0 && tick != m_pointDrag.LastTick)
        {
            auto const step = tick > m_pointDrag.LastTick ? grid : -grid;
            auto const span = static_cast<double>(tick - m_pointDrag.LastTick);

            for (auto t = m_pointDrag.LastTick + step; step > 0 ? t < tick : t > tick; t += step)
            {
                auto const fraction = static_cast<double>(t - m_pointDrag.LastTick) / span;
                auto const between = static_cast<double>(m_pointDrag.LastValue) + (static_cast<double>(value) - static_cast<double>(m_pointDrag.LastValue)) * fraction;
                drawn[t] = static_cast<uint32_t>(std::clamp(std::llround(between), 0LL, 0xFFFFFFFFLL));
            }
        }

        drawn[tick] = value;
        m_pointDrag.LastTick = tick;
        m_pointDrag.LastValue = value;

        auto const before = std::find_if(m_pointDrag.LanesBefore.begin(), m_pointDrag.LanesBefore.end(),
            [this](seq::AutomationLane const& candidate) { return candidate.Id == m_pointDrag.LaneId; });

        if (before == m_pointDrag.LanesBefore.end())
        {
            return;
        }

        std::vector<seq::LanePoint> points{};
        points.reserve(drawn.size());

        for (auto const& [at, level] : drawn)
        {
            points.push_back(seq::LanePoint{ at, level, lane->Target == seq::LaneTarget::Program });
        }

        lane->Points = before->Points;
        seq::ReplaceLanePoints(*lane, drawn.begin()->first, drawn.rbegin()->first, points);

        InvalidateArrange();
    }

    void MainWindow::OnPointDragReleased()
    {
        try
        {
            auto drag = std::move(m_pointDrag);
            m_pointDrag = PointDrag{};

            if (drag.What == PointDrag::Kind::None)
            {
                return;
            }

            if (!drag.Moved)
            {
                InvalidateArrange();
                return;
            }

            if (drag.What == PointDrag::Kind::TempoPoint)
            {
                seq::ChangeList changes{};
                changes.push_back(seq::MakeTempoChange(std::move(drag.TempoBefore), m_doc.Tempo));
                Commit(std::wstring{ res::GetString(L"UndoChangeTempo") }, std::move(changes));
                return;
            }

            auto const track = seq::FindTrack(m_doc, drag.TrackId);

            if (track == nullptr)
            {
                return;
            }

            seq::ChangeList changes{};
            changes.push_back(seq::MakeTrackLanesChange(drag.TrackId, std::move(drag.LanesBefore), track->Lanes));
            Commit(std::wstring{ res::GetString(drag.What == PointDrag::Kind::Pencil ? L"UndoDrawAutomation" : L"UndoEditAutomation") }, std::move(changes));
        }
        MIDI_SEQUENCER_CATCH_AND_LOG(L"Unable to finish an automation edit.")
    }

    _Use_decl_annotations_
    bool MainWindow::OnAutomationRightTapped(bool pinned, seq::ArrangeRow const& row, input::RightTappedRoutedEventArgs const& args)
    {
        auto const canvas = pinned ? m_pinnedLaneCanvas : m_scrollLaneCanvas;
        auto const position = args.GetPosition(canvas);
        auto const top = static_cast<float>(row.Top - (pinned ? 0.0 : m_scrollY));
        auto const height = static_cast<float>(row.Height);

        if (row.Kind == seq::ArrangeRowKind::Tempo)
        {
            auto const index = HitTestTempoPoint(top, height, position);

            if (index == SIZE_MAX)
            {
                return false;
            }

            ClearAutomationSelection();
            m_selectedTempoPoint = index;
            InvalidateArrange();
            ShowTempoPointMenu(index, canvas, position);
            args.Handled(true);
            return true;
        }

        auto const trackId = row.TrackId;
        auto const laneId = row.LaneId;
        auto const track = seq::FindTrack(m_doc, trackId);
        auto const lane = track != nullptr ? seq::FindLane(track->Lanes, laneId) : nullptr;

        args.Handled(true);

        if (lane == nullptr)
        {
            return true;
        }

        SelectTrack(trackId);

        auto const index = HitTestLanePoint(*lane, top, height, position);

        if (index != SIZE_MAX)
        {
            m_selectedLaneTrackId = trackId;
            m_selectedLaneId = laneId;
            m_selectedLanePoint = index;
            m_selectedTempoPoint = SIZE_MAX;
            InvalidateArrange();
            ShowLanePointMenu(trackId, laneId, index, canvas, position);
            return true;
        }

        seq::LanePoint at{};
        at.Tick = seq::SnapTick(TickAtLaneX(position.X), SnapGrid());
        at.Value = seq::LaneValueAtY(position.Y, top, height);
        at.Hold = lane->Target == seq::LaneTarget::Program;

        ShowLaneRowMenu(trackId, laneId, at, canvas, position);
        return true;
    }
}
