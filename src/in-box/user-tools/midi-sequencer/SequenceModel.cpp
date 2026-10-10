// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#include "SequenceModel.h"

#include <algorithm>
#include <cmath>
#include <format>
#include <random>
#include <unordered_set>

namespace midisequencer
{
    namespace
    {
        // One body for the const and non-const searches: the pointer type follows the vector's.
        template<typename TVector>
        auto FindTrackIn(_In_ TVector& tracks, _In_ std::wstring_view id) noexcept -> decltype(tracks.data())
        {
            for (auto& track : tracks)
            {
                if (track.Id == id)
                {
                    return &track;
                }

                if (auto found = FindTrackIn(track.Children, id); found != nullptr)
                {
                    return found;
                }
            }

            return nullptr;
        }

        bool VisitTracks(
            _In_ std::vector<Track> const& tracks,
            _In_ size_t depth,
            _In_ std::function<bool(Track const&, size_t)> const& visit)
        {
            for (auto const& track : tracks)
            {
                if (!visit(track, depth))
                {
                    return false;
                }

                if (!VisitTracks(track.Children, depth + 1, visit))
                {
                    return false;
                }
            }

            return true;
        }

        bool IsPowerOfTwoDenominator(_In_ uint8_t value) noexcept
        {
            return value == 1 || value == 2 || value == 4 || value == 8 || value == 16 || value == 32;
        }

        uint64_t SplitMix64(_In_ uint64_t value) noexcept
        {
            value += 0x9E3779B97F4A7C15ull;
            value = (value ^ (value >> 30)) * 0xBF58476D1CE4E5B9ull;
            value = (value ^ (value >> 27)) * 0x94D049BB133111EBull;
            return value ^ (value >> 31);
        }

        void NormalizeTags(_Inout_ std::vector<Tag>& tags)
        {
            std::erase_if(tags, [](Tag const& tag) { return tag.Text.empty(); });

            for (auto& tag : tags)
            {
                tag.Tick = std::max<int64_t>(0, tag.Tick);
            }

            std::stable_sort(tags.begin(), tags.end(), [](Tag const& a, Tag const& b) { return a.Tick < b.Tick; });
        }

        void NormalizeTracks(
            _Inout_ std::vector<Track>& tracks,
            _In_ Sequence const& sequence,
            _In_ std::unordered_set<std::wstring> const& clipIds,
            _Inout_ std::unordered_set<std::wstring>& trackIds)
        {
            for (auto& track : tracks)
            {
                if (track.Id.empty() || trackIds.contains(track.Id))
                {
                    track.Id = NewId(track.IsFolder ? L"f" : L"t");
                }

                trackIds.insert(track.Id);
                track.Color &= 0xFFFFFF;

                track.Source.Group = static_cast<int8_t>(std::clamp<int>(track.Source.Group, -1, 15));
                track.Destination.Group = static_cast<uint8_t>(std::min<int>(track.Destination.Group, 15));
                track.Destination.Channel = static_cast<int8_t>(std::clamp<int>(track.Destination.Channel, -1, 15));

                NormalizeTags(track.Tags);

                if (track.IsFolder)
                {
                    // A folder plays nothing of its own.
                    track.Timeline.clear();
                    track.Slots.clear();
                    track.Startup.clear();

                    NormalizeTracks(track.Children, sequence, clipIds, trackIds);
                    continue;
                }

                track.Children.clear();

                std::erase_if(track.Timeline, [&clipIds](Placement const& placement)
                {
                    return !clipIds.contains(placement.ClipId);
                });

                for (auto& placement : track.Timeline)
                {
                    placement.Tick = std::max<int64_t>(0, placement.Tick);
                    placement.Length = std::max<int64_t>(0, placement.Length);
                }

                std::stable_sort(track.Timeline.begin(), track.Timeline.end(),
                    [](Placement const& a, Placement const& b) { return a.Tick < b.Tick; });

                track.Slots.resize(sequence.Scenes.size());

                for (auto& slot : track.Slots)
                {
                    if (!slot.empty() && !clipIds.contains(slot))
                    {
                        slot.clear();
                    }
                }

                std::erase_if(track.Startup, [](ClipEvent const& event)
                {
                    return event.WordCount == 0 || event.WordCount != UmpWordCount(event.Words[0]);
                });
            }
        }
    }

    _Use_decl_annotations_
    Track* FindTrack(Sequence& sequence, std::wstring_view id) noexcept
    {
        return FindTrackIn(sequence.Tracks, id);
    }

