// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

// Deliberately free of pch.h, WinRT and XAML so that this file compiles unchanged into the unit
// test project, exactly like the reader it mirrors.

#include "midi_file_smf_writer.h"

#include <windows.h>

#include <algorithm>
#include <array>
#include <cstring>

namespace midifile
{
    namespace
    {
        constexpr uint8_t MetaEventStatus = 0xFF;
        constexpr uint8_t SystemExclusiveStatus = 0xF0;
        constexpr uint8_t SystemExclusiveEndStatus = 0xF7;

        constexpr uint8_t MetaEndOfTrack = 0x2F;
        constexpr uint8_t MetaSetTempo = 0x51;
        constexpr uint8_t MetaTimeSignature = 0x58;
        constexpr uint8_t MetaKeySignature = 0x59;

        constexpr uint8_t MetaTextCopyright = 0x02;
        constexpr uint8_t MetaTextTrackName = 0x03;
        constexpr uint8_t MetaTextInstrumentName = 0x04;
        constexpr uint8_t MetaTextDeviceName = 0x09;

        constexpr uint32_t MaximumVariableLengthValue = 0x0FFFFFFF;

        // Meta events sharing a tick are written in this order so that a tempo or a meter change
        // is already in force for the notes at the same tick.
        constexpr uint32_t OrderTrackHeading = 0;
        constexpr uint32_t OrderTimeSignature = 1;
        constexpr uint32_t OrderKeySignature = 2;
        constexpr uint32_t OrderTempo = 3;
        constexpr uint32_t OrderText = 4;

        constexpr uint32_t DefaultTempoMicrosecondsPerQuarterNote = 500000;  // 120 beats per minute

        // Words in a Universal MIDI Packet, indexed by message type.
        constexpr uint8_t UmpWordCounts[16]{ 1, 1, 1, 2, 2, 4, 1, 1, 2, 2, 2, 3, 3, 4, 4, 4 };

        constexpr uint8_t ControllerBankSelectMsb = 0x00;
        constexpr uint8_t ControllerBankSelectLsb = 0x20;
        constexpr uint8_t ControllerDataEntryMsb = 0x06;
        constexpr uint8_t ControllerDataEntryLsb = 0x26;
        constexpr uint8_t ControllerNonRegisteredLsb = 0x62;
        constexpr uint8_t ControllerNonRegisteredMsb = 0x63;
        constexpr uint8_t ControllerRegisteredLsb = 0x64;
        constexpr uint8_t ControllerRegisteredMsb = 0x65;

        // How a MIDI 1.0 message is put into a track. The three are written differently: a
        // channel message may use running status, a dump carries its own length, and a system
        // real time or common byte has to be escaped or a reader would mistake it for a status.
        enum class Midi1MessageClass : uint8_t
        {
            ChannelVoice,
            SystemExclusive,
            SystemEscape
        };

        void AppendVariableLength(std::vector<uint8_t>& bytes, uint32_t value)
        {
            if (value > MaximumVariableLengthValue)
            {
                value = MaximumVariableLengthValue;
            }

            uint32_t buffer = value & 0x7F;

            while ((value >>= 7) != 0)
            {
                buffer <<= 8;
                buffer |= 0x80;
                buffer |= value & 0x7F;
            }

            for (;;)
            {
                bytes.push_back(static_cast<uint8_t>(buffer & 0xFF));

                if ((buffer & 0x80) == 0)
                {
                    break;
                }

                buffer >>= 8;
            }
        }

        void AppendUInt16(std::vector<uint8_t>& bytes, uint16_t value)
        {
            bytes.push_back(static_cast<uint8_t>(value >> 8));
            bytes.push_back(static_cast<uint8_t>(value & 0xFF));
        }

        void AppendUInt32(std::vector<uint8_t>& bytes, uint32_t value)
        {
            bytes.push_back(static_cast<uint8_t>(value >> 24));
            bytes.push_back(static_cast<uint8_t>((value >> 16) & 0xFF));
            bytes.push_back(static_cast<uint8_t>((value >> 8) & 0xFF));
            bytes.push_back(static_cast<uint8_t>(value & 0xFF));
        }

        void AppendChunkId(std::vector<uint8_t>& bytes, char const* id)
        {
            for (size_t index = 0; index < 4; ++index)
            {
                bytes.push_back(static_cast<uint8_t>(id[index]));
            }
        }

