// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

// Deliberately free of pch.h, WinRT and XAML so that this file compiles unchanged into the unit
// test project. See SmfReader.h for the rules this reader follows about untrusted input.

#include "midi_file_smf_reader.h"

#include <windows.h>

#include <algorithm>
#include <array>
#include <cctype>

namespace midifile
{
    namespace
    {
        constexpr uint32_t MaximumTick = 0xFFFFFFF0;

        constexpr uint8_t MetaEventStatus = 0xFF;
        constexpr uint8_t SystemExclusiveStatus = 0xF0;
        constexpr uint8_t SystemExclusiveEndStatus = 0xF7;

        constexpr uint8_t MetaEndOfTrack = 0x2F;
        constexpr uint8_t MetaSetTempo = 0x51;
        constexpr uint8_t MetaTimeSignature = 0x58;
        constexpr uint8_t MetaKeySignature = 0x59;

        constexpr uint8_t ControllerBankSelectMsb = 0x00;
        constexpr uint8_t ControllerBankSelectLsb = 0x20;

        // Chord symbols in the wild travel in a manufacturer system exclusive, because version 1
        // of the file format never defined an event for them:
        //   F0 00 20 24 00 01 <chord name in ASCII> 0A F7
        // Seen across a large corpus of real files, always carrying names like "Bb", "F#m7" or
        // "D/F#" for a lead sheet display. The message is still passed to the device as well as
        // being read here, since it is the manufacturer's own and may mean something to them.
        constexpr uint8_t ChordSymbolPrefix[]{ 0xF0, 0x00, 0x20, 0x24, 0x00, 0x01 };
        constexpr size_t MaximumChordSymbolLength = 32;

        class ByteReader
        {
        public:
            explicit ByteReader(std::span<uint8_t const> bytes) noexcept : m_bytes(bytes) {}

            size_t Offset() const noexcept { return m_offset; }
            size_t Remaining() const noexcept { return m_offset >= m_bytes.size() ? 0 : m_bytes.size() - m_offset; }
            bool AtEnd() const noexcept { return Remaining() == 0; }

            bool TryPeek(uint8_t& value) const noexcept
            {
                if (Remaining() == 0)
                {
                    return false;
                }

                value = m_bytes[m_offset];
                return true;
            }

            bool TryReadByte(uint8_t& value) noexcept
            {
                if (!TryPeek(value))
                {
                    return false;
                }

                ++m_offset;
                return true;
            }

            bool TryReadUInt16(uint16_t& value) noexcept
            {
                if (Remaining() < 2)
                {
                    return false;
                }

                value = static_cast<uint16_t>((m_bytes[m_offset] << 8) | m_bytes[m_offset + 1]);
                m_offset += 2;

                return true;
            }

            bool TryReadUInt32(uint32_t& value) noexcept
            {
                if (Remaining() < 4)
                {
                    return false;
                }

                value = (static_cast<uint32_t>(m_bytes[m_offset]) << 24)
                    | (static_cast<uint32_t>(m_bytes[m_offset + 1]) << 16)
                    | (static_cast<uint32_t>(m_bytes[m_offset + 2]) << 8)
                    | static_cast<uint32_t>(m_bytes[m_offset + 3]);

                m_offset += 4;

                return true;
            }

            // RP-001 variable length quantity. Four bytes is the documented ceiling, and stopping
            // there is what stops a run of high-bit bytes from being read as one enormous number.
            bool TryReadVariableLength(uint32_t& value) noexcept
            {
                value = 0;

                for (int index = 0; index < 4; ++index)
                {
                    uint8_t byte{};

                    if (!TryReadByte(byte))
                    {
                        return false;
                    }

                    value = (value << 7) | static_cast<uint32_t>(byte & 0x7F);

                    if ((byte & 0x80) == 0)
                    {
                        return true;
                    }
                }

                return false;
            }

            bool TryReadSpan(size_t count, std::span<uint8_t const>& value) noexcept
            {
                if (Remaining() < count)
                {
                    return false;
                }

                value = m_bytes.subspan(m_offset, count);
                m_offset += count;

                return true;
            }

