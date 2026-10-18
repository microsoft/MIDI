// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

// The mixer: a strip for each track with its volume, pan and sends, which are the controllers a
// General MIDI synth and most others listen to. A control shows the track's lane at the playhead
// when it has one, and its start-up value otherwise. Moving it sends the value straight away and
// keeps it: as the start-up value, as a point on the lane at the playhead, or, while the sequence
// plays and the track's W is on, written into the lane as it moves.

#include "pch.h"
#include "MainWindow.xaml.h"

#include "SequenceRender.h"
#include "StringResources.h"
#include "MixerSlots.h"

namespace res = ::midisequencer::resources;

namespace winrt::midisequencer::implementation
{
    using namespace mixer;

    namespace
    {
        // A change waits this long for the next before it's kept as one edit.
        constexpr auto CommitDelay = std::chrono::milliseconds{ 700 };

        // While a control is being moved, its lane doesn't move it back.
        constexpr auto TouchHold = std::chrono::milliseconds{ 1200 };

        // How often the engine gets what's being written, so it plays it rather than the old lane.
        constexpr auto PublishEvery = std::chrono::milliseconds{ 200 };

        // While writing, a point for each 64th note the playhead passes.
        constexpr int64_t WriteStep = seq::TicksPerQuarterNote / 16;

        // A recording is one stretch of its lane. When the playhead has moved on by more than
        // twice the time since the last point, and this much more, it jumped rather than played.
        constexpr double JumpSlackSeconds = 0.5;

        bool IsControllerMessage(seq::ClipEvent const& message, uint8_t controller) noexcept
        {
            auto const word0 = message.Words[0];
            auto const type = word0 >> 28;

            return (type == 0x2 || type == 0x4) && ((word0 >> 20) & 0x0F) == 0xB && ((word0 >> 8) & 0x7F) == controller;
        }

        // A start-up message that sets the controller, MIDI 1.0 or MIDI 2.0, as a 32-bit value.
        std::optional<uint32_t> StartupValue(seq::Track const& track, uint8_t controller) noexcept
        {
            for (auto const& message : track.Startup)
            {
                if (!IsControllerMessage(message, controller))
                {
                    continue;
                }

                if ((message.Words[0] >> 28) == 0x2)
                {
                    return seq::ScaleUp(message.Words[0] & 0x7F, 7, 32);
                }

                if (message.WordCount >= 2)
                {
                    return message.Words[1];
                }
            }

            return std::nullopt;
        }

        seq::ClipEvent ControllerMessage(uint8_t channel, uint8_t controller, uint32_t value) noexcept
        {
            seq::ClipEvent message{};
            message.Words[0] = 0x40000000u | (0xBu << 20) | (static_cast<uint32_t>(channel & 0x0F) << 16) | (static_cast<uint32_t>(controller & 0x7F) << 8);
            message.Words[1] = value;
            message.WordCount = 2;
            return message;
        }

        uint8_t TrackChannel(seq::Track const& track) noexcept
        {
            return static_cast<uint8_t>(std::max<int8_t>(0, track.Destination.Channel));
        }

        void SetStartupValue(seq::Track& track, uint8_t controller, uint32_t value)
        {
            for (auto& message : track.Startup)
            {
                if (IsControllerMessage(message, controller))
                {
                    message = ControllerMessage(static_cast<uint8_t>((message.Words[0] >> 16) & 0x0F), controller, value);
                    return;
                }
            }

            track.Startup.push_back(ControllerMessage(TrackChannel(track), controller, value));
        }

        seq::AutomationLane* ControllerLane(seq::Track& track, uint8_t controller) noexcept
        {
            for (auto& lane : track.Lanes)
            {
                if (lane.Target == seq::LaneTarget::Controller && lane.Number == controller)
                {
                    return &lane;
                }
            }

            return nullptr;
        }

