// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#include "StandardMidiFileBridge.h"

#include "SequenceRender.h"

#include "midi_file_smf_writer.h"

#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <cstring>
#include <format>
#include <span>

namespace midisequencer
{
    namespace
    {
        // The same colors the comps use, in order, so imported tracks are told apart at a glance.
        constexpr uint32_t TrackPalette[]
        {
            0xFF8A65, 0x5BC0EB, 0xB4A7FF, 0x6CCB5F, 0x4DD0C4, 0xF7C948, 0xFF6F91, 0xFFB547, 0x9C8CFF, 0xBDBDBD,
        };

        constexpr uint8_t ControllerBankSelect = 0;
        constexpr uint8_t ControllerVolume = 7;
        constexpr uint8_t ControllerPan = 10;
        constexpr uint8_t ControllerBankSelectLsb = 32;

        uint32_t Midi1Word(_In_ uint8_t status, _In_ uint8_t data1, _In_ uint8_t data2) noexcept
        {
            return 0x20000000u | (static_cast<uint32_t>(status) << 16) | (static_cast<uint32_t>(data1 & 0x7F) << 8) | (data2 & 0x7Fu);
        }

        ClipEvent EventFromWords(_In_ int64_t tick, _In_reads_(count) uint32_t const* words, _In_ uint8_t count)
        {
            ClipEvent event{};
            event.Tick = tick;
            event.WordCount = std::min<uint8_t>(count, 4);
            std::copy_n(words, event.WordCount, event.Words.begin());
            return event;
        }

        // A system exclusive dump, F0 ... F7, as 7-bit system exclusive packets of six bytes each.
        void AppendSystemExclusive(_Inout_ std::vector<ClipEvent>& events, _In_ int64_t tick, _In_ std::span<uint8_t const> bytes)
        {
            auto first = bytes.begin();
            auto last = bytes.end();

            if (first != last && *first == 0xF0)
            {
                ++first;
            }

            if (first != last && *(last - 1) == 0xF7)
            {
                --last;
            }

            auto const total = static_cast<size_t>(std::distance(first, last));
            auto const packets = std::max<size_t>(1, (total + 5) / 6);

            for (size_t packet = 0; packet < packets; ++packet)
            {
                uint32_t status{ 0 };

                if (packets > 1)
                {
                    status = packet == 0 ? 1u : packet + 1 == packets ? 3u : 2u;
                }

                auto const offset = packet * 6;
                auto const count = std::min<size_t>(6, total - std::min(total, offset));

                uint8_t data[6]{};

                for (size_t i = 0; i < count; ++i)
                {
                    data[i] = static_cast<uint8_t>(*(first + static_cast<ptrdiff_t>(offset + i)) & 0x7F);
                }

                uint32_t words[2]{};
                words[0] = 0x30000000u | (status << 20) | (static_cast<uint32_t>(count) << 16) | (static_cast<uint32_t>(data[0]) << 8) | data[1];
                words[1] = (static_cast<uint32_t>(data[2]) << 24) | (static_cast<uint32_t>(data[3]) << 16) | (static_cast<uint32_t>(data[4]) << 8) | data[5];

                events.push_back(EventFromWords(tick, words, 2));
            }
        }

        bool IsStartupMessage(_In_ midifile::SequenceEvent const& event, _In_ std::span<uint8_t const> bytes) noexcept
        {
            if (event.Kind == midifile::EventKind::ProgramChange)
            {
                return true;
            }

            if (event.Kind == midifile::EventKind::ControlChange && bytes.size() >= 2)
            {
                auto const controller = bytes[1];
                return controller == ControllerBankSelect || controller == ControllerBankSelectLsb ||
                    controller == ControllerVolume || controller == ControllerPan;
            }

            return false;
        }

        struct OutgoingEvent
        {
            int64_t Tick{ 0 };
            uint16_t TrackIndex{ 0 };
            std::array<uint32_t, 4> Words{};
            uint8_t WordCount{ 0 };
        };

        void CollectExportTracks(_In_ std::vector<Track> const& tracks, _In_ std::vector<std::wstring> const& ids, _Inout_ std::vector<Track const*>& result)
        {
            for (auto const& track : tracks)
            {
                if (track.IsFolder)
                {
                    CollectExportTracks(track.Children, ids, result);
                }
                else if (ids.empty() || std::find(ids.begin(), ids.end(), track.Id) != ids.end())
                {
                    result.push_back(&track);
                }
            }
        }
    }

