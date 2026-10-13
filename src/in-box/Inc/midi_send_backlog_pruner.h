// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// Thins out what is waiting behind a send speed limit once it backs up, for the Network MIDI 2.0
// and RTP-MIDI transports and MIDI Patchbay. A fader, wheel or pressure sweep that arrives faster
// than the connection carries would otherwise go out late, one step at a time. Pruning keeps
// where each sweep starts, every point where it turns around, and where it ends, and drops a
// share of the steps in between, so the device catches up and still ends up at the right value.
//
// Only steps strictly between their neighbors on one line are ever removed: one controller,
// pressure or pitch bend on one channel, or on one note. A value that repeats is kept, so a
// button that sends the same value each time it is pressed loses nothing. Notes, program
// changes, system exclusive and everything else always go, and each of them ends the lines on
// its channel, so the value in effect when a note starts is always the one that was sent.
//
// The first steps each time a line sets off one way always go too, so only a long, steady move
// is thinned. A short run is often not a position at all: Mackie Control meters share one
// channel pressure value among eight strips, and an accelerating relative encoder sends how
// fast it turns.
//
// Nothing here waits or locks; the caller does both. Not thread safe.
// ============================================================================

#pragma once

#include <sal.h>

#include <algorithm>
#include <array>
#include <cstdint>
#include <deque>
#include <unordered_map>
#include <vector>

#include "midi_send_pacer.h"

namespace WindowsMidiServicesInternal
{
    // A backlog is thinned once it holds more than about this much time at the connection's speed
    constexpr uint64_t MidiSendBacklogPruneAfterMilliseconds = 100;
    constexpr uint64_t MidiSendBacklogPruneMinimumWireBytes = 256;

    // Longer than a run of Mackie Control meters (eight strips) or of encoder speeds (up to 15)
    constexpr uint32_t MidiSendBacklogPruneStepsKeptInEveryRun = 16;

    // What a backlog may hold before it is thinned, in MIDI 1.0 bytes. 0 when there is no limit.
    inline uint64_t MidiSendBacklogPruneThreshold(_In_ uint32_t const speedMultiple) noexcept
    {
        auto const multiple = ClampMidiSendSpeedMultiple(speedMultiple);

        if (multiple == 0)
        {
            return 0;
        }

        return (std::max)(
            (multiple * MidiWireSpeedBytesPerSecond * MidiSendBacklogPruneAfterMilliseconds) / 1000,
            MidiSendBacklogPruneMinimumWireBytes);
    }

    // How hard to thin a backlog this far over its threshold: keep one in-between step in every
    // so many. 1 keeps them all, and 0 keeps none.
    inline uint32_t ChooseMidiSendBacklogKeepOneIn(_In_ uint64_t const backlogWireBytes, _In_ uint64_t const thresholdWireBytes) noexcept
    {
        if (thresholdWireBytes == 0 || backlogWireBytes <= thresholdWireBytes)
        {
            return 1;
        }

        auto const times = (backlogWireBytes + thresholdWireBytes - 1) / thresholdWireBytes;

        if (times <= 2) return 2;
        if (times <= 4) return 4;
        if (times <= 8) return 8;

        return 0;
    }

    struct MidiSendBacklogPruneResult
    {
        uint32_t MessagesRemoved{ 0 };
        uint64_t WireBytesRemoved{ 0 };
    };

    // One pass over a backlog: Begin, each message oldest first, then IsPruned for each. Or one
    // of the Prune functions, which do all of that to a whole queue. Reused from one pass to the
    // next, so its memory is too.
    class MidiSendBacklogPruner
    {
    public:
        // Anything other than the default is for testing the other rules on their own
        explicit MidiSendBacklogPruner(_In_ uint32_t const stepsKeptInEveryRun = MidiSendBacklogPruneStepsKeptInEveryRun) noexcept :
            m_stepsKeptInEveryRun{ stepsKeptInEveryRun }
        {
        }

        // How many words a UMP message is, from the type in its first word
        static uint32_t UmpWordCount(_In_ uint32_t const firstWord) noexcept
        {
            switch (firstWord >> 28)
            {
            case 0x0: case 0x1: case 0x2: case 0x6: case 0x7:
                return 1;

            case 0x3: case 0x4: case 0x8: case 0x9: case 0xA:
                return 2;

            case 0xB: case 0xC:
                return 3;

            default:
                return 4;
            }
        }