        seq::AutomationLane const* ControllerLane(seq::Track const& track, uint8_t controller) noexcept
        {
            return ControllerLane(const_cast<seq::Track&>(track), controller);
        }
    }

    // ---------------------------------------------------------------- showing it

    bool MainWindow::MixerShown() noexcept
    {
        try
        {
            return MixerPanel().Visibility() == xaml::Visibility::Visible;
        }
        catch (...)
        {
            return false;
        }
    }

    _Use_decl_annotations_
    void MainWindow::ShowMixer(bool show) noexcept
    {
        try
        {
            if (!show)
            {
                CommitMixerEdit();
            }

            auto const visible = [](bool on) { return on ? xaml::Visibility::Visible : xaml::Visibility::Collapsed; };

            MixerPanel().Visibility(visible(show));
            EditorHeader().Visibility(visible(!show));
            EditorBody().Visibility(visible(!show));
            EditorPanel().Visibility(visible(show || !m_editorClipId.empty()));
            MixerToggle().IsChecked(show);
            seq::AppSettings::Current().ShowMixer(show);

            if (show)
            {
                RebuildMixer();
            }
            else
            {
                MixerStrips().Children().Clear();
                m_mixerControls.clear();
                m_mixerTrackIds.clear();
                m_mixerStrips.clear();
            }
        }
        MIDI_SEQUENCER_CATCH_AND_LOG(L"Unable to show or hide the mixer.")
    }

    _Use_decl_annotations_
    void MainWindow::OnMixerToggleClick(foundation::IInspectable const&, xaml::RoutedEventArgs const&)
    {
        ShowMixer(MixerToggle().IsChecked() != nullptr && MixerToggle().IsChecked().GetBoolean());
    }

    _Use_decl_annotations_
    void MainWindow::OnMixerCloseClick(foundation::IInspectable const&, xaml::RoutedEventArgs const&)
    {
        ShowMixer(false);
    }

    // ---------------------------------------------------------------- the strips

    void MainWindow::RebuildMixer() noexcept
    {
        try
        {
            if (!MixerShown())
            {
                return;
            }

            if (m_mixerCommitTimer == nullptr)
            {
                m_mixerCommitTimer = xaml::DispatcherTimer{};
                m_mixerCommitTimer.Interval(CommitDelay);
                m_mixerCommitTimer.Tick([weak = get_weak()](auto&&, auto&&)
                {
                    if (auto strong = weak.get())
                    {
                        strong->CommitMixerEdit();
                    }
                });
            }

            MixerStrips().Children().Clear();
            m_mixerControls.clear();
            m_mixerTrackIds.clear();
            m_mixerStrips.clear();

            seq::ForEachTrack(m_doc, [this](seq::Track const& track, size_t)
            {
                if (!track.IsFolder)
                {
                    MixerStrips().Children().Append(MakeMixerStrip(track));
                    m_mixerTrackIds.push_back(track.Id);
                }

                return true;
            });

            if (m_mixerTrackIds.empty())
            {
                controls::TextBlock empty{};
                empty.Text(res::GetString(L"MixerEmpty"));
                empty.Margin(xaml::Thickness{ 6, 6, 6, 6 });
                empty.Foreground(BrushFor(m_palette.Text3));
                MixerStrips().Children().Append(empty);
            }

            UpdateMixer(m_position);
        }
        MIDI_SEQUENCER_CATCH_AND_LOG(L"Unable to build the mixer.")
    }

