// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

// The arpeggiator (design section 4, comp 4): its settings where the piano roll goes, Make notes,
// and the keys played on the source of a track whose arpeggiator plays what's held. Those keys go
// to the engine. While the arpeggiator plays them, they aren't echoed or recorded; its notes are.

#include "pch.h"
#include "MainWindow.xaml.h"

#include "SequenceArpeggiator.h"
#include "StringResources.h"

namespace res = ::midisequencer::resources;

namespace winrt::midisequencer::implementation
{
    namespace
    {
        constexpr std::array<wchar_t const*, 5> OrderKeys{ L"ArpOrderUp", L"ArpOrderDown", L"ArpOrderUpAndDown", L"ArpOrderAsHeld", L"ArpOrderRandom" };

        // No more notes than this wait for the window to record them.
        constexpr size_t MaximumWaitingNotes = 8192;

        // The same reach as Capture has for what's played on a source.
        constexpr size_t MaximumHeardNotes = 50000;
        constexpr double HeardSeconds = 120.0;

        bool SameEndpoint(std::wstring const& left, std::wstring const& right) noexcept
        {
            try
            {
                return midiapp::EndpointIdsMatch(winrt::hstring{ left }, winrt::hstring{ right });
            }
            catch (...)
            {
                return false;
            }
        }

        // The source's group and channels, and notes only: that's all an arpeggiator listens to.
        seq::RecordFilter KeysFilter(seq::TrackSource const& source) noexcept
        {
            seq::RecordFilter filter{};
            filter.Group = source.Group;
            filter.Channels = source.Channels;
            filter.Notes = true;
            filter.Controllers = false;
            filter.PitchBend = false;
            filter.Pressure = false;
            filter.Program = false;
            filter.SystemExclusive = false;
            return filter;
        }

        controls::TextBlock Text(winrt::hstring const& text, media::Brush const& brush, double size = 12)
        {
            controls::TextBlock block{};
            block.Text(text);
            block.FontSize(size);
            block.TextWrapping(xaml::TextWrapping::Wrap);
            block.Foreground(brush);
            return block;
        }

        // A radio button with a second, quieter line under its name, like comp 4.
        controls::RadioButton Choice(winrt::hstring const& name, controls::TextBlock const& detail)
        {
            controls::TextBlock label{};
            label.Text(name);
            label.FontSize(14);
            label.TextWrapping(xaml::TextWrapping::Wrap);

            controls::StackPanel content{};
            content.Children().Append(label);
            content.Children().Append(detail);

            controls::RadioButton button{};
            button.GroupName(L"ArpeggiatorSource");
            button.Content(content);
            automation::AutomationProperties::SetName(button, name);
            automation::AutomationProperties::SetHelpText(button, detail.Text());
            return button;
        }
    }

    // ---------------------------------------------------------------- settings

