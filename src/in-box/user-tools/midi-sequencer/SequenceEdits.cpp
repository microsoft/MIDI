// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#include "SequenceEdits.h"

#include <algorithm>
#include <format>
#include <random>
#include <unordered_set>

namespace midisequencer
{
    namespace
    {
        class ClipPresenceChange final : public SequenceChange
        {
        public:
            ClipPresenceChange(_In_ Clip clip, _In_ bool added) : m_clip(std::move(clip)), m_added(added)
            {
            }

            void Apply(_Inout_ Sequence& sequence) override
            {
                if (m_added) { Add(sequence); } else { Remove(sequence); }
            }

            void Revert(_Inout_ Sequence& sequence) override
            {
                if (m_added) { Remove(sequence); } else { Add(sequence); }
            }

            size_t Cost() const noexcept override
            {
                return sizeof(*this) + m_clip.Notes.size() * sizeof(Note) + m_clip.Events.size() * sizeof(ClipEvent);
            }

        private:
            void Add(_Inout_ Sequence& sequence)
            {
                if (FindClip(sequence, m_clip.Id) == nullptr)
                {
                    sequence.Clips.push_back(m_clip);
                }
            }

            void Remove(_Inout_ Sequence& sequence)
            {
                std::erase_if(sequence.Clips, [this](Clip const& clip) { return clip.Id == m_clip.Id; });
            }

            Clip m_clip{};
            bool m_added{ true };
        };

        // A clip without its notes and events: what a settings change restores.
        struct ClipSettings
        {
            std::wstring Name{};
            std::optional<uint32_t> Color{};
            int64_t Length{ 0 };
            bool Loop{ true };
            uint32_t Seed{ 0 };
            ClipKind Kind{ ClipKind::Notes };
            winrt::Windows::Data::Json::JsonObject Settings{ nullptr };

            static ClipSettings From(_In_ Clip const& clip)
            {
                ClipSettings settings{};
                settings.Name = clip.Name;
                settings.Color = clip.Color;
                settings.Length = clip.Length;
                settings.Loop = clip.Loop;
                settings.Seed = clip.Seed;
                settings.Kind = clip.Kind;

                if (clip.Settings != nullptr)
                {
                    settings.Settings = winrt::Windows::Data::Json::JsonObject::Parse(clip.Settings.Stringify());
                }

                return settings;
            }

            void To(_Inout_ Clip& clip) const
            {
                clip.Name = Name;
                clip.Color = Color;
                clip.Length = Length;
                clip.Loop = Loop;
                clip.Seed = Seed;
                clip.Kind = Kind;
                clip.Settings = Settings == nullptr ? nullptr : winrt::Windows::Data::Json::JsonObject::Parse(Settings.Stringify());
            }
        };

        class ClipSettingsChange final : public SequenceChange
        {
        public:
            ClipSettingsChange(_In_ std::wstring id, _In_ ClipSettings before, _In_ ClipSettings after) :
                m_id(std::move(id)), m_before(std::move(before)), m_after(std::move(after))
            {
            }

            void Apply(_Inout_ Sequence& sequence) override
            {
                if (auto clip = FindClip(sequence, m_id); clip != nullptr) { m_after.To(*clip); }
            }

            void Revert(_Inout_ Sequence& sequence) override
            {
                if (auto clip = FindClip(sequence, m_id); clip != nullptr) { m_before.To(*clip); }
            }

            size_t Cost() const noexcept override { return sizeof(*this) + 256; }

        private:
            std::wstring m_id{};
            ClipSettings m_before{};
            ClipSettings m_after{};
        };

        class ClipEventsChange final : public SequenceChange
        {
        public:
            ClipEventsChange(_In_ std::wstring id, _In_ std::vector<ClipEvent> before, _In_ std::vector<ClipEvent> after) :
                m_id(std::move(id)), m_before(std::move(before)), m_after(std::move(after))
            {
            }

            void Apply(_Inout_ Sequence& sequence) override
            {
                if (auto clip = FindClip(sequence, m_id); clip != nullptr) { clip->Events = m_after; }
            }

            void Revert(_Inout_ Sequence& sequence) override
            {
                if (auto clip = FindClip(sequence, m_id); clip != nullptr) { clip->Events = m_before; }
            }

            size_t Cost() const noexcept override
            {
                return sizeof(*this) + (m_before.size() + m_after.size()) * sizeof(ClipEvent);
            }

        private:
            std::wstring m_id{};
            std::vector<ClipEvent> m_before{};
            std::vector<ClipEvent> m_after{};
        };