            bool TrySkip(size_t count) noexcept
            {
                if (Remaining() < count)
                {
                    m_offset = m_bytes.size();
                    return false;
                }

                m_offset += count;
                return true;
            }

            void SeekTo(size_t offset) noexcept
            {
                m_offset = offset > m_bytes.size() ? m_bytes.size() : offset;
            }

        private:
            std::span<uint8_t const> m_bytes{};
            size_t m_offset{ 0 };
        };

        bool ChunkIdIs(std::span<uint8_t const> bytes, size_t offset, char const* id) noexcept
        {
            if (offset + 4 > bytes.size())
            {
                return false;
            }

            for (size_t index = 0; index < 4; ++index)
            {
                if (bytes[offset + index] != static_cast<uint8_t>(id[index]))
                {
                    return false;
                }
            }

            return true;
        }

        bool LooksLikeUtf8(std::span<uint8_t const> bytes) noexcept
        {
            size_t index = 0;

            while (index < bytes.size())
            {
                auto const lead = bytes[index];

                if (lead < 0x80)
                {
                    ++index;
                    continue;
                }

                size_t continuations = 0;

                if ((lead & 0xE0) == 0xC0) { continuations = 1; }
                else if ((lead & 0xF0) == 0xE0) { continuations = 2; }
                else if ((lead & 0xF8) == 0xF0) { continuations = 3; }
                else { return false; }

                if (index + continuations >= bytes.size())
                {
                    return false;
                }

                for (size_t offset = 1; offset <= continuations; ++offset)
                {
                    if ((bytes[index + offset] & 0xC0) != 0x80)
                    {
                        return false;
                    }
                }

                index += continuations + 1;
            }

            return true;
        }

        // Text in a Standard MIDI File is bytes with no declared encoding. Modern files are
        // usually UTF-8; older ones are Latin-1. Guessing UTF-8 first and falling back keeps both
        // readable, and nothing here can fail.
        //
        // Lyrics are the one kind where the whitespace is the content: a karaoke file marks its
        // word breaks with a trailing space and its line breaks with a carriage return, so tidying
        // those away would run a whole verse together.
        std::string DecodeText(std::span<uint8_t const> bytes, bool preserveWhitespace = false) noexcept
        {
            std::string text{};

            if (bytes.empty())
            {
                return text;
            }

            if (LooksLikeUtf8(bytes))
            {
                text.assign(reinterpret_cast<char const*>(bytes.data()), bytes.size());
            }
            else
            {
                text.reserve(bytes.size() * 2);

                for (auto const byte : bytes)
                {
                    if (byte < 0x80)
                    {
                        text.push_back(static_cast<char>(byte));
                    }
                    else
                    {
                        text.push_back(static_cast<char>(0xC0 | (byte >> 6)));
                        text.push_back(static_cast<char>(0x80 | (byte & 0x3F)));
                    }
                }
            }

            // Control characters in a track name wreck a text block's layout.
            std::erase_if(text, [preserveWhitespace](char value) noexcept
                {
                    auto const byte = static_cast<unsigned char>(value);

                    if (preserveWhitespace && (byte == '\r' || byte == '\n'))
                    {
                        return false;
                    }

                    return byte < 0x20 && byte != '\t';
                });

            while (!preserveWhitespace && !text.empty() && (text.back() == ' ' || text.back() == '\t'))
            {
                text.pop_back();
            }

            return text;
        }

        EventKind KindFromStatus(uint8_t status) noexcept
        {
            if (status >= 0xF8)
            {
                return EventKind::SystemRealTime;
            }

            switch (status & 0xF0)
            {
            case 0x80: return EventKind::NoteOff;
            case 0x90: return EventKind::NoteOn;
            case 0xA0: return EventKind::PolyphonicPressure;
            case 0xB0: return EventKind::ControlChange;
            case 0xC0: return EventKind::ProgramChange;
            case 0xD0: return EventKind::ChannelPressure;
            case 0xE0: return EventKind::PitchBend;
            default:   return EventKind::SystemCommon;
            }
        }