    _Use_decl_annotations_
    Track const* FindTrack(Sequence const& sequence, std::wstring_view id) noexcept
    {
        return FindTrackIn(sequence.Tracks, id);
    }

    _Use_decl_annotations_
    Clip* FindClip(Sequence& sequence, std::wstring_view id) noexcept
    {
        auto found = std::find_if(sequence.Clips.begin(), sequence.Clips.end(), [id](Clip const& clip) { return clip.Id == id; });
        return found == sequence.Clips.end() ? nullptr : &*found;
    }

    _Use_decl_annotations_
    Clip const* FindClip(Sequence const& sequence, std::wstring_view id) noexcept
    {
        auto found = std::find_if(sequence.Clips.begin(), sequence.Clips.end(), [id](Clip const& clip) { return clip.Id == id; });
        return found == sequence.Clips.end() ? nullptr : &*found;
    }

    _Use_decl_annotations_
    void ForEachTrack(
        Sequence const& sequence,
        std::function<bool(Track const& track, size_t depth)> const& visit)
    {
        VisitTracks(sequence.Tracks, 0, visit);
    }

    _Use_decl_annotations_
    size_t CountTracks(Sequence const& sequence) noexcept
    {
        size_t count{ 0 };

        try
        {
            ForEachTrack(sequence, [&count](Track const&, size_t) { ++count; return true; });
        }
        catch (...)
        {
        }

        return count;
    }

    _Use_decl_annotations_
    size_t CountClipUses(Sequence const& sequence, std::wstring_view clipId) noexcept
    {
        size_t count{ 0 };

        try
        {
            ForEachTrack(sequence, [&count, clipId](Track const& track, size_t)
            {
                count += static_cast<size_t>(std::count_if(track.Timeline.begin(), track.Timeline.end(),
                    [clipId](Placement const& placement) { return placement.ClipId == clipId; }));
                count += static_cast<size_t>(std::count_if(track.Slots.begin(), track.Slots.end(),
                    [clipId](std::wstring const& slot) { return slot == clipId; }));
                return true;
            });
        }
        catch (...)
        {
        }

        return count;
    }

    _Use_decl_annotations_
    std::wstring NewId(std::wstring_view prefix)
    {
        static thread_local std::mt19937 generator{ std::random_device{}() };

        return std::format(L"{}-{:08x}", prefix, static_cast<uint32_t>(generator()));
    }

    _Use_decl_annotations_
    void SortClip(Clip& clip)
    {
        std::stable_sort(clip.Notes.begin(), clip.Notes.end(), [](Note const& a, Note const& b)
        {
            return a.Tick != b.Tick ? a.Tick < b.Tick : a.Number < b.Number;
        });

        std::stable_sort(clip.Events.begin(), clip.Events.end(), [](ClipEvent const& a, ClipEvent const& b)
        {
            return a.Tick < b.Tick;
        });
    }