    _Use_decl_annotations_
    void MainWindow::AddArpeggiatorSettings(
        controls::StackPanel const& body,
        seq::GeneratorSettings const& generator,
        std::function<void(std::function<void(seq::GeneratorSettings&)>)> const& edit)
    {
        auto const secondary = BrushFor(m_palette.Text3);
        auto const held = generator.ArpSource == seq::ArpeggiatorSource::Held;
        auto const track = EditorTrack();

        // Where the notes come from on the left, and how they're played on the right.
        controls::StackPanel form{};
        form.Spacing(12);
        form.Width(400);

        controls::StackPanel play{};
        play.Spacing(12);
        play.MaxWidth(480);

        form.Children().Append(Text(res::GetString(L"ArpNotesFrom"), secondary));

        auto const sourceText = track != nullptr && !track->Source.Endpoint.IsEmpty()
            ? winrt::hstring{ DescribeSource(*track) }
            : res::GetString(L"ArpNoSource");

        auto fromHeld = Choice(res::GetString(L"ArpFromHeld"), Text(sourceText, secondary));
        fromHeld.IsChecked(held);
        form.Children().Append(fromHeld);

        auto fromTrack = Choice(res::GetString(L"ArpFromTrack"), Text(res::GetString(L"ArpFromTrackHelp"), secondary));
        fromTrack.IsChecked(!held);
        form.Children().Append(fromTrack);

        // The tracks it can follow: any track that plays notes, other than its own.
        std::vector<std::wstring> trackIds{};
        controls::ComboBox follow{};
        follow.Header(winrt::box_value(res::GetString(L"ArpTrackName")));
        follow.MinWidth(240);
        follow.Margin(xaml::Thickness{ 28, -4, 0, 0 });
        automation::AutomationProperties::SetName(follow, res::GetString(L"ArpTrackName"));

        int32_t followed{ -1 };

        seq::ForEachTrack(m_doc, [&](seq::Track const& candidate, size_t)
        {
            if (!candidate.IsFolder && candidate.Id != m_editorTrackId)
            {
                if (candidate.Id == generator.ArpTrackId)
                {
                    followed = static_cast<int32_t>(trackIds.size());
                }

                trackIds.push_back(candidate.Id);
                follow.Items().Append(winrt::box_value(winrt::hstring{ candidate.Name }));
            }

            return true;
        });

        follow.SelectedIndex(followed);
        follow.IsEnabled(!held && !trackIds.empty());
        form.Children().Append(follow);

        fromHeld.Checked([edit](auto&&, auto&&)
        {
            edit([](seq::GeneratorSettings& edited) { edited.ArpSource = seq::ArpeggiatorSource::Held; });
        });

        // Following another track starts with the first one when none was chosen.
        auto const firstTrack = trackIds.empty() ? std::wstring{} : trackIds.front();

        fromTrack.Checked([edit, firstTrack](auto&&, auto&&)
        {
            edit([firstTrack](seq::GeneratorSettings& edited)
            {
                edited.ArpSource = seq::ArpeggiatorSource::Track;

                if (edited.ArpTrackId.empty())
                {
                    edited.ArpTrackId = firstTrack;
                }
            });
        });

        follow.SelectionChanged([edit, trackIds](foundation::IInspectable const& sender, auto&&)
        {
            auto const index = sender.as<controls::ComboBox>().SelectedIndex();

            if (index >= 0 && static_cast<size_t>(index) < trackIds.size())
            {
                auto const id = trackIds[static_cast<size_t>(index)];
                edit([id](seq::GeneratorSettings& edited) { edited.ArpTrackId = id; });
            }
        });

        // The order, as a row of chips like the comp's segments.
        play.Children().Append(Text(res::GetString(L"ArpOrder"), secondary));

        controls::StackPanel orders{};
        orders.Orientation(controls::Orientation::Horizontal);
        orders.Spacing(6);

        auto const chipStyle = RootGrid().Resources().Lookup(winrt::box_value(L"ChipToggleStyle")).as<xaml::Style>();

        for (size_t i = 0; i < OrderKeys.size(); ++i)
        {
            primitives::ToggleButton chip{};
            chip.Style(chipStyle);
            chip.Content(winrt::box_value(res::GetString(OrderKeys[i])));
            chip.IsChecked(static_cast<size_t>(generator.ArpOrder) == i);
            automation::AutomationProperties::SetName(chip, res::GetString(OrderKeys[i]));
            automation::AutomationProperties::SetHelpText(chip, res::GetString(L"ArpOrder"));
            automation::AutomationProperties::SetPositionInSet(chip, static_cast<int32_t>(i) + 1);
            automation::AutomationProperties::SetSizeOfSet(chip, static_cast<int32_t>(OrderKeys.size()));

            // The panel is built again after the edit, which sets each chip's state.
            chip.Click([edit, i](auto&&, auto&&)
            {
                edit([i](seq::GeneratorSettings& edited) { edited.ArpOrder = static_cast<seq::ArpeggiatorOrder>(i); });
            });

            orders.Children().Append(chip);
        }

        play.Children().Append(orders);

        // Octaves and how long each note lasts.
        controls::StackPanel numbers{};
        numbers.Orientation(controls::Orientation::Horizontal);
        numbers.Spacing(18);

        auto const numberBox = [&](wchar_t const* headerKey, double minimum, double maximum, double value, std::function<void(seq::GeneratorSettings&, double)> apply)
        {
            controls::NumberBox box{};
            box.Header(winrt::box_value(res::GetString(headerKey)));
            automation::AutomationProperties::SetName(box, res::GetString(headerKey));
            box.Minimum(minimum);
            box.Maximum(maximum);
            box.SmallChange(1);
            box.LargeChange(10);
            box.Value(value);
            box.Width(170);
            box.SpinButtonPlacementMode(controls::NumberBoxSpinButtonPlacementMode::Compact);
            box.ValueChanged([edit, apply](controls::NumberBox const&, controls::NumberBoxValueChangedEventArgs const& args)
            {
                auto const newValue = args.NewValue();

                if (std::isfinite(newValue) && newValue != args.OldValue())
                {
                    edit([apply, newValue](seq::GeneratorSettings& edited) { apply(edited, newValue); });
                }
            });
            numbers.Children().Append(box);
        };

        numberBox(L"ArpOctaves", 1, seq::MaximumArpeggiatorOctaves, generator.ArpOctaves,
            [](seq::GeneratorSettings& edited, double value) { edited.ArpOctaves = static_cast<uint8_t>(std::clamp(std::lround(value), 1L, static_cast<long>(seq::MaximumArpeggiatorOctaves))); });

        numberBox(L"GeneratorNoteLength", 1, 1600, generator.NoteLength,
            [](seq::GeneratorSettings& edited, double value) { edited.NoteLength = static_cast<uint16_t>(std::clamp(std::lround(value), 1L, 1600L)); });

        play.Children().Append(numbers);

        // Latch, which only means something for keys that are held.
        controls::ToggleSwitch latch{};
        latch.Header(winrt::box_value(res::GetString(L"ArpLatch")));
        latch.IsOn(generator.ArpLatch);
        latch.IsEnabled(held);
        automation::AutomationProperties::SetName(latch, res::GetString(L"ArpLatch"));
        automation::AutomationProperties::SetHelpText(latch, res::GetString(L"ArpLatchHelp"));
        latch.Toggled([edit](foundation::IInspectable const& sender, auto&&)
        {
            auto const on = sender.as<controls::ToggleSwitch>().IsOn();
            edit([on](seq::GeneratorSettings& edited) { edited.ArpLatch = on; });
        });
        play.Children().Append(latch);

        auto latchHelp = Text(res::GetString(L"ArpLatchHelp"), secondary);
        latchHelp.Margin(xaml::Thickness{ 0, -8, 0, 0 });
        play.Children().Append(latchHelp);

        play.Children().Append(Text(res::GetString(L"ArpKeepsWhatItPlayed"), secondary, 11));

        controls::StackPanel columns{};
        columns.Orientation(controls::Orientation::Horizontal);
        columns.Spacing(40);
        columns.Children().Append(form);
        columns.Children().Append(play);
        body.Children().Append(columns);
    }