        bool Begin(_In_ uint32_t const keepOneIn, _In_ size_t const messageCount) noexcept
        {
            m_keepOneIn = keepOneIn;
            m_next = 0;
            m_prunedCount = 0;
            m_failed = false;
            m_channelEpochs.fill(0);
            m_groupEpochs.fill(0);
            m_allEpoch = 0;
            m_lines.clear();

            try
            {
                m_pruned.assign(messageCount, 0);
            }
            catch (...)
            {
                m_failed = true;
            }

            return !m_failed;
        }

        // The next message, whole
        void AddUmp(_In_reads_(wordCount) uint32_t const* words, _In_ uint32_t const wordCount) noexcept
        {
            if (!Accept())
            {
                return;
            }

            if (words == nullptr || wordCount == 0)
            {
                EndAll();
                return;
            }

            auto const word = words[0];
            auto const group = static_cast<uint8_t>((word >> 24) & 0x0F);

            switch (word >> 28)
            {
            case 0x0:
            {
                // NOOP and JR Clock stand alone. A timestamp belongs to the message after it.
                auto const status = (word >> 20) & 0x0F;

                if (status > 0x1)
                {
                    EndAll();
                }

                return;
            }

            case 0x1:
                if (((word >> 16) & 0xFF) == 0xFF)
                {
                    EndGroup(group);
                }
                return;

            case 0x2:
                AddMidi1ChannelVoice(group, static_cast<uint8_t>(word >> 16), static_cast<uint8_t>(word >> 8), static_cast<uint8_t>(word));
                return;

            case 0x3:
            case 0x5:
                // system exclusive can change what any controller means
                EndGroup(group);
                return;

            case 0x4:
                AddMidi2ChannelVoice(group, words, wordCount);
                return;

            case 0xD:
                // tempo, signatures and text
                return;

            default:
                // stream messages, and types this does not know
                EndAll();
                return;
            }
        }

        // The next message as MIDI 1.0 bytes, as the RTP-MIDI transport queues them. The first
        // three bytes, 0 where there are fewer, and how many bytes it is in all.
        void AddMidi1(_In_ uint8_t const first, _In_ uint8_t const second, _In_ uint8_t const third, _In_ uint32_t const byteCount) noexcept
        {
            if (!Accept())
            {
                return;
            }

            if (first < 0x80)
            {
                // the middle of a system exclusive message
                EndAll();
                return;
            }

            if (first < 0xF0)
            {
                // program change and channel pressure are two bytes, the rest three
                uint32_t const expected = (first & 0xE0) == 0xC0 ? 2 : 3;

                if (byteCount != expected)
                {
                    // several messages made from one, such as a MIDI 2.0 registered controller
                    EndChannel(static_cast<size_t>(first & 0x0F));
                    return;
                }

                AddMidi1ChannelVoice(0, first, second, third);
                return;
            }

            if (first == 0xF0 || first == 0xF7 || first == 0xFF)
            {
                EndAll();
            }
        }

        bool IsPruned(_In_ size_t const index) const noexcept
        {
            return !m_failed && index < m_pruned.size() && m_pruned[index] != 0;
        }

        uint32_t PrunedCount() const noexcept
        {
            return m_failed ? 0 : m_prunedCount;
        }

        // Lets go of what a large backlog made the pruner allocate
        void Release() noexcept
        {
            std::vector<uint8_t>{}.swap(m_pruned);
            m_lines.clear();
            m_failed = true;
        }

