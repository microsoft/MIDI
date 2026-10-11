// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#include "SequenceRender.h"
#include "Lyrics.h"
#include "SequenceArpeggiator.h"
#include "SequenceGenerator.h"
#include "SequencePattern.h"

#include <algorithm>
#include <cmath>
#include <optional>

namespace midisequencer
{
    namespace
    {
        // A ramp longer than this many steps in one render is stepped more coarsely, so a hostile
        // file can't keep an export busy for minutes.
        constexpr int64_t MaximumLaneStepsPerSegment = 2000000;

        // A file can loop a one-tick clip with nothing in it for billions of ticks, which renders
        // nothing and would take minutes. One render stops after this many passes; a real song
        // loops a short clip a few thousand times.
        constexpr size_t MaximumRenderedPasses = 4000000;

        int64_t PlacementLength(_In_ Placement const& placement, _In_ Clip const& clip) noexcept
        {
            return placement.Length > 0 ? placement.Length : clip.Length;
        }

        void Add(
            _Inout_ std::vector<RenderedMessage>& messages,
            _In_ size_t maximum,
            _In_ int64_t tick,
            _In_ RenderedKind kind,
            _In_reads_(count) uint32_t const* words,
            _In_ uint8_t count)
        {
            if (messages.size() >= maximum)
            {
                return;
            }

            RenderedMessage message{};
            message.Tick = tick;
            message.Kind = kind;
            message.WordCount = std::min<uint8_t>(count, 4);
            std::copy_n(words, message.WordCount, message.Words.begin());
            messages.push_back(message);
        }

        int64_t FirstStepAtOrAfter(_In_ int64_t tick, _In_ int64_t step) noexcept
        {
            return tick <= 0 ? 0 : ((tick + step - 1) / step) * step;
        }

        int64_t LastStepBefore(_In_ int64_t tick, _In_ int64_t step) noexcept
        {
            return tick <= 0 ? -1 : ((tick - 1) / step) * step;
        }

        // A stretch of the timeline where one lane decides the value.
        struct LaneSpan
        {
            AutomationLane const* Lane{ nullptr };

            // Where the lane's tick 0 is on the timeline: 0 for a track's lane, the pass start for
            // a clip's.
            int64_t Offset{ 0 };

            int64_t Start{ 0 };
            int64_t End{ 0 };

            // Sends the value where the span starts, because it takes over from another lane or
            // from the clip's last pass.
            bool SendAtStart{ false };
        };

