// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#include "SequenceMerge.h"

#include <algorithm>
#include <map>
#include <tuple>

#include "SequenceEdits.h"
#include "SequenceRender.h"

namespace midisequencer
{
    namespace
    {
        bool SameTarget(AutomationLane const& a, AutomationLane const& b) noexcept
        {
            return a.Target == b.Target && a.Number == b.Number && a.Channel == b.Channel;
        }

        // Points into the lane with the same target, or a new one like it.
        void AddLanePoints(std::vector<AutomationLane>& lanes, AutomationLane const& like, std::vector<LanePoint> const& points)
        {
            auto found = std::find_if(lanes.begin(), lanes.end(), [&](AutomationLane const& lane) { return SameTarget(lane, like); });

            if (found == lanes.end())
            {
                auto lane = like;
                lane.Points.clear();
                lanes.push_back(std::move(lane));
                found = lanes.end() - 1;
            }

            found->Points.insert(found->Points.end(), points.begin(), points.end());
            std::stable_sort(found->Points.begin(), found->Points.end(), [](LanePoint const& a, LanePoint const& b) { return a.Tick < b.Tick; });
        }

        // A notes clip, note for note: each pass through it while the placement plays.
        void TileNotesClip(Clip const& clip, int64_t start, int64_t length, int64_t fromTick, int64_t toTick, PlacedContent& content)
        {
            auto const clipLength = std::max<int64_t>(1, clip.Length);
            auto const end = std::min(start + length, toTick);

            for (int64_t pass = 0; start + pass * clipLength < end; ++pass)
            {
                if (pass > 0 && !clip.Loop)
                {
                    break;
                }

                auto const passStart = start + pass * clipLength;

                // A looping clip's notes end where the pass does, as they play.
                auto const passEnd = std::min(clip.Loop ? passStart + clipLength : start + length, end);

                for (auto const& note : clip.Notes)
                {
                    auto const at = passStart + note.Tick;
                    auto const noteEnd = std::min(at + note.Length, passEnd);

                    // A note sounding as the stretch begins keeps the part that's in it.
                    if (note.Tick >= clipLength || at >= passEnd || noteEnd <= fromTick)
                    {
                        continue;
                    }

                    auto const begin = std::max(at, fromTick);
                    auto copy = note;
                    copy.Tick = begin - fromTick;
                    copy.Length = std::max<int64_t>(1, noteEnd - begin);
                    content.Notes.push_back(copy);
                }

                for (auto const& event : clip.Events)
                {
                    auto const at = passStart + event.Tick;

                    if (event.Tick < clipLength && at < passEnd && at >= fromTick)
                    {
                        auto copy = event;
                        copy.Tick = at - fromTick;
                        content.Events.push_back(copy);
                    }
                }

                for (auto const& lane : clip.Lanes)
                {
                    std::vector<LanePoint> points{};

                    for (auto const& point : lane.Points)
                    {
                        auto const at = passStart + point.Tick;

                        if (point.Tick < clipLength && at < passEnd && at >= fromTick)
                        {
                            auto copy = point;
                            copy.Tick = at - fromTick;
                            points.push_back(copy);
                        }
                    }

                    if (!points.empty())
                    {
                        AddLanePoints(content.Lanes, lane, points);
                    }
                }
            }
        }