        uint32_t DataByteCountForSystemStatus(uint8_t status) noexcept
        {
            switch (status)
            {
            case 0xF1: return 1u;   // MIDI time code quarter frame
            case 0xF2: return 2u;   // song position pointer
            case 0xF3: return 1u;   // song select
            default:   return 0u;
            }
        }

        // Scaling down to MIDI 1.0 keeps the most significant bits, which is what the UMP
        // specification asks for and what makes the result stable across a round trip.
        constexpr uint8_t Scale16To7(uint16_t value) noexcept { return static_cast<uint8_t>(value >> 9); }
        constexpr uint8_t Scale32To7(uint32_t value) noexcept { return static_cast<uint8_t>(value >> 25); }
        constexpr uint16_t Scale32To14(uint32_t value) noexcept { return static_cast<uint16_t>(value >> 18); }

        struct PendingMeta
        {
            uint32_t Tick{ 0 };
            uint32_t Order{ 0 };
            std::vector<uint8_t> Bytes{};
        };

        void AppendMetaEvent(
            std::vector<PendingMeta>& metas,
            uint32_t tick,
            uint32_t order,
            uint8_t metaType,
            std::span<uint8_t const> data)
        {
            PendingMeta meta{};

            meta.Tick = tick;
            meta.Order = order;

            meta.Bytes.push_back(MetaEventStatus);
            meta.Bytes.push_back(metaType);

            AppendVariableLength(meta.Bytes, static_cast<uint32_t>(data.size()));

            meta.Bytes.insert(meta.Bytes.end(), data.begin(), data.end());

            metas.push_back(std::move(meta));
        }

        void AppendMetaText(
            std::vector<PendingMeta>& metas,
            uint32_t tick,
            uint32_t order,
            uint8_t metaType,
            std::string const& text)
        {
            if (text.empty())
            {
                return;
            }

            AppendMetaEvent(
                metas,
                tick,
                order,
                metaType,
                std::span<uint8_t const>{ reinterpret_cast<uint8_t const*>(text.data()), text.size() });
        }