        // A lane's messages inside one span. Every choice here depends only on the lane and the
        // tick, never on what an earlier render sent, so a span rendered in pieces matches one
        // rendered whole: a value goes out at a point or a ramp step only where it differs, at the
        // destination's resolution, from the last place a value could have gone out before it.
        void RenderLaneSpan(
            _In_ LaneSpan const& span,
            _In_ int64_t fromTick,
            _In_ int64_t toTick,
            _In_ RenderOptions const& options,
            _In_ size_t maximum,
            _Inout_ std::vector<RenderedMessage>& messages)
        {
            auto const& lane = *span.Lane;
            auto const& points = lane.Points;
            auto const windowStart = std::max(fromTick, span.Start);
            auto const windowEnd = std::min(toTick, span.End);

            if (points.empty() || windowStart >= windowEnd)
            {
                return;
            }

            auto const bits = LaneResolutionBits(lane.Target, options.Midi2);
            auto const baseStep = std::max<int64_t>(1, options.LaneStepTicks);

            auto const valueAt = [&](int64_t tick) { return LaneValueAt(lane, tick - span.Offset).value_or(0); };
            auto const levelAt = [&](int64_t tick) { return ScaleDown(valueAt(tick), 32, bits); };
            auto const pointTick = [&](size_t index) { return points[index].Tick + span.Offset; };

            int64_t lastSent{ -1 };

            auto const send = [&](int64_t tick)
            {
                if (tick != lastSent)
                {
                    lastSent = tick;

                    uint32_t words[2]{};
                    BuildLaneMessage(lane, valueAt(tick), words);
                    Add(messages, maximum, tick, RenderedKind::Other, words, 2);
                }
            };

            if (span.SendAtStart && span.Start >= fromTick && span.Start < toTick)
            {
                send(span.Start);
            }

            if (options.LaneStartValues && windowStart == fromTick)
            {
                send(fromTick);
            }

            // The steps of the ramp that leaves point `index`, from `from` on.
            auto const rampSteps = [&](size_t index, int64_t from)
            {
                auto const segmentStart = pointTick(index);
                auto const segmentEnd = std::min(pointTick(index + 1), windowEnd);
                auto const first = std::max({ from, segmentStart + 1, windowStart });

                if (first >= segmentEnd)
                {
                    return;
                }

                auto step = baseStep;
                auto const count = (segmentEnd - first) / step;

                if (count > MaximumLaneStepsPerSegment)
                {
                    step *= (count + MaximumLaneStepsPerSegment - 1) / MaximumLaneStepsPerSegment;
                }

                for (auto tick = FirstStepAtOrAfter(first, step); tick < segmentEnd && messages.size() < maximum; tick += step)
                {
                    // The last place a value could have gone out: a step ago, or the point or the
                    // span start when that's nearer.
                    auto previous = std::max(tick - step, segmentStart);

                    if (span.Start < tick)
                    {
                        previous = std::max(previous, span.Start);
                    }

                    if (levelAt(tick) != levelAt(previous))
                    {
                        send(tick);
                    }
                }
            };

            // The first point at or after the window, and the ramp the window starts inside.
            auto index = static_cast<size_t>(std::distance(points.begin(),
                std::lower_bound(points.begin(), points.end(), windowStart - span.Offset,
                    [](LanePoint const& point, int64_t tick) { return point.Tick < tick; })));

            if (index > 0 && LaneRampsAfter(lane, index - 1))
            {
                rampSteps(index - 1, windowStart);
            }

            while (index < points.size() && pointTick(index) < windowEnd && messages.size() < maximum)
            {
                auto const tick = pointTick(index);

                // Points at one tick are one jump, and the value after it is the last one's.
                auto last = index;

                while (last + 1 < points.size() && points[last + 1].Tick == points[index].Tick)
                {
                    ++last;
                }

                std::optional<int64_t> previous{};

                if (index > 0)
                {
                    previous = pointTick(index - 1);

                    if (LaneRampsAfter(lane, index - 1))
                    {
                        previous = std::max(*previous, LastStepBefore(tick, baseStep));
                    }
                }

                if (span.Start < tick)
                {
                    previous = std::max(previous.value_or(span.Start), span.Start);
                }

                if (!previous.has_value() || levelAt(tick) != levelAt(*previous))
                {
                    send(tick);
                }

                if (LaneRampsAfter(lane, last))
                {
                    rampSteps(last, tick + 1);
                }

                index = last + 1;
            }
        }

