// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#include "MidiClipFile.h"

#include "SequenceRender.h"

#include <algorithm>
#include <cmath>
#include <deque>
#include <unordered_map>

namespace midisequencer
{
    namespace
    {
        constexpr uint8_t FileHeader[8]{ 'S', 'M', 'F', '2', 'C', 'L', 'I', 'P' };

        constexpr uint32_t UtilityStatusNoop = 0x0;
        constexpr uint32_t UtilityStatusTicksPerQuarterNote = 0x3;
        constexpr uint32_t UtilityStatusDeltaClockstamp = 0x4;

        // A Delta Clockstamp has 20 bits. A longer gap is a full one, a NOOP to restart the count,
        // then the rest (M2-104-UM 7.2.3.2).
        constexpr int64_t MaximumDelta = 0xFFFFF;

        constexpr uint32_t StreamStatusStartOfClip = 0x20;
        constexpr uint32_t StreamStatusEndOfClip = 0x21;

        // A Set Tempo in a file goes where a MIDI clock would: every 1/24 of a quarter note.
        constexpr int64_t TicksPerMidiClock = TicksPerQuarterNote / 24;

        void AppendWord(_Inout_ std::vector<uint8_t>& bytes, _In_ uint32_t word)
        {
            bytes.push_back(static_cast<uint8_t>(word >> 24));
            bytes.push_back(static_cast<uint8_t>(word >> 16));
            bytes.push_back(static_cast<uint8_t>(word >> 8));
            bytes.push_back(static_cast<uint8_t>(word));
        }

        void AppendMessage(_Inout_ std::vector<uint8_t>& bytes, _In_ ClipEvent const& event)
        {
            for (uint8_t i = 0; i < event.WordCount && i < 4; ++i)
            {
                AppendWord(bytes, event.Words[i]);
            }
        }

        void AppendDelta(_Inout_ std::vector<uint8_t>& bytes, _In_ int64_t ticks)
        {
            ticks = std::max<int64_t>(0, ticks);

            while (ticks > MaximumDelta)
            {
                AppendWord(bytes, (UtilityStatusDeltaClockstamp << 20) | static_cast<uint32_t>(MaximumDelta));
                AppendWord(bytes, UtilityStatusNoop);
                ticks -= MaximumDelta;
            }

            AppendWord(bytes, (UtilityStatusDeltaClockstamp << 20) | static_cast<uint32_t>(ticks));
        }

        void AppendStreamMessage(_Inout_ std::vector<uint8_t>& bytes, _In_ uint32_t status)
        {
            AppendWord(bytes, 0xF0000000u | (status << 16));
            AppendWord(bytes, 0);
            AppendWord(bytes, 0);
            AppendWord(bytes, 0);
        }

        ClipEvent MakeEvent(_In_ int64_t tick, _In_reads_(count) uint32_t const* words, _In_ uint8_t count)
        {
            ClipEvent event{};
            event.Tick = tick;
            event.WordCount = std::min<uint8_t>(count, 4);
            std::copy_n(words, event.WordCount, event.Words.begin());
            return event;
        }

        std::vector<ClipEvent> SortedByTick(_In_ std::vector<ClipEvent> const& events)
        {
            auto sorted = events;
            std::stable_sort(sorted.begin(), sorted.end(), [](ClipEvent const& a, ClipEvent const& b) { return a.Tick < b.Tick; });
            return sorted;
        }

        int64_t SnapToClock(_In_ int64_t tick) noexcept
        {
            return ((tick + TicksPerMidiClock / 2) / TicksPerMidiClock) * TicksPerMidiClock;
        }

        struct OpenNote
        {
            size_t Index{ 0 };
        };
    }

    _Use_decl_annotations_
    void BuildSetTempo(double beatsPerMinute, uint8_t group, uint32_t* words) noexcept
    {
        auto const bpm = std::clamp(std::isfinite(beatsPerMinute) ? beatsPerMinute : 120.0, 1.5, 100000.0);
        auto const units = std::clamp(std::llround(6.0e9 / bpm), 1ll, static_cast<long long>(UINT32_MAX));

        words[0] = 0xD0100000u | (static_cast<uint32_t>(group & 0x0F) << 24);
        words[1] = static_cast<uint32_t>(units);
        words[2] = 0;
        words[3] = 0;
    }