        // Translates one Universal MIDI Packet into the MIDI 1.0 messages this format can hold.
        // Returns false when MIDI 1.0 has no equivalent at all, which is what the skipped count
        // reports. A system exclusive that is still being assembled returns true and emits
        // nothing, because its bytes are not lost - they are waiting for the packet that ends it.
        template<typename TEmit>
        bool TranslateUniversalPacket(
            std::span<uint32_t const> words,
            std::array<std::vector<uint8_t>, 16>& pendingSystemExclusive,
            TEmit&& emit)
        {
            if (words.empty())
            {
                return false;
            }

            auto const word0 = words[0];
            auto const messageType = static_cast<uint8_t>((word0 >> 28) & 0x0F);
            auto const group = static_cast<uint8_t>((word0 >> 24) & 0x0F);

            switch (messageType)
            {
            case 0x1:   // system real time and system common
            {
                std::array<uint8_t, 3> message{};

                message[0] = static_cast<uint8_t>((word0 >> 16) & 0xFF);

                if (message[0] < 0xF0)
                {
                    return false;
                }

                message[1] = static_cast<uint8_t>((word0 >> 8) & 0x7F);
                message[2] = static_cast<uint8_t>(word0 & 0x7F);

                auto const count = DataByteCountForSystemStatus(message[0]);

                emit(Midi1MessageClass::SystemEscape, std::span<uint8_t const>{ message.data(), count + 1u });

                return true;
            }

            case 0x2:   // MIDI 1.0 channel voice
            {
                std::array<uint8_t, 3> message{};

                message[0] = static_cast<uint8_t>((word0 >> 16) & 0xFF);

                if (message[0] < 0x80 || message[0] >= 0xF0)
                {
                    return false;
                }

                message[1] = static_cast<uint8_t>((word0 >> 8) & 0x7F);
                message[2] = static_cast<uint8_t>(word0 & 0x7F);

                auto const highNibble = static_cast<uint8_t>(message[0] & 0xF0);
                auto const count = (highNibble == 0xC0 || highNibble == 0xD0) ? 1u : 2u;

                emit(Midi1MessageClass::ChannelVoice, std::span<uint8_t const>{ message.data(), count + 1u });

                return true;
            }

            case 0x3:   // 64 bit data, which is 7 bit system exclusive
            {
                if (words.size() < 2)
                {
                    return false;
                }

                auto const status = static_cast<uint8_t>((word0 >> 20) & 0x0F);
                auto byteCount = static_cast<uint8_t>((word0 >> 16) & 0x0F);

                if (byteCount > 6)
                {
                    byteCount = 6;
                }

                std::array<uint8_t, 6> data{};

                data[0] = static_cast<uint8_t>((word0 >> 8) & 0x7F);
                data[1] = static_cast<uint8_t>(word0 & 0x7F);
                data[2] = static_cast<uint8_t>((words[1] >> 24) & 0x7F);
                data[3] = static_cast<uint8_t>((words[1] >> 16) & 0x7F);
                data[4] = static_cast<uint8_t>((words[1] >> 8) & 0x7F);
                data[5] = static_cast<uint8_t>(words[1] & 0x7F);

                auto& buffer = pendingSystemExclusive[group];

                if (status == 0x0 || status == 0x1)
                {
                    buffer.clear();
                    buffer.push_back(SystemExclusiveStatus);
                }
                else if (buffer.empty())
                {
                    // A continuation with nothing in front of it. There is no dump to finish.
                    return false;
                }

                buffer.insert(buffer.end(), data.begin(), data.begin() + byteCount);

                if (status == 0x0 || status == 0x3)
                {
                    buffer.push_back(SystemExclusiveEndStatus);

                    emit(Midi1MessageClass::SystemExclusive, std::span<uint8_t const>{ buffer });

                    buffer.clear();
                }

                return true;
            }

            case 0x4:   // MIDI 2.0 channel voice
            {
                if (words.size() < 2)
                {
                    return false;
                }

                auto const opcode = static_cast<uint8_t>((word0 >> 20) & 0x0F);
                auto const channel = static_cast<uint8_t>((word0 >> 16) & 0x0F);
                auto const index1 = static_cast<uint8_t>((word0 >> 8) & 0x7F);
                auto const index2 = static_cast<uint8_t>(word0 & 0xFF);
                auto const data = words[1];

                std::array<uint8_t, 3> message{};

                switch (opcode)
                {
                case 0x8:   // note off
                    message[0] = static_cast<uint8_t>(0x80 | channel);
                    message[1] = index1;
                    message[2] = Scale16To7(static_cast<uint16_t>(data >> 16));
                    emit(Midi1MessageClass::ChannelVoice, std::span<uint8_t const>{ message.data(), 3 });
                    return true;

                case 0x9:   // note on
                {
                    auto velocity = Scale16To7(static_cast<uint16_t>(data >> 16));

                    // MIDI 2.0 really can send a note on at velocity zero. MIDI 1.0 reads that as
                    // a note off, so the quietest velocity MIDI 1.0 has is used instead.
                    if (velocity == 0)
                    {
                        velocity = 1;
                    }

                    message[0] = static_cast<uint8_t>(0x90 | channel);
                    message[1] = index1;
                    message[2] = velocity;
                    emit(Midi1MessageClass::ChannelVoice, std::span<uint8_t const>{ message.data(), 3 });
                    return true;
                }

                case 0xA:   // polyphonic pressure
                    message[0] = static_cast<uint8_t>(0xA0 | channel);
                    message[1] = index1;
                    message[2] = Scale32To7(data);
                    emit(Midi1MessageClass::ChannelVoice, std::span<uint8_t const>{ message.data(), 3 });
                    return true;

                case 0xB:   // control change
                    message[0] = static_cast<uint8_t>(0xB0 | channel);
                    message[1] = index1;
                    message[2] = Scale32To7(data);
                    emit(Midi1MessageClass::ChannelVoice, std::span<uint8_t const>{ message.data(), 3 });
                    return true;

                case 0xC:   // program change
                {
                    if ((index2 & 0x01) != 0)
                    {
                        // MIDI 2.0 carries the bank with the program. MIDI 1.0 has to send it in
                        // front, or the program selects a sound from whatever bank was last set.
                        message[0] = static_cast<uint8_t>(0xB0 | channel);
                        message[1] = ControllerBankSelectMsb;
                        message[2] = static_cast<uint8_t>((data >> 8) & 0x7F);
                        emit(Midi1MessageClass::ChannelVoice, std::span<uint8_t const>{ message.data(), 3 });

                        message[1] = ControllerBankSelectLsb;
                        message[2] = static_cast<uint8_t>(data & 0x7F);
                        emit(Midi1MessageClass::ChannelVoice, std::span<uint8_t const>{ message.data(), 3 });
                    }

                    message[0] = static_cast<uint8_t>(0xC0 | channel);
                    message[1] = static_cast<uint8_t>((data >> 24) & 0x7F);
                    emit(Midi1MessageClass::ChannelVoice, std::span<uint8_t const>{ message.data(), 2 });
                    return true;
                }

                case 0xD:   // channel pressure
                    message[0] = static_cast<uint8_t>(0xD0 | channel);
                    message[1] = Scale32To7(data);
                    emit(Midi1MessageClass::ChannelVoice, std::span<uint8_t const>{ message.data(), 2 });
                    return true;

                case 0xE:   // pitch bend
                {
                    auto const bend = Scale32To14(data);

                    message[0] = static_cast<uint8_t>(0xE0 | channel);
                    message[1] = static_cast<uint8_t>(bend & 0x7F);
                    message[2] = static_cast<uint8_t>((bend >> 7) & 0x7F);
                    emit(Midi1MessageClass::ChannelVoice, std::span<uint8_t const>{ message.data(), 3 });
                    return true;
                }

                case 0x2:   // registered controller
                case 0x3:   // assignable controller
                {
                    auto const value = Scale32To14(data);

                    message[0] = static_cast<uint8_t>(0xB0 | channel);
                    message[1] = opcode == 0x2 ? ControllerRegisteredMsb : ControllerNonRegisteredMsb;
                    message[2] = index1;
                    emit(Midi1MessageClass::ChannelVoice, std::span<uint8_t const>{ message.data(), 3 });

                    message[1] = opcode == 0x2 ? ControllerRegisteredLsb : ControllerNonRegisteredLsb;
                    message[2] = static_cast<uint8_t>(index2 & 0x7F);
                    emit(Midi1MessageClass::ChannelVoice, std::span<uint8_t const>{ message.data(), 3 });

                    message[1] = ControllerDataEntryMsb;
                    message[2] = static_cast<uint8_t>((value >> 7) & 0x7F);
                    emit(Midi1MessageClass::ChannelVoice, std::span<uint8_t const>{ message.data(), 3 });

                    message[1] = ControllerDataEntryLsb;
                    message[2] = static_cast<uint8_t>(value & 0x7F);
                    emit(Midi1MessageClass::ChannelVoice, std::span<uint8_t const>{ message.data(), 3 });
                    return true;
                }

                default:
                    // Per note controllers, per note pitch bend, per note management and the
                    // relative controllers. MIDI 1.0 has nothing to say here.
                    return false;
                }
            }

            default:
                // Utility, 8 bit data, flex data, stream messages and the reserved types.
                return false;
            }
        }