    // ---------------------------------------------------------------- Make notes

    void MainWindow::MakeNotesFromArpeggiator() noexcept
    {
        try
        {
            auto const clip = seq::FindClip(m_doc, m_editorClipId);

            if (clip == nullptr || !seq::IsArpeggiator(*clip))
            {
                return;
            }

            if (seq::PlaysHeldKeys(*clip))
            {
                ShowMessage(res::GetString(L"ArpHeldMakeNotes"));
                return;
            }

            auto const trackId = m_editorTrackId;
            auto const arpeggiator = *clip;
            auto const before = m_doc.Tracks;

            // Each place it plays on this track gets a notes clip of its own, because what it
            // plays depends on the other track's notes there.
            std::vector<std::pair<size_t, seq::Clip>> made{};

            if (auto const track = seq::FindTrack(m_doc, trackId); track != nullptr)
            {
                for (size_t i = 0; i < track->Timeline.size(); ++i)
                {
                    if (track->Timeline[i].ClipId == arpeggiator.Id)
                    {
                        made.emplace_back(i, seq::MakeNotesFromArpeggiator(m_doc, arpeggiator, track->Timeline[i]));
                    }
                }
            }

            if (made.empty())
            {
                ShowMessage(res::GetString(L"ArpNothingPlaced"));
                return;
            }

            seq::ChangeList changes{};
            auto const firstId = made.front().second.Id;
            auto track = seq::FindTrack(m_doc, trackId);

            for (auto& [index, notes] : made)
            {
                track->Timeline[index].ClipId = notes.Id;
                track->Timeline[index].Length = 0;
                changes.push_back(seq::MakeClipPresenceChange(notes, true));
                m_doc.Clips.push_back(std::move(notes));
            }

            changes.push_back(seq::MakeTracksChange(before, m_doc.Tracks));

            // The arpeggiator stays while a slot or another track uses it. Undo brings it back
            // either way.
            for (auto& unused : seq::RemoveUnusedClips(m_doc))
            {
                changes.push_back(seq::MakeClipPresenceChange(std::move(unused), false));
            }

            Commit(std::wstring{ res::GetString(L"UndoMakeNotes") }, std::move(changes));
            OpenClipInEditor(firstId, trackId);
        }
        MIDI_SEQUENCER_CATCH_AND_LOG(L"Unable to make notes from the arpeggiator.")
    }