    _Use_decl_annotations_
    bool ReadSetTempo(ClipEvent const& event, double& beatsPerMinute) noexcept
    {
        beatsPerMinute = 0;

        if (event.WordCount != 4 || (event.Words[0] >> 28) != 0xD || (event.Words[0] & 0xFFFF) != 0x0000 || event.Words[1] == 0)
        {
            return false;
        }

        beatsPerMinute = 6.0e9 / static_cast<double>(event.Words[1]);
        return true;
    }

    _Use_decl_annotations_
    void BuildSetTimeSignature(MeterChange const& meter, uint8_t group, uint32_t* words) noexcept
    {
        uint32_t power{ 0 };

        while (power < 5 && (1u << power) < meter.Denominator)
        {
            ++power;
        }

        words[0] = 0xD0100001u | (static_cast<uint32_t>(group & 0x0F) << 24);
        words[1] = (static_cast<uint32_t>(meter.Numerator) << 24) | (power << 16) | (8u << 8);
        words[2] = 0;
        words[3] = 0;
    }

    _Use_decl_annotations_
    bool ReadSetTimeSignature(ClipEvent const& event, MeterChange& meter) noexcept
    {
        meter = MeterChange{};

        if (event.WordCount != 4 || (event.Words[0] >> 28) != 0xD || (event.Words[0] & 0xFFFF) != 0x0001)
        {
            return false;
        }

        auto const numerator = event.Words[1] >> 24;
        auto const power = (event.Words[1] >> 16) & 0xFF;

        // A power of 0 means a denominator the format can't name.
        if (numerator == 0 || power == 0 || power > 5)
        {
            return false;
        }

        meter.Numerator = static_cast<uint8_t>(std::min<uint32_t>(numerator, 64));
        meter.Denominator = static_cast<uint8_t>(1u << power);
        return true;
    }

    _Use_decl_annotations_
    void AppendMetadataText(std::vector<ClipEvent>& events, int64_t tick, uint8_t status, std::wstring_view text, uint8_t group)
    {
        auto const bytes = ToUtf8(text);

        if (bytes.empty())
        {
            return;
        }

        auto const packets = (bytes.size() + 11) / 12;

        for (size_t packet = 0; packet < packets; ++packet)
        {
            uint32_t form{ 0 };

            if (packets > 1)
            {
                form = packet == 0 ? 1u : packet + 1 == packets ? 3u : 2u;
            }

            uint32_t words[4]{};
            words[0] = 0xD0000000u | (static_cast<uint32_t>(group & 0x0F) << 24) | (form << 22) | (1u << 20) | (0x01u << 8) | status;

            for (size_t i = 0; i < 12; ++i)
            {
                auto const at = packet * 12 + i;
                auto const value = at < bytes.size() ? static_cast<uint8_t>(bytes[at]) : 0u;
                words[1 + i / 4] |= static_cast<uint32_t>(value) << (24 - 8 * (i % 4));
            }

            events.push_back(MakeEvent(tick, words, 4));
        }
    }

    _Use_decl_annotations_
    std::vector<uint8_t> WriteClipFile(ClipFile const& file)
    {
        std::vector<uint8_t> bytes{};
        bytes.reserve(64 + (file.Events.size() + file.Configuration.size()) * 16);
        bytes.insert(bytes.end(), std::begin(FileHeader), std::end(FileHeader));

        for (auto const& profile : file.Profiles)
        {
            AppendMessage(bytes, profile);
        }

        AppendDelta(bytes, 0);
        AppendWord(bytes, (UtilityStatusTicksPerQuarterNote << 20) | std::max<uint16_t>(1, file.TicksPerQuarterNote));

        int64_t last{ 0 };

        for (auto const& message : SortedByTick(file.Configuration))
        {
            AppendDelta(bytes, message.Tick - last);
            AppendMessage(bytes, message);
            last = std::max(last, message.Tick);
        }

        AppendDelta(bytes, 0);
        AppendStreamMessage(bytes, StreamStatusStartOfClip);

        last = 0;

        for (auto const& message : SortedByTick(file.Events))
        {
            AppendDelta(bytes, message.Tick - last);
            AppendMessage(bytes, message);
            last = std::max(last, message.Tick);
        }

        AppendDelta(bytes, std::max(file.EndTick, last) - last);
        AppendStreamMessage(bytes, StreamStatusEndOfClip);

        return bytes;
    }

