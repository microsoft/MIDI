// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

// Play chords and Default output (Key and chords proposal, revision 2, section 1). Pete, November 1
// 2026: hearing the chords "would really help new musicians right from the start", if it's super
// simple. So there's nothing to set up on the row: the chords play on Default output from Settings,
// which is the General MIDI Synth until someone picks something else.
//
// The engine plays tracks, so while Play chords is on it's handed the sequence with one more track,
// which nobody sees, holding the chords as notes (Harmony.h's ChordNotes) on Default output.

#include "pch.h"
#include "MainWindow.xaml.h"

#include "Harmony.h"
#include "HarmonyFollow.h"
#include "StringResources.h"

namespace res = ::midisequencer::resources;

namespace winrt::midisequencer::implementation
{
    namespace
    {
        constexpr wchar_t ChordsTrackId[] = L"key-and-chords-playback";
        constexpr wchar_t ChordsClipId[] = L"key-and-chords-playback-clip";

        // Softer than most parts, so the chords sit behind what's played over them.
        constexpr uint16_t ChordVelocity = 0x6000;
    }

    seq::EndpointRef MainWindow::DefaultOutput() const
    {
        auto const& settings = seq::AppSettings::Current();

        if (!settings.DefaultOutputName().empty() || !settings.DefaultOutputId().empty())
        {
            return seq::EndpointRef{ settings.DefaultOutputName(), settings.DefaultOutputId() };
        }

        if (auto const synth = m_directory->Resolve(seq::EndpointRef{ L"", m_directory->SynthEndpointId() }); synth.has_value())
        {
            return seq::EndpointDirectory::MakeRef(synth->Live);
        }

        return seq::EndpointRef{};
    }

    _Use_decl_annotations_
    bool MainWindow::IsSynthEndpoint(seq::EndpointRef const& endpoint) const
    {
        auto const synthId = m_directory->SynthEndpointId();

        if (endpoint.IsEmpty() || synthId.empty())
        {
            return false;
        }

        if (!endpoint.Id.empty() && _wcsicmp(endpoint.Id.c_str(), synthId.c_str()) == 0)
        {
            return true;
        }

        // Saved by name, or by an id from another PC: what it is here.
        if (auto const resolved = m_directory->Resolve(endpoint); resolved.has_value())
        {
            auto const id = seq::EndpointDirectory::MakeRef(resolved->Live).Id;
            return _wcsicmp(id.c_str(), synthId.c_str()) == 0;
        }

        return false;
    }

    seq::EndpointRef MainWindow::ChordsDestination() const
    {
        if (!seq::AppSettings::Current().PlayChords() || !m_doc.Harmony || m_doc.Harmony->Hidden || m_doc.Harmony->Chords.empty())
        {
            return seq::EndpointRef{};
        }

        return DefaultOutput();
    }

    std::shared_ptr<seq::Sequence const> MainWindow::PlayableSequence() const
    {
        auto const destination = ChordsDestination();

        if (destination.IsEmpty())
        {
            return std::make_shared<seq::Sequence const>(m_doc);
        }

        auto playable = std::make_shared<seq::Sequence>(m_doc);

        auto clip = seq::ChordsAsNotes(m_doc.Harmony->Chords, ChordVelocity);
        clip.Id = ChordsClipId;
        auto const start = m_doc.Harmony->Chords.front().Tick;

        seq::Track track{};
        track.Id = ChordsTrackId;
        track.Name = std::wstring{ res::GetString(L"KeyAndChordsRowName") };
        track.Destination.Endpoint = destination;
        track.Destination.Group = 0;
        track.Destination.Channel = 0;
        track.Timeline.push_back(seq::Placement{ clip.Id, start, clip.Length });

        playable->Clips.push_back(std::move(clip));
        playable->Tracks.push_back(std::move(track));
        return playable;
    }