        // A pattern or generator: what it plays there, as playback works it out.
        void RenderPlacement(Sequence const& sequence, Track const& track, Placement const& placement, int64_t start, int64_t length, int64_t fromTick, int64_t toTick, PlacedContent& content)
        {
            auto alone = track;
            alone.Timeline = { placement };
            alone.Lanes.clear();
            alone.Children.clear();

            // From the placement's start, so a note sounding as the stretch begins is heard; what's
            // before the stretch is trimmed off below.
            auto const from = start;
            auto const to = std::min(start + length, toTick);

            if (to <= std::max(start, fromTick))
            {
                return;
            }

            std::vector<RenderedMessage> messages{};
            RenderOptions options{};
            options.Lanes = true;
            RenderTrack(sequence, alone, from, to, messages, options);

            // Notes on: when, how hard and what attribute, by channel and number.
            std::map<std::pair<uint8_t, uint8_t>, Note> open{};

            auto const close = [&](Note note, int64_t at)
            {
                auto const begin = std::max(note.Tick, fromTick);

                if (at > begin)
                {
                    note.Length = at - begin;
                    note.Tick = begin - fromTick;
                    content.Notes.push_back(note);
                }
            };

            for (auto const& message : messages)
            {
                auto const word0 = message.Words[0];
                auto const type = word0 >> 28;
                auto const status = (word0 >> 20) & 0x0F;

                if (type == 0x4 && message.WordCount >= 2 && (status == 0x9 || status == 0x8))
                {
                    auto const channel = static_cast<uint8_t>((word0 >> 16) & 0x0F);
                    auto const number = static_cast<uint8_t>((word0 >> 8) & 0x7F);
                    auto const key = std::make_pair(channel, number);

                    if (auto const found = open.find(key); found != open.end())
                    {
                        auto note = found->second;

                        if (status == 0x8)
                        {
                            note.ReleaseVelocity = static_cast<uint16_t>(message.Words[1] >> 16);
                        }

                        close(note, message.Tick);
                        open.erase(found);
                    }

                    if (status == 0x9)
                    {
                        Note note{};
                        note.Tick = message.Tick;
                        note.Channel = channel;
                        note.Number = number;
                        note.Velocity = static_cast<uint16_t>(message.Words[1] >> 16);
                        note.AttributeType = static_cast<uint8_t>(word0 & 0xFF);
                        note.AttributeData = static_cast<uint16_t>(message.Words[1] & 0xFFFF);
                        open.emplace(key, note);
                    }

                    continue;
                }

                if (message.Tick < fromTick)
                {
                    continue;
                }

                ClipEvent event{};
                event.Tick = message.Tick - fromTick;
                event.Words = message.Words;
                event.WordCount = message.WordCount;
                content.Events.push_back(event);
            }

            for (auto const& [key, note] : open)
            {
                close(note, to);
            }
        }

        void SortContent(PlacedContent& content)
        {
            content.Notes = MergeOverlappingNotes(std::move(content.Notes));
            std::stable_sort(content.Events.begin(), content.Events.end(), [](ClipEvent const& a, ClipEvent const& b) { return a.Tick < b.Tick; });
        }

        void Append(PlacedContent& to, PlacedContent&& from)
        {
            to.Notes.insert(to.Notes.end(), from.Notes.begin(), from.Notes.end());
            to.Events.insert(to.Events.end(), from.Events.begin(), from.Events.end());

            for (auto const& lane : from.Lanes)
            {
                AddLanePoints(to.Lanes, lane, lane.Points);
            }
        }

        Clip ClipFrom(PlacedContent&& content, std::wstring id, std::wstring name, int64_t length, bool loop)
        {
            auto clip = MakeNotesClip(std::move(name), std::max<int64_t>(1, length));
            clip.Id = std::move(id);
            clip.Loop = loop;
            clip.Origin = ClipOrigin::Drawn;
            clip.Notes = std::move(content.Notes);
            clip.Events = std::move(content.Events);
            clip.Lanes = std::move(content.Lanes);

            for (auto& lane : clip.Lanes)
            {
                lane.Id = NewId(L"l");
            }

            SortClip(clip);
            return clip;
        }

        void SortTimeline(Track& track)
        {
            std::stable_sort(track.Timeline.begin(), track.Timeline.end(), [](Placement const& a, Placement const& b) { return a.Tick < b.Tick; });
        }
    }

    _Use_decl_annotations_
    int64_t PlacementLength(Sequence const& sequence, Placement const& placement) noexcept
    {
        if (placement.Length > 0)
        {
            return placement.Length;
        }

        auto const clip = FindClip(sequence, placement.ClipId);
        return clip != nullptr ? std::max<int64_t>(1, clip->Length) : TicksPerQuarterNote * 4;
    }

    _Use_decl_annotations_
    PlacedContent ContentOfPlacements(Sequence const& sequence, Track const& track, std::vector<Placement> const& placements, int64_t fromTick, int64_t toTick)
    {
        PlacedContent content{};

        for (auto const& placement : placements)
        {
            auto const clip = FindClip(sequence, placement.ClipId);

            if (clip == nullptr)
            {
                continue;
            }

            auto const length = PlacementLength(sequence, placement);

            if (clip->Kind == ClipKind::Notes)
            {
                TileNotesClip(*clip, placement.Tick, length, fromTick, toTick, content);
            }
            else
            {
                RenderPlacement(sequence, track, placement, placement.Tick, length, fromTick, toTick, content);
            }
        }

        SortContent(content);
        return content;
    }

