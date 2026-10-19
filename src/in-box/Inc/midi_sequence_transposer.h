// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

#include <sal.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <span>

namespace midiplayer
{
    // Moves notes up or down by a number of semitones as they go out. Drums are left alone,
    // because on a drum kit the note number picks the drum, not the pitch.
    //
    // A note keeps the pitch it started at. Its end, and anything aimed at it while it sounds,
    // such as poly pressure, goes to the note that actually started, even when the transposition
    // changed in between. Without that, changing key during a held chord would leave the chord
    // sounding with nothing to end it.
    //
    // A note that would land outside 0 to 127 is not played, and neither is anything aimed at it.
    //
    // Every note goes through here, transposed or not, so a later change always knows what is
    // sounding. Until the transposition changes, the words come out exactly as they went in.
    //
    // Not thread safe. The playback engine only calls it with its own lock held.
    class NoteTransposer
    {
    public:
        static constexpr int32_t MinimumSemitones = -12;
        static constexpr int32_t MaximumSemitones = 12;

        // What starting a note came to. Either can be -1, for nothing.
        struct NoteStart
        {
            // The note to start. -1 when it would land outside 0 to 127.
            int16_t Note{ -1 };

            // A note on the same key and channel to end first: one started at the old
            // transposition and still sounding, which nothing else would end.
            int16_t EndFirst{ -1 };
        };

        // Clamped to the limits.
        void SetSemitones(_In_ int32_t semitones) noexcept
        {
            m_semitones = std::clamp(semitones, MinimumSemitones, MaximumSemitones);
        }

        int32_t Semitones() const noexcept
        {
            return m_semitones;
        }

        // Forgets every sounding note. For when everything has just been silenced.
        void Clear() noexcept
        {
            for (auto& channel : m_keys)
            {
                channel.fill(Key{});
            }
        }

        // True when the packet that starts with this word carries a note number: a note on or
        // off, poly pressure, or one of the MIDI 2.0 per-note messages.
        static constexpr bool NamesNote(_In_ uint32_t word0) noexcept
        {
            auto const status = StatusOf(word0);

            switch (MessageTypeOf(word0))
            {
            case Midi1ChannelVoice:
                return status == NoteOff || status == NoteOn || status == PolyPressure;

            case Midi2ChannelVoice:
                return status == NoteOff || status == NoteOn || status == PolyPressure ||
                    status == RegisteredPerNoteController || status == AssignablePerNoteController ||
                    status == PerNotePitchBend || status == PerNoteManagement;

            default:
                return false;
            }
        }

        // True when any packet in a run of them carries a note number.
        static bool NamesAnyNote(_In_ std::span<uint32_t const> words) noexcept
        {
            for (size_t index = 0; index < words.size(); )
            {
                if (NamesNote(words[index]))
                {
                    return true;
                }

                index += WordsInPacket(words[index]);
            }

            return false;
        }

        // Rewrites one event's packets into output and returns how many words it wrote, which
        // can be none. Packets that carry no note number are copied unchanged.
        //
        // output needs room for twice the input: a key struck again while it still sounds at an
        // old pitch has that note ended first.
        //
        // percussionChannels has a bit set for each channel playing a drum kit, channel 0 in the
        // lowest bit. Their notes are not moved.
        size_t Rewrite(
            _In_ std::span<uint32_t const> input,
            _In_ std::span<uint32_t> output,
            _In_ uint16_t percussionChannels) noexcept
        {
            size_t read = 0;
            size_t written = 0;

            auto const append = [&output, &written](uint32_t word) noexcept
                {
                    if (written < output.size())
                    {
                        output[written++] = word;
                    }
                };

            while (read < input.size())
            {
                auto const word0 = input[read];
                auto const packetWords = WordsInPacket(word0);

                if (read + packetWords > input.size() || !NamesNote(word0))
                {
                    // Cut short or not about a note: unchanged. A packet cut short carries the
                    // rest of the event with it, the way the engine has always sent it.
                    auto const count = read + packetWords > input.size() ? input.size() - read : packetWords;

                    for (size_t index = 0; index < count; ++index)
                    {
                        append(input[read + index]);
                    }

                    read += count;
                    continue;
                }

                auto const packet = input.subspan(read, packetWords);
                read += packetWords;

                auto const channel = static_cast<uint8_t>((word0 >> 16) & 0x0F);
                auto const note = static_cast<uint8_t>((word0 >> 8) & 0x7F);
                auto const offset = (percussionChannels & (1u << channel)) != 0 ? 0 : m_semitones;

                int32_t pitch{ -1 };

                if (IsNoteStart(packet))
                {
                    auto const start = Start(channel, note, offset);

                    if (start.EndFirst >= 0)
                    {
                        append(NoteOffWord(word0, channel, static_cast<uint8_t>(start.EndFirst)));

                        if (MessageTypeOf(word0) == Midi2ChannelVoice)
                        {
                            append(0);
                        }
                    }

                    pitch = start.Note;
                }
                else
                {
                    pitch = IsNoteEnd(packet) ? End(channel, note, offset) : Aimed(channel, note, offset);
                }

                if (pitch < 0)
                {
                    continue;
                }

                auto const shift = pitch - static_cast<int32_t>(note);

                append((word0 & 0xFFFF00FFu) | (static_cast<uint32_t>(pitch) << 8));

                for (size_t index = 1; index < packet.size(); ++index)
                {
                    append(index == 1 ? ShiftPitchData(word0, packet[1], shift) : packet[index]);
                }
            }

            return written;
        }

