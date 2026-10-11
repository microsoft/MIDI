// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#include "RemoteControl.h"

#include <algorithm>

namespace midisequencer
{
    namespace
    {
        void CollectTracks(_In_ std::vector<Track> const& tracks, _Inout_ std::vector<std::wstring>& order)
        {
            for (auto const& track : tracks)
            {
                if (track.IsFolder)
                {
                    CollectTracks(track.Children, order);
                }
                else if (order.size() < RemoteMaximumTracks)
                {
                    order.push_back(track.Id);
                }
            }
        }

        uint32_t Midi1Word(uint8_t group, uint8_t status, uint8_t channel, uint8_t data1, uint8_t data2) noexcept
        {
            return 0x20000000u |
                (static_cast<uint32_t>(group & 0x0F) << 24) |
                (static_cast<uint32_t>(status & 0x0F) << 20) |
                (static_cast<uint32_t>(channel & 0x0F) << 16) |
                (static_cast<uint32_t>(data1 & 0x7F) << 8) |
                (data2 & 0x7Fu);
        }

        uint32_t Light(uint8_t group, uint8_t channel, uint8_t note, bool on) noexcept
        {
            return on ? Midi1Word(group, 0x9, channel, note, 127) : Midi1Word(group, 0x8, channel, note, 0);
        }

        uint32_t Switch(uint8_t group, uint8_t channel, uint8_t controller, bool on) noexcept
        {
            return Midi1Word(group, 0xB, channel, controller, on ? 127 : 0);
        }

        bool At(_In_ std::vector<bool> const& values, _In_ size_t index) noexcept
        {
            return index < values.size() && values[index];
        }
    }

    _Use_decl_annotations_
    std::pair<uint8_t, uint8_t> RemoteTrackAddress(size_t track) noexcept
    {
        return { static_cast<uint8_t>((track / RemoteTracksPerGroup) & 0x0F), static_cast<uint8_t>(track % RemoteTracksPerGroup) };
    }

    _Use_decl_annotations_
    std::optional<RemoteCommand> ParseRemoteMessage(uint32_t const* words, uint8_t wordCount) noexcept
    {
        if (words == nullptr || wordCount == 0)
        {
            return std::nullopt;
        }

        auto const word0 = words[0];
        auto const type = word0 >> 28;
        auto const group = static_cast<uint8_t>((word0 >> 24) & 0x0F);
        auto const status = (word0 >> 20) & 0x0F;
        auto const channel = static_cast<uint8_t>((word0 >> 16) & 0x0F);
        auto const number = static_cast<uint8_t>((word0 >> 8) & 0x7F);

        auto noteOn = false;
        auto controller = false;
        uint32_t value{ 0 };

        if (type == 0x2)
        {
            value = word0 & 0x7F;
            noteOn = status == 0x9 && value > 0;
            controller = status == 0xB;
        }
        else if (type == 0x4 && wordCount >= 2)
        {
            noteOn = status == 0x9 && (words[1] >> 16) > 0;
            controller = status == 0xB;
            value = words[1] >> 25;
        }

        if (!noteOn && !controller)
        {
            return std::nullopt;
        }

        if (channel == RemoteSequenceChannel)
        {
            // The sequence's own commands are on group 1 only.
            if (group != 0 || !noteOn)
            {
                return std::nullopt;
            }

            RemoteCommand command{};

            if (number < RemoteMaximumScenes)
            {
                command.Action = RemoteAction::LaunchScene;
                command.Scene = number;
                return command;
            }

            switch (number)
            {
            case RemoteNoteStop: command.Action = RemoteAction::StopAll; break;
            case RemoteNoteToTimeline: command.Action = RemoteAction::AllToTimeline; break;
            case RemoteNotePlay: command.Action = RemoteAction::Play; break;
            case RemoteNoteStopTransport: command.Action = RemoteAction::Stop; break;
            case RemoteNoteRecord: command.Action = RemoteAction::Record; break;
            case RemoteNoteLoop: command.Action = RemoteAction::Loop; break;
            case RemoteNoteMetronome: command.Action = RemoteAction::Metronome; break;
            case RemoteNoteGoToStart: command.Action = RemoteAction::GoToStart; break;
            default: return std::nullopt;
            }

            return command;
        }

        RemoteCommand command{};
        command.Track = static_cast<size_t>(group) * RemoteTracksPerGroup + channel;

        if (noteOn)
        {
            if (number < RemoteMaximumScenes)
            {
                command.Action = RemoteAction::LaunchSlot;
                command.Scene = number;
                return command;
            }

            if (number == RemoteNoteStop)
            {
                command.Action = RemoteAction::StopTrack;
                return command;
            }

            if (number == RemoteNoteToTimeline)
            {
                command.Action = RemoteAction::TrackToTimeline;
                return command;
            }

            return std::nullopt;
        }

        command.On = value >= 64;

        switch (number)
        {
        case RemoteControllerMute: command.Action = RemoteAction::Mute; break;
        case RemoteControllerSolo: command.Action = RemoteAction::Solo; break;
        case RemoteControllerArm: command.Action = RemoteAction::Arm; break;
        default: return std::nullopt;
        }

        return command;
    }