    _Use_decl_annotations_
    void NormalizeSequence(Sequence& sequence)
    {
        // ---- tempo ----

        for (auto& point : sequence.Tempo)
        {
            point.Tick = std::max<int64_t>(0, point.Tick);

            if (!std::isfinite(point.BeatsPerMinute))
            {
                point.BeatsPerMinute = 120.0;
            }

            point.BeatsPerMinute = std::clamp(point.BeatsPerMinute, MinimumBeatsPerMinute, MaximumBeatsPerMinute);
        }

        std::stable_sort(sequence.Tempo.begin(), sequence.Tempo.end(),
            [](TempoPoint const& a, TempoPoint const& b) { return a.Tick < b.Tick; });

        // The last point at a tick wins, the way a later message overrides an earlier one.
        for (size_t i = 0; i + 1 < sequence.Tempo.size();)
        {
            if (sequence.Tempo[i].Tick == sequence.Tempo[i + 1].Tick)
            {
                sequence.Tempo.erase(sequence.Tempo.begin() + static_cast<ptrdiff_t>(i));
            }
            else
            {
                ++i;
            }
        }

        if (sequence.Tempo.empty())
        {
            sequence.Tempo.push_back(TempoPoint{});
        }
        else if (sequence.Tempo.front().Tick > 0)
        {
            auto first = sequence.Tempo.front();
            first.Tick = 0;
            first.RampToNext = false;
            sequence.Tempo.insert(sequence.Tempo.begin(), first);
        }

        sequence.Tempo.back().RampToNext = false;

        // ---- meter ----

        for (auto& meter : sequence.Meter)
        {
            meter.Tick = std::max<int64_t>(0, meter.Tick);
            meter.Numerator = std::clamp<uint8_t>(meter.Numerator, 1, 64);

            if (!IsPowerOfTwoDenominator(meter.Denominator))
            {
                meter.Denominator = 4;
            }
        }

        std::stable_sort(sequence.Meter.begin(), sequence.Meter.end(),
            [](MeterChange const& a, MeterChange const& b) { return a.Tick < b.Tick; });

        if (sequence.Meter.empty())
        {
            sequence.Meter.push_back(MeterChange{});
        }
        else if (sequence.Meter.front().Tick > 0)
        {
            auto first = sequence.Meter.front();
            first.Tick = 0;
            sequence.Meter.insert(sequence.Meter.begin(), first);
        }

        // A meter change can only start a bar. One that doesn't moves back to where its bar starts.
        for (size_t i = 1; i < sequence.Meter.size(); ++i)
        {
            auto const& previous = sequence.Meter[i - 1];
            auto const barTicks = TicksPerBar(previous);
            auto const offset = (sequence.Meter[i].Tick - previous.Tick) % barTicks;
            sequence.Meter[i].Tick -= offset;
        }

        for (size_t i = 0; i + 1 < sequence.Meter.size();)
        {
            if (sequence.Meter[i].Tick == sequence.Meter[i + 1].Tick)
            {
                sequence.Meter.erase(sequence.Meter.begin() + static_cast<ptrdiff_t>(i));
            }
            else
            {
                ++i;
            }
        }

        NormalizeTags(sequence.Tags);

        // ---- scenes ----

        std::unordered_set<std::wstring> sceneIds{};

        for (auto& scene : sequence.Scenes)
        {
            if (scene.Id.empty() || sceneIds.contains(scene.Id))
            {
                scene.Id = NewId(L"s");
            }

            sceneIds.insert(scene.Id);
        }

        // ---- clips ----

        std::unordered_set<std::wstring> clipIds{};

        for (auto& clip : sequence.Clips)
        {
            if (clip.Id.empty() || clipIds.contains(clip.Id))
            {
                clip.Id = NewId(L"c");
            }

            clipIds.insert(clip.Id);

            clip.Length = std::max<int64_t>(1, clip.Length);

            if (clip.Color.has_value())
            {
                clip.Color = *clip.Color & 0xFFFFFF;
            }

            std::erase_if(clip.Notes, [](Note const& note) { return note.Tick < 0; });

            for (auto& note : clip.Notes)
            {
                note.Length = std::max<int64_t>(1, note.Length);
                note.Channel &= 0x0F;
                note.Number &= 0x7F;
                note.Chance = std::min<uint8_t>(note.Chance, 100);
            }

            std::erase_if(clip.Events, [](ClipEvent const& event)
            {
                return event.Tick < 0 || event.WordCount == 0 || event.WordCount != UmpWordCount(event.Words[0]);
            });

            SortClip(clip);
        }

        // ---- tracks ----

        std::unordered_set<std::wstring> trackIds{};
        NormalizeTracks(sequence.Tracks, sequence, clipIds, trackIds);
    }

    // ---- tempo map ----

    _Use_decl_annotations_
    TempoMap::TempoMap(std::vector<TempoPoint> const& points)
    {
        if (points.empty())
        {
            return;
        }

        m_segments.clear();
        m_segments.reserve(points.size());

        double seconds{ 0 };

        for (size_t i = 0; i < points.size(); ++i)
        {
            Segment segment{};
            segment.StartTick = i == 0 ? 0 : points[i].Tick;
            segment.EndTick = i + 1 < points.size() ? points[i + 1].Tick : INT64_MAX;
            segment.StartSeconds = seconds;
            segment.StartBeatsPerMinute = points[i].BeatsPerMinute;
            segment.EndBeatsPerMinute = (points[i].RampToNext && i + 1 < points.size())
                ? points[i + 1].BeatsPerMinute
                : points[i].BeatsPerMinute;

            m_segments.push_back(segment);

            if (segment.EndTick != INT64_MAX)
            {
                auto const length = static_cast<double>(segment.EndTick - segment.StartTick);
                auto const b0 = segment.StartBeatsPerMinute;
                auto const b1 = segment.EndBeatsPerMinute;

                if (b0 == b1 || length <= 0)
                {
                    seconds += length * 60.0 / (b0 * TicksPerQuarterNote);
                }
                else
                {
                    auto const slope = (b1 - b0) / length;
                    seconds += (60.0 / TicksPerQuarterNote) * std::log(b1 / b0) / slope;
                }
            }
        }
    }

