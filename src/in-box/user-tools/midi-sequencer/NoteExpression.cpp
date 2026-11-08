// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#include "NoteExpression.h"

#include <algorithm>
#include <cmath>
#include <set>

namespace midisequencer
{
    namespace
    {
        constexpr double Pi = 3.14159265358979323846;

        // Most bend messages a vibrato puts on one note, so a very long note can't fill the clip.
        constexpr size_t MostBendsPerNote = 4000;

        // Clip ticks to seconds and back, through the tempo map where the clip plays.
        struct Clock
        {
            NoteToolContext const& Context;

            double Seconds(_In_ int64_t tick) const noexcept
            {
                auto const song = Context.ClipStart + tick;
                return Context.Tempo != nullptr ? Context.Tempo->SecondsAtTick(song) : static_cast<double>(song) / (2.0 * TicksPerQuarterNote);
            }

            int64_t Tick(_In_ double seconds) const noexcept
            {
                auto const song = Context.Tempo != nullptr ? Context.Tempo->TickAtSeconds(std::max(0.0, seconds))
                    : static_cast<int64_t>(std::llround(std::max(0.0, seconds) * 2.0 * TicksPerQuarterNote));
                return song - Context.ClipStart;
            }
        };

        // The events without the Per-Note Pitch Bend on these notes.
        std::vector<ClipEvent> Without(_In_ std::vector<ClipEvent> const& events, _In_ std::vector<Note> const& notes, _Inout_ size_t& removed)
        {
            std::vector<ClipEvent> kept{};

            for (auto const& event : events)
            {
                auto const bent = PerNotePitchBendOf(event);
                auto const covered = bent.has_value() && std::any_of(notes.begin(), notes.end(), [&](Note const& note)
                {
                    return note.Channel == bent->Channel && note.Number == bent->Number && event.Tick >= note.Tick - 1 && event.Tick <= note.Tick + note.Length;
                });

                if (covered)
                {
                    ++removed;
                }
                else
                {
                    kept.push_back(event);
                }
            }

            return kept;
        }

        // The range at the clip's start for each channel bent, unless the same is there already.
        void AddRanges(_Inout_ std::vector<ClipEvent>& events, _In_ std::set<uint8_t> const& channels, _In_ double range, _In_ bool send, _Inout_ size_t& added)
        {
            if (!send)
            {
                return;
            }

            for (auto const channel : channels)
            {
                auto const event = PerNoteBendRange(0, channel, range);

                if (std::find(events.begin(), events.end(), event) == events.end())
                {
                    events.push_back(event);
                    ++added;
                }
            }
        }

        void SortByTick(_Inout_ std::vector<ClipEvent>& events)
        {
            std::stable_sort(events.begin(), events.end(), [](ClipEvent const& a, ClipEvent const& b) { return a.Tick < b.Tick; });
        }
    }

    _Use_decl_annotations_
    ClipEvent PerNotePitchBend(int64_t tick, uint8_t channel, uint8_t note, uint32_t value) noexcept
    {
        ClipEvent event{};
        event.Tick = std::max<int64_t>(0, tick);
        event.Words[0] = (0x4u << 28) | (0x6u << 20) | (static_cast<uint32_t>(channel & 0x0F) << 16) | (static_cast<uint32_t>(note & 0x7F) << 8);
        event.Words[1] = value;
        event.WordCount = 2;
        return event;
    }

    _Use_decl_annotations_
    ClipEvent PerNoteBendRange(int64_t tick, uint8_t channel, double semitones) noexcept
    {
        // Registered Controller (status 0x2), bank 0, index 7, as 7.25 fixed point.
        auto const fixed = std::clamp(std::round(std::clamp(semitones, 0.0, 127.0) * 33554432.0), 0.0, 4294967295.0);

        ClipEvent event{};
        event.Tick = std::max<int64_t>(0, tick);
        event.Words[0] = (0x4u << 28) | (0x2u << 20) | (static_cast<uint32_t>(channel & 0x0F) << 16) | (0u << 8) | 7u;
        event.Words[1] = static_cast<uint32_t>(fixed);
        event.WordCount = 2;
        return event;
    }

    _Use_decl_annotations_
    std::optional<BentNote> PerNotePitchBendOf(ClipEvent const& event) noexcept
    {
        auto const word = event.Words[0];

        if (event.WordCount < 2 || (word >> 28) != 0x4 || ((word >> 20) & 0x0F) != 0x6)
        {
            return std::nullopt;
        }

        return BentNote{ static_cast<uint8_t>((word >> 16) & 0x0F), static_cast<uint8_t>((word >> 8) & 0x7F) };
    }

    _Use_decl_annotations_
    uint32_t BendValue(double semitones, double range) noexcept
    {
        if (!(range > 0.0) || !std::isfinite(semitones))
        {
            return PitchBendCenter;
        }

        auto const share = std::clamp(semitones / range, -1.0, 1.0);

        return share >= 0
            ? PitchBendCenter + static_cast<uint32_t>(std::llround(share * 2147483647.0))
            : PitchBendCenter - static_cast<uint32_t>(std::llround(-share * 2147483648.0));
    }

