// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#include "SequenceRender.h"

#include <algorithm>

namespace midisequencer
{
    namespace
    {
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

        // One pass through a clip, which starts at passStart and is cut off at passEnd.
        void RenderPass(
            _In_ Clip const& clip,
            _In_ int64_t passStart,
            _In_ int64_t passEnd,
            _In_ uint64_t pass,
            _In_ int64_t fromTick,
            _In_ int64_t toTick,
            _In_ size_t maximum,
            _Inout_ std::vector<RenderedMessage>& messages)
        {
            auto const windowStart = std::max(fromTick, passStart);
            auto const windowEnd = std::min(toTick, passEnd);

            if (windowStart >= windowEnd)
            {
                return;
            }

            // A note comes out with its end, even when the end is past the window, so the engine
            // never has to remember which notes it started. Ends are harmless to send early.
            auto firstNote = std::lower_bound(clip.Notes.begin(), clip.Notes.end(), windowStart - passStart,
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
    void RenderTrack(
        Sequence const& sequence,
        Track const& track,
        int64_t fromTick,
        int64_t toTick,
        std::vector<RenderedMessage>& messages,
        size_t maximumMessages)
    {
        auto const firstNew = messages.size();
        auto const maximum = firstNew + maximumMessages;

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
                RenderPass(*clip, placement.Tick, std::min(end, placement.Tick + clip->Length), 0, fromTick, toTick, maximum, messages);
                continue;
            }

            // Only the passes that touch the window.
            auto const firstPass = fromTick > placement.Tick ? (fromTick - placement.Tick) / clip->Length : 0;

            for (auto pass = firstPass;; ++pass)
            {
                auto const passStart = placement.Tick + pass * clip->Length;

                if (passStart >= end || passStart >= toTick || messages.size() >= maximum)
                {
                    break;
                }

                RenderPass(*clip, passStart, std::min(end, passStart + clip->Length), static_cast<uint64_t>(pass), fromTick, toTick, maximum, messages);
            }
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