        // How many data bytes follow a status byte inside a track. Getting this wrong for the
        // system statuses desynchronizes the rest of the track, because the reader would swallow
        // bytes belonging to the next delta time.
        uint32_t DataByteCountForStatus(uint8_t status) noexcept
        {
            if (status < 0xF0)
            {
                auto const highNibble = static_cast<uint8_t>(status & 0xF0);

                return (highNibble == 0xC0 || highNibble == 0xD0) ? 1u : 2u;
            }

            switch (status)
            {
            case 0xF1: return 1u;   // MIDI time code quarter frame
            case 0xF2: return 2u;   // song position pointer
            case 0xF3: return 1u;   // song select
            default:   return 0u;   // tune request, the undefined statuses and all of real time
            }
        }

        // Empty when the dump is not a chord symbol.
        std::string ReadChordSymbol(std::span<uint8_t const> message) noexcept
        {
            if (message.size() <= sizeof(ChordSymbolPrefix) + 1)
            {
                return {};
            }

            for (size_t index = 0; index < sizeof(ChordSymbolPrefix); ++index)
            {
                if (message[index] != ChordSymbolPrefix[index])
                {
                    return {};
                }
            }

            std::string name{};

            for (size_t index = sizeof(ChordSymbolPrefix); index < message.size(); ++index)
            {
                auto const byte = message[index];

                // The name ends at the line feed the format uses as a terminator, or at the end
                // of the dump.
                if (byte == 0x0A || byte == SystemExclusiveEndStatus)
                {
                    break;
                }

                // Anything outside printable ASCII means this is not the message it looked like.
                if (byte < 0x20 || byte > 0x7E || name.size() >= MaximumChordSymbolLength)
                {
                    return {};
                }

                name.push_back(static_cast<char>(byte));
            }

            return name;
        }

        uint32_t AddTicks(uint32_t tick, uint32_t delta) noexcept
        {
            if (delta > MaximumTick - tick)
            {
                return MaximumTick;
            }

            return tick + delta;
        }

        struct TrackParseState
        {
            uint32_t Tick{ 0 };
            uint8_t RunningStatus{ 0 };
            std::array<uint8_t, 16> BankMsb{};
            std::array<uint8_t, 16> BankLsb{};
            std::vector<uint8_t> PendingSystemExclusive{};
        };

        class SequenceWriter
        {
        public:
            SequenceWriter(MidiSequence& sequence, ReadLimits const& limits) noexcept :
                m_sequence(sequence), m_limits(limits)
            {
            }

            bool AtCapacity() const noexcept { return m_atCapacity; }

            bool Append(
                uint32_t tick,
                uint16_t trackIndex,
                EventKind kind,
                uint8_t channel,
                std::span<uint8_t const> bytes) noexcept
            {
                if (m_atCapacity || bytes.empty())
                {
                    return !m_atCapacity;
                }

                if (m_sequence.Events.size() >= m_limits.MaximumEvents ||
                    m_sequence.EventBytes.size() + bytes.size() > m_limits.MaximumEventBytes)
                {
                    m_atCapacity = true;
                    return false;
                }

                SequenceEvent event{};

                event.Tick = tick;
                event.ByteOffset = static_cast<uint32_t>(m_sequence.EventBytes.size());
                event.ByteCount = static_cast<uint32_t>(bytes.size());
                event.TrackIndex = trackIndex;
                event.Kind = kind;
                event.Channel = channel;

                m_sequence.EventBytes.insert(m_sequence.EventBytes.end(), bytes.begin(), bytes.end());
                m_sequence.Events.push_back(event);

                return true;
            }

        private:
            MidiSequence& m_sequence;
            ReadLimits const& m_limits;
            bool m_atCapacity{ false };
        };