    _Use_decl_annotations_
    ClipFileReadResult ReadClipFile(std::span<uint8_t const> bytes, ClipFile& file, size_t maximumMessages)
    {
        file = ClipFile{};

        ClipFileReadResult result{};

        if (bytes.size() < sizeof(FileHeader) || !std::equal(std::begin(FileHeader), std::end(FileHeader), bytes.begin()))
        {
            result.Status = ClipFileReadStatus::NotAClipFile;
            return result;
        }

        enum class Part { Profiles, Configuration, Sequence, Done };

        auto part = Part::Profiles;
        int64_t tick{ 0 };
        int64_t sequenceStart{ 0 };
        size_t messages{ 0 };
        size_t position{ sizeof(FileHeader) };

        auto readWord = [&bytes](size_t at) noexcept
        {
            return (static_cast<uint32_t>(bytes[at]) << 24) |
                (static_cast<uint32_t>(bytes[at + 1]) << 16) |
                (static_cast<uint32_t>(bytes[at + 2]) << 8) |
                static_cast<uint32_t>(bytes[at + 3]);
        };

        while (part != Part::Done)
        {
            if (position + 4 > bytes.size())
            {
                result.Truncated = true;
                break;
            }

            uint32_t words[4]{};
            words[0] = readWord(position);

            auto const count = UmpWordCount(words[0]);

            if (position + static_cast<size_t>(count) * 4 > bytes.size())
            {
                result.Truncated = true;
                break;
            }

            for (uint8_t i = 1; i < count; ++i)
            {
                words[i] = readWord(position + static_cast<size_t>(i) * 4);
            }

            position += static_cast<size_t>(count) * 4;

            if (++messages > maximumMessages)
            {
                result.Status = ClipFileReadStatus::TooMuchData;
                return result;
            }

            auto const messageType = words[0] >> 28;

            if (messageType == 0x0)
            {
                auto const status = (words[0] >> 20) & 0x0F;

                if (status == UtilityStatusDeltaClockstamp)
                {
                    tick += words[0] & 0xFFFFF;

                    if (part == Part::Profiles)
                    {
                        part = Part::Configuration;
                    }
                }
                else if (status == UtilityStatusTicksPerQuarterNote)
                {
                    auto const ticks = static_cast<uint16_t>(words[0] & 0xFFFF);
                    file.TicksPerQuarterNote = ticks == 0 ? static_cast<uint16_t>(TicksPerQuarterNote) : ticks;
                }

                // NOOPs and jitter reduction messages carry nothing a sequence keeps.
                continue;
            }

            if (messageType == 0xF)
            {
                auto const status = (words[0] >> 16) & 0x3FF;

                if (status == StreamStatusStartOfClip && part != Part::Sequence)
                {
                    part = Part::Sequence;
                    sequenceStart = tick;
                }
                else if (status == StreamStatusEndOfClip && part == Part::Sequence)
                {
                    file.EndTick = tick - sequenceStart;
                    part = Part::Done;
                }
                else
                {
                    ++result.SkippedMessages;
                }

                continue;
            }

            switch (part)
            {
            case Part::Profiles:
                file.Profiles.push_back(MakeEvent(0, words, count));
                break;

            case Part::Configuration:
                file.Configuration.push_back(MakeEvent(tick, words, count));
                break;

            case Part::Sequence:
                file.Events.push_back(MakeEvent(tick - sequenceStart, words, count));
                break;

            default:
                break;
            }
        }

        if (part == Part::Sequence)
        {
            // No End of Clip: the clip ends at the last message.
            file.EndTick = tick - sequenceStart;
            result.Truncated = true;
        }

        return result;
    }