        // One track's worth of bytes, without the chunk header.
        class TrackBodyWriter
        {
        public:
            TrackBodyWriter(std::vector<uint8_t>& bytes, bool useRunningStatus) noexcept :
                m_bytes(bytes), m_useRunningStatus(useRunningStatus)
            {
            }

            void WriteMeta(uint32_t tick, std::span<uint8_t const> completeEvent)
            {
                WriteDelta(tick);

                m_bytes.insert(m_bytes.end(), completeEvent.begin(), completeEvent.end());
                m_runningStatus = 0;
            }

            void WriteMidi1Message(uint32_t tick, Midi1MessageClass messageClass, std::span<uint8_t const> bytes)
            {
                if (bytes.empty())
                {
                    return;
                }

                switch (messageClass)
                {
                case Midi1MessageClass::ChannelVoice:
                {
                    auto const status = bytes[0];

                    if (status < 0x80 || status >= 0xF0)
                    {
                        return;
                    }

                    WriteDelta(tick);

                    if (m_useRunningStatus && status == m_runningStatus)
                    {
                        m_bytes.insert(m_bytes.end(), bytes.begin() + 1, bytes.end());
                    }
                    else
                    {
                        m_bytes.insert(m_bytes.end(), bytes.begin(), bytes.end());
                        m_runningStatus = status;
                    }

                    break;
                }

                case Midi1MessageClass::SystemExclusive:
                {
                    // Stored complete, with both the leading F0 and the closing F7. The format
                    // writes the F0 as the event status and everything after it as the payload.
                    WriteDelta(tick);

                    m_bytes.push_back(SystemExclusiveStatus);
                    AppendVariableLength(m_bytes, static_cast<uint32_t>(bytes.size() - 1));
                    m_bytes.insert(m_bytes.end(), bytes.begin() + 1, bytes.end());
                    m_runningStatus = 0;

                    break;
                }

                case Midi1MessageClass::SystemEscape:
                {
                    WriteDelta(tick);

                    m_bytes.push_back(SystemExclusiveEndStatus);
                    AppendVariableLength(m_bytes, static_cast<uint32_t>(bytes.size()));
                    m_bytes.insert(m_bytes.end(), bytes.begin(), bytes.end());
                    m_runningStatus = 0;

                    break;
                }
                }
            }