        // Thins whole UMP messages in place from start to the end of the vector. A partial
        // message at the end is left where it is.
        MidiSendBacklogPruneResult PruneUmp(
            _Inout_ std::vector<uint32_t>& words,
            _In_ size_t const start,
            _In_ uint32_t const keepOneIn) noexcept
        {
            MidiSendBacklogPruneResult result{};

            if (keepOneIn == 1 || start >= words.size())
            {
                return result;
            }

            size_t end{ start };
            size_t messageCount{ 0 };

            while (end < words.size())
            {
                size_t const length = UmpWordCount(words[end]);

                if (end + length > words.size())
                {
                    break;
                }

                end += length;
                messageCount++;
            }

            // nothing is between two others with fewer than three
            if (messageCount < 3 || !Begin(keepOneIn, messageCount))
            {
                return result;
            }

            for (size_t position = start; position < end; )
            {
                auto const length = UmpWordCount(words[position]);

                AddUmp(words.data() + position, length);
                position += length;
            }

            if (PrunedCount() == 0)
            {
                return result;
            }

            size_t write{ start };
            size_t index{ 0 };

            for (size_t read = start; read < end; index++)
            {
                size_t const length = UmpWordCount(words[read]);

                if (IsPruned(index))
                {
                    result.MessagesRemoved++;
                    result.WireBytesRemoved += EstimateMidi1WireByteCount(words[read], static_cast<uint32_t>(length));
                }
                else
                {
                    if (write != read)
                    {
                        std::copy_n(words.begin() + read, length, words.begin() + write);
                    }

                    write += length;
                }

                read += length;
            }

            if (end < words.size())
            {
                std::copy(words.begin() + end, words.end(), words.begin() + write);
                write += words.size() - end;
            }

            words.erase(words.begin() + write, words.end());

            return result;
        }

        // Thins MIDI 1.0 messages in place, sizes[i] being how many of the bytes message i is
        MidiSendBacklogPruneResult PruneMidi1(
            _Inout_ std::deque<uint8_t>& bytes,
            _Inout_ std::deque<uint32_t>& sizes,
            _In_ uint32_t const keepOneIn) noexcept
        {
            MidiSendBacklogPruneResult result{};

            if (keepOneIn == 1 || sizes.size() < 3 || !Begin(keepOneIn, sizes.size()))
            {
                return result;
            }

            size_t offset{ 0 };

            for (auto const size : sizes)
            {
                if (size == 0 || offset + size > bytes.size())
                {
                    // the sizes do not describe the bytes, so nothing can be moved safely
                    m_failed = true;
                    return result;
                }

                AddMidi1(bytes[offset], size > 1 ? bytes[offset + 1] : uint8_t{ 0 }, size > 2 ? bytes[offset + 2] : uint8_t{ 0 }, size);
                offset += size;
            }

            if (PrunedCount() == 0)
            {
                return result;
            }

            size_t read{ 0 };
            size_t write{ 0 };
            size_t kept{ 0 };

            for (size_t index = 0; index < sizes.size(); index++)
            {
                auto const size = sizes[index];

                if (IsPruned(index))
                {
                    result.MessagesRemoved++;
                    result.WireBytesRemoved += size;
                }
                else
                {
                    if (write != read)
                    {
                        std::copy_n(bytes.begin() + read, size, bytes.begin() + write);
                    }

                    sizes[kept++] = size;
                    write += size;
                }

                read += size;
            }

            if (read < bytes.size())
            {
                std::copy(bytes.begin() + read, bytes.end(), bytes.begin() + write);
                write += bytes.size() - read;
            }

            bytes.erase(bytes.begin() + write, bytes.end());
            sizes.erase(sizes.begin() + kept, sizes.end());

            return result;
        }

    private:
        enum class LineKind : uint32_t
        {
            Midi1ControlChange = 1,
            Midi1PitchBend,
            Midi1ChannelPressure,
            Midi1PolyPressure,
            Midi2ControlChange,
            Midi2PitchBend,
            Midi2ChannelPressure,
            Midi2PolyPressure,
            Midi2AssignableController,
            Midi2PerNotePitchBend,
            Midi2RegisteredPerNoteController,
            Midi2AssignablePerNoteController,
        };

        // Where one line is in this pass
        struct Line
        {
            uint32_t Epoch{ 0 };
            uint32_t LastIndex{ 0 };
            uint32_t PreviousValue{ 0 };
            uint32_t LastValue{ 0 };
            uint32_t Steps{ 0 };

            // steps since the line last turned around
            uint32_t RunSteps{ 0 };

            uint8_t Points{ 0 };
        };