    _Use_decl_annotations_
    std::vector<Note> MergeOverlappingNotes(std::vector<Note> notes)
    {
        std::stable_sort(notes.begin(), notes.end(), [](Note const& a, Note const& b)
        {
            return std::tie(a.Channel, a.Number, a.Tick) < std::tie(b.Channel, b.Number, b.Tick);
        });

        std::vector<Note> merged{};
        merged.reserve(notes.size());

        for (auto const& note : notes)
        {
            if (!merged.empty())
            {
                auto& last = merged.back();

                if (last.Channel == note.Channel && last.Number == note.Number && note.Tick < last.Tick + last.Length)
                {
                    auto const end = std::max(last.Tick + last.Length, note.Tick + note.Length);

                    if (note.Tick + note.Length >= last.Tick + last.Length)
                    {
                        last.ReleaseVelocity = note.ReleaseVelocity;
                    }

                    last.Length = end - last.Tick;
                    continue;
                }
            }

            merged.push_back(note);
        }

        std::stable_sort(merged.begin(), merged.end(), [](Note const& a, Note const& b)
        {
            return std::tie(a.Tick, a.Number) < std::tie(b.Tick, b.Number);
        });

        return merged;
    }

    _Use_decl_annotations_
    std::optional<MergedClip> MergePlacements(Sequence& sequence, std::wstring_view trackId, std::vector<size_t> indexes, std::wstring clipId, std::wstring name)
    {
        auto track = FindTrack(sequence, trackId);

        if (track == nullptr || track->IsFolder)
        {
            return std::nullopt;
        }

        std::sort(indexes.begin(), indexes.end());
        indexes.erase(std::unique(indexes.begin(), indexes.end()), indexes.end());

        if (indexes.size() < 2 || indexes.back() >= track->Timeline.size())
        {
            return std::nullopt;
        }

        std::vector<Placement> chosen{};
        int64_t start{ INT64_MAX };
        int64_t end{ INT64_MIN };

        for (auto const index : indexes)
        {
            auto const& placement = track->Timeline[index];
            chosen.push_back(placement);
            start = std::min(start, placement.Tick);
            end = std::max(end, placement.Tick + PlacementLength(sequence, placement));
        }

        // One color when they all had the same, or the track's.
        std::optional<uint32_t> color{};
        auto first = true;
        auto sameColor = true;

        for (auto const& placement : chosen)
        {
            auto const clip = FindClip(sequence, placement.ClipId);
            auto const own = clip != nullptr ? clip->Color : std::optional<uint32_t>{};

            if (first)
            {
                color = own;
                first = false;
            }
            else if (own != color)
            {
                sameColor = false;
            }
        }

        auto made = ClipFrom(ContentOfPlacements(sequence, *track, chosen, start, end), std::move(clipId), std::move(name), end - start, false);
        made.Color = sameColor ? color : std::nullopt;

        for (auto it = indexes.rbegin(); it != indexes.rend(); ++it)
        {
            track->Timeline.erase(track->Timeline.begin() + static_cast<ptrdiff_t>(*it));
        }

        Placement merged{ made.Id, start, 0 };
        track->Timeline.push_back(merged);
        SortTimeline(*track);
        sequence.Clips.push_back(made);

        MergedClip result{ made, SIZE_MAX };

        for (size_t i = 0; i < track->Timeline.size(); ++i)
        {
            if (track->Timeline[i] == merged)
            {
                result.Placement = i;
            }
        }

        return result;
    }