        size_t TreeCost(_In_ std::vector<Track> const& tracks) noexcept
        {
            size_t cost{ 0 };

            for (auto const& track : tracks)
            {
                cost += sizeof(Track) + track.Timeline.size() * sizeof(Placement) + track.Slots.size() * 48 +
                    track.Tags.size() * sizeof(Tag) + track.Startup.size() * sizeof(ClipEvent) + TreeCost(track.Children);
            }

            return cost;
        }

        bool LocateIn(
            _In_ std::vector<Track>& tracks,
            _In_ Track* parent,
            _In_ std::wstring_view id,
            _Out_ TrackLocation& location) noexcept
        {
            for (size_t i = 0; i < tracks.size(); ++i)
            {
                if (tracks[i].Id == id)
                {
                    location.Container = &tracks;
                    location.Index = i;
                    location.Parent = parent;
                    return true;
                }

                if (tracks[i].IsFolder && LocateIn(tracks[i].Children, &tracks[i], id, location))
                {
                    return true;
                }
            }

            return false;
        }

        void CollectUsedClips(_In_ std::vector<Track> const& tracks, _Inout_ std::unordered_set<std::wstring>& used)
        {
            for (auto const& track : tracks)
            {
                for (auto const& placement : track.Timeline)
                {
                    used.insert(placement.ClipId);
                }

                for (auto const& slot : track.Slots)
                {
                    if (!slot.empty())
                    {
                        used.insert(slot);
                    }
                }

                CollectUsedClips(track.Children, used);
            }
        }

        uint32_t NewSeed()
        {
            static thread_local std::mt19937 generator{ std::random_device{}() };
            return static_cast<uint32_t>(generator() & 0xFFFF);
        }
    }

    _Use_decl_annotations_
    std::unique_ptr<SequenceChange> MakeClipPresenceChange(Clip clip, bool added)
    {
        return std::make_unique<ClipPresenceChange>(std::move(clip), added);
    }

    _Use_decl_annotations_
    std::unique_ptr<SequenceChange> MakeTracksChange(std::vector<Track> before, std::vector<Track> after)
    {
        auto const cost = TreeCost(before) + TreeCost(after);

        return std::make_unique<SnapshotChange<std::vector<Track>>>(
            [](Sequence& sequence) { return &sequence.Tracks; }, std::move(before), std::move(after), cost);
    }

    _Use_decl_annotations_
    std::unique_ptr<SequenceChange> MakeClipSettingsChange(Clip const& before, Clip const& after)
    {
        return std::make_unique<ClipSettingsChange>(before.Id, ClipSettings::From(before), ClipSettings::From(after));
    }

    _Use_decl_annotations_
    std::unique_ptr<SequenceChange> MakeClipEventsChange(std::wstring clipId, std::vector<ClipEvent> before, std::vector<ClipEvent> after)
    {
        return std::make_unique<ClipEventsChange>(std::move(clipId), std::move(before), std::move(after));
    }

    _Use_decl_annotations_
    std::unique_ptr<SequenceChange> MakeTempoChange(std::vector<TempoPoint> before, std::vector<TempoPoint> after)
    {
        auto const cost = (before.size() + after.size()) * sizeof(TempoPoint);
        return std::make_unique<SnapshotChange<std::vector<TempoPoint>>>(
            [](Sequence& sequence) { return &sequence.Tempo; }, std::move(before), std::move(after), cost);
    }

    _Use_decl_annotations_
    std::unique_ptr<SequenceChange> MakeMeterChange(std::vector<MeterChange> before, std::vector<MeterChange> after)
    {
        auto const cost = (before.size() + after.size()) * sizeof(MeterChange);
        return std::make_unique<SnapshotChange<std::vector<MeterChange>>>(
            [](Sequence& sequence) { return &sequence.Meter; }, std::move(before), std::move(after), cost);
    }

    _Use_decl_annotations_
    std::unique_ptr<SequenceChange> MakeSequenceTagsChange(std::vector<Tag> before, std::vector<Tag> after)
    {
        auto const cost = (before.size() + after.size()) * sizeof(Tag);
        return std::make_unique<SnapshotChange<std::vector<Tag>>>(
            [](Sequence& sequence) { return &sequence.Tags; }, std::move(before), std::move(after), cost);
    }