        // For a note the engine starts on its own, such as one restored after a seek. Remembered
        // like any other, so its end finds it.
        NoteStart StartNote(_In_ uint8_t channel, _In_ uint8_t note, _In_ bool percussion) noexcept
        {
            return Start(static_cast<uint8_t>(channel & 0x0F), static_cast<uint8_t>(note & 0x7F), percussion ? 0 : m_semitones);
        }

        // Words in a Universal MIDI Packet, from the first word's message type.
        static constexpr size_t WordsInPacket(_In_ uint32_t word0) noexcept
        {
            constexpr std::array<uint8_t, 16> counts{ 1, 1, 1, 2, 2, 4, 1, 1, 2, 2, 2, 3, 3, 4, 4, 4 };
            return counts[MessageTypeOf(word0)];
        }

    private:
        static constexpr uint8_t Midi1ChannelVoice = 0x2;
        static constexpr uint8_t Midi2ChannelVoice = 0x4;

        static constexpr uint8_t RegisteredPerNoteController = 0x0;
        static constexpr uint8_t AssignablePerNoteController = 0x1;
        static constexpr uint8_t PerNotePitchBend = 0x6;
        static constexpr uint8_t NoteOff = 0x8;
        static constexpr uint8_t NoteOn = 0x9;
        static constexpr uint8_t PolyPressure = 0xA;
        static constexpr uint8_t PerNoteManagement = 0xF;

        // A MIDI 2.0 note on or off can carry the exact pitch in 7.9 form, and registered
        // per-note controller 3 sets it in 7.25 form. A receiver plays that pitch rather than the
        // note number, so it moves with the note.
        static constexpr uint8_t PitchAttribute = 0x03;
        static constexpr uint8_t PitchPerNoteController = 0x03;

        // What a key on a channel is sounding: the pitch it went out at plus one, Silent, or
        // Dropped for notes that landed outside 0 to 127. Starts counts the notes on the key still
        // waiting for their end, because a file can strike a key again before ending it, and
        // every one of those ends has to reach the pitch its start went out at.
        static constexpr uint8_t Silent = 0;
        static constexpr uint8_t Dropped = 0xFF;

        struct Key
        {
            uint8_t Sounding{ Silent };
            uint8_t Starts{ 0 };
        };

        static constexpr uint8_t MessageTypeOf(uint32_t word0) noexcept
        {
            return static_cast<uint8_t>((word0 >> 28) & 0x0F);
        }

        static constexpr uint8_t StatusOf(uint32_t word0) noexcept
        {
            return static_cast<uint8_t>((word0 >> 20) & 0x0F);
        }

        // A MIDI 1.0 note on with no velocity is a note off. A MIDI 2.0 one is a real note.
        static bool IsNoteStart(std::span<uint32_t const> packet) noexcept
        {
            if (StatusOf(packet[0]) != NoteOn)
            {
                return false;
            }

            return MessageTypeOf(packet[0]) == Midi2ChannelVoice || (packet[0] & 0x7F) != 0;
        }