    _Use_decl_annotations_
    ImportedStandardMidiFile ImportStandardMidiFile(
        midifile::MidiSequence const& file,
        std::wstring const& fileName,
        StandardMidiFileImportOptions const& options)
    {
        ImportedStandardMidiFile imported{};

        // Musical files scale by resolution. A file timed in SMPTE frames has no beats, so its
        // seconds are laid out at 120 BPM, two beats a second.
        double scale{ 1.0 };

        if (file.Division.IsSmpte)
        {
            auto const ticksPerSecond = file.Division.TicksPerSecond();
            scale = ticksPerSecond > 0 ? (2.0 * TicksPerQuarterNote) / ticksPerSecond : 1.0;
        }
        else
        {
            scale = static_cast<double>(TicksPerQuarterNote) / static_cast<double>(std::max<uint16_t>(1, file.Division.TicksPerQuarterNote));
        }

        auto const toTick = [scale](uint32_t tick) { return static_cast<int64_t>(std::llround(static_cast<double>(tick) * scale)); };

        imported.Title = FromUtf8(file.Title);
        imported.Copyright = FromUtf8(file.Copyright);

        if (file.Division.IsSmpte)
        {
            imported.Tempo.push_back(TempoPoint{ 0, 120.0, false });
        }
        else
        {
            for (auto const& change : file.TempoMap)
            {
                if (change.MicrosecondsPerQuarterNote > 0)
                {
                    imported.Tempo.push_back(TempoPoint{ toTick(change.Tick), 60000000.0 / change.MicrosecondsPerQuarterNote, false });
                }
            }
        }

        for (auto const& change : file.TimeSignatureMap)
        {
            MeterChange meter{};
            meter.Tick = toTick(change.Tick);
            meter.Numerator = std::max<uint8_t>(1, change.Numerator);
            meter.Denominator = static_cast<uint8_t>(1u << std::min<uint8_t>(change.DenominatorPowerOfTwo, 5));
            imported.Meter.push_back(meter);
        }

        if (options.MarkersAsTags)
        {
            for (auto const& text : file.TextEvents)
            {
                if ((text.Kind == midifile::TextKind::Marker || text.Kind == midifile::TextKind::CuePoint) && !text.Text.empty())
                {
                    imported.Tags.push_back(Tag{ toTick(text.Tick), midiapp::SanitizeProvenanceText(FromUtf8(text.Text), 1024), std::nullopt });
                }
            }
        }

        auto meter = imported.Meter;

        if (meter.empty())
        {
            meter.push_back(MeterChange{});
        }

        std::vector<Track> tracks(file.Tracks.size());
        std::vector<Clip> clips(file.Tracks.size());

        for (size_t i = 0; i < file.Tracks.size(); ++i)
        {
            auto const& source = file.Tracks[i];
            auto& track = tracks[i];
            auto& clip = clips[i];

            track.Id = NewId(L"t");
            track.Name = midiapp::SanitizeProvenanceText(FromUtf8(source.Name), 256);

            if (track.Name.empty())
            {
                track.Name = std::format(L"Track {}", i + 1);
            }

            track.Color = TrackPalette[i % std::size(TrackPalette)];

            // One channel in the file means the track plays on that channel. Several means each
            // message keeps its own.
            auto const mask = source.ChannelMask;
            track.Destination.Channel = (mask != 0 && (mask & (mask - 1)) == 0)
                ? static_cast<int8_t>(std::countr_zero(static_cast<uint32_t>(mask)))
                : static_cast<int8_t>(-1);

            clip.Id = NewId(L"c");
            clip.Name = track.Name;
            clip.Loop = false;
            clip.Origin = ClipOrigin::Imported;
            clip.OriginDetail = fileName;
        }

        for (auto const& note : file.Notes)
        {
            if (note.TrackIndex >= clips.size())
            {
                continue;
            }

            Note converted{};
            converted.Tick = toTick(note.StartTick);
            converted.Length = std::max<int64_t>(1, toTick(note.EndTick) - converted.Tick);
            converted.Channel = static_cast<uint8_t>(note.Channel & 0x0F);
            converted.Number = static_cast<uint8_t>(note.NoteNumber & 0x7F);
            converted.Velocity = static_cast<uint16_t>(ScaleUp(note.Velocity, 7, 16));

            clips[note.TrackIndex].Notes.push_back(converted);
        }

        for (auto const& event : file.Events)
        {
            if (event.TrackIndex >= clips.size() || event.Kind == midifile::EventKind::NoteOn || event.Kind == midifile::EventKind::NoteOff)
            {
                continue;
            }

            auto const bytes = file.BytesOf(event);
            auto const tick = toTick(event.Tick);
            auto& clip = clips[event.TrackIndex];

            switch (event.Kind)
            {
            case midifile::EventKind::PolyphonicPressure:
            case midifile::EventKind::ControlChange:
            case midifile::EventKind::ProgramChange:
            case midifile::EventKind::ChannelPressure:
            case midifile::EventKind::PitchBend:
            {
                if (bytes.empty())
                {
                    break;
                }

                auto const word = Midi1Word(bytes[0], bytes.size() > 1 ? bytes[1] : 0, bytes.size() > 2 ? bytes[2] : 0);

                if (options.StartupFromFile && event.Tick == 0 && IsStartupMessage(event, bytes))
                {
                    tracks[event.TrackIndex].Startup.push_back(EventFromWords(0, &word, 1));
                }
                else
                {
                    clip.Events.push_back(EventFromWords(tick, &word, 1));
                }

                break;
            }

            case midifile::EventKind::SystemExclusive:
                AppendSystemExclusive(clip.Events, tick, bytes);
                break;

            case midifile::EventKind::SystemCommon:
            case midifile::EventKind::SystemRealTime:
            {
                if (bytes.empty())
                {
                    break;
                }

                auto const word = 0x10000000u | (static_cast<uint32_t>(bytes[0]) << 16) |
                    (static_cast<uint32_t>(bytes.size() > 1 ? bytes[1] & 0x7F : 0) << 8) |
                    (bytes.size() > 2 ? bytes[2] & 0x7Fu : 0u);
                clip.Events.push_back(EventFromWords(tick, &word, 1));
                break;
            }

            case midifile::EventKind::UniversalPacket:
            {
                std::array<uint32_t, 4> words{};
                auto const count = static_cast<uint8_t>(std::min<size_t>(bytes.size() / 4, 4));
                std::memcpy(words.data(), bytes.data(), static_cast<size_t>(count) * 4);

                if (count > 0 && count == UmpWordCount(words[0]))
                {
                    clip.Events.push_back(EventFromWords(tick, words.data(), count));
                }

                break;
            }

            default:
                break;
            }
        }

        for (size_t i = 0; i < clips.size(); ++i)
        {
            auto& clip = clips[i];

            if (clip.Notes.empty() && clip.Events.empty())
            {
                // A track with only a name or text in it, like the tempo track of most files.
                continue;
            }

            SortClip(clip);

            int64_t end{ 0 };

            for (auto const& note : clip.Notes)
            {
                end = std::max(end, note.Tick + note.Length);
            }

            if (!clip.Events.empty())
            {
                end = std::max(end, clip.Events.back().Tick + 1);
            }

            // Round up to a whole bar, so the clip lines up with the grid.
            auto const position = BarPositionAtTick(meter, end);
            auto const barStart = TickAtBar(meter, position.Bar);
            clip.Length = std::max<int64_t>(barStart == end ? end : TickAtBar(meter, position.Bar + 1), TicksPerBar(meter.front()));

            Placement placement{};
            placement.ClipId = clip.Id;
            placement.Tick = 0;
            tracks[i].Timeline.push_back(placement);

            imported.Clips.push_back(std::move(clip));
            imported.Tracks.push_back(std::move(tracks[i]));
        }

        return imported;
    }