    // ---------------------------------------------------------------- the keys

    void MainWindow::UpdateArpeggiatorRoutes() noexcept
    {
        try
        {
            std::vector<ArpeggiatorRoute> routes{};
            std::unordered_set<std::wstring> tracks{};

            seq::ForEachTrack(m_doc, [&](seq::Track const& track, size_t)
            {
                if (track.IsFolder || track.Source.Endpoint.IsEmpty() || !seq::TrackHasHeldKeysArpeggiator(m_doc, track))
                {
                    return true;
                }

                ArpeggiatorRoute route{};
                route.TrackId = track.Id;
                route.SourceId = m_directory->ResolveId(track.Source.Endpoint);
                route.Filter = KeysFilter(track.Source);

                if (!route.SourceId.empty())
                {
                    tracks.insert(track.Id);
                    routes.push_back(std::move(route));
                }

                return true;
            });

            // A track that stops listening forgets its keys, so one that went up unheard doesn't
            // stay down.
            if (m_engine != nullptr)
            {
                for (auto const& id : m_arpeggiatorTracks)
                {
                    if (!tracks.contains(id))
                    {
                        m_engine->ForgetKeys(id);
                    }
                }
            }

            m_arpeggiatorTracks = std::move(tracks);

            std::scoped_lock guard{ m_inputLock };
            m_arpeggiatorRoutes = std::move(routes);
        }
        MIDI_SEQUENCER_CATCH_AND_LOG(L"Unable to update what the arpeggiators listen to.")
    }