    _Use_decl_annotations_
    size_t TempoMap::SegmentIndexAtTick(int64_t tick) const noexcept
    {
        auto found = std::upper_bound(m_segments.begin(), m_segments.end(), tick,
            [](int64_t value, Segment const& segment) { return value < segment.StartTick; });

        return found == m_segments.begin() ? 0 : static_cast<size_t>(std::distance(m_segments.begin(), found) - 1);
    }

    _Use_decl_annotations_
    size_t TempoMap::SegmentIndexAtSeconds(double seconds) const noexcept
    {
        auto found = std::upper_bound(m_segments.begin(), m_segments.end(), seconds,
            [](double value, Segment const& segment) { return value < segment.StartSeconds; });

        return found == m_segments.begin() ? 0 : static_cast<size_t>(std::distance(m_segments.begin(), found) - 1);
    }

    _Use_decl_annotations_
    double TempoMap::SecondsAtTick(int64_t tick) const noexcept
    {
        if (tick <= 0)
        {
            return 0.0;
        }

        auto const& segment = m_segments[SegmentIndexAtTick(tick)];
        auto const into = static_cast<double>(tick - segment.StartTick);
        auto const b0 = segment.StartBeatsPerMinute;
        auto const b1 = segment.EndBeatsPerMinute;

        if (b0 == b1 || segment.EndTick == INT64_MAX)
        {
            return segment.StartSeconds + into * 60.0 / (b0 * TicksPerQuarterNote);
        }

        auto const slope = (b1 - b0) / static_cast<double>(segment.EndTick - segment.StartTick);
        return segment.StartSeconds + (60.0 / TicksPerQuarterNote) * std::log((b0 + slope * into) / b0) / slope;
    }

    _Use_decl_annotations_
    int64_t TempoMap::TickAtSeconds(double seconds) const noexcept
    {
        if (!(seconds > 0.0))
        {
            return 0;
        }

        auto const& segment = m_segments[SegmentIndexAtSeconds(seconds)];
        auto const into = seconds - segment.StartSeconds;
        auto const b0 = segment.StartBeatsPerMinute;
        auto const b1 = segment.EndBeatsPerMinute;

        double ticks{ 0 };

        if (b0 == b1 || segment.EndTick == INT64_MAX)
        {
            ticks = into * b0 * TicksPerQuarterNote / 60.0;
        }
        else
        {
            auto const slope = (b1 - b0) / static_cast<double>(segment.EndTick - segment.StartTick);
            ticks = b0 * (std::exp(into * slope * TicksPerQuarterNote / 60.0) - 1.0) / slope;
        }

        return segment.StartTick + static_cast<int64_t>(std::floor(ticks + 1e-6));
    }

    _Use_decl_annotations_
    double TempoMap::BeatsPerMinuteAtTick(int64_t tick) const noexcept
    {
        auto const& segment = m_segments[SegmentIndexAtTick(std::max<int64_t>(0, tick))];

        if (segment.StartBeatsPerMinute == segment.EndBeatsPerMinute || segment.EndTick == INT64_MAX)
        {
            return segment.StartBeatsPerMinute;
        }

        auto const fraction = static_cast<double>(tick - segment.StartTick) / static_cast<double>(segment.EndTick - segment.StartTick);
        return segment.StartBeatsPerMinute + (segment.EndBeatsPerMinute - segment.StartBeatsPerMinute) * fraction;
    }

    // ---- meter ----

    _Use_decl_annotations_
    int64_t TicksPerBeat(MeterChange const& meter) noexcept
    {
        return TicksPerQuarterNote * 4 / std::max<int64_t>(1, meter.Denominator);
    }

    _Use_decl_annotations_
    int64_t TicksPerBar(MeterChange const& meter) noexcept
    {
        return std::max<int64_t>(1, meter.Numerator) * TicksPerBeat(meter);
    }

    _Use_decl_annotations_
    MeterChange const& MeterAtTick(std::vector<MeterChange> const& meter, int64_t tick) noexcept
    {
        static MeterChange const fallback{};

        if (meter.empty())
        {
            return fallback;
        }

        auto found = std::upper_bound(meter.begin(), meter.end(), tick,
            [](int64_t value, MeterChange const& change) { return value < change.Tick; });

        return found == meter.begin() ? meter.front() : *(found - 1);
    }