        void HandleMetaEvent(
            MidiSequence& sequence,
            ReadLimits const& limits,
            uint16_t trackIndex,
            uint32_t tick,
            uint8_t metaType,
            std::span<uint8_t const> data) noexcept
        {
            if (metaType >= 0x01 && metaType <= 0x0F)
            {
                if (data.size() > limits.MaximumTextBytes)
                {
                    data = data.subspan(0, limits.MaximumTextBytes);
                }

                auto const kind = metaType <= 0x09
                    ? static_cast<TextKind>(metaType)
                    : TextKind::Text;

                // A karaoke file carries its words in plain text events, so both kinds keep their
                // spacing; everything else is a name or a note to a reader and is tidied up.
                auto const preserveWhitespace = kind == TextKind::Lyric || kind == TextKind::Text;

                auto text = DecodeText(data, preserveWhitespace);

                if (text.empty())
                {
                    return;
                }

                if (trackIndex < sequence.Tracks.size())
                {
                    auto& track = sequence.Tracks[trackIndex];

                    switch (kind)
                    {
                    case TextKind::TrackName:
                        if (track.Name.empty()) { track.Name = text; }
                        break;

                    case TextKind::InstrumentName:
                        if (track.InstrumentName.empty()) { track.InstrumentName = text; }
                        break;

                    case TextKind::DeviceName:
                        if (track.DeviceName.empty()) { track.DeviceName = text; }
                        break;

                    default:
                        break;
                    }
                }

                if (kind == TextKind::Copyright && sequence.Copyright.empty())
                {
                    sequence.Copyright = text;
                }

                // A Soft Karaoke file announces itself before its first word, so from here on its
                // plain text events are lyrics and have to be kept.
                if (kind == TextKind::Text && !sequence.IsKaraoke && text.rfind("@K", 0) == 0)
                {
                    sequence.IsKaraoke = true;
                }

                // Only the kinds a display uses are kept, so a file full of editor comments does
                // not grow the model for nothing.
                if (kind == TextKind::Lyric || kind == TextKind::Marker || kind == TextKind::CuePoint ||
                    kind == TextKind::TrackName || kind == TextKind::ProgramName ||
                    (kind == TextKind::Text && sequence.IsKaraoke))
                {
                    if (sequence.TextEvents.size() < limits.MaximumTextEvents)
                    {
                        sequence.TextEvents.push_back(TextEvent{ tick, trackIndex, kind, std::move(text) });
                    }
                }

                return;
            }

            switch (metaType)
            {
            case MetaSetTempo:
                if (data.size() >= 3)
                {
                    TempoChange change{};

                    change.Tick = tick;
                    change.MicrosecondsPerQuarterNote =
                        (static_cast<uint32_t>(data[0]) << 16) |
                        (static_cast<uint32_t>(data[1]) << 8) |
                        static_cast<uint32_t>(data[2]);

                    if (change.MicrosecondsPerQuarterNote < MinimumMicrosecondsPerQuarterNote)
                    {
                        change.MicrosecondsPerQuarterNote = MinimumMicrosecondsPerQuarterNote;
                    }

                    sequence.TempoMap.push_back(change);
                }
                break;

            case MetaTimeSignature:
                if (data.size() >= 4)
                {
                    TimeSignatureChange change{};

                    change.Tick = tick;
                    change.Numerator = data[0] == 0 ? uint8_t{ 4 } : data[0];
                    change.DenominatorPowerOfTwo = data[1];
                    change.ClocksPerClick = data[2];
                    change.ThirtySecondNotesPerQuarter = data[3];

                    sequence.TimeSignatureMap.push_back(change);
                }
                break;

            case MetaKeySignature:
                if (data.size() >= 2)
                {
                    KeySignatureChange change{};

                    change.Tick = tick;
                    change.Accidentals = static_cast<int8_t>(data[0]);
                    change.IsMinor = data[1] != 0;

                    sequence.KeySignatureMap.push_back(change);
                }
                break;

            default:
                break;
            }
        }