    void MainWindow::RefreshMixer() noexcept
    {
        try
        {
            if (!MixerShown())
            {
                return;
            }

            // The same tracks in the same order keep their strips, so a control being moved isn't
            // taken away from under the pointer. Anything else builds them again.
            std::vector<std::wstring> ids{};

            seq::ForEachTrack(m_doc, [&ids](seq::Track const& track, size_t)
            {
                if (!track.IsFolder)
                {
                    ids.push_back(track.Id);
                }

                return true;
            });

            if (ids != m_mixerTrackIds || m_mixerStrips.size() != ids.size())
            {
                RebuildMixer();
                return;
            }

            for (auto const& strip : m_mixerStrips)
            {
                auto const track = seq::FindTrack(m_doc, strip.TrackId);

                if (track == nullptr)
                {
                    continue;
                }

                strip.Name.Text(winrt::hstring{ track->Name });
                strip.Destination.Text(winrt::hstring{ DescribeDestination(*track) });
                controls::ToolTipService::SetToolTip(strip.Destination, winrt::box_value(DestinationToolTip(*track)));
                ColorMixerPlate(strip, track->Color);
                strip.Mute.IsChecked(track->Muted);
                strip.Solo.IsChecked(track->Soloed);
                strip.Write.IsChecked(m_automationArmed.contains(track->Id));
            }

            UpdateMixer(m_position);
        }
        MIDI_SEQUENCER_CATCH_AND_LOG(L"Unable to refresh the mixer.")
    }

    _Use_decl_annotations_
    std::optional<uint32_t> MainWindow::MixerValue(seq::Track const& track, uint8_t controller, int64_t tick, bool& followsLane) const
    {
        followsLane = false;

        if (auto const lane = ControllerLane(track, controller); lane != nullptr && !lane->Points.empty())
        {
            followsLane = true;
            return PlayingLaneValue(track, *lane, tick);
        }

        return StartupValue(track, controller);
    }

    _Use_decl_annotations_
    void MainWindow::UpdateMixer(int64_t tick) noexcept
    {
        try
        {
            if (m_mixerControls.empty())
            {
                return;
            }

            auto const now = std::chrono::steady_clock::now();
            m_mixerUpdating = true;

            for (auto& control : m_mixerControls)
            {
                auto const track = seq::FindTrack(m_doc, control.TrackId);

                if (track == nullptr || control.Slider == nullptr)
                {
                    continue;
                }

                // Being moved: the person's hand wins.
                if (now - control.TouchedAt < TouchHold)
                {
                    continue;
                }

                auto followsLane{ false };
                auto const value = MixerValue(*track, control.Controller, tick, followsLane);
                auto const position = value.has_value() ? ToPosition(*value) : static_cast<double>(SlotFor(control.Controller).Standard);

                if (control.Slider.Value() != position)
                {
                    control.Slider.Value(position);
                }

                // A value the track never set shows where a synth starts, faded.
                control.Slider.Opacity(value.has_value() ? 1.0 : 0.55);

                if (control.Value != nullptr)
                {
                    // Pan still says where it is, faded; the volume says it has none.
                    auto const shown = value.has_value() || control.Controller == Pan.Controller;
                    winrt::hstring const text{ shown ? ValueText(control.Controller, position) : std::wstring{ L"\u2013" } };

                    if (control.Value.Text() != text)
                    {
                        control.Value.Text(text);
                    }

                    control.Value.Opacity(value.has_value() ? 1.0 : 0.55);
                }

                if (control.LaneMark != nullptr)
                {
                    control.LaneMark.Visibility(followsLane ? xaml::Visibility::Visible : xaml::Visibility::Collapsed);
                }
            }

            m_mixerUpdating = false;

            UpdateMixerMeters();
            UpdateMixerSelection();
        }
        catch (...)
        {
            m_mixerUpdating = false;
        }
    }

    // ---------------------------------------------------------------- sending a move