    _Use_decl_annotations_
    BarPosition BarPositionAtTick(std::vector<MeterChange> const& meter, int64_t tick) noexcept
    {
        static std::vector<MeterChange> const fallback{ MeterChange{} };
        auto const& changes = meter.empty() ? fallback : meter;

        tick = std::max<int64_t>(0, tick);

        int64_t bar{ 1 };

        for (size_t i = 0; i < changes.size(); ++i)
        {
            auto const& change = changes[i];
            auto const barTicks = TicksPerBar(change);
            auto const next = i + 1 < changes.size() ? changes[i + 1].Tick : INT64_MAX;

            if (tick < next)
            {
                auto const into = tick - change.Tick;
                auto const beatTicks = TicksPerBeat(change);
                auto const intoBar = into % barTicks;

                return BarPosition{ bar + into / barTicks, 1 + intoBar / beatTicks, intoBar % beatTicks };
            }

            bar += (next - change.Tick + barTicks - 1) / barTicks;
        }

        return BarPosition{ bar, 1, 0 };
    }

    _Use_decl_annotations_
    int64_t TickAtBar(std::vector<MeterChange> const& meter, int64_t bar) noexcept
    {
        static std::vector<MeterChange> const fallback{ MeterChange{} };
        auto const& changes = meter.empty() ? fallback : meter;

        bar = std::max<int64_t>(1, bar);

        int64_t firstBarOfChange{ 1 };

        for (size_t i = 0; i < changes.size(); ++i)
        {
            auto const& change = changes[i];
            auto const barTicks = TicksPerBar(change);

            if (i + 1 < changes.size())
            {
                auto const barsInChange = (changes[i + 1].Tick - change.Tick + barTicks - 1) / barTicks;

                if (bar < firstBarOfChange + barsInChange)
                {
                    return change.Tick + (bar - firstBarOfChange) * barTicks;
                }

                firstBarOfChange += barsInChange;
            }
            else
            {
                return change.Tick + (bar - firstBarOfChange) * barTicks;
            }
        }

        return 0;
    }

    // ---- values ----

    _Use_decl_annotations_
    uint32_t ScaleUp(uint32_t value, uint8_t fromBits, uint8_t toBits) noexcept
    {
        if (fromBits == 0 || fromBits > 32 || toBits > 32)
        {
            return 0;
        }

        if (toBits <= fromBits)
        {
            return ScaleDown(value, fromBits, toBits);
        }

        auto const fromMask = fromBits == 32 ? 0xFFFFFFFFu : ((1u << fromBits) - 1u);
        value &= fromMask;

        auto const scaleBits = static_cast<uint8_t>(toBits - fromBits);
        auto shifted = static_cast<uint32_t>(static_cast<uint64_t>(value) << scaleBits);
        auto const center = 1u << (fromBits - 1);

        if (value <= center || fromBits == 1)
        {
            return shifted;
        }

        // Above the center, repeat the bits below the top one into the new low bits, so the
        // largest value maps to the largest value.
        auto const repeatBits = static_cast<uint8_t>(fromBits - 1);
        auto const repeatMask = (1u << repeatBits) - 1u;
        auto repeat = value & repeatMask;

        if (scaleBits > repeatBits)
        {
            repeat <<= (scaleBits - repeatBits);
        }
        else
        {
            repeat >>= (repeatBits - scaleBits);
        }

        while (repeat != 0)
        {
            shifted |= repeat;
            repeat >>= repeatBits;
        }

        return shifted;
    }

    _Use_decl_annotations_
    uint32_t ScaleDown(uint32_t value, uint8_t fromBits, uint8_t toBits) noexcept
    {
        if (fromBits == 0 || fromBits > 32 || toBits > 32 || toBits == 0)
        {
            return 0;
        }

        if (toBits >= fromBits)
        {
            return toBits == fromBits ? value : ScaleUp(value, fromBits, toBits);
        }

        auto const fromMask = fromBits == 32 ? 0xFFFFFFFFu : ((1u << fromBits) - 1u);
        return (value & fromMask) >> (fromBits - toBits);
    }