        static uint32_t LineKey(
            _In_ LineKind const kind,
            _In_ uint8_t const group,
            _In_ uint8_t const channel,
            _In_ uint8_t const first,
            _In_ uint8_t const second) noexcept
        {
            return static_cast<uint32_t>(kind) |
                (static_cast<uint32_t>(group & 0x0F) << 4) |
                (static_cast<uint32_t>(channel & 0x0F) << 8) |
                (static_cast<uint32_t>(first) << 12) |
                (static_cast<uint32_t>(second) << 20);
        }

        static size_t Slot(_In_ uint8_t const group, _In_ uint8_t const channel) noexcept
        {
            return (static_cast<size_t>(group & 0x0F) << 4) | (channel & 0x0F);
        }

        // Controllers that select or carry a parameter, belong to the next note, or change the
        // channel's mode. They are sent, and they end every line on their channel.
        static bool EndsChannelLines(_In_ uint8_t const controller) noexcept
        {
            switch (controller)
            {
            case 0:     // bank select
            case 6:     // data entry
            case 32:    // bank select LSB
            case 38:    // data entry LSB
            case 84:    // portamento control, which the next note on glides from
            case 88:    // high resolution velocity prefix of the next note on
                return true;

            default:
                return (controller >= 96 && controller <= 101) || controller >= 120;
            }
        }

        bool Accept() noexcept
        {
            if (m_failed)
            {
                return false;
            }

            if (m_next >= m_pruned.size())
            {
                m_failed = true;
                return false;
            }

            m_next++;
            return true;
        }

        // Every counter only goes up, so the sum changes whenever any of them does
        uint32_t Epoch(_In_ size_t const slot) const noexcept
        {
            return m_channelEpochs[slot & 0xFF] + m_groupEpochs[(slot >> 4) & 0x0F] + m_allEpoch;
        }

        void EndChannel(_In_ size_t const slot) noexcept
        {
            m_channelEpochs[slot & 0xFF]++;
        }

        void EndGroup(_In_ uint8_t const group) noexcept
        {
            m_groupEpochs[group & 0x0F]++;
        }

        void EndAll() noexcept
        {
            m_allEpoch++;
        }

        void EndLine(_In_ uint32_t const key) noexcept
        {
            auto const found = m_lines.find(key);

            if (found != m_lines.end())
            {
                found->second.Points = 0;
            }
        }

        void AddPoint(_In_ uint32_t const key, _In_ size_t const slot, _In_ uint32_t const value) noexcept
        {
            auto const index = static_cast<uint32_t>(m_next - 1);
            auto const epoch = Epoch(slot);

            try
            {
                auto& line = m_lines[key];

                if (line.Points == 0 || line.Epoch != epoch)
                {
                    line.Epoch = epoch;
                    line.Points = 1;
                    line.Steps = 0;
                    line.RunSteps = 0;
                    line.LastIndex = index;
                    line.LastValue = value;
                    return;
                }

                // The last point now has a point on each side. Judged against the original
                // neighbors, so which of the steps go doesn't change which turn around.
                if (line.Points > 1)
                {
                    bool const between =
                        (line.PreviousValue < line.LastValue && line.LastValue < value) ||
                        (line.PreviousValue > line.LastValue && line.LastValue > value);

                    if (!between)
                    {
                        line.RunSteps = 0;
                    }
                    else if (line.RunSteps++ >= m_stepsKeptInEveryRun)
                    {
                        auto const step = line.Steps++;

                        if (m_keepOneIn != 1 && (m_keepOneIn == 0 || step % m_keepOneIn != m_keepOneIn - 1))
                        {
                            m_pruned[line.LastIndex] = 1;
                            m_prunedCount++;
                        }
                    }
                }

                line.PreviousValue = line.LastValue;
                line.LastValue = value;
                line.LastIndex = index;
                line.Points = 2;
            }
            catch (...)
            {
                m_failed = true;
            }
        }

        void AddControlChange(
            _In_ LineKind const kind,
            _In_ uint8_t const group,
            _In_ uint8_t const channel,
            _In_ uint8_t const controller,
            _In_ uint32_t const value) noexcept
        {
            if (EndsChannelLines(controller))
            {
                EndChannel(Slot(group, channel));
                return;
            }

            if (controller >= 33 && controller <= 63)
            {
                // The fine half of a 14-bit controller. A receiver puts the two halves together,
                // so the coarse half is never thinned around it.
                EndLine(LineKey(kind, group, channel, static_cast<uint8_t>(controller - 32), 0));
                return;
            }

            AddPoint(LineKey(kind, group, channel, controller, 0), Slot(group, channel), value);
        }