            uint32_t LastTick() const noexcept { return m_previousTick; }

        private:
            void WriteDelta(uint32_t tick)
            {
                auto const delta = tick > m_previousTick ? tick - m_previousTick : 0u;

                AppendVariableLength(m_bytes, delta);

                m_previousTick = tick > m_previousTick ? tick : m_previousTick;
            }

            std::vector<uint8_t>& m_bytes;
            bool m_useRunningStatus{ false };
            uint8_t m_runningStatus{ 0 };
            uint32_t m_previousTick{ 0 };
        };

        Midi1MessageClass ClassForEventKind(EventKind kind) noexcept
        {
            switch (kind)
            {
            case EventKind::SystemExclusive: return Midi1MessageClass::SystemExclusive;
            case EventKind::SystemCommon:
            case EventKind::SystemRealTime:  return Midi1MessageClass::SystemEscape;
            default:                         return Midi1MessageClass::ChannelVoice;
            }
        }

        struct WriteContext
        {
            MidiSequence const& Sequence;
            WriteOptions const& Options;

            bool SingleTrack{ false };
            uint16_t OutputTrackCount{ 1 };
            uint16_t Division{ DefaultTicksPerQuarterNote };

            // An absolutely timed sequence counts microseconds, and the format has no way of
            // saying so, so it is laid out on a musical timeline at a fixed tempo instead.
            bool RemapTicks{ false };
            double TickScale{ 1.0 };

            uint32_t MapTick(uint32_t tick) const noexcept
            {
                if (!RemapTicks)
                {
                    return tick;
                }

                auto const mapped = static_cast<double>(tick) * TickScale;

                return mapped >= 4294967040.0 ? 0xFFFFFF00u : static_cast<uint32_t>(mapped + 0.5);
            }

            uint16_t OutputTrackFor(uint16_t trackIndex) const noexcept
            {
                if (SingleTrack || trackIndex == TrackNone || trackIndex >= OutputTrackCount)
                {
                    return 0;
                }

                return trackIndex;
            }
        };