    _Use_decl_annotations_
    std::unique_ptr<SequenceChange> MakeScenesChange(std::vector<Scene> before, std::vector<Scene> after)
    {
        auto const cost = (before.size() + after.size()) * sizeof(Scene);
        return std::make_unique<SnapshotChange<std::vector<Scene>>>(
            [](Sequence& sequence) { return &sequence.Scenes; }, std::move(before), std::move(after), cost);
    }

    _Use_decl_annotations_
    std::unique_ptr<SequenceChange> MakeNameChange(std::wstring before, std::wstring after)
    {
        return std::make_unique<SnapshotChange<std::wstring>>(
            [](Sequence& sequence) { return &sequence.Name; }, std::move(before), std::move(after), 128);
    }

    _Use_decl_annotations_
    TrackLocation LocateTrack(Sequence& sequence, std::wstring_view id) noexcept
    {
        TrackLocation location{};
        LocateIn(sequence.Tracks, nullptr, id, location);
        return location;
    }

    _Use_decl_annotations_
    Track const* ParentFolder(Sequence const& sequence, std::wstring_view id) noexcept
    {
        auto location = LocateTrack(const_cast<Sequence&>(sequence), id);
        return location.Parent;
    }

    _Use_decl_annotations_
    bool IsInside(Sequence const& sequence, std::wstring_view candidateId, std::wstring_view folderId) noexcept
    {
        if (candidateId == folderId)
        {
            return true;
        }

        auto const folder = FindTrack(sequence, folderId);

        if (folder == nullptr || !folder->IsFolder)
        {
            return false;
        }

        bool inside{ false };

        auto const visit = [&](auto const& self, std::vector<Track> const& tracks) -> void
        {
            for (auto const& track : tracks)
            {
                if (track.Id == candidateId)
                {
                    inside = true;
                    return;
                }

                self(self, track.Children);
            }
        };

        visit(visit, folder->Children);
        return inside;
    }

    _Use_decl_annotations_
    Track MakeTrack(Sequence const& sequence, std::wstring name, bool folder)
    {
        Track track{};
        track.Id = NewId(folder ? L"f" : L"t");
        track.IsFolder = folder;
        track.Name = std::move(name);

        auto const& swatches = TrackColorSwatches();
        track.Color = swatches[CountTracks(sequence) % swatches.size()];

        if (!folder)
        {
            track.Slots.resize(sequence.Scenes.size());
            track.Destination.Channel = 0;
        }

        return track;
    }

    _Use_decl_annotations_
    void InsertTrackAfter(Sequence& sequence, std::wstring_view afterId, Track track)
    {
        if (!track.IsFolder)
        {
            track.Slots.resize(sequence.Scenes.size());
        }

        auto location = afterId.empty() ? TrackLocation{} : LocateTrack(sequence, afterId);

        if (location.Container == nullptr)
        {
            sequence.Tracks.push_back(std::move(track));
            return;
        }

        auto& anchor = (*location.Container)[location.Index];

        // After an open folder means inside it, at the end, which is where a person looking at the
        // rows expects a new track to appear.
        if (anchor.IsFolder && anchor.Open)
        {
            anchor.Children.push_back(std::move(track));
            return;
        }

        location.Container->insert(location.Container->begin() + static_cast<ptrdiff_t>(location.Index + 1), std::move(track));
    }

    _Use_decl_annotations_
    bool MoveTrackToFolder(Sequence& sequence, std::wstring_view trackId, std::wstring_view folderId)
    {
        if (!folderId.empty() && IsInside(sequence, folderId, trackId))
        {
            return false;
        }

        if (!folderId.empty())
        {
            auto const folder = FindTrack(sequence, folderId);

            if (folder == nullptr || !folder->IsFolder)
            {
                return false;
            }
        }

        auto removed = RemoveTrack(sequence, trackId);

        if (!removed.has_value())
        {
            return false;
        }

        if (folderId.empty())
        {
            sequence.Tracks.push_back(std::move(*removed));
            return true;
        }

        auto folder = FindTrack(sequence, folderId);

        if (folder == nullptr)
        {
            sequence.Tracks.push_back(std::move(*removed));
            return false;
        }

        folder->Children.push_back(std::move(*removed));
        return true;
    }

    _Use_decl_annotations_
    bool MoveTrackBy(Sequence& sequence, std::wstring_view trackId, int32_t delta)
    {
        auto location = LocateTrack(sequence, trackId);

        if (location.Container == nullptr || delta == 0)
        {
            return false;
        }

        auto const target = static_cast<int64_t>(location.Index) + delta;

        if (target < 0 || target >= static_cast<int64_t>(location.Container->size()))
        {
            return false;
        }

        std::swap((*location.Container)[location.Index], (*location.Container)[static_cast<size_t>(target)]);
        return true;
    }