    _Use_decl_annotations_
    ExpressionResult Glide(std::vector<ClipEvent> const& events, std::vector<Note> const& notes, NoteToolContext const& context, GlideSettings const& settings)
    {
        ExpressionResult result{};
        result.Events = Without(events, notes, result.Removed);

        auto sorted = notes;
        std::stable_sort(sorted.begin(), sorted.end(), [](Note const& a, Note const& b) { return a.Tick != b.Tick ? a.Tick < b.Tick : a.Number < b.Number; });

        Clock const clock{ context };
        auto const range = std::clamp(settings.RangeSemitones, 1.0, 96.0);
        auto const seconds = std::clamp(settings.TimeMs, 1.0, 2000.0) / 1000.0;
        auto const steps = std::clamp<int64_t>(std::llround(seconds * 1000.0 / 8.0), 4, 64);
        std::set<uint8_t> channels{};

        for (size_t i = 1; i < sorted.size(); ++i)
        {
            auto const& from = sorted[i - 1];
            auto const& to = sorted[i];

            // Notes of a chord don't slide into each other, and a gap breaks the line.
            if (to.Tick <= from.Tick || (settings.OnlyJoined && to.Tick > from.Tick + from.Length + 1))
            {
                continue;
            }

            auto const interval = static_cast<double>(from.Number) - static_cast<double>(to.Number);

            if (interval == 0.0 || std::abs(interval) > range)
            {
                continue;
            }

            auto end = std::min(clock.Tick(clock.Seconds(to.Tick) + seconds), to.Tick + to.Length - 1);
            end = std::max(end, to.Tick + 1);

            // Set before the note starts, so it starts at the note before's pitch.
            result.Events.push_back(PerNotePitchBend(to.Tick - 1, to.Channel, to.Number, BendValue(interval, range)));

            for (int64_t k = 1; k <= steps; ++k)
            {
                auto const x = static_cast<double>(k) / static_cast<double>(steps);
                auto const eased = x * x * (3.0 - 2.0 * x);
                auto const tick = to.Tick + (end - to.Tick) * k / steps;
                auto const value = k == steps ? PitchBendCenter : BendValue(interval * (1.0 - eased), range);
                result.Events.push_back(PerNotePitchBend(tick, to.Channel, to.Number, value));
            }

            result.Added += static_cast<size_t>(steps) + 1;
            channels.insert(to.Channel);
        }

        AddRanges(result.Events, channels, range, settings.SendRange, result.Added);
        SortByTick(result.Events);
        return result;
    }

    _Use_decl_annotations_
    ExpressionResult Vibrato(std::vector<ClipEvent> const& events, std::vector<Note> const& notes, NoteToolContext const& context, VibratoSettings const& settings)
    {
        ExpressionResult result{};
        result.Events = Without(events, notes, result.Removed);

        Clock const clock{ context };
        auto const range = std::clamp(settings.RangeSemitones, 0.1, 96.0);
        auto const rate = std::clamp(settings.RateHz, 0.5, 20.0);
        auto const depth = std::clamp(settings.DepthCents, 0.0, 1200.0) / 100.0;
        auto const fade = std::max(0.0, settings.FadeInMs) / 1000.0;
        auto const stepSeconds = 1.0 / (rate * 16.0);
        std::set<uint8_t> channels{};

        for (auto const& note : notes)
        {
            auto const startSeconds = clock.Seconds(note.Tick);
            auto const endSeconds = clock.Seconds(note.Tick + note.Length);

            if (endSeconds - startSeconds < std::max(0.0, settings.ShortestMs) / 1000.0)
            {
                continue;
            }

            auto const from = startSeconds + std::max(0.0, settings.DelayMs) / 1000.0;
            auto const last = note.Tick + note.Length - 1;
            int64_t previous{ -1 };
            size_t made{ 0 };

            for (auto at = from; at < endSeconds && made < MostBendsPerNote; at += stepSeconds)
            {
                auto const tick = clock.Tick(at);

                if (tick >= last)
                {
                    break;
                }

                if (tick <= previous)
                {
                    continue;
                }

                auto const into = at - from;
                auto const swell = fade > 0.0 ? std::min(1.0, into / fade) : 1.0;
                auto const semitones = depth * swell * std::sin(2.0 * Pi * rate * into);
                result.Events.push_back(PerNotePitchBend(tick, note.Channel, note.Number, BendValue(semitones, range)));
                previous = tick;
                ++made;
            }

            if (made == 0)
            {
                continue;
            }

            // Back to the middle just before the note ends, so the next note of the same number
            // starts in tune.
            result.Events.push_back(PerNotePitchBend(std::max(previous + 1, last), note.Channel, note.Number, PitchBendCenter));
            result.Added += made + 1;
            channels.insert(note.Channel);
        }

        AddRanges(result.Events, channels, range, settings.SendRange, result.Added);
        SortByTick(result.Events);
        return result;
    }

    _Use_decl_annotations_
    ExpressionResult RemovePerNoteBend(std::vector<ClipEvent> const& events, std::vector<Note> const& notes)
    {
        ExpressionResult result{};
        result.Events = Without(events, notes, result.Removed);
        return result;
    }
}