        void AddMidi1ChannelVoice(
            _In_ uint8_t const group,
            _In_ uint8_t const statusByte,
            _In_ uint8_t const data1,
            _In_ uint8_t const data2) noexcept
        {
            auto const channel = static_cast<uint8_t>(statusByte & 0x0F);
            auto const slot = Slot(group, channel);
            auto const first = static_cast<uint8_t>(data1 & 0x7F);
            auto const second = static_cast<uint8_t>(data2 & 0x7F);

            switch (statusByte >> 4)
            {
            case 0xA:
                AddPoint(LineKey(LineKind::Midi1PolyPressure, group, channel, first, 0), slot, second);
                return;

            case 0xB:
                AddControlChange(LineKind::Midi1ControlChange, group, channel, first, second);
                return;

            case 0xD:
                AddPoint(LineKey(LineKind::Midi1ChannelPressure, group, channel, 0, 0), slot, first);
                return;

            case 0xE:
                AddPoint(LineKey(LineKind::Midi1PitchBend, group, channel, 0, 0), slot,
                    static_cast<uint32_t>(first) | (static_cast<uint32_t>(second) << 7));
                return;

            default:
                EndChannel(slot);
                return;
            }
        }

        void AddMidi2ChannelVoice(
            _In_ uint8_t const group,
            _In_reads_(wordCount) uint32_t const* words,
            _In_ uint32_t const wordCount) noexcept
        {
            auto const word = words[0];
            auto const channel = static_cast<uint8_t>((word >> 16) & 0x0F);
            auto const slot = Slot(group, channel);

            // note, bank or controller number, then the controller number within it
            auto const first = static_cast<uint8_t>((word >> 8) & 0x7F);
            auto const second = static_cast<uint8_t>(word & 0xFF);

            if (wordCount < 2)
            {
                EndChannel(slot);
                return;
            }

            auto const value = words[1];

            switch ((word >> 20) & 0x0F)
            {
            case 0x0:
                AddPoint(LineKey(LineKind::Midi2RegisteredPerNoteController, group, channel, first, second), slot, value);
                return;

            case 0x1:
                AddPoint(LineKey(LineKind::Midi2AssignablePerNoteController, group, channel, first, second), slot, value);
                return;

            case 0x3:
                AddPoint(LineKey(LineKind::Midi2AssignableController, group, channel, first, static_cast<uint8_t>(second & 0x7F)), slot, value);
                return;

            case 0x6:
                AddPoint(LineKey(LineKind::Midi2PerNotePitchBend, group, channel, first, 0), slot, value);
                return;

            case 0xA:
                AddPoint(LineKey(LineKind::Midi2PolyPressure, group, channel, first, 0), slot, value);
                return;

            case 0xB:
                AddControlChange(LineKind::Midi2ControlChange, group, channel, first, value);
                return;

            case 0xD:
                AddPoint(LineKey(LineKind::Midi2ChannelPressure, group, channel, 0, 0), slot, value);
                return;

            case 0xE:
                AddPoint(LineKey(LineKind::Midi2PitchBend, group, channel, 0, 0), slot, value);
                return;

            default:
                // Notes, program changes, per-note management, registered controllers, which
                // can change what other values mean, and relative controllers, which add up.
                EndChannel(slot);
                return;
            }
        }

        std::unordered_map<uint32_t, Line> m_lines{};
        std::vector<uint8_t> m_pruned{};

        // Moved on each time something ends the lines on a channel, in a group, or everywhere
        std::array<uint32_t, 256> m_channelEpochs{};
        std::array<uint32_t, 16> m_groupEpochs{};
        uint32_t m_allEpoch{ 0 };

        size_t m_next{ 0 };
        uint32_t m_keepOneIn{ 1 };
        uint32_t m_stepsKeptInEveryRun{ MidiSendBacklogPruneStepsKeptInEveryRun };
        uint32_t m_prunedCount{ 0 };
        bool m_failed{ false };
    };
}