    _Use_decl_annotations_
    std::vector<Clip> SplitPlacement(Sequence& sequence, std::wstring_view trackId, size_t index, int64_t tick, std::function<std::wstring()> const& newClipId, std::wstring const& rightName)
    {
        auto track = FindTrack(sequence, trackId);

        if (track == nullptr || track->IsFolder || index >= track->Timeline.size())
        {
            return {};
        }

        auto const placement = track->Timeline[index];
        auto const length = PlacementLength(sequence, placement);
        auto const end = placement.Tick + length;
        auto const clip = FindClip(sequence, placement.ClipId);

        if (clip == nullptr || tick <= placement.Tick || tick >= end)
        {
            return {};
        }

        std::vector<Clip> added{};

        // The right part: what played from the cut on, as notes.
        auto right = ClipFrom(ContentOfPlacements(sequence, *track, { placement }, tick, end), newClipId(), rightName, end - tick, false);
        right.Color = clip->Color;

        // The left part: a notes clip keeps only what's before the cut. A pattern or generator
        // stays what it is, and stops at the cut.
        Placement left{ placement.ClipId, placement.Tick, tick - placement.Tick };

        if (clip->Kind == ClipKind::Notes)
        {
            auto trimmed = ClipFrom(ContentOfPlacements(sequence, *track, { placement }, placement.Tick, tick), newClipId(), clip->Name, tick - placement.Tick, false);
            trimmed.Color = clip->Color;
            trimmed.Origin = clip->Origin;
            trimmed.OriginDetail = clip->OriginDetail;
            left = Placement{ trimmed.Id, placement.Tick, 0 };
            added.push_back(std::move(trimmed));
        }

        added.push_back(right);

        track = FindTrack(sequence, trackId);
        track->Timeline[index] = left;
        track->Timeline.push_back(Placement{ right.Id, tick, 0 });
        SortTimeline(*track);

        sequence.Clips.insert(sequence.Clips.end(), added.begin(), added.end());
        return added;
    }

    _Use_decl_annotations_
    TrackMerge MergeTrackInto(
        Sequence& sequence,
        std::wstring_view sourceId,
        std::wstring_view destinationId,
        std::function<std::wstring()> const& newClipId,
        std::function<std::wstring(std::wstring const&, std::wstring const&)> const& mergedName)
    {
        TrackMerge result{};

        auto const source = FindTrack(sequence, sourceId);
        auto const destination = FindTrack(sequence, destinationId);

        if (source == nullptr || destination == nullptr || source == destination || source->IsFolder || destination->IsFolder)
        {
            return result;
        }

        auto const from = *source;
        auto const into = *destination;

        // ---- the timeline: runs of placements that overlap, across both tracks ----

        struct Span
        {
            int64_t Start;
            int64_t End;
            bool FromSource;
            Placement Where;
        };

        std::vector<Span> spans{};

        for (auto const& placement : into.Timeline)
        {
            spans.push_back({ placement.Tick, placement.Tick + PlacementLength(sequence, placement), false, placement });
        }

        for (auto const& placement : from.Timeline)
        {
            spans.push_back({ placement.Tick, placement.Tick + PlacementLength(sequence, placement), true, placement });
        }

        std::stable_sort(spans.begin(), spans.end(), [](Span const& a, Span const& b) { return a.Start < b.Start; });

        std::vector<Placement> timeline{};
        std::vector<Clip> added{};

        for (size_t i = 0; i < spans.size();)
        {
            auto runEnd = spans[i].End;
            auto j = i + 1;

            while (j < spans.size() && spans[j].Start < runEnd)
            {
                runEnd = std::max(runEnd, spans[j].End);
                ++j;
            }

            auto const both = std::any_of(spans.begin() + static_cast<ptrdiff_t>(i), spans.begin() + static_cast<ptrdiff_t>(j), [](Span const& s) { return s.FromSource; }) &&
                std::any_of(spans.begin() + static_cast<ptrdiff_t>(i), spans.begin() + static_cast<ptrdiff_t>(j), [](Span const& s) { return !s.FromSource; });

            if (!both)
            {
                for (auto k = i; k < j; ++k)
                {
                    timeline.push_back(spans[k].Where);
                }
            }
            else
            {
                std::vector<Placement> ours{};
                std::vector<Placement> theirs{};
                std::wstring ourName{};
                std::wstring theirName{};

                for (auto k = i; k < j; ++k)
                {
                    auto const clip = FindClip(sequence, spans[k].Where.ClipId);
                    (spans[k].FromSource ? theirs : ours).push_back(spans[k].Where);

                    auto& name = spans[k].FromSource ? theirName : ourName;

                    if (name.empty() && clip != nullptr)
                    {
                        name = clip->Name;
                    }
                }

                auto const start = spans[i].Start;
                auto content = ContentOfPlacements(sequence, into, ours, start, runEnd);
                Append(content, ContentOfPlacements(sequence, from, theirs, start, runEnd));
                SortContent(content);

                auto clip = ClipFrom(std::move(content), newClipId(), mergedName(ourName, theirName), runEnd - start, false);
                timeline.push_back(Placement{ clip.Id, start, 0 });
                added.push_back(std::move(clip));
            }

            i = j;
        }

        // ---- the launcher ----

        auto slots = into.Slots;
        slots.resize(std::max(slots.size(), from.Slots.size()));

        for (size_t scene = 0; scene < from.Slots.size(); ++scene)
        {
            auto const& theirs = from.Slots[scene];
            auto& ours = slots[scene];

            if (theirs.empty() || theirs == ours)
            {
                continue;
            }

            if (ours.empty())
            {
                ours = theirs;
                continue;
            }

            auto const a = FindClip(sequence, ours);
            auto const b = FindClip(sequence, theirs);

            if (a == nullptr || b == nullptr)
            {
                ours = a == nullptr ? theirs : ours;
                continue;
            }

            auto const length = std::max(std::max<int64_t>(1, a->Length), std::max<int64_t>(1, b->Length));
            auto content = ContentOfPlacements(sequence, into, { Placement{ ours, 0, length } }, 0, length);
            Append(content, ContentOfPlacements(sequence, from, { Placement{ theirs, 0, length } }, 0, length));
            SortContent(content);

            auto clip = ClipFrom(std::move(content), newClipId(), mergedName(a->Name, b->Name), length, true);
            ours = clip.Id;
            added.push_back(std::move(clip));
        }

        // ---- lanes and tags ----

        auto lanes = into.Lanes;

        for (auto const& lane : from.Lanes)
        {
            if (std::any_of(lanes.begin(), lanes.end(), [&](AutomationLane const& mine) { return SameTarget(mine, lane); }))
            {
                ++result.LanesSkipped;
                continue;
            }

            lanes.push_back(lane);
        }

        auto tags = into.Tags;
        tags.insert(tags.end(), from.Tags.begin(), from.Tags.end());
        std::stable_sort(tags.begin(), tags.end(), [](Tag const& a, Tag const& b) { return a.Tick < b.Tick; });

        // ---- into the sequence ----

        auto target = FindTrack(sequence, destinationId);
        target->Timeline = std::move(timeline);
        SortTimeline(*target);
        target->Slots = std::move(slots);
        target->Lanes = std::move(lanes);
        target->Tags = std::move(tags);

        if (sequence.Lyrics.has_value() && sequence.Lyrics->TrackId == sourceId)
        {
            sequence.Lyrics->TrackId = std::wstring{ destinationId };
        }

        RemoveTrack(sequence, sourceId);
        sequence.Clips.insert(sequence.Clips.end(), added.begin(), added.end());

        result.Merged = true;
        result.Added = std::move(added);
        return result;
    }