        // Returns false when the track ran out of bytes part way through an event.
        bool ParseTrack(
            ByteReader& reader,
            size_t trackEndOffset,
            MidiSequence& sequence,
            SequenceWriter& writer,
            ReadLimits const& limits,
            uint16_t trackIndex,
            uint32_t tickOffset,
            uint32_t& lastTick) noexcept
        {
            TrackParseState state{};

            // A gap this long is never a musical rest, so it is used as the signal that the reader
            // has lost the event boundary. Generous on purpose: at a musical division this is about
            // twenty five minutes of silence at 120 beats per minute, and slower tempos stretch it
            // further still, so a real file cannot trip it.
            auto const maximumDelta = sequence.Division.IsSmpte
                ? static_cast<uint32_t>(sequence.Division.TicksPerSecond() * 1500.0)
                : static_cast<uint32_t>(sequence.Division.TicksPerQuarterNote) * 3000u;

            state.Tick = tickOffset;
            lastTick = tickOffset;

            while (reader.Offset() < trackEndOffset && !writer.AtCapacity())
            {
                uint32_t delta{};

                if (!reader.TryReadVariableLength(delta))
                {
                    return false;
                }

                if (delta > maximumDelta)
                {
                    // Nothing musical waits this long. The reader has lost the event boundary, and
                    // carrying on would invent a timeline hours or days long out of whatever bytes
                    // follow, so keep what is real and stop here.
                    return false;
                }

                state.Tick = AddTicks(state.Tick, delta);
                lastTick = state.Tick;

                uint8_t leadByte{};

                if (!reader.TryPeek(leadByte))
                {
                    return false;
                }

                if (leadByte == MetaEventStatus)
                {
                    state.RunningStatus = 0;

                    uint8_t metaType{};
                    uint32_t length{};

                    if (!reader.TryReadByte(leadByte) || !reader.TryReadByte(metaType) ||
                        !reader.TryReadVariableLength(length))
                    {
                        return false;
                    }

                    std::span<uint8_t const> data{};

                    if (!reader.TryReadSpan(length, data))
                    {
                        return false;
                    }

                    if (metaType == MetaEndOfTrack)
                    {
                        return true;
                    }

                    HandleMetaEvent(sequence, limits, trackIndex, state.Tick, metaType, data);
                    continue;
                }

                if (leadByte == SystemExclusiveStatus || leadByte == SystemExclusiveEndStatus)
                {
                    state.RunningStatus = 0;

                    uint32_t length{};

                    if (!reader.TryReadByte(leadByte) || !reader.TryReadVariableLength(length))
                    {
                        return false;
                    }

                    if (length > limits.MaximumSingleMessageBytes)
                    {
                        // Not plausible as a real dump. Skip it rather than carry it around.
                        if (!reader.TrySkip(length))
                        {
                            return false;
                        }

                        state.PendingSystemExclusive.clear();
                        continue;
                    }

                    std::span<uint8_t const> data{};

                    if (!reader.TryReadSpan(length, data))
                    {
                        return false;
                    }

                    if (leadByte == SystemExclusiveStatus)
                    {
                        state.PendingSystemExclusive.clear();
                        state.PendingSystemExclusive.push_back(SystemExclusiveStatus);
                        state.PendingSystemExclusive.insert(
                            state.PendingSystemExclusive.end(), data.begin(), data.end());

                        // Read before the terminator check below, because these are written both
                        // with and without a closing F7 and the symbol is worth having either way.
                        auto chord = ReadChordSymbol(std::span<uint8_t const>{ state.PendingSystemExclusive });

                        if (!chord.empty() && sequence.TextEvents.size() < limits.MaximumTextEvents)
                        {
                            sequence.TextEvents.push_back(
                                TextEvent{ state.Tick, trackIndex, TextKind::ChordSymbol, std::move(chord) });
                        }
                    }
                    else if (!state.PendingSystemExclusive.empty())
                    {
                        // A continuation of a dump that was split across several events.
                        state.PendingSystemExclusive.insert(
                            state.PendingSystemExclusive.end(), data.begin(), data.end());
                    }
                    else
                    {
                        // An escape carrying raw bytes. Only a single short system message is
                        // worth passing on; anything else would be guesswork on the wire.
                        if (data.size() >= 1 && data.size() <= 3 && data[0] >= 0xF0)
                        {
                            auto const kind = data[0] >= 0xF8 ? EventKind::SystemRealTime : EventKind::SystemCommon;
                            writer.Append(state.Tick, trackIndex, kind, ChannelNone, data);
                        }

                        continue;
                    }

                    if (!state.PendingSystemExclusive.empty() &&
                        state.PendingSystemExclusive.back() == SystemExclusiveEndStatus)
                    {
                        writer.Append(
                            state.Tick,
                            trackIndex,
                            EventKind::SystemExclusive,
                            ChannelNone,
                            std::span<uint8_t const>{ state.PendingSystemExclusive });

                        state.PendingSystemExclusive.clear();
                    }

                    continue;
                }

                uint8_t status{};

                if (leadByte >= 0x80)
                {
                    if (!reader.TryReadByte(status))
                    {
                        return false;
                    }

                    // Real time may be interleaved anywhere and leaves running status alone;
                    // system common cancels it; only a channel status becomes the new one.
                    if (status < 0xF0)
                    {
                        state.RunningStatus = status;
                    }
                    else if (status < 0xF8)
                    {
                        state.RunningStatus = 0;
                    }
                }
                else
                {
                    status = state.RunningStatus;

                    if (status < 0x80)
                    {
                        // A data byte with no status in front of it. Nothing after this point in
                        // the track can be interpreted, so keep what was read and stop.
                        return false;
                    }
                }

                auto const dataByteCount = DataByteCountForStatus(status);

                std::array<uint8_t, 3> message{};

                message[0] = status;

                for (uint32_t index = 0; index < dataByteCount; ++index)
                {
                    uint8_t value{};

                    if (!reader.TryReadByte(value))
                    {
                        return false;
                    }

                    // Out of range, but files written by buggy tools do this consistently while
                    // staying perfectly aligned, so the value is clamped rather than distrusted.
                    message[index + 1] = static_cast<uint8_t>(value & 0x7F);
                }

                auto const kind = KindFromStatus(status);
                auto const channel = status < 0xF0 ? static_cast<uint8_t>(status & 0x0F) : ChannelNone;

                if (kind == EventKind::ControlChange)
                {
                    if (message[1] == ControllerBankSelectMsb) { state.BankMsb[channel] = message[2]; }
                    else if (message[1] == ControllerBankSelectLsb) { state.BankLsb[channel] = message[2]; }
                }
                else if (kind == EventKind::ProgramChange)
                {
                    ProgramChangeEvent change{};

                    change.Tick = state.Tick;
                    change.TrackIndex = trackIndex;
                    change.Channel = channel;
                    change.Program = message[1];
                    change.BankMsb = state.BankMsb[channel];
                    change.BankLsb = state.BankLsb[channel];

                    sequence.ProgramChanges.push_back(change);
                }

                writer.Append(
                    state.Tick,
                    trackIndex,
                    kind,
                    channel,
                    std::span<uint8_t const>{ message.data(), dataByteCount + 1 });
            }

            return true;
        }