    _Use_decl_annotations_
    bool NotePlaysOnPass(Note const& note, uint32_t clipSeed, uint32_t noteIndex, uint64_t pass) noexcept
    {
        if (note.Chance >= 100)
        {
            return true;
        }

        if (note.Chance == 0)
        {
            return false;
        }

        auto const mixed = SplitMix64((static_cast<uint64_t>(clipSeed) << 32) ^ SplitMix64(pass) ^ (static_cast<uint64_t>(noteIndex) * 0x9E3779B1ull));
        return (mixed % 100) < note.Chance;
    }

    _Use_decl_annotations_
    uint8_t UmpWordCount(uint32_t firstWord) noexcept
    {
        static constexpr uint8_t counts[16]{ 1, 1, 1, 2, 2, 4, 1, 1, 2, 2, 2, 3, 3, 4, 4, 4 };
        return counts[(firstWord >> 28) & 0x0F];
    }

    _Use_decl_annotations_
    std::string ToUtf8(std::wstring_view text)
    {
        std::string bytes{};
        bytes.reserve(text.size());

        for (size_t i = 0; i < text.size(); ++i)
        {
            uint32_t code = text[i];

            if (code >= 0xD800 && code <= 0xDBFF && i + 1 < text.size() && text[i + 1] >= 0xDC00 && text[i + 1] <= 0xDFFF)
            {
                code = 0x10000 + ((code - 0xD800) << 10) + (static_cast<uint32_t>(text[i + 1]) - 0xDC00);
                ++i;
            }
            else if (code >= 0xD800 && code <= 0xDFFF)
            {
                code = 0xFFFD;
            }

            if (code < 0x80)
            {
                bytes.push_back(static_cast<char>(code));
            }
            else if (code < 0x800)
            {
                bytes.push_back(static_cast<char>(0xC0 | (code >> 6)));
                bytes.push_back(static_cast<char>(0x80 | (code & 0x3F)));
            }
            else if (code < 0x10000)
            {
                bytes.push_back(static_cast<char>(0xE0 | (code >> 12)));
                bytes.push_back(static_cast<char>(0x80 | ((code >> 6) & 0x3F)));
                bytes.push_back(static_cast<char>(0x80 | (code & 0x3F)));
            }
            else
            {
                bytes.push_back(static_cast<char>(0xF0 | (code >> 18)));
                bytes.push_back(static_cast<char>(0x80 | ((code >> 12) & 0x3F)));
                bytes.push_back(static_cast<char>(0x80 | ((code >> 6) & 0x3F)));
                bytes.push_back(static_cast<char>(0x80 | (code & 0x3F)));
            }
        }

        return bytes;
    }

    _Use_decl_annotations_
    std::wstring FromUtf8(std::string_view bytes)
    {
        std::wstring text{};
        text.reserve(bytes.size());

        for (size_t i = 0; i < bytes.size();)
        {
            auto const lead = static_cast<uint8_t>(bytes[i]);
            uint32_t code{ 0xFFFD };
            size_t length{ 1 };

            if (lead < 0x80)
            {
                code = lead;
            }
            else if ((lead & 0xE0) == 0xC0) { length = 2; code = lead & 0x1Fu; }
            else if ((lead & 0xF0) == 0xE0) { length = 3; code = lead & 0x0Fu; }
            else if ((lead & 0xF8) == 0xF0) { length = 4; code = lead & 0x07u; }

            if (length > 1)
            {
                bool valid = i + length <= bytes.size();

                for (size_t k = 1; valid && k < length; ++k)
                {
                    auto const next = static_cast<uint8_t>(bytes[i + k]);
                    valid = (next & 0xC0) == 0x80;
                    code = (code << 6) | (next & 0x3Fu);
                }

                auto const minimum = length == 2 ? 0x80u : length == 3 ? 0x800u : 0x10000u;

                if (!valid || code < minimum || code > 0x10FFFF || (code >= 0xD800 && code <= 0xDFFF))
                {
                    code = 0xFFFD;
                    length = 1;
                }
            }
            else if (lead >= 0x80)
            {
                // A continuation byte with no lead, or a lead byte UTF-8 doesn't use.
                code = 0xFFFD;
            }

            if (code >= 0x10000)
            {
                code -= 0x10000;
                text.push_back(static_cast<wchar_t>(0xD800 + (code >> 10)));
                text.push_back(static_cast<wchar_t>(0xDC00 + (code & 0x3FF)));
            }
            else
            {
                text.push_back(static_cast<wchar_t>(code));
            }

            i += length;
        }

        return text;
    }
}