    _Use_decl_annotations_
    StandardMidiFileExport ExportStandardMidiFile(
        Sequence const& sequence,
        std::vector<std::wstring> const& trackIds,
        ClipFileText const& text)
    {
        StandardMidiFileExport result{};

        midifile::MidiSequence file{};
        file.Format = midifile::SequenceFormat::MultiTrack;
        file.Division.TicksPerQuarterNote = static_cast<uint16_t>(TicksPerQuarterNote);
        file.Title = ToUtf8(text.SequenceName);
        file.Copyright = ToUtf8(text.Copyright);

        auto const clampTick = [](int64_t tick) { return static_cast<uint32_t>(std::clamp<int64_t>(tick, 0, 0xFFFFFFF0)); };

        midifile::Track conductor{};
        conductor.Name = ToUtf8(text.SequenceName);
        file.Tracks.push_back(conductor);

        // ---- tempo, with ramps as a step at every MIDI clock ----

        TempoMap const tempoMap{ sequence.Tempo };

        auto const addTempo = [&file, &clampTick](int64_t tick, double bpm)
        {
            midifile::TempoChange change{};
            change.Tick = clampTick(tick);
            change.MicrosecondsPerQuarterNote = static_cast<uint32_t>(std::clamp<long long>(
                std::llround(60000000.0 / bpm),
                midifile::MinimumMicrosecondsPerQuarterNote,
                midifile::MaximumMicrosecondsPerQuarterNote));

            if (!file.TempoMap.empty() && file.TempoMap.back().Tick == change.Tick)
            {
                file.TempoMap.back() = change;
            }
            else
            {
                file.TempoMap.push_back(change);
            }
        };

        constexpr int64_t ticksPerClock = TicksPerQuarterNote / 24;

        for (size_t i = 0; i < sequence.Tempo.size(); ++i)
        {
            auto const& point = sequence.Tempo[i];

            if (point.RampToNext && i + 1 < sequence.Tempo.size())
            {
                for (auto at = point.Tick; at < sequence.Tempo[i + 1].Tick; at += ticksPerClock)
                {
                    addTempo(at, tempoMap.BeatsPerMinuteAtTick(at));
                }
            }
            else
            {
                addTempo(point.Tick, point.BeatsPerMinute);
            }
        }

        if (file.TempoMap.empty() || file.TempoMap.front().Tick != 0)
        {
            file.TempoMap.insert(file.TempoMap.begin(), midifile::TempoChange{});
        }

        for (auto const& change : sequence.Meter)
        {
            midifile::TimeSignatureChange signature{};
            signature.Tick = clampTick(change.Tick);
            signature.Numerator = change.Numerator;

            uint8_t power{ 0 };

            while (power < 5 && (1u << power) < change.Denominator)
            {
                ++power;
            }

            signature.DenominatorPowerOfTwo = power;
            file.TimeSignatureMap.push_back(signature);
        }

        if (file.TimeSignatureMap.empty() || file.TimeSignatureMap.front().Tick != 0)
        {
            file.TimeSignatureMap.insert(file.TimeSignatureMap.begin(), midifile::TimeSignatureChange{});
        }

        for (auto const& tag : sequence.Tags)
        {
            file.TextEvents.push_back(midifile::TextEvent{ clampTick(tag.Tick), 0, midifile::TextKind::Marker, ToUtf8(tag.Text) });
        }

        // ---- tracks ----

        std::vector<Track const*> tracks{};
        CollectExportTracks(sequence.Tracks, trackIds, tracks);

        std::vector<OutgoingEvent> outgoing{};
        std::vector<RenderedMessage> rendered{};

        for (size_t t = 0; t < tracks.size() && file.Tracks.size() < 0xFFFE; ++t)
        {
            auto const& track = *tracks[t];
            auto const trackIndex = static_cast<uint16_t>(file.Tracks.size());

            midifile::Track fileTrack{};
            fileTrack.Name = ToUtf8(track.Name);
            file.Tracks.push_back(fileTrack);

            for (auto startup : track.Startup)
            {
                ApplyDestination(track.Destination, startup.Words, startup.WordCount);
                outgoing.push_back(OutgoingEvent{ 0, trackIndex, startup.Words, startup.WordCount });
            }

            rendered.clear();
            RenderTrack(sequence, track, 0, INT64_MAX, rendered);

            for (auto& message : rendered)
            {
                ApplyDestination(track.Destination, message.Words, message.WordCount);
                outgoing.push_back(OutgoingEvent{ message.Tick, trackIndex, message.Words, message.WordCount });
            }

            for (auto const& tag : track.Tags)
            {
                file.TextEvents.push_back(midifile::TextEvent{ clampTick(tag.Tick), trackIndex, midifile::TextKind::Marker, ToUtf8(tag.Text) });
            }
        }

        std::stable_sort(outgoing.begin(), outgoing.end(), [](OutgoingEvent const& a, OutgoingEvent const& b) { return a.Tick < b.Tick; });

        file.Events.reserve(outgoing.size());
        file.EventBytes.reserve(outgoing.size() * 8);

        for (auto const& event : outgoing)
        {
            midifile::SequenceEvent sequenceEvent{};
            sequenceEvent.Tick = clampTick(event.Tick);
            sequenceEvent.TrackIndex = event.TrackIndex;
            sequenceEvent.Kind = midifile::EventKind::UniversalPacket;
            sequenceEvent.ByteOffset = static_cast<uint32_t>(file.EventBytes.size());
            sequenceEvent.ByteCount = static_cast<uint32_t>(event.WordCount) * 4;

            auto const messageType = event.Words[0] >> 28;

            if (messageType == 0x2 || messageType == 0x4)
            {
                sequenceEvent.Channel = static_cast<uint8_t>((event.Words[0] >> 16) & 0x0F);
            }

            auto const* bytes = reinterpret_cast<uint8_t const*>(event.Words.data());
            file.EventBytes.insert(file.EventBytes.end(), bytes, bytes + sequenceEvent.ByteCount);
            file.Events.push_back(sequenceEvent);
        }

        std::stable_sort(file.TextEvents.begin(), file.TextEvents.end(),
            [](midifile::TextEvent const& a, midifile::TextEvent const& b) { return a.Tick < b.Tick; });

        file.Finalize();

        auto const written = midifile::WriteStandardMidiFile(file, result.Bytes);
        result.Succeeded = written.Succeeded();
        result.SkippedMessages = written.SkippedEventCount;

        return result;
    }
}