    _Use_decl_annotations_
    std::vector<std::wstring> RemoteTrackOrder(Sequence const& sequence)
    {
        std::vector<std::wstring> order{};
        CollectTracks(sequence.Tracks, order);
        return order;
    }

    _Use_decl_annotations_
    std::vector<uint32_t> RemoteFeedback(RemoteState const* before, RemoteState const& now)
    {
        std::vector<uint32_t> messages{};

        // A track or a scene that went away is turned off, so nothing stays lit for it.
        auto const tracks = std::max(now.SlotsLit.size(), before != nullptr ? before->SlotsLit.size() : size_t{ 0 });

        for (size_t track = 0; track < std::min(tracks, RemoteMaximumTracks); ++track)
        {
            auto const [group, channel] = RemoteTrackAddress(track);
            auto const& slots = track < now.SlotsLit.size() ? now.SlotsLit[track] : std::vector<bool>{};
            auto const* was = before != nullptr && track < before->SlotsLit.size() ? &before->SlotsLit[track] : nullptr;
            auto const scenes = std::max(slots.size(), was != nullptr ? was->size() : size_t{ 0 });

            for (size_t scene = 0; scene < std::min(scenes, RemoteMaximumScenes); ++scene)
            {
                auto const lit = At(slots, scene);

                if (before == nullptr || was == nullptr || At(*was, scene) != lit)
                {
                    messages.push_back(Light(group, channel, static_cast<uint8_t>(scene), lit));
                }
            }

            auto const sendSwitch = [&](std::vector<bool> const& nowValues, std::vector<bool> const* beforeValues, uint8_t number)
            {
                auto const on = At(nowValues, track);

                if (before == nullptr || beforeValues == nullptr || At(*beforeValues, track) != on)
                {
                    messages.push_back(Switch(group, channel, number, on));
                }
            };

            sendSwitch(now.Muted, before != nullptr ? &before->Muted : nullptr, RemoteControllerMute);
            sendSwitch(now.Soloed, before != nullptr ? &before->Soloed : nullptr, RemoteControllerSolo);
            sendSwitch(now.Armed, before != nullptr ? &before->Armed : nullptr, RemoteControllerArm);
        }

        auto const transport = [&](bool on, bool was, uint8_t note)
        {
            if (before == nullptr || was != on)
            {
                messages.push_back(Light(0, RemoteSequenceChannel, note, on));
            }
        };

        transport(now.Playing, before != nullptr && before->Playing, RemoteNotePlay);
        transport(now.Recording, before != nullptr && before->Recording, RemoteNoteRecord);
        transport(now.Loop, before != nullptr && before->Loop, RemoteNoteLoop);
        transport(now.Metronome, before != nullptr && before->Metronome, RemoteNoteMetronome);

        return messages;
    }
}