        static bool IsNoteEnd(std::span<uint32_t const> packet) noexcept
        {
            auto const status = StatusOf(packet[0]);

            return status == NoteOff || (status == NoteOn && !IsNoteStart(packet));
        }

        static uint32_t NoteOffWord(uint32_t word0, uint8_t channel, uint8_t note) noexcept
        {
            // Same message type and group, no velocity, and for MIDI 2.0 no attribute.
            return (word0 & 0xFF000000u)
                | (static_cast<uint32_t>((NoteOff << 4) | channel) << 16)
                | (static_cast<uint32_t>(note) << 8);
        }

        static uint32_t ShiftPitchData(uint32_t word0, uint32_t word1, int32_t shift) noexcept
        {
            if (shift == 0 || MessageTypeOf(word0) != Midi2ChannelVoice)
            {
                return word1;
            }

            auto const status = StatusOf(word0);
            auto const index = static_cast<uint8_t>(word0 & 0xFF);

            if ((status == NoteOn || status == NoteOff) && index == PitchAttribute)
            {
                auto const pitch = std::clamp(static_cast<int64_t>(word1 & 0xFFFF) + shift * int64_t{ 1 << 9 }, int64_t{ 0 }, int64_t{ 0xFFFF });
                return (word1 & 0xFFFF0000u) | static_cast<uint32_t>(pitch);
            }

            if (status == RegisteredPerNoteController && index == PitchPerNoteController)
            {
                auto const pitch = std::clamp(static_cast<int64_t>(word1) + shift * int64_t{ 1 << 25 }, int64_t{ 0 }, int64_t{ 0xFFFFFFFF });
                return static_cast<uint32_t>(pitch);
            }

            return word1;
        }

        NoteStart Start(uint8_t channel, uint8_t note, int32_t offset) noexcept
        {
            auto& key = m_keys[channel][note];

            auto const wanted = static_cast<int32_t>(note) + offset;
            auto const fits = wanted >= 0 && wanted <= 127;
            auto const target = fits ? static_cast<uint8_t>(wanted + 1) : Dropped;

            NoteStart start{};

            if (key.Sounding == Silent)
            {
                key.Sounding = target;
                key.Starts = 1;
            }
            else if (key.Sounding == target)
            {
                // Struck again where it already is: the same as without a transposer.
                if (key.Starts < UINT8_MAX)
                {
                    ++key.Starts;
                }
            }
            else if (key.Sounding != Dropped && !fits)
            {
                // Nothing can start, and the note already sounding still needs this key's ends
                // to reach it.
                return start;
            }
            else
            {
                // Struck again after the key changed. Nothing else would end the note at the old
                // pitch, so it ends here, and the key starts over at the new one.
                if (key.Sounding != Dropped)
                {
                    start.EndFirst = static_cast<int16_t>(key.Sounding - 1);
                }

                key.Sounding = target;
                key.Starts = 1;
            }

            start.Note = fits ? static_cast<int16_t>(wanted) : int16_t{ -1 };

            return start;
        }

        // Where an end goes, and one fewer start waiting on the key.
        int32_t End(uint8_t channel, uint8_t note, int32_t offset) noexcept
        {
            auto const pitch = Aimed(channel, note, offset);

            auto& key = m_keys[channel][note];

            if (key.Starts > 1)
            {
                --key.Starts;
            }
            else
            {
                key = Key{};
            }

            return pitch;
        }

        // Where an end or a per-note message goes: the note this key is sounding, or, with
        // nothing sounding, where the transposition puts it now.
        int32_t Aimed(uint8_t channel, uint8_t note, int32_t offset) const noexcept
        {
            auto const sounding = m_keys[channel][note].Sounding;

            if (sounding == Dropped)
            {
                return -1;
            }

            if (sounding != Silent)
            {
                return sounding - 1;
            }

            auto const wanted = static_cast<int32_t>(note) + offset;

            return (wanted >= 0 && wanted <= 127) ? wanted : -1;
        }

        int32_t m_semitones{ 0 };

        std::array<std::array<Key, 128>, 16> m_keys{};
    };
}