        void CollectMetaEvents(
            WriteContext const& context,
            uint16_t outputTrackIndex,
            std::vector<PendingMeta>& metas)
        {
            auto const& sequence = context.Sequence;

            // The conductor data belongs on the first track of a format 1 file, and there is
            // nowhere else to put it in a format 0 one.
            if (outputTrackIndex == 0)
            {
                AppendMetaText(metas, 0, OrderTrackHeading, MetaTextCopyright, sequence.Copyright);

                if (context.RemapTicks)
                {
                    std::array<uint8_t, 3> tempo{
                        static_cast<uint8_t>((DefaultTempoMicrosecondsPerQuarterNote >> 16) & 0xFF),
                        static_cast<uint8_t>((DefaultTempoMicrosecondsPerQuarterNote >> 8) & 0xFF),
                        static_cast<uint8_t>(DefaultTempoMicrosecondsPerQuarterNote & 0xFF) };

                    AppendMetaEvent(metas, 0, OrderTempo, MetaSetTempo, std::span<uint8_t const>{ tempo });
                }
                else
                {
                    for (auto const& change : sequence.TempoMap)
                    {
                        std::array<uint8_t, 3> tempo{
                            static_cast<uint8_t>((change.MicrosecondsPerQuarterNote >> 16) & 0xFF),
                            static_cast<uint8_t>((change.MicrosecondsPerQuarterNote >> 8) & 0xFF),
                            static_cast<uint8_t>(change.MicrosecondsPerQuarterNote & 0xFF) };

                        AppendMetaEvent(
                            metas, context.MapTick(change.Tick), OrderTempo, MetaSetTempo,
                            std::span<uint8_t const>{ tempo });
                    }
                }

                for (auto const& change : sequence.TimeSignatureMap)
                {
                    std::array<uint8_t, 4> signature{
                        change.Numerator,
                        change.DenominatorPowerOfTwo,
                        change.ClocksPerClick == 0 ? uint8_t{ 24 } : change.ClocksPerClick,
                        change.ThirtySecondNotesPerQuarter == 0 ? uint8_t{ 8 } : change.ThirtySecondNotesPerQuarter };

                    AppendMetaEvent(
                        metas, context.MapTick(change.Tick), OrderTimeSignature, MetaTimeSignature,
                        std::span<uint8_t const>{ signature });
                }

                for (auto const& change : sequence.KeySignatureMap)
                {
                    std::array<uint8_t, 2> key{
                        static_cast<uint8_t>(change.Accidentals),
                        change.IsMinor ? uint8_t{ 1 } : uint8_t{ 0 } };

                    AppendMetaEvent(
                        metas, context.MapTick(change.Tick), OrderKeySignature, MetaKeySignature,
                        std::span<uint8_t const>{ key });
                }
            }

            // The reader puts a track name meta into both the track and the text events, so
            // writing both would duplicate it. The track's own copy is only needed when nothing
            // in the text events will supply it, which is the case for a sequence built in memory.
            bool trackNameWritten = false;

            for (auto const& text : sequence.TextEvents)
            {
                if (context.OutputTrackFor(text.TrackIndex) != outputTrackIndex)
                {
                    continue;
                }

                if (text.Kind == TextKind::ChordSymbol)
                {
                    // Carried in a manufacturer system exclusive, which is still in the events.
                    continue;
                }

                auto const metaType = static_cast<uint8_t>(text.Kind);

                if (metaType < 0x01 || metaType > 0x0F)
                {
                    continue;
                }

                if (text.Kind == TextKind::TrackName)
                {
                    trackNameWritten = true;
                }

                AppendMetaText(metas, context.MapTick(text.Tick), OrderText, metaType, text.Text);
            }

            auto const trackCount = sequence.Tracks.size() > 0xFFFFu ? size_t{ 0xFFFFu } : sequence.Tracks.size();

            for (uint16_t index = 0; index < trackCount; ++index)
            {
                if (context.OutputTrackFor(index) != outputTrackIndex)
                {
                    continue;
                }

                auto const& track = sequence.Tracks[index];

                if (!trackNameWritten)
                {
                    AppendMetaText(metas, 0, OrderTrackHeading, MetaTextTrackName, track.Name);
                    trackNameWritten = !track.Name.empty();
                }

                AppendMetaText(metas, 0, OrderTrackHeading, MetaTextInstrumentName, track.InstrumentName);
                AppendMetaText(metas, 0, OrderTrackHeading, MetaTextDeviceName, track.DeviceName);

                // Merging into one track keeps the first track's headings. The rest have nowhere
                // to go, and repeating them would leave several names on one track.
                break;
            }

            std::stable_sort(
                metas.begin(),
                metas.end(),
                [](PendingMeta const& left, PendingMeta const& right) noexcept
                {
                    return left.Tick != right.Tick ? left.Tick < right.Tick : left.Order < right.Order;
                });
        }