    _Use_decl_annotations_
    std::optional<Track> RemoveTrack(Sequence& sequence, std::wstring_view trackId)
    {
        auto location = LocateTrack(sequence, trackId);

        if (location.Container == nullptr)
        {
            return std::nullopt;
        }

        auto track = std::move((*location.Container)[location.Index]);
        location.Container->erase(location.Container->begin() + static_cast<ptrdiff_t>(location.Index));
        return track;
    }

    _Use_decl_annotations_
    std::vector<Clip> RemoveUnusedClips(Sequence& sequence)
    {
        std::unordered_set<std::wstring> used{};
        CollectUsedClips(sequence.Tracks, used);

        std::vector<Clip> removed{};

        for (auto it = sequence.Clips.begin(); it != sequence.Clips.end();)
        {
            if (!used.contains(it->Id))
            {
                removed.push_back(std::move(*it));
                it = sequence.Clips.erase(it);
            }
            else
            {
                ++it;
            }
        }

        return removed;
    }

    _Use_decl_annotations_
    Clip MakeNotesClip(std::wstring name, int64_t length)
    {
        Clip clip{};
        clip.Id = NewId(L"c");
        clip.Kind = ClipKind::Notes;
        clip.Name = std::move(name);
        clip.Length = std::max<int64_t>(1, length);
        clip.Loop = true;
        clip.Seed = NewSeed();
        clip.Origin = ClipOrigin::Drawn;
        return clip;
    }

    std::array<uint32_t, 10> const& TrackColorSwatches() noexcept
    {
        static constexpr std::array<uint32_t, 10> swatches{
            0xFF8A65, 0xF7C948, 0xFFB547, 0x6CCB5F, 0x4DD0C4, 0x5BC0EB, 0x9C8CFF, 0xB4A7FF, 0xFF6F91, 0xBDBDBD };

        return swatches;
    }

    _Use_decl_annotations_
    int8_t NextFreeChannel(Sequence const& sequence, EndpointRef const& endpoint, uint8_t group) noexcept
    {
        std::array<bool, 16> used{};

        try
        {
            ForEachTrack(sequence, [&](Track const& track, size_t)
            {
                if (!track.IsFolder && track.Destination.Group == group && track.Destination.Channel >= 0 &&
                    track.Destination.Endpoint.Name == endpoint.Name)
                {
                    used[static_cast<size_t>(track.Destination.Channel & 0x0F)] = true;
                }

                return true;
            });
        }
        catch (...)
        {
        }

        for (int8_t channel = 0; channel < 16; ++channel)
        {
            // Channel 10 is drums in General MIDI, so it's kept for a track that asks for it.
            if (channel != 9 && !used[static_cast<size_t>(channel)])
            {
                return channel;
            }
        }

        return 0;
    }

    _Use_decl_annotations_
    std::wstring NextName(Sequence const& sequence, std::wstring_view stem)
    {
        std::unordered_set<std::wstring> names{};

        ForEachTrack(sequence, [&names](Track const& track, size_t)
        {
            names.insert(track.Name);
            return true;
        });

        for (size_t number = 1; number < 100000; ++number)
        {
            auto candidate = std::format(L"{} {}", stem, number);

            if (!names.contains(candidate))
            {
                return candidate;
            }
        }

        return std::wstring{ stem };
    }

    _Use_decl_annotations_
    int64_t SnapTick(int64_t tick, int64_t grid) noexcept
    {
        if (grid <= 0)
        {
            return tick;
        }

        auto const below = (tick >= 0 ? tick / grid : (tick - grid + 1) / grid) * grid;
        return tick - below >= grid / 2 ? below + grid : below;
    }

    _Use_decl_annotations_
    std::vector<size_t> NotesInBox(Clip const& clip, int64_t fromTick, int64_t toTick, uint8_t lowNote, uint8_t highNote)
    {
        std::vector<size_t> found{};

        auto first = std::lower_bound(clip.Notes.begin(), clip.Notes.end(), fromTick,
            [](Note const& note, int64_t tick) { return note.Tick < tick; });

        for (auto it = first; it != clip.Notes.end() && it->Tick < toTick; ++it)
        {
            if (it->Number >= lowNote && it->Number <= highNote)
            {
                found.push_back(static_cast<size_t>(std::distance(clip.Notes.begin(), it)));
            }
        }

        return found;
    }
}