    _Use_decl_annotations_
    void MainWindow::SendMixerValue(seq::Track const& track, uint8_t controller, uint32_t value) noexcept
    {
        try
        {
            auto const now = std::chrono::steady_clock::now();
            auto const interval = std::chrono::milliseconds{ m_directory->Lookup(track.Destination.Endpoint, track.Destination.Group).AutomationIntervalMilliseconds };
            auto& send = m_mixerSends[{ track.Id, controller }];

            // Sooner than the device wants, so the newest value waits for its turn.
            if (interval.count() > 0 && now - send.SentAt < interval)
            {
                send.Waiting = value;
                ScheduleMixerSend(send.SentAt + interval);
                return;
            }

            send.SentAt = now;
            send.Waiting.reset();

            auto const message = ControllerMessage(TrackChannel(track), controller, value);
            SendNow(track.Destination, message.Words.data(), message.WordCount);
        }
        MIDI_SEQUENCER_CATCH_AND_LOG(L"Unable to send a mixer move.")
    }

    void MainWindow::SendWaitingMixerValues() noexcept
    {
        try
        {
            m_mixerSendDue = {};

            if (m_closing)
            {
                return;
            }

            auto const now = std::chrono::steady_clock::now();
            std::optional<std::chrono::steady_clock::time_point> next{};

            for (auto it = m_mixerSends.begin(); it != m_mixerSends.end();)
            {
                auto const& key = it->first;
                auto const track = seq::FindTrack(m_doc, key.first);

                if (track == nullptr || track->IsFolder)
                {
                    it = m_mixerSends.erase(it);
                    continue;
                }

                auto& send = it->second;

                if (send.Waiting.has_value())
                {
                    auto const interval = std::chrono::milliseconds{ m_directory->Lookup(track->Destination.Endpoint, track->Destination.Group).AutomationIntervalMilliseconds };
                    auto const due = send.SentAt + interval;

                    if (now >= due)
                    {
                        auto const message = ControllerMessage(TrackChannel(*track), key.second, *send.Waiting);

                        send.SentAt = now;
                        send.Waiting.reset();
                        SendNow(track->Destination, message.Words.data(), message.WordCount);
                    }
                    else
                    {
                        next = next.has_value() ? std::min(*next, due) : due;
                    }
                }

                ++it;
            }

            if (next.has_value())
            {
                ScheduleMixerSend(*next);
            }
        }
        MIDI_SEQUENCER_CATCH_AND_LOG(L"Unable to send a mixer move.")
    }

    _Use_decl_annotations_
    void MainWindow::ScheduleMixerSend(std::chrono::steady_clock::time_point due) noexcept
    {
        try
        {
            if (m_mixerSendTimer == nullptr)
            {
                m_mixerSendTimer = xaml::DispatcherTimer{};
                m_mixerSendTimer.Tick([weak = get_weak()](foundation::IInspectable const& sender, auto&&)
                {
                    sender.as<xaml::DispatcherTimer>().Stop();

                    if (auto strong = weak.get())
                    {
                        strong->SendWaitingMixerValues();
                    }
                });
            }

            // Already set to go off by then: it sends this one too.
            if (m_mixerSendTimer.IsEnabled() && m_mixerSendDue <= due)
            {
                return;
            }

            auto const wait = std::max<std::chrono::steady_clock::duration>(due - std::chrono::steady_clock::now(), std::chrono::milliseconds{ 1 });

            m_mixerSendDue = due;
            m_mixerSendTimer.Stop();
            m_mixerSendTimer.Interval(std::chrono::duration_cast<foundation::TimeSpan>(wait));
            m_mixerSendTimer.Start();
        }
        MIDI_SEQUENCER_CATCH_AND_LOG(L"Unable to send a mixer move.")
    }

    // ---------------------------------------------------------------- moving a control