        void WriteTrackBody(
            WriteContext const& context,
            uint16_t outputTrackIndex,
            std::vector<uint32_t> const& eventIndexes,
            std::vector<uint8_t>& body,
            uint32_t& skippedEventCount)
        {
            auto const& sequence = context.Sequence;

            std::vector<PendingMeta> metas{};

            CollectMetaEvents(context, outputTrackIndex, metas);

            TrackBodyWriter writer{ body, context.Options.UseRunningStatus };

            std::array<std::vector<uint8_t>, 16> pendingSystemExclusive{};

            size_t metaIndex = 0;

            for (auto const eventIndex : eventIndexes)
            {
                auto const& event = sequence.Events[eventIndex];
                auto const tick = context.MapTick(event.Tick);

                while (metaIndex < metas.size() && metas[metaIndex].Tick <= tick)
                {
                    writer.WriteMeta(metas[metaIndex].Tick, std::span<uint8_t const>{ metas[metaIndex].Bytes });
                    ++metaIndex;
                }

                auto const bytes = sequence.BytesOf(event);

                if (bytes.empty())
                {
                    continue;
                }

                if (event.Kind != EventKind::UniversalPacket)
                {
                    writer.WriteMidi1Message(tick, ClassForEventKind(event.Kind), bytes);
                    continue;
                }

                // Words are packed into the shared blob end to end, so one can land on any byte
                // offset. They are copied out rather than read in place.
                auto const wordCount = static_cast<size_t>(bytes.size() / sizeof(uint32_t));

                if (wordCount == 0)
                {
                    ++skippedEventCount;
                    continue;
                }

                std::array<uint32_t, 4> words{};
                auto const usable = wordCount > words.size() ? words.size() : wordCount;

                std::memcpy(words.data(), bytes.data(), usable * sizeof(uint32_t));

                auto const expected = UmpWordCounts[(words[0] >> 28) & 0x0F];

                if (usable < expected)
                {
                    ++skippedEventCount;
                    continue;
                }

                auto const translated = TranslateUniversalPacket(
                    std::span<uint32_t const>{ words.data(), expected },
                    pendingSystemExclusive,
                    [&writer, tick](Midi1MessageClass messageClass, std::span<uint8_t const> message)
                    {
                        writer.WriteMidi1Message(tick, messageClass, message);
                    });

                if (!translated)
                {
                    ++skippedEventCount;
                }
            }

            while (metaIndex < metas.size())
            {
                writer.WriteMeta(metas[metaIndex].Tick, std::span<uint8_t const>{ metas[metaIndex].Bytes });
                ++metaIndex;
            }

            std::array<uint8_t, 3> endOfTrack{ MetaEventStatus, MetaEndOfTrack, 0x00 };

            AppendVariableLength(body, 0);
            body.insert(body.end(), endOfTrack.begin(), endOfTrack.end());
        }

        WriteResult BuildFile(
            MidiSequence const& sequence,
            WriteOptions const& options,
            std::vector<uint8_t>& fileBytes)
        {
            WriteResult result{};

            WriteContext context{ sequence, options };

            auto const declaredTrackCount = sequence.Tracks.size();

            context.SingleTrack = options.SingleTrack || declaredTrackCount <= 1;
            context.OutputTrackCount = context.SingleTrack
                ? uint16_t{ 1 }
                : static_cast<uint16_t>(declaredTrackCount > 0xFFFE ? 0xFFFE : declaredTrackCount);

            if (sequence.Timing == TimingMode::Absolute)
            {
                context.RemapTicks = true;

                auto ticksPerQuarterNote = options.AbsoluteTimingTicksPerQuarterNote;

                if (ticksPerQuarterNote == 0 || ticksPerQuarterNote > 0x7FFF)
                {
                    ticksPerQuarterNote = DefaultTicksPerQuarterNote;
                }

                context.Division = ticksPerQuarterNote;
                context.TickScale =
                    static_cast<double>(ticksPerQuarterNote) / static_cast<double>(DefaultTempoMicrosecondsPerQuarterNote);
            }
            else if (sequence.Division.IsSmpte)
            {
                auto const frames = sequence.Division.FramesPerSecond == 0
                    ? uint8_t{ 25 }
                    : sequence.Division.FramesPerSecond;

                auto const ticksPerFrame = sequence.Division.TicksPerFrame == 0
                    ? uint8_t{ 40 }
                    : sequence.Division.TicksPerFrame;

                context.Division = static_cast<uint16_t>(
                    (static_cast<uint16_t>(static_cast<uint8_t>(0x100 - frames)) << 8) | ticksPerFrame);
            }
            else
            {
                auto ticksPerQuarterNote = sequence.Division.TicksPerQuarterNote;

                if (ticksPerQuarterNote == 0 || ticksPerQuarterNote > 0x7FFF)
                {
                    ticksPerQuarterNote = DefaultTicksPerQuarterNote;
                }

                context.Division = ticksPerQuarterNote;
            }

            // One pass to work out which events belong to which output track, so that a file with
            // many tracks does not walk the whole event list once per track.
            std::vector<std::vector<uint32_t>> eventIndexes{};

            eventIndexes.resize(context.OutputTrackCount);

            for (uint32_t index = 0; index < sequence.Events.size(); ++index)
            {
                eventIndexes[context.OutputTrackFor(sequence.Events[index].TrackIndex)].push_back(index);
            }

            fileBytes.clear();

            AppendChunkId(fileBytes, "MThd");
            AppendUInt32(fileBytes, 6);
            AppendUInt16(fileBytes, context.OutputTrackCount <= 1 ? uint16_t{ 0 } : uint16_t{ 1 });
            AppendUInt16(fileBytes, context.OutputTrackCount);
            AppendUInt16(fileBytes, context.Division);

            std::vector<uint8_t> body{};

            for (uint16_t trackIndex = 0; trackIndex < context.OutputTrackCount; ++trackIndex)
            {
                body.clear();

                WriteTrackBody(context, trackIndex, eventIndexes[trackIndex], body, result.SkippedEventCount);

                if (fileBytes.size() + body.size() + 8 > options.MaximumFileBytes)
                {
                    result.Status = WriteStatus::TooMuchData;
                    return result;
                }

                AppendChunkId(fileBytes, "MTrk");
                AppendUInt32(fileBytes, static_cast<uint32_t>(body.size()));

                fileBytes.insert(fileBytes.end(), body.begin(), body.end());
            }

            result.TrackCount = context.OutputTrackCount;
            result.ByteCount = fileBytes.size();

            return result;
        }
    }