    _Use_decl_annotations_
    ClipFile ExportTrackToClipFile(Sequence const& sequence, Track const& track, ClipFileText const& text)
    {
        ClipFile file{};

        auto const group = track.Destination.Group;
        uint32_t words[4]{};

        // ---- Clip Configuration Header ----

        BuildSetTempo(sequence.Tempo.empty() ? 120.0 : sequence.Tempo.front().BeatsPerMinute, group, words);
        file.Configuration.push_back(MakeEvent(0, words, 4));

        BuildSetTimeSignature(sequence.Meter.empty() ? MeterChange{} : sequence.Meter.front(), group, words);
        file.Configuration.push_back(MakeEvent(0, words, 4));

        for (auto startup : track.Startup)
        {
            startup.Tick = 0;
            ApplyDestination(track.Destination, startup.Words, startup.WordCount);
            file.Configuration.push_back(startup);
        }

        // ---- Clip Sequence Data ----

        AppendMetadataText(file.Events, 0, MetadataText::ProjectName, text.SequenceName, group);
        AppendMetadataText(file.Events, 0, MetadataText::ClipName, track.Name, group);
        AppendMetadataText(file.Events, 0, MetadataText::Copyright, text.Copyright, group);
        AppendMetadataText(file.Events, 0, MetadataText::Composer, text.Composer, group);

        TempoMap const tempoMap{ sequence.Tempo };

        for (size_t i = 0; i < sequence.Tempo.size(); ++i)
        {
            auto const& point = sequence.Tempo[i];
            auto const start = SnapToClock(point.Tick);

            if (point.RampToNext && i + 1 < sequence.Tempo.size())
            {
                // A ramp goes out as a step at every MIDI clock, which is as fine as a file allows.
                auto const end = SnapToClock(sequence.Tempo[i + 1].Tick);

                for (auto at = start; at < end; at += TicksPerMidiClock)
                {
                    BuildSetTempo(tempoMap.BeatsPerMinuteAtTick(at), group, words);
                    file.Events.push_back(MakeEvent(at, words, 4));
                }
            }
            else
            {
                BuildSetTempo(point.BeatsPerMinute, group, words);
                file.Events.push_back(MakeEvent(start, words, 4));
            }
        }

        for (auto const& change : sequence.Meter)
        {
            BuildSetTimeSignature(change, group, words);
            file.Events.push_back(MakeEvent(change.Tick, words, 4));
        }

        for (auto const* tags : { &sequence.Tags, &track.Tags })
        {
            for (auto const& tag : *tags)
            {
                AppendMetadataText(file.Events, tag.Tick, MetadataText::Unknown, tag.Text, group);
            }
        }

        std::vector<RenderedMessage> rendered{};
        RenderTrack(sequence, track, 0, INT64_MAX, rendered);

        for (auto& message : rendered)
        {
            ApplyDestination(track.Destination, message.Words, message.WordCount);
            file.Events.push_back(MakeEvent(message.Tick, message.Words.data(), message.WordCount));
            file.EndTick = std::max(file.EndTick, message.Tick);
        }

        std::stable_sort(file.Events.begin(), file.Events.end(), [](ClipEvent const& a, ClipEvent const& b) { return a.Tick < b.Tick; });

        file.EndTick = std::max(file.EndTick, TrackEndTick(sequence, track));

        return file;
    }