    namespace
    {
        // Unmuted tracks, not in a muted folder. Solo is left out on purpose (Pete, Oct 22): a solo
        // left on somewhere shouldn't hide what the rest of the sequence plays.
        void CollectUnmuted(Sequence const& sequence, std::vector<Track> const& tracks, std::wstring_view exceptTrackId, int64_t fromTick, int64_t toTick,
            bool insideMutedFolder, std::vector<OtherTrackNotes>& result)
        {
            for (auto const& track : tracks)
            {
                if (track.IsFolder)
                {
                    CollectUnmuted(sequence, track.Children, exceptTrackId, fromTick, toTick, insideMutedFolder || track.Muted, result);
                    continue;
                }

                if (track.Muted || insideMutedFolder || track.Id == exceptTrackId)
                {
                    continue;
                }

                std::vector<Placement> overlapping{};

                for (auto const& placement : track.Timeline)
                {
                    if (placement.Tick < toTick && placement.Tick + PlacementLength(sequence, placement) > fromTick)
                    {
                        overlapping.push_back(placement);
                    }
                }

                if (overlapping.empty())
                {
                    continue;
                }

                auto content = ContentOfPlacements(sequence, track, overlapping, fromTick, toTick);

                if (!content.Notes.empty())
                {
                    result.push_back(OtherTrackNotes{ track.Id, track.Color, std::move(content.Notes) });
                }
            }
        }
    }

    _Use_decl_annotations_
    std::vector<OtherTrackNotes> NotesOtherTracksPlay(Sequence const& sequence, std::wstring_view exceptTrackId, int64_t fromTick, int64_t toTick)
    {
        std::vector<OtherTrackNotes> result{};

        if (toTick <= fromTick)
        {
            return result;
        }

        CollectUnmuted(sequence, sequence.Tracks, exceptTrackId, fromTick, toTick, false, result);
        return result;
    }
}