    _Use_decl_annotations_
    std::vector<std::wstring> MainWindow::FeedArpeggiators(std::wstring const& endpointId, uint32_t const* words, uint8_t wordCount) noexcept
    {
        // On the service's thread. Only notes, and the engine is told outside the input lock.
        std::vector<std::wstring> fed{};

        try
        {
            if (m_engine == nullptr || words == nullptr || wordCount == 0)
            {
                return fed;
            }

            auto const type = words[0] >> 28;
            auto const status = (words[0] >> 20) & 0x0F;

            if ((type != 0x2 && type != 0x4) || (status != 0x8 && status != 0x9) || (type == 0x4 && wordCount < 2))
            {
                return fed;
            }

            {
                std::scoped_lock guard{ m_inputLock };

                for (auto const& route : m_arpeggiatorRoutes)
                {
                    if (SameEndpoint(route.SourceId, endpointId) && seq::PassesRecordFilter(route.Filter, words, wordCount))
                    {
                        fed.push_back(route.TrackId);
                    }
                }
            }

            auto const number = static_cast<uint8_t>((words[0] >> 8) & 0x7F);

            // A MIDI 1.0 note on with velocity 0 is a note off.
            auto const velocity7 = words[0] & 0x7F;
            auto const on = status == 0x9 && (type == 0x4 || velocity7 > 0);
            auto const velocity = type == 0x4 ? static_cast<uint16_t>(words[1] >> 16) : static_cast<uint16_t>(seq::ScaleUp(velocity7, 7, 16));

            for (auto const& id : fed)
            {
                if (on)
                {
                    m_engine->PressKey(id, number, velocity);
                }
                else
                {
                    m_engine->ReleaseKey(id, number);
                }
            }
        }
        catch (...)
        {
        }

        return fed;
    }

    // On the UI thread, each frame: what the arpeggiators played goes where the keys would have
    // gone, into a take being recorded and into what Capture can reach back for.
    void MainWindow::RecordArpeggiatedNotes() noexcept
    {
        try
        {
            if (m_engine == nullptr)
            {
                return;
            }

            auto played = m_engine->TakeArpeggiated();

            if (played.empty())
            {
                return;
            }

            if (played.size() > MaximumWaitingNotes)
            {
                played.erase(played.begin(), played.end() - static_cast<ptrdiff_t>(MaximumWaitingNotes));
            }

            // Places on the timeline, worked out before the input lock: the engine has its own.
            std::vector<int64_t> ticks(played.size(), -1);
            std::vector<int64_t> slotTicks(played.size(), -1);

            for (size_t i = 0; i < played.size(); ++i)
            {
                ticks[i] = m_recording ? m_engine->TickAtTime(played[i].Timestamp) : -1;
                slotTicks[i] = m_slotRecording ? m_engine->UnwrappedTickAtTime(played[i].Timestamp) : -1;
            }

            std::scoped_lock guard{ m_inputLock };

            for (size_t i = 0; i < played.size(); ++i)
            {
                auto const& note = played[i];

                if (!m_armed.contains(note.TrackId))
                {
                    continue;
                }

                if (ticks[i] >= 0)
                {
                    if (auto take = m_takes.find(note.TrackId); take != m_takes.end())
                    {
                        take->second.Add(ticks[i], note.Words.data(), note.WordCount);
                    }
                }

                if (slotTicks[i] >= 0 && m_slotInput.has_value() && m_slotInput->TrackId == note.TrackId &&
                    slotTicks[i] >= m_slotInput->Start && slotTicks[i] < m_slotInput->Start + m_slotInput->Length)
                {
                    m_slotInput->Take.Add(slotTicks[i], note.Words.data(), note.WordCount);
                }

                // Kept in time order: they're handed over a little ahead of when they're heard.
                auto& heard = m_heard[note.TrackId];

                HeardMessage message{};
                message.Timestamp = note.Timestamp;
                message.Words = note.Words;
                message.WordCount = note.WordCount;

                auto at = heard.end();

                while (at != heard.begin() && std::prev(at)->Timestamp > message.Timestamp)
                {
                    --at;
                }

                heard.insert(at, message);

                // A latched arpeggiator plays on with no keys coming in to trim this.
                auto const frequency = static_cast<double>(midi2::MidiClock::TimestampFrequency());

                while (heard.size() > MaximumHeardNotes ||
                    (!heard.empty() && static_cast<double>(heard.back().Timestamp - heard.front().Timestamp) / frequency > HeardSeconds))
                {
                    heard.pop_front();
                }
            }
        }
        MIDI_SEQUENCER_CATCH_AND_LOG(L"Unable to record what the arpeggiators played.")
    }
}