    _Use_decl_annotations_
    ImportedClipFile ImportClipFile(ClipFile const& file)
    {
        ImportedClipFile imported{};
        auto& clip = imported.Content;

        clip.Id = NewId(L"c");
        clip.Origin = ClipOrigin::Imported;
        clip.Loop = false;

        auto const scale = static_cast<double>(TicksPerQuarterNote) / static_cast<double>(std::max<uint16_t>(1, file.TicksPerQuarterNote));
        auto const toTick = [scale](int64_t tick) { return static_cast<int64_t>(std::llround(static_cast<double>(tick) * scale)); };

        std::wstring projectName{};
        std::string text{};
        uint8_t textStatus{ 0 };

        // Returns true when a Flex Data text message is complete, with its text in "complete".
        auto collectText = [&text, &textStatus](ClipEvent const& event, std::wstring& complete, uint8_t& status) -> bool
        {
            auto const form = (event.Words[0] >> 22) & 0x03;

            if (form == 0 || form == 1)
            {
                text.clear();
                textStatus = static_cast<uint8_t>(event.Words[0] & 0xFF);
            }

            for (size_t i = 0; i < 12; ++i)
            {
                auto const value = static_cast<char>((event.Words[1 + i / 4] >> (24 - 8 * (i % 4))) & 0xFF);

                if (value == 0)
                {
                    break;
                }

                if (text.size() < 4096)
                {
                    text.push_back(value);
                }
            }

            if (form == 0 || form == 3)
            {
                complete = FromUtf8(text);
                status = textStatus;
                text.clear();
                return true;
            }

            return false;
        };

        // ---- configuration ----

        for (auto const& message : file.Configuration)
        {
            double bpm{};
            MeterChange meter{};
            std::wstring complete{};
            uint8_t status{};

            if (ReadSetTempo(message, bpm))
            {
                imported.Tempo.push_back(TempoPoint{ 0, bpm, false });
            }
            else if (ReadSetTimeSignature(message, meter))
            {
                imported.Meter.push_back(meter);
            }
            else if ((message.Words[0] >> 28) == 0xD && ((message.Words[0] >> 8) & 0xFF) == 0x01)
            {
                if (collectText(message, complete, status) && status == MetadataText::ProjectName)
                {
                    projectName = complete;
                }
            }
            else
            {
                auto startup = message;
                startup.Tick = 0;
                imported.Startup.push_back(startup);
            }
        }

        // ---- sequence data ----

        std::unordered_map<uint32_t, std::deque<size_t>> open{};
        int64_t lastTick{ 0 };

        auto closeNote = [&clip, &open](uint32_t key, int64_t tick, uint16_t releaseVelocity)
        {
            auto found = open.find(key);

            if (found == open.end() || found->second.empty())
            {
                return;
            }

            auto& note = clip.Notes[found->second.front()];
            note.Length = std::max<int64_t>(1, tick - note.Tick);
            note.ReleaseVelocity = releaseVelocity;
            found->second.pop_front();
        };

        for (auto const& message : file.Events)
        {
            auto const tick = toTick(message.Tick);
            lastTick = std::max(lastTick, tick);

            auto const word0 = message.Words[0];
            auto const messageType = word0 >> 28;

            double bpm{};
            MeterChange meter{};

            if (ReadSetTempo(message, bpm))
            {
                imported.Tempo.push_back(TempoPoint{ tick, bpm, false });
                continue;
            }

            if (ReadSetTimeSignature(message, meter))
            {
                meter.Tick = tick;
                imported.Meter.push_back(meter);
                continue;
            }

            if (messageType == 0xD && ((word0 >> 8) & 0xFF) == 0x01)
            {
                std::wstring complete{};
                uint8_t status{};

                if (collectText(message, complete, status))
                {
                    if (status == MetadataText::ClipName && imported.Name.empty())
                    {
                        imported.Name = complete;
                    }
                    else if (status == MetadataText::ProjectName && projectName.empty())
                    {
                        projectName = complete;
                    }
                    else if (status == MetadataText::Unknown && !complete.empty())
                    {
                        imported.Tags.push_back(Tag{ tick, complete, std::nullopt });
                    }
                }

                continue;
            }

            auto const status = (word0 >> 20) & 0x0F;
            auto const channel = static_cast<uint8_t>((word0 >> 16) & 0x0F);
            auto const number = static_cast<uint8_t>((word0 >> 8) & 0x7F);
            auto const key = (static_cast<uint32_t>(channel) << 7) | number;

            if (messageType == 0x4 && (status == 0x9 || status == 0x8))
            {
                if (status == 0x9)
                {
                    Note note{};
                    note.Tick = tick;
                    note.Channel = channel;
                    note.Number = number;
                    note.Velocity = static_cast<uint16_t>(message.Words[1] >> 16);
                    note.AttributeType = static_cast<uint8_t>(word0 & 0xFF);
                    note.AttributeData = static_cast<uint16_t>(message.Words[1] & 0xFFFF);
                    open[key].push_back(clip.Notes.size());
                    clip.Notes.push_back(note);
                }
                else
                {
                    closeNote(key, tick, static_cast<uint16_t>(message.Words[1] >> 16));
                }

                continue;
            }

            if (messageType == 0x2 && (status == 0x9 || status == 0x8))
            {
                auto const velocity = static_cast<uint8_t>(word0 & 0x7F);

                if (status == 0x9 && velocity > 0)
                {
                    Note note{};
                    note.Tick = tick;
                    note.Channel = channel;
                    note.Number = number;
                    note.Velocity = static_cast<uint16_t>(ScaleUp(velocity, 7, 16));
                    open[key].push_back(clip.Notes.size());
                    clip.Notes.push_back(note);
                }
                else
                {
                    closeNote(key, tick, status == 0x8 ? static_cast<uint16_t>(ScaleUp(velocity, 7, 16)) : 0);
                }

                continue;
            }

            auto event = message;
            event.Tick = tick;
            clip.Events.push_back(event);
        }

        auto const endTick = std::max(toTick(file.EndTick), lastTick);

        // A note that never ended ends with the clip.
        for (auto& [key, indexes] : open)
        {
            for (auto const index : indexes)
            {
                auto& note = clip.Notes[index];
                note.Length = std::max<int64_t>(1, endTick - note.Tick);
            }
        }

        clip.Length = std::max<int64_t>(1, endTick);
        clip.Name = !imported.Name.empty() ? imported.Name : projectName;
        imported.Name = clip.Name;

        SortClip(clip);
        return imported;
    }
}