        // One pass through a clip, which starts at passStart and is cut off at passEnd.
        void RenderPass(
            _In_ Sequence const& sequence,
            _In_ Clip const& clip,
            _In_ int64_t passStart,
            _In_ int64_t passEnd,
            _In_ uint64_t pass,
            _In_ int64_t fromTick,
            _In_ int64_t toTick,
            _In_ RenderOptions const& options,
            _In_ size_t maximum,
            _Inout_ std::vector<RenderedMessage>& messages)
        {
            auto const windowStart = std::max(fromTick, passStart);
            auto const windowEnd = std::min(toTick, passEnd);

            if (windowStart >= windowEnd)
            {
                return;
            }

            // A pattern's notes come from its steps, and a generator's from its settings. The notes
            // they keep are only for drawing.
            auto const fromSteps = clip.Kind == ClipKind::Pattern && clip.Pattern.has_value();
            auto const generated = clip.Kind == ClipKind::Generator && clip.Generator.has_value();

            auto const addNote = [&](Note const& note)
            {
                uint32_t words[2]{};
                BuildNoteOn(note, 0, words);
                Add(messages, maximum, note.Tick, RenderedKind::NoteOn, words, 2);

                BuildNoteOff(note, 0, words);
                Add(messages, maximum, std::min(note.Tick + note.Length, passEnd), RenderedKind::NoteOff, words, 2);
            };

            if (fromSteps)
            {
                ForEachPatternNote(clip, passStart, passEnd, pass, windowStart, windowEnd, true, addNote);
            }
            else if (generated && clip.Generator->Type == GeneratorType::Arpeggiator)
            {
                ForEachArpeggiatorNote(sequence, clip, passStart, passEnd, pass, windowStart, windowEnd, options.Arpeggiator, [&](Note const& note)
                {
                    auto const first = messages.size();
                    addNote(note);

                    for (auto i = first; i < messages.size(); ++i)
                    {
                        messages[i].Arpeggiated = true;
                    }
                });
            }
            else if (generated)
            {
                ForEachGeneratorNote(clip, passStart, passEnd, pass, windowStart, windowEnd, true, addNote);
            }

            // A note comes out with its end, even when the end is past the window. The engine holds
            // the end until it's due: one handed over early can't be taken back.
            auto firstNote = fromSteps || generated ? clip.Notes.end() : std::lower_bound(clip.Notes.begin(), clip.Notes.end(), windowStart - passStart,
                [](Note const& note, int64_t tick) { return note.Tick < tick; });

            for (auto it = firstNote; it != clip.Notes.end(); ++it)
            {
                auto const on = passStart + it->Tick;

                if (on >= windowEnd)
                {
                    break;
                }

                auto const index = static_cast<uint32_t>(std::distance(clip.Notes.begin(), it));

                if (!NotePlaysOnPass(*it, clip.Seed, index, pass))
                {
                    continue;
                }

                auto const off = std::min(on + std::max<int64_t>(1, it->Length), passEnd);

                uint32_t words[2]{};
                BuildNoteOn(*it, 0, words);
                Add(messages, maximum, on, RenderedKind::NoteOn, words, 2);

                BuildNoteOff(*it, 0, words);
                Add(messages, maximum, off, RenderedKind::NoteOff, words, 2);
            }

            auto firstEvent = std::lower_bound(clip.Events.begin(), clip.Events.end(), windowStart - passStart,
                [](ClipEvent const& event, int64_t tick) { return event.Tick < tick; });

            for (auto it = firstEvent; it != clip.Events.end(); ++it)
            {
                auto const at = passStart + it->Tick;

                if (at >= windowEnd)
                {
                    break;
                }

                Add(messages, maximum, at, RenderedKind::Other, it->Words.data(), it->WordCount);
            }

            // A clip's lanes start over with each pass.
            if (options.Lanes)
            {
                for (auto const& lane : clip.Lanes)
                {
                    RenderLaneSpan(LaneSpan{ &lane, passStart, passStart, passEnd, true }, fromTick, toTick, options, maximum, messages);
                }
            }
        }

        // A track's lane plays everywhere except inside the placements whose clips have a lane for
        // the same target, and takes over again where each of those ends.
        void RenderTrackLane(
            _In_ Sequence const& sequence,
            _In_ Track const& track,
            _In_ AutomationLane const& lane,
            _In_ int64_t fromTick,
            _In_ int64_t toTick,
            _In_ RenderOptions const& options,
            _In_ size_t maximum,
            _Inout_ std::vector<RenderedMessage>& messages)
        {
            if (lane.Points.empty())
            {
                return;
            }

            std::vector<std::pair<int64_t, int64_t>> covered{};

            for (auto const& placement : track.Timeline)
            {
                auto const clip = FindClip(sequence, placement.ClipId);

                if (clip == nullptr || clip->Length <= 0)
                {
                    continue;
                }

                auto const hasOwn = std::any_of(clip->Lanes.begin(), clip->Lanes.end(), [&lane](AutomationLane const& own)
                {
                    return !own.Points.empty() && SameLaneTarget(own, lane);
                });

                if (hasOwn)
                {
                    auto const length = PlacementLength(placement, *clip);
                    covered.emplace_back(placement.Tick, placement.Tick + (clip->Loop ? length : std::min(length, clip->Length)));
                }
            }

            std::sort(covered.begin(), covered.end());

            int64_t start{ 0 };
            bool resumes{ false };

            for (auto const& [coveredStart, coveredEnd] : covered)
            {
                if (coveredStart > start)
                {
                    RenderLaneSpan(LaneSpan{ &lane, 0, start, coveredStart, resumes }, fromTick, toTick, options, maximum, messages);
                }

                if (coveredEnd > start)
                {
                    start = coveredEnd;
                    resumes = true;
                }
            }

            RenderLaneSpan(LaneSpan{ &lane, 0, start, INT64_MAX, resumes }, fromTick, toTick, options, maximum, messages);
        }
    }