        // .rmi files wrap a Standard MIDI File in a RIFF container. Unwrapping it here means the
        // rest of the reader never has to know.
        std::span<uint8_t const> UnwrapRiffMidi(std::span<uint8_t const> fileBytes) noexcept
        {
            if (fileBytes.size() < 20 || !ChunkIdIs(fileBytes, 0, "RIFF") || !ChunkIdIs(fileBytes, 8, "RMID"))
            {
                return fileBytes;
            }

            size_t offset = 12;

            while (offset + 8 <= fileBytes.size())
            {
                uint32_t const length =
                    static_cast<uint32_t>(fileBytes[offset + 4]) |
                    (static_cast<uint32_t>(fileBytes[offset + 5]) << 8) |
                    (static_cast<uint32_t>(fileBytes[offset + 6]) << 16) |
                    (static_cast<uint32_t>(fileBytes[offset + 7]) << 24);

                auto const payload = offset + 8;
                auto const available = fileBytes.size() - payload;
                auto const usable = static_cast<size_t>(length) > available ? available : static_cast<size_t>(length);

                if (ChunkIdIs(fileBytes, offset, "data"))
                {
                    return fileBytes.subspan(payload, usable);
                }

                // RIFF chunks are word aligned.
                offset = payload + usable + (usable & 1);
            }

            return fileBytes;
        }
    }