    _Use_decl_annotations_
    void MainWindow::OnMixerValueChanged(std::wstring const& trackId, uint8_t controller, uint32_t value) noexcept
    {
        try
        {
            auto track = seq::FindTrack(m_doc, trackId);

            if (track == nullptr || track->IsFolder)
            {
                return;
            }

            auto const now = std::chrono::steady_clock::now();

            for (auto& control : m_mixerControls)
            {
                if (control.TrackId == trackId && control.Controller == controller)
                {
                    control.TouchedAt = now;
                    control.Slider.Opacity(1.0);

                    if (control.Value != nullptr)
                    {
                        control.Value.Text(winrt::hstring{ ValueText(controller, ToPosition(value)) });
                    }
                }
            }

            // Heard straight away, on the track's channel unless its destination sets one, but no
            // faster than the device's automation interval from MIDI Settings.
            SendMixerValue(*track, controller, value);

            if (m_readOnly)
            {
                return;
            }

            auto const playing = m_engine != nullptr && m_engine->IsPlaying();
            auto const writing = playing && m_automationArmed.contains(trackId);
            auto lane = ControllerLane(*track, controller);

            // While it plays without W, a lane plays over whatever's moved, so nothing is kept.
            if (playing && !writing && lane != nullptr && !lane->Points.empty())
            {
                return;
            }

            auto const kind = writing ? MixerEdit::Kind::Write : (lane != nullptr && !lane->Points.empty()) ? MixerEdit::Kind::Point : MixerEdit::Kind::Startup;

            // Writing goes on from the last place written. When the playhead goes back, round the
            // loop or to an earlier place, or jumps on further than it could have played since,
            // that's a new recording. Kept as one, it would replace the lane all the way between.
            auto const writeAt = kind == MixerEdit::Kind::Write ? (m_engine->PositionTick() / WriteStep) * WriteStep : int64_t{ -1 };
            auto jumped = false;

            if (kind == MixerEdit::Kind::Write && m_mixerEdit.What == MixerEdit::Kind::Write && !m_mixerEdit.Drawn.empty())
            {
                auto const lastAt = m_mixerEdit.Drawn.rbegin()->first;

                if (writeAt < lastAt)
                {
                    jumped = true;
                }
                else
                {
                    seq::TempoMap const tempo{ m_doc.Tempo };
                    auto const played = tempo.SecondsAtTick(writeAt) - tempo.SecondsAtTick(lastAt);
                    auto const waited = std::chrono::duration<double>(now - m_mixerEdit.WrittenAt).count();
                    jumped = played > 2.0 * waited + JumpSlackSeconds;
                }
            }

            // Another control, another kind of change, or a new recording is its own edit.
            if (m_mixerEdit.What != MixerEdit::Kind::None && (jumped || m_mixerEdit.What != kind || m_mixerEdit.TrackId != trackId || m_mixerEdit.Controller != controller))
            {
                CommitMixerEdit();
                track = seq::FindTrack(m_doc, trackId);
                lane = track != nullptr ? ControllerLane(*track, controller) : nullptr;

                if (track == nullptr)
                {
                    return;
                }
            }

            if (m_mixerEdit.What == MixerEdit::Kind::None)
            {
                m_mixerEdit = MixerEdit{};
                m_mixerEdit.What = kind;
                m_mixerEdit.TrackId = trackId;
                m_mixerEdit.Controller = controller;
                m_mixerEdit.TracksBefore = m_doc.Tracks;
                m_mixerEdit.PublishedAt = now;

                if (kind == MixerEdit::Kind::Write)
                {
                    // Written into the track's lane for this controller, made when there isn't one.
                    if (lane == nullptr)
                    {
                        track->Lanes.push_back(seq::MakeLane(seq::LaneTarget::Controller, controller, TrackChannel(*track)));
                        track->LanesOpen = true;
                        lane = &track->Lanes.back();
                    }

                    m_mixerEdit.LaneId = lane->Id;
                    m_mixerEdit.PointsBefore = lane->Points;
                }
            }

            switch (kind)
            {
            case MixerEdit::Kind::Startup:
                SetStartupValue(*track, controller, value);
                break;

            case MixerEdit::Kind::Point:
            {
                // The point at the playhead, made when there isn't one there.
                auto& points = lane->Points;
                auto const at = std::find_if(points.begin(), points.end(), [this](seq::LanePoint const& point) { return point.Tick == m_position; });

                if (at != points.end())
                {
                    at->Value = value;
                }
                else
                {
                    seq::AddLanePoint(*lane, seq::LanePoint{ m_position, value, false });
                }

                InvalidateArrange();
                break;
            }

            case MixerEdit::Kind::Write:
            {
                // Where the playhead is now, a point for each 64th note it passes, replacing what
                // the lane had there.
                lane = seq::FindLane(track->Lanes, m_mixerEdit.LaneId);

                if (lane == nullptr)
                {
                    break;
                }

                m_mixerEdit.Drawn[writeAt] = value;
                m_mixerEdit.WrittenAt = now;

                std::vector<seq::LanePoint> points{};
                points.reserve(m_mixerEdit.Drawn.size());

                for (auto const& [at, level] : m_mixerEdit.Drawn)
                {
                    points.push_back(seq::LanePoint{ at, level, false });
                }

                lane->Points = m_mixerEdit.PointsBefore;
                seq::ReplaceLanePoints(*lane, m_mixerEdit.Drawn.begin()->first, m_mixerEdit.Drawn.rbegin()->first, points);

                // The engine plays what's been written, not the lane as it was.
                if (now - m_mixerEdit.PublishedAt >= PublishEvery)
                {
                    m_mixerEdit.PublishedAt = now;
                    m_engine->SetSequence(std::make_shared<seq::Sequence const>(m_doc));
                }

                InvalidateArrange();
                break;
            }

            default:
                break;
            }

            // The track as this move left it, which is all the edit changes.
            if (auto const changed = seq::FindTrack(m_doc, trackId); changed != nullptr)
            {
                m_mixerEdit.TrackAfter = *changed;
            }

            m_mixerCommitTimer.Stop();
            m_mixerCommitTimer.Start();
        }
        MIDI_SEQUENCER_CATCH_AND_LOG(L"Unable to change a mixer control.")
    }