    _Use_decl_annotations_
    void BuildNoteOn(Note const& note, uint8_t group, uint32_t* words) noexcept
    {
        words[0] = 0x40000000u |
            (static_cast<uint32_t>(group & 0x0F) << 24) |
            (0x9u << 20) |
            (static_cast<uint32_t>(note.Channel & 0x0F) << 16) |
            (static_cast<uint32_t>(note.Number & 0x7F) << 8) |
            note.AttributeType;
        words[1] = (static_cast<uint32_t>(note.Velocity) << 16) | note.AttributeData;
    }

    _Use_decl_annotations_
    void BuildNoteOff(Note const& note, uint8_t group, uint32_t* words) noexcept
    {
        words[0] = 0x40000000u |
            (static_cast<uint32_t>(group & 0x0F) << 24) |
            (0x8u << 20) |
            (static_cast<uint32_t>(note.Channel & 0x0F) << 16) |
            (static_cast<uint32_t>(note.Number & 0x7F) << 8);
        words[1] = static_cast<uint32_t>(note.ReleaseVelocity) << 16;
    }

    _Use_decl_annotations_
    double FastestTempo(std::vector<TempoPoint> const& tempo) noexcept
    {
        auto fastest = tempo.empty() ? 120.0 : MinimumBeatsPerMinute;

        for (auto const& point : tempo)
        {
            if (std::isfinite(point.BeatsPerMinute))
            {
                fastest = std::max(fastest, point.BeatsPerMinute);
            }
        }

        return std::clamp(fastest, MinimumBeatsPerMinute, MaximumBeatsPerMinute);
    }

    _Use_decl_annotations_
    int64_t LaneStepTicks(double fastestBeatsPerMinute, uint32_t intervalMilliseconds) noexcept
    {
        auto const milliseconds = intervalMilliseconds == 0 ? DefaultLaneIntervalMilliseconds : std::min<uint32_t>(intervalMilliseconds, 1000);
        auto const bpm = std::isfinite(fastestBeatsPerMinute) ? std::clamp(fastestBeatsPerMinute, MinimumBeatsPerMinute, MaximumBeatsPerMinute) : 120.0;

        // Rounded up, so a step is never shorter than the interval.
        auto const ticks = static_cast<double>(milliseconds) * bpm * static_cast<double>(TicksPerQuarterNote) / 60000.0;
        return std::max<int64_t>(1, static_cast<int64_t>(std::ceil(ticks)));
    }

    _Use_decl_annotations_
    void BuildLaneMessage(AutomationLane const& lane, uint32_t value, uint32_t* words) noexcept
    {
        auto const channel = static_cast<uint32_t>(lane.Channel & 0x0F) << 16;
        auto const bank = static_cast<uint32_t>((lane.Number >> 7) & 0x7F);
        auto const index = static_cast<uint32_t>(lane.Number & 0x7F);

        words[1] = value;

        switch (lane.Target)
        {
        case LaneTarget::PitchBend:
            words[0] = 0x40E00000u | channel;
            break;

        case LaneTarget::ChannelPressure:
            words[0] = 0x40D00000u | channel;
            break;

        case LaneTarget::Program:
            words[0] = 0x40C00000u | channel;
            words[1] = ScaleDown(value, 32, 7) << 24;
            break;

        case LaneTarget::Registered:
            words[0] = 0x40200000u | channel | (bank << 8) | index;
            break;

        case LaneTarget::Assignable:
            words[0] = 0x40300000u | channel | (bank << 8) | index;
            break;

        case LaneTarget::Controller:
        default:
            words[0] = 0x40B00000u | channel | (index << 8);
            break;
        }
    }