    void MainWindow::CopyChordsToNewTrack() noexcept
    {
        try
        {
            if (!m_doc.Harmony || m_doc.Harmony->Chords.empty())
            {
                return;
            }

            auto const& chords = m_doc.Harmony->Chords;
            auto clip = seq::ChordsAsNotes(chords, ChordVelocity);
            clip.Id = seq::NewId(L"c");
            clip.Origin = seq::ClipOrigin::Drawn;

            // Named after its first and last chords, spelled in their keys: "Am – E7".
            auto const nameOf = [this](seq::ChordEvent const& chord)
            {
                return seq::ChordName(chord, seq::KeyAt(*m_doc.Harmony, chord.Tick));
            };

            clip.Name = chords.size() == 1 ? nameOf(chords.front()) : nameOf(chords.front()) + L" \u2013 " + nameOf(chords.back());

            // On Default output, the next free channel there. From here on it's plain MIDI: it
            // doesn't change when the chords do.
            auto track = seq::MakeTrack(m_doc, seq::NextName(m_doc, std::wstring{ res::GetString(L"ChordsTrackName") }), false);
            track.Slots.resize(m_doc.Scenes.size());

            if (auto const output = DefaultOutput(); !output.IsEmpty())
            {
                track.Destination.Endpoint = output;
                track.Destination.Group = 0;
                track.Destination.Channel = seq::NextFreeChannel(m_doc, output, 0);
            }

            track.Timeline.push_back(seq::Placement{ clip.Id, chords.front().Tick, 0 });

            auto const trackId = track.Id;
            auto const tracksBefore = m_doc.Tracks;

            seq::ChangeList changes{};
            changes.push_back(seq::MakeClipPresenceChange(clip, true));
            m_doc.Clips.push_back(std::move(clip));
            m_doc.Tracks.push_back(std::move(track));
            changes.push_back(seq::MakeTracksChange(tracksBefore, m_doc.Tracks));

            Commit(std::wstring{ res::GetString(L"UndoCopyChordsToTrack") }, std::move(changes));
            SelectTrack(trackId);
            PrepareConnectionsAsync();
        }
        MIDI_SEQUENCER_CATCH_AND_LOG(L"Unable to copy the chords to a new track.")
    }

    xaml::UIElement MainWindow::MakeDefaultOutputSetting()
    {
        controls::ComboBox box{};
        box.Header(winrt::box_value(res::GetString(L"DefaultOutputHeader")));
        box.MinWidth(240);
        box.Margin(xaml::Thickness{ 0, 8, 0, 0 });
        automation::AutomationProperties::SetName(box, res::GetString(L"DefaultOutputHeader"));
        automation::AutomationProperties::SetHelpText(box, res::GetString(L"DefaultOutputHelp"));
        controls::ToolTipService::SetToolTip(box, winrt::box_value(res::GetString(L"DefaultOutputHelp")));

        // Everything that can be sent to, by name.
        auto refs = std::make_shared<std::vector<seq::EndpointRef>>();
        auto const current = DefaultOutput();
        int32_t selected{ -1 };

        for (auto const& summary : m_directory->Snapshot())
        {
            auto const sends = std::any_of(summary.Live.DestinationGroups.begin(), summary.Live.DestinationGroups.end(), [](bool on) { return on; });

            if (!sends)
            {
                continue;
            }

            auto const ref = seq::EndpointDirectory::MakeRef(summary.Live);

            if ((!current.Id.empty() && ref.Id == current.Id) || (current.Id.empty() && ref.Name == current.Name))
            {
                selected = static_cast<int32_t>(refs->size());
            }

            refs->push_back(ref);
            box.Items().Append(winrt::box_value(winrt::hstring{ summary.Live.Name }));
        }

        if (refs->empty())
        {
            box.Items().Append(winrt::box_value(res::GetString(L"DefaultOutputNone")));
            box.IsEnabled(false);
            box.SelectedIndex(0);
            return box;
        }

        box.SelectedIndex(selected);

        box.SelectionChanged([weak = get_weak(), refs](foundation::IInspectable const& sender, auto&&)
        {
            auto const index = sender.as<controls::ComboBox>().SelectedIndex();
            auto strong = weak.get();

            if (!strong || index < 0 || static_cast<size_t>(index) >= refs->size())
            {
                return;
            }

            auto const& ref = (*refs)[static_cast<size_t>(index)];
            seq::AppSettings::Current().DefaultOutput(ref.Name, ref.Id);
            strong->Publish();
        });

        return box;
    }
}