    _Use_decl_annotations_
    ReadResult ParseStandardMidiFile(
        std::span<uint8_t const> fileBytes,
        MidiSequence& sequence,
        ReadLimits const& limits) noexcept
    {
        ReadResult result{};

        sequence.Clear();

        try
        {
            auto const bytes = UnwrapRiffMidi(fileBytes);

            ByteReader reader{ bytes };

            if (bytes.size() < 14 || !ChunkIdIs(bytes, 0, "MThd"))
            {
                result.Status = ReadStatus::NotAMidiFile;
                return result;
            }

            reader.TrySkip(4);

            uint32_t headerLength{};
            uint16_t format{};
            uint16_t trackCount{};
            uint16_t division{};

            if (!reader.TryReadUInt32(headerLength) || headerLength < 6 ||
                !reader.TryReadUInt16(format) || !reader.TryReadUInt16(trackCount) ||
                !reader.TryReadUInt16(division))
            {
                result.Status = ReadStatus::CorruptHeader;
                result.ByteOffset = reader.Offset();
                return result;
            }

            // A header longer than six bytes is legal; the extra is reserved.
            if (headerLength > 6)
            {
                reader.TrySkip(headerLength - 6);
            }

            sequence.Format = format > 2
                ? SequenceFormat::MultiTrack
                : static_cast<SequenceFormat>(format);

            if ((division & 0x8000) != 0)
            {
                sequence.Division.IsSmpte = true;
                sequence.Division.FramesPerSecond = static_cast<uint8_t>(-static_cast<int8_t>(division >> 8));
                sequence.Division.TicksPerFrame = static_cast<uint8_t>(division & 0xFF);

                if (sequence.Division.FramesPerSecond == 0 || sequence.Division.TicksPerFrame == 0)
                {
                    sequence.Division = TimeDivision{};
                }
            }
            else
            {
                sequence.Division.IsSmpte = false;
                sequence.Division.TicksPerQuarterNote = (division & 0x7FFF) == 0
                    ? DefaultTicksPerQuarterNote
                    : static_cast<uint16_t>(division & 0x7FFF);
            }

            result.TracksDeclared = trackCount;

            auto const tracksToRead = trackCount > limits.MaximumTracks
                ? limits.MaximumTracks
                : static_cast<uint32_t>(trackCount);

            SequenceWriter writer{ sequence, limits };

            // Independent sequences are laid end to end rather than played on top of each other.
            uint32_t sequentialOffset = 0;

            while (sequence.Tracks.size() < tracksToRead && !reader.AtEnd())
            {
                if (reader.Remaining() < 8)
                {
                    result.Truncated = true;
                    break;
                }

                auto const chunkStart = reader.Offset();
                auto const isTrack = ChunkIdIs(bytes, chunkStart, "MTrk");

                reader.TrySkip(4);

                uint32_t chunkLength{};

                if (!reader.TryReadUInt32(chunkLength))
                {
                    result.Truncated = true;
                    break;
                }

                auto const payloadStart = reader.Offset();
                auto const available = reader.Remaining();

                if (static_cast<size_t>(chunkLength) > available)
                {
                    // The declared length runs past the end of the file. Play what is there.
                    chunkLength = static_cast<uint32_t>(available);
                    result.Truncated = true;
                }

                if (!isTrack)
                {
                    // An unknown chunk between tracks is legal and must be skipped.
                    reader.TrySkip(chunkLength);
                    continue;
                }

                auto const trackIndex = static_cast<uint16_t>(sequence.Tracks.size());

                sequence.Tracks.push_back(Track{});

                uint32_t trackLastTick = sequentialOffset;

                if (!ParseTrack(
                    reader,
                    payloadStart + chunkLength,
                    sequence,
                    writer,
                    limits,
                    trackIndex,
                    sequence.Format == SequenceFormat::MultiSequence ? sequentialOffset : 0,
                    trackLastTick))
                {
                    result.Truncated = true;
                }

                if (sequence.Format == SequenceFormat::MultiSequence)
                {
                    sequentialOffset = trackLastTick;
                }

                ++result.TracksRead;

                if (writer.AtCapacity())
                {
                    result.Truncated = true;
                    break;
                }

                reader.SeekTo(payloadStart + chunkLength);
            }

            if (sequence.Events.empty())
            {
                result.Status = ReadStatus::NoPlayableData;
                result.ByteOffset = reader.Offset();
                return result;
            }

            sequence.Finalize();

            return result;
        }
        catch (...)
        {
            // Only an allocation failure can get here; everything else is checked.
            sequence.Clear();

            result.Status = ReadStatus::TooMuchData;
            return result;
        }
    }