    _Use_decl_annotations_
    WriteResult WriteStandardMidiFile(
        MidiSequence const& sequence,
        std::vector<uint8_t>& fileBytes,
        WriteOptions const& options) noexcept
    {
        WriteResult result{};

        fileBytes.clear();

        if (sequence.Events.empty() && sequence.TextEvents.empty() && sequence.Tracks.empty())
        {
            result.Status = WriteStatus::NothingToWrite;
            return result;
        }

        try
        {
            return BuildFile(sequence, options, fileBytes);
        }
        catch (std::bad_alloc const&)
        {
            fileBytes.clear();

            result.Status = WriteStatus::OutOfMemory;
            return result;
        }
        catch (...)
        {
            fileBytes.clear();

            result.Status = WriteStatus::WriteFailed;
            return result;
        }
    }

    _Use_decl_annotations_
    WriteResult WriteStandardMidiFile(
        std::wstring const& path,
        MidiSequence const& sequence,
        WriteOptions const& options) noexcept
    {
        std::vector<uint8_t> fileBytes{};

        auto result = WriteStandardMidiFile(sequence, fileBytes, options);

        if (!result.Succeeded())
        {
            return result;
        }

        HANDLE const file = ::CreateFileW(
            path.c_str(),
            GENERIC_WRITE,
            FILE_SHARE_READ,
            nullptr,
            CREATE_ALWAYS,
            FILE_ATTRIBUTE_NORMAL,
            nullptr);

        if (file == INVALID_HANDLE_VALUE)
        {
            auto const error = ::GetLastError();

            result.Status = error == ERROR_ACCESS_DENIED ? WriteStatus::AccessDenied : WriteStatus::WriteFailed;
            result.ByteCount = 0;

            return result;
        }

        size_t totalWritten = 0;

        while (totalWritten < fileBytes.size())
        {
            auto const remaining = fileBytes.size() - totalWritten;
            auto const chunk = remaining > 0x10000000u ? 0x10000000u : static_cast<DWORD>(remaining);

            DWORD written = 0;

            if (!::WriteFile(file, fileBytes.data() + totalWritten, chunk, &written, nullptr) || written == 0)
            {
                break;
            }

            totalWritten += written;
        }

        auto const flushed = ::FlushFileBuffers(file) != FALSE;

        ::CloseHandle(file);

        if (totalWritten != fileBytes.size() || !flushed)
        {
            result.Status = WriteStatus::WriteFailed;
            result.ByteCount = totalWritten;
        }

        return result;
    }
}