    bool MainWindow::RecordMixerEdit() noexcept
    {
        try
        {
            if (m_mixerCommitTimer != nullptr)
            {
                m_mixerCommitTimer.Stop();
            }

            if (m_mixerEdit.What == MixerEdit::Kind::None)
            {
                return false;
            }

            auto edit = std::move(m_mixerEdit);
            m_mixerEdit = MixerEdit{};

            auto const& slot = SlotFor(edit.Controller);
            auto const nameKey =
                edit.What == MixerEdit::Kind::Write ? L"UndoRecordAutomation" :
                edit.What == MixerEdit::Kind::Point ? L"UndoEditAutomation" : nullptr;

            std::wstring const name = nameKey != nullptr
                ? std::wstring{ res::GetString(nameKey) }
                : std::wstring{ res::FormatString(L"UndoMixerFormat", res::GetString(slot.NameKey)) };

            // The tracks as they were, with only this edit's track changed. Something else may
            // have changed the tracks since, and that's a step of its own, after this one.
            seq::Sequence after{};
            after.Tracks = edit.TracksBefore;

            if (auto const changed = seq::FindTrack(after, edit.TrackId); changed != nullptr && edit.TrackAfter.has_value())
            {
                *changed = std::move(*edit.TrackAfter);
            }
            else
            {
                after.Tracks = m_doc.Tracks;
            }

            seq::ChangeList changes{};
            changes.push_back(seq::MakeTracksChange(std::move(edit.TracksBefore), std::move(after.Tracks)));
            m_undo.Commit(name, std::move(changes));
            return true;
        }
        MIDI_SEQUENCER_CATCH_AND_LOG(L"Unable to keep a mixer change.")

        return false;
    }

    void MainWindow::CommitMixerEdit() noexcept
    {
        if (RecordMixerEdit())
        {
            DocumentChanged();
        }
    }

    void MainWindow::ReleaseMixerControls() noexcept
    {
        for (auto& control : m_mixerControls)
        {
            control.TouchedAt = std::chrono::steady_clock::time_point{};
        }
    }
}