    _Use_decl_annotations_
    ReadResult ReadStandardMidiFile(
        std::wstring const& path,
        MidiSequence& sequence,
        ReadLimits const& limits) noexcept
    {
        ReadResult result{};

        sequence.Clear();

        HANDLE const file = ::CreateFileW(
            path.c_str(),
            GENERIC_READ,
            FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
            nullptr,
            OPEN_EXISTING,
            FILE_ATTRIBUTE_NORMAL | FILE_FLAG_SEQUENTIAL_SCAN,
            nullptr);

        if (file == INVALID_HANDLE_VALUE)
        {
            auto const error = ::GetLastError();

            result.Status = (error == ERROR_FILE_NOT_FOUND || error == ERROR_PATH_NOT_FOUND)
                ? ReadStatus::FileNotFound
                : (error == ERROR_ACCESS_DENIED ? ReadStatus::AccessDenied : ReadStatus::ReadFailed);

            return result;
        }

        LARGE_INTEGER size{};

        if (!::GetFileSizeEx(file, &size))
        {
            ::CloseHandle(file);

            result.Status = ReadStatus::ReadFailed;
            return result;
        }

        if (size.QuadPart <= 0)
        {
            ::CloseHandle(file);

            result.Status = ReadStatus::NotAMidiFile;
            return result;
        }

        if (static_cast<uint64_t>(size.QuadPart) > limits.MaximumFileBytes)
        {
            ::CloseHandle(file);

            result.Status = ReadStatus::FileTooLarge;
            return result;
        }

        std::vector<uint8_t> buffer{};

        try
        {
            buffer.resize(static_cast<size_t>(size.QuadPart));
        }
        catch (...)
        {
            ::CloseHandle(file);

            result.Status = ReadStatus::TooMuchData;
            return result;
        }

        size_t totalRead = 0;

        while (totalRead < buffer.size())
        {
            auto const remaining = buffer.size() - totalRead;
            auto const chunk = remaining > 0x10000000u ? 0x10000000u : static_cast<DWORD>(remaining);

            DWORD read = 0;

            if (!::ReadFile(file, buffer.data() + totalRead, chunk, &read, nullptr) || read == 0)
            {
                break;
            }

            totalRead += read;
        }

        ::CloseHandle(file);

        if (totalRead == 0)
        {
            result.Status = ReadStatus::ReadFailed;
            return result;
        }

        buffer.resize(totalRead);

        return ParseStandardMidiFile(std::span<uint8_t const>{ buffer }, sequence, limits);
    }

    _Use_decl_annotations_
    bool IsStandardMidiFileExtension(std::wstring const& path) noexcept
    {
        auto const dot = path.find_last_of(L'.');

        if (dot == std::wstring::npos)
        {
            return false;
        }

        std::wstring extension = path.substr(dot);

        std::transform(extension.begin(), extension.end(), extension.begin(),
            [](wchar_t value) noexcept { return static_cast<wchar_t>(::towlower(value)); });

        return extension == L".mid"
            || extension == L".midi"
            || extension == L".smf"
            || extension == L".rmi"
            || extension == L".kar";
    }
}
