// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#include "SequenceEdits.h"
#include "GeneratorKinds.h"
#include "SequenceArpeggiator.h"
#include "SequenceGenerator.h"
#include "SequencePattern.h"

#include <algorithm>
#include <cmath>
#include <format>
#include <iterator>
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
            std::wstring SeedWords{};
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
                settings.SeedWords = clip.SeedWords;
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
                clip.SeedWords = SeedWords;
                clip.Kind = Kind;
                clip.Settings = Settings == nullptr ? nullptr : winrt::Windows::Data::Json::JsonObject::Parse(Settings.Stringify());

                // A generator's drawn notes depend on its length and seed.
                RefreshGeneratorPreview(clip);
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

        size_t LanesCost(_In_ std::vector<AutomationLane> const& lanes) noexcept
        {
            size_t cost{ 0 };

            for (auto const& lane : lanes)
            {
                cost += sizeof(AutomationLane) + lane.Points.size() * sizeof(LanePoint);
            }

            return cost;
        }

        size_t PatternCost(_In_ PatternSettings const& pattern) noexcept
        {
            size_t cost{ sizeof(PatternSettings) };

            for (auto const& lane : pattern.Lanes)
            {
                cost += sizeof(PatternLane) + lane.Steps.size() * sizeof(PatternStep);
            }

            return cost;
        }

        class ClipPatternChange final : public SequenceChange
        {
        public:
            ClipPatternChange(_In_ std::wstring id, _In_ PatternSettings before, _In_ PatternSettings after) :
                m_id(std::move(id)), m_before(std::move(before)), m_after(std::move(after))
            {
            }

            void Apply(_Inout_ Sequence& sequence) override { Set(sequence, m_after); }
            void Revert(_Inout_ Sequence& sequence) override { Set(sequence, m_before); }

            size_t Cost() const noexcept override
            {
                return sizeof(*this) + PatternCost(m_before) + PatternCost(m_after);
            }

        private:
            void Set(_Inout_ Sequence& sequence, _In_ PatternSettings const& pattern)
            {
                if (auto clip = FindClip(sequence, m_id); clip != nullptr && clip->Kind == ClipKind::Pattern)
                {
                    clip->Pattern = pattern;
                    RefreshPatternPreview(*clip);
                }
            }

            std::wstring m_id{};
            PatternSettings m_before{};
            PatternSettings m_after{};
        };

        class ClipGeneratorChange final : public SequenceChange
        {
        public:
            ClipGeneratorChange(_In_ std::wstring id, _In_ GeneratorSettings before, _In_ GeneratorSettings after) :
                m_id(std::move(id)), m_before(std::move(before)), m_after(std::move(after))
            {
            }

            void Apply(_Inout_ Sequence& sequence) override { Set(sequence, m_after); }
            void Revert(_Inout_ Sequence& sequence) override { Set(sequence, m_before); }

            size_t Cost() const noexcept override
            {
                return sizeof(*this) + (m_before.Rings.size() + m_after.Rings.size()) * sizeof(EuclidRing);
            }

        private:
            void Set(_Inout_ Sequence& sequence, _In_ GeneratorSettings const& generator)
            {
                if (auto clip = FindClip(sequence, m_id); clip != nullptr && clip->Kind == ClipKind::Generator)
                {
                    clip->Generator = generator;
                    RefreshGeneratorPreview(*clip);
                }
            }

            std::wstring m_id{};
            GeneratorSettings m_before{};
            GeneratorSettings m_after{};
        };

        size_t TreeCost(_In_ std::vector<Track> const& tracks) noexcept
        {
            size_t cost{ 0 };

            for (auto const& track : tracks)
            {
                cost += sizeof(Track) + track.Timeline.size() * sizeof(Placement) + track.Slots.size() * 48 +
                    track.Tags.size() * sizeof(Tag) + track.Startup.size() * sizeof(ClipEvent) + LanesCost(track.Lanes) + TreeCost(track.Children);
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

    namespace
    {
        // How deep a track sits: 0 at the top, SIZE_MAX when it isn't there.
        size_t DepthOf(Sequence const& sequence, std::wstring_view id)
        {
            size_t found{ SIZE_MAX };

            ForEachTrack(sequence, [&](Track const& track, size_t depth)
            {
                if (track.Id == id)
                {
                    found = depth;
                    return false;
                }

                return true;
            });

            return found;
        }

        // How many levels a track has under it: 0 for a track, 1 for a folder of tracks.
        size_t LevelsBelow(Track const& track)
        {
            size_t below{ 0 };

            for (auto const& child : track.Children)
            {
                below = std::max(below, 1 + LevelsBelow(child));
            }

            return below;
        }

        // Whether a track and everything in it fits at a depth a sequence file keeps.
        bool FitsAtDepth(Track const& track, size_t depth)
        {
            return depth != SIZE_MAX && depth + LevelsBelow(track) < SequenceLimits{}.MaximumFolderDepth;
        }
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
        // rows expects a new track to appear. Deeper than a sequence file keeps, it goes after it.
        if (auto const depth = DepthOf(sequence, anchor.Id); anchor.IsFolder && anchor.Open && depth != SIZE_MAX && FitsAtDepth(track, depth + 1))
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

            // No deeper than a sequence file keeps.
            auto const moving = FindTrack(sequence, trackId);
            auto const depth = DepthOf(sequence, folderId);

            if (moving == nullptr || depth == SIZE_MAX || !FitsAtDepth(*moving, depth + 1))
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
    bool MoveTracks(Sequence& sequence, std::vector<std::wstring> const& trackIds, std::wstring_view folderId, std::wstring_view beforeId)
    {
        if (!folderId.empty())
        {
            auto const folder = FindTrack(sequence, folderId);

            if (folder == nullptr || !folder->IsFolder)
            {
                return false;
            }
        }

        // Each id once, and not when a folder moving too already carries it.
        std::vector<std::wstring> moving{};

        for (auto const& id : trackIds)
        {
            if (id.empty() || FindTrack(sequence, id) == nullptr || std::find(moving.begin(), moving.end(), id) != moving.end())
            {
                continue;
            }

            moving.push_back(id);
        }

        // Tracks inside a folder that's moving go with it, so only the outermost move themselves.
        std::vector<std::wstring> outermost{};

        for (auto const& id : moving)
        {
            auto const carried = std::any_of(moving.begin(), moving.end(), [&](std::wstring const& other)
            {
                return other != id && IsInside(sequence, id, other);
            });

            if (!carried)
            {
                outermost.push_back(id);
            }
        }

        moving = std::move(outermost);

        if (moving.empty())
        {
            return false;
        }

        // A folder can't go inside itself.
        for (auto const& id : moving)
        {
            if (!folderId.empty() && IsInside(sequence, folderId, id))
            {
                return false;
            }
        }

        // The place has to be in the destination list. A place that's moving itself means the
        // first track after it that stays.
        std::wstring anchor{ beforeId };

        if (!anchor.empty())
        {
            auto const location = LocateTrack(sequence, anchor);
            auto const parentId = location.Parent != nullptr ? std::wstring_view{ location.Parent->Id } : std::wstring_view{};

            if (location.Container == nullptr || parentId != folderId)
            {
                return false;
            }

            anchor.clear();

            for (auto i = location.Index; i < location.Container->size(); ++i)
            {
                auto const& candidate = (*location.Container)[i].Id;

                if (std::find(moving.begin(), moving.end(), candidate) == moving.end())
                {
                    anchor = candidate;
                    break;
                }
            }
        }

        // How deep everything ends up.
        size_t destinationDepth{ 0 };

        if (!folderId.empty())
        {
            ForEachTrack(sequence, [&](Track const& track, size_t depth)
            {
                if (track.Id == folderId)
                {
                    destinationDepth = depth + 1;
                    return false;
                }

                return true;
            });
        }

        auto const deepest = [](auto const& self, Track const& track) -> size_t
        {
            size_t below{ 0 };

            for (auto const& child : track.Children)
            {
                below = std::max(below, 1 + self(self, child));
            }

            return below;
        };

        SequenceLimits const limits{};

        for (auto const& id : moving)
        {
            if (destinationDepth + deepest(deepest, *FindTrack(sequence, id)) >= limits.MaximumFolderDepth)
            {
                return false;
            }
        }

        // Where every track sits now, to tell whether anything moved.
        auto const shape = [&sequence]()
        {
            std::vector<std::pair<std::wstring, std::wstring>> rows{};

            auto const walk = [&](auto const& self, std::vector<Track> const& tracks, std::wstring const& parent) -> void
            {
                for (auto const& track : tracks)
                {
                    rows.emplace_back(track.Id, parent);
                    self(self, track.Children, track.Id);
                }
            };

            walk(walk, sequence.Tracks, std::wstring{});
            return rows;
        };

        auto const before = shape();

        std::vector<Track> taken{};
        taken.reserve(moving.size());

        for (auto const& id : moving)
        {
            if (auto removed = RemoveTrack(sequence, id); removed.has_value())
            {
                taken.push_back(std::move(*removed));
            }
        }

        auto container = &sequence.Tracks;

        if (!folderId.empty())
        {
            container = &FindTrack(sequence, folderId)->Children;
        }

        auto at = container->size();

        if (!anchor.empty())
        {
            for (size_t i = 0; i < container->size(); ++i)
            {
                if ((*container)[i].Id == anchor)
                {
                    at = i;
                    break;
                }
            }
        }

        container->insert(container->begin() + static_cast<ptrdiff_t>(at), std::make_move_iterator(taken.begin()), std::make_move_iterator(taken.end()));

        return shape() != before;
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

    _Use_decl_annotations_
    Clip MakePatternClip(std::wstring name, PatternMode mode)
    {
        Clip clip{};
        clip.Id = NewId(L"c");
        clip.Kind = ClipKind::Pattern;
        clip.Name = std::move(name);
        clip.Loop = true;
        clip.Seed = NewSeed();
        clip.Origin = ClipOrigin::Drawn;
        clip.Pattern = mode == PatternMode::Melodic ? MakeMelodicPattern() : MakeDrumPattern();
        RefreshPatternPreview(clip);
        return clip;
    }

    uint32_t MakeSeed()
    {
        return NewSeed();
    }

    _Use_decl_annotations_
    std::unique_ptr<SequenceChange> MakeClipPatternChange(std::wstring clipId, PatternSettings before, PatternSettings after)
    {
        return std::make_unique<ClipPatternChange>(std::move(clipId), std::move(before), std::move(after));
    }

    _Use_decl_annotations_
    Clip MakeGeneratorClip(std::wstring name, GeneratorType type, int64_t length)
    {
        Clip clip{};
        clip.Id = NewId(L"c");
        clip.Kind = ClipKind::Generator;
        clip.Name = std::move(name);
        clip.Length = std::max<int64_t>(1, length);
        clip.Loop = true;
        clip.Seed = NewSeed();
        clip.Origin = ClipOrigin::Generated;
        clip.Generator = IsSectionTwentyEightGenerator(type) ? MakeSectionTwentyEightGenerator(type) :
            type == GeneratorType::ChanceMelody ? MakeChanceMelodyGenerator() :
            type == GeneratorType::Arpeggiator ? MakeArpeggiatorGenerator() : MakeEuclideanGenerator();
        clip.Generator->Type = type;
        RefreshGeneratorPreview(clip);
        return clip;
    }

    _Use_decl_annotations_
    std::unique_ptr<SequenceChange> MakeClipGeneratorChange(std::wstring clipId, GeneratorSettings before, GeneratorSettings after)
    {
        return std::make_unique<ClipGeneratorChange>(std::move(clipId), std::move(before), std::move(after));
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

    // ---- automation ----

    _Use_decl_annotations_
    AutomationLane MakeLane(LaneTarget target, uint16_t number, uint8_t channel)
    {
        AutomationLane lane{};
        lane.Id = NewId(L"a");
        lane.Target = target;
        lane.Channel = static_cast<uint8_t>(channel & 0x0F);

        switch (target)
        {
        case LaneTarget::Controller:
            lane.Number = std::min<uint16_t>(number, 127);
            break;

        case LaneTarget::Registered:
        case LaneTarget::Assignable:
            lane.Number = std::min<uint16_t>(number, 16383);
            break;

        default:
            lane.Number = 0;
            break;
        }

        return lane;
    }

    _Use_decl_annotations_
    AutomationLane* FindLane(std::vector<AutomationLane>& lanes, std::wstring_view id) noexcept
    {
        auto found = std::find_if(lanes.begin(), lanes.end(), [id](AutomationLane const& lane) { return lane.Id == id; });
        return found == lanes.end() ? nullptr : &*found;
    }

    _Use_decl_annotations_
    size_t AddLanePoint(AutomationLane& lane, LanePoint point)
    {
        point.Tick = std::max<int64_t>(0, point.Tick);

        auto const at = std::upper_bound(lane.Points.begin(), lane.Points.end(), point.Tick,
            [](int64_t tick, LanePoint const& existing) { return tick < existing.Tick; });

        auto const index = static_cast<size_t>(std::distance(lane.Points.begin(), at));
        lane.Points.insert(at, point);
        return index;
    }

    _Use_decl_annotations_
    void MoveLanePoint(AutomationLane& lane, size_t index, int64_t tick, uint32_t value) noexcept
    {
        if (index >= lane.Points.size())
        {
            return;
        }

        auto const low = index > 0 ? lane.Points[index - 1].Tick : 0;
        auto const high = index + 1 < lane.Points.size() ? lane.Points[index + 1].Tick : INT64_MAX;

        lane.Points[index].Tick = std::clamp(tick, low, high);
        lane.Points[index].Value = value;
    }

    _Use_decl_annotations_
    void ReplaceLanePoints(AutomationLane& lane, int64_t fromTick, int64_t toTick, std::vector<LanePoint> const& drawn)
    {
        if (toTick < fromTick)
        {
            std::swap(fromTick, toTick);
        }

        std::erase_if(lane.Points, [fromTick, toTick](LanePoint const& point)
        {
            return point.Tick >= fromTick && point.Tick <= toTick;
        });

        auto sorted = drawn;

        for (auto& point : sorted)
        {
            point.Tick = std::clamp(point.Tick, fromTick, toTick);
        }

        std::stable_sort(sorted.begin(), sorted.end(), [](LanePoint const& a, LanePoint const& b) { return a.Tick < b.Tick; });

        auto const at = std::lower_bound(lane.Points.begin(), lane.Points.end(), fromTick,
            [](LanePoint const& point, int64_t tick) { return point.Tick < tick; });

        lane.Points.insert(at, sorted.begin(), sorted.end());
    }

    _Use_decl_annotations_
    std::unique_ptr<SequenceChange> MakeTrackLanesChange(std::wstring trackId, std::vector<AutomationLane> before, std::vector<AutomationLane> after)
    {
        auto const cost = LanesCost(before) + LanesCost(after);

        return std::make_unique<SnapshotChange<std::vector<AutomationLane>>>(
            [trackId = std::move(trackId)](Sequence& sequence) -> std::vector<AutomationLane>*
            {
                auto const track = FindTrack(sequence, trackId);
                return track != nullptr ? &track->Lanes : nullptr;
            },
            std::move(before), std::move(after), cost);
    }

    _Use_decl_annotations_
    std::unique_ptr<SequenceChange> MakeClipLanesChange(std::wstring clipId, std::vector<AutomationLane> before, std::vector<AutomationLane> after)
    {
        auto const cost = LanesCost(before) + LanesCost(after);

        return std::make_unique<SnapshotChange<std::vector<AutomationLane>>>(
            [clipId = std::move(clipId)](Sequence& sequence) -> std::vector<AutomationLane>*
            {
                auto const clip = FindClip(sequence, clipId);
                return clip != nullptr ? &clip->Lanes : nullptr;
            },
            std::move(before), std::move(after), cost);
    }

    // ---- tempo and meter ----

    namespace
    {
        double ValidTempo(_In_ double beatsPerMinute) noexcept
        {
            return std::isfinite(beatsPerMinute) ? std::clamp(beatsPerMinute, MinimumBeatsPerMinute, MaximumBeatsPerMinute) : 120.0;
        }

        // Where meter changes are in bars, so they can be put back at the same bars after the
        // meter before them changes.
        struct MeterAtBar
        {
            int64_t Bar{ 1 };
            uint8_t Numerator{ 4 };
            uint8_t Denominator{ 4 };
        };

        void PlaceAtBars(_Inout_ std::vector<MeterChange>& meter, _In_ std::vector<MeterAtBar> const& later)
        {
            for (auto const& change : later)
            {
                auto const tick = TickAtBar(meter, change.Bar);

                if (tick > meter.back().Tick)
                {
                    meter.push_back(MeterChange{ tick, change.Numerator, change.Denominator });
                }
            }
        }
    }

    _Use_decl_annotations_
    void SetTempoAt(std::vector<TempoPoint>& tempo, int64_t tick, double beatsPerMinute)
    {
        tick = std::max<int64_t>(0, tick);
        beatsPerMinute = ValidTempo(beatsPerMinute);

        if (tempo.empty())
        {
            tempo.push_back(TempoPoint{});
        }

        auto const found = std::find_if(tempo.begin(), tempo.end(), [tick](TempoPoint const& point) { return point.Tick == tick; });

        if (found != tempo.end())
        {
            found->BeatsPerMinute = beatsPerMinute;
            return;
        }

        auto const at = std::upper_bound(tempo.begin(), tempo.end(), tick,
            [](int64_t value, TempoPoint const& point) { return value < point.Tick; });

        tempo.insert(at, TempoPoint{ tick, beatsPerMinute, false });
    }

    _Use_decl_annotations_
    void MoveTempoPoint(std::vector<TempoPoint>& tempo, size_t index, int64_t tick, double beatsPerMinute) noexcept
    {
        if (index >= tempo.size())
        {
            return;
        }

        tempo[index].BeatsPerMinute = ValidTempo(beatsPerMinute);

        if (index == 0)
        {
            tempo[index].Tick = 0;
            return;
        }

        auto const low = tempo[index - 1].Tick + 1;
        auto const high = index + 1 < tempo.size() ? tempo[index + 1].Tick - 1 : INT64_MAX;
        tempo[index].Tick = std::clamp(tick, low, std::max(low, high));
    }

    _Use_decl_annotations_
    bool RemoveTempoPoint(std::vector<TempoPoint>& tempo, size_t index) noexcept
    {
        if (index == 0 || index >= tempo.size())
        {
            return false;
        }

        tempo.erase(tempo.begin() + static_cast<ptrdiff_t>(index));
        tempo.back().RampToNext = false;
        return true;
    }

    _Use_decl_annotations_
    void SetMeterAt(std::vector<MeterChange>& meter, int64_t bar, uint8_t numerator, uint8_t denominator)
    {
        bar = std::max<int64_t>(1, bar);
        numerator = std::clamp<uint8_t>(numerator, 1, 64);

        if (denominator != 1 && denominator != 2 && denominator != 4 && denominator != 8 && denominator != 16 && denominator != 32)
        {
            denominator = 4;
        }

        if (meter.empty())
        {
            meter.push_back(MeterChange{});
        }

        std::vector<MeterChange> kept{};
        std::vector<MeterAtBar> later{};

        for (auto const& change : meter)
        {
            auto const changeBar = BarPositionAtTick(meter, change.Tick).Bar;

            if (changeBar < bar)
            {
                kept.push_back(change);
            }
            else if (changeBar > bar)
            {
                later.push_back(MeterAtBar{ changeBar, change.Numerator, change.Denominator });
            }
        }

        kept.push_back(MeterChange{ kept.empty() ? 0 : TickAtBar(kept, bar), numerator, denominator });
        PlaceAtBars(kept, later);
        meter = std::move(kept);
    }

    _Use_decl_annotations_
    bool RemoveMeterChange(std::vector<MeterChange>& meter, size_t index)
    {
        if (index == 0 || index >= meter.size())
        {
            return false;
        }

        std::vector<MeterAtBar> later{};

        for (size_t i = index + 1; i < meter.size(); ++i)
        {
            later.push_back(MeterAtBar{ BarPositionAtTick(meter, meter[i].Tick).Bar, meter[i].Numerator, meter[i].Denominator });
        }

        meter.resize(index);
        PlaceAtBars(meter, later);
        return true;
    }
}