    _Use_decl_annotations_
    void RenderTrack(
        Sequence const& sequence,
        Track const& track,
        int64_t fromTick,
        int64_t toTick,
        std::vector<RenderedMessage>& messages,
        RenderOptions const& options)
    {
        auto const firstNew = messages.size();
        auto const maximum = firstNew + options.MaximumMessages;
        size_t passes{ 0 };

        for (auto const& placement : track.Timeline)
        {
            auto const clip = FindClip(sequence, placement.ClipId);

            if (clip == nullptr || clip->Length <= 0)
            {
                continue;
            }

            auto const length = PlacementLength(placement, *clip);
            auto const end = placement.Tick + length;

            if (end <= fromTick || placement.Tick >= toTick)
            {
                continue;
            }

            if (!clip->Loop)
            {
                RenderPass(sequence, *clip, placement.Tick, std::min(end, placement.Tick + clip->Length), 0, fromTick, toTick, options, maximum, messages);
                continue;
            }

            // Only the passes that touch the window.
            auto const firstPass = fromTick > placement.Tick ? (fromTick - placement.Tick) / clip->Length : 0;

            for (auto pass = firstPass;; ++pass)
            {
                auto const passStart = placement.Tick + pass * clip->Length;

                if (passStart >= end || passStart >= toTick || messages.size() >= maximum || ++passes > MaximumRenderedPasses)
                {
                    break;
                }

                RenderPass(sequence, *clip, passStart, std::min(end, passStart + clip->Length), static_cast<uint64_t>(pass), fromTick, toTick, options, maximum, messages);
            }
        }

        if (options.Lanes)
        {
            for (auto const& lane : track.Lanes)
            {
                RenderTrackLane(sequence, track, lane, fromTick, toTick, options, maximum, messages);
            }
        }

        if (options.Lyrics && options.Midi2)
        {
            RenderLyrics(sequence, track, fromTick, toTick, options.LaneStartValues, maximum, messages);
        }

        std::stable_sort(messages.begin() + static_cast<ptrdiff_t>(firstNew), messages.end(),
            [](RenderedMessage const& a, RenderedMessage const& b)
            {
                return a.Tick != b.Tick ? a.Tick < b.Tick : a.Kind < b.Kind;
            });
    }

    _Use_decl_annotations_
    int64_t TrackEndTick(Sequence const& sequence, Track const& track) noexcept
    {
        int64_t end{ 0 };

        for (auto const& placement : track.Timeline)
        {
            if (auto const clip = FindClip(sequence, placement.ClipId); clip != nullptr)
            {
                auto const length = clip->Loop ? PlacementLength(placement, *clip) : std::min(PlacementLength(placement, *clip), clip->Length);
                end = std::max(end, placement.Tick + length);
            }
        }

        return end;
    }

    _Use_decl_annotations_
    int64_t SequenceEndTick(Sequence const& sequence) noexcept
    {
        int64_t end{ 0 };

        try
        {
            ForEachTrack(sequence, [&end, &sequence](Track const& track, size_t)
            {
                end = std::max(end, TrackEndTick(sequence, track));
                return true;
            });
        }
        catch (...)
        {
        }

        return end;
    }

    _Use_decl_annotations_
    void ApplyDestination(TrackDestination const& destination, std::array<uint32_t, 4>& words, uint8_t wordCount) noexcept
    {
        if (wordCount == 0)
        {
            return;
        }

        auto const messageType = words[0] >> 28;

        // Utility and stream messages have no group.
        if (messageType != 0x0 && messageType != 0xF)
        {
            words[0] = (words[0] & 0xF0FFFFFFu) | (static_cast<uint32_t>(destination.Group & 0x0F) << 24);
        }

        if ((messageType == 0x2 || messageType == 0x4) && destination.Channel >= 0)
        {
            words[0] = (words[0] & 0xFFF0FFFFu) | (static_cast<uint32_t>(destination.Channel & 0x0F) << 16);
        }
    }
}
