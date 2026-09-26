// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

#include "pch.h"

#include "console_output.h"
#include "message_capture_writer.h"
#include "midi_formatting.h"
#include "strings.h"

#include "midi_file_smf_writer.h"

namespace midi2console
{
    namespace
    {
        // Ticks per quarter note for the Standard MIDI File, at a fixed 120 beats per minute, so
        // one tick is a little over half a millisecond. This is the division nearly every
        // sequencer uses natively. A capture being studied for timing wants the comma separated
        // format instead, which carries the real microsecond figures.
        constexpr uint16_t SmfTicksPerQuarterNote = 960;
        constexpr uint32_t SmfMicrosecondsPerQuarterNote = 500000;      // 120 beats per minute

        std::string ResolveDelimiter(_In_ std::string const& delimiterName)
        {
            if (EqualsIgnoreCase(delimiterName, "comma")) return ",";
            if (EqualsIgnoreCase(delimiterName, "tab"))   return "\t";
            if (EqualsIgnoreCase(delimiterName, "pipe"))  return "|";

            return " ";
        }

        MessageCaptureFormat ResolveFormat(_In_ std::string const& formatName)
        {
            if (EqualsIgnoreCase(formatName, "csv")) return MessageCaptureFormat::CommaSeparated;
            if (EqualsIgnoreCase(formatName, "smf")) return MessageCaptureFormat::StandardMidiFile;
            if (EqualsIgnoreCase(formatName, "mid")) return MessageCaptureFormat::StandardMidiFile;

            return MessageCaptureFormat::UniversalPackets;
        }

        std::string LocalTimeOfDay()
        {
            SYSTEMTIME now{};
            GetLocalTime(&now);

            return fmt::format("{:02}:{:02}:{:02}.{:03}",
                now.wHour, now.wMinute, now.wSecond, now.wMilliseconds);
        }

        // The shipping console writes the enum name here, not the friendly name, so a capture
        // stays greppable by message type.
        std::string MessageTypeName(_In_ uint32_t word0)
        {
            switch (midi2msg::MidiMessageHelper::GetMessageTypeFromMessageFirstWord(word0))
            {
            case midi2::MidiMessageType::UtilityMessage32:      return "UtilityMessage32";
            case midi2::MidiMessageType::SystemCommon32:        return "SystemCommon32";
            case midi2::MidiMessageType::Midi1ChannelVoice32:   return "Midi1ChannelVoice32";
            case midi2::MidiMessageType::DataMessage64:         return "DataMessage64";
            case midi2::MidiMessageType::Midi2ChannelVoice64:   return "Midi2ChannelVoice64";
            case midi2::MidiMessageType::DataMessage128:        return "DataMessage128";
            case midi2::MidiMessageType::FlexData128:           return "FlexData128";
            case midi2::MidiMessageType::Stream128:             return "Stream128";
            default:                                            return "Reserved";
            }
        }

        double TimestampFrequency() noexcept
        {
            static double const frequency = []() noexcept -> double
                {
                    try
                    {
                        auto const ticksPerSecond = static_cast<double>(midi2::MidiClock::TimestampFrequency());

                        if (ticksPerSecond > 0.0)
                        {
                            return ticksPerSecond;
                        }
                    }
                    catch (...)
                    {
                    }

                    LARGE_INTEGER performanceFrequency{};

                    if (::QueryPerformanceFrequency(&performanceFrequency) && performanceFrequency.QuadPart > 0)
                    {
                        return static_cast<double>(performanceFrequency.QuadPart);
                    }

                    return 10000000.0;
                }();

            return frequency;
        }

        double TicksToMicroseconds(_In_ uint64_t ticks) noexcept
        {
            return (static_cast<double>(ticks) / TimestampFrequency()) * 1000000.0;
        }

        // RFC 4180: a field is quoted when it holds a comma, a quote or a line break, and an
        // embedded quote is doubled.
        std::string CsvField(_In_ std::string const& value)
        {
            if (value.find_first_of(",\"\r\n") == std::string::npos)
            {
                return value;
            }

            std::string quoted{ "\"" };

            for (auto const character : value)
            {
                if (character == '"')
                {
                    quoted.push_back('"');
                }

                quoted.push_back(character);
            }

            quoted.push_back('"');

            return quoted;
        }
    }

    MessageCaptureWriter::~MessageCaptureWriter()
    {
        if (m_open)
        {
            Close();
        }
    }

    _Use_decl_annotations_
    bool MessageCaptureWriter::IsValidDelimiterName(std::string const& delimiterName)
    {
        return EqualsIgnoreCase(delimiterName, "space") ||
            EqualsIgnoreCase(delimiterName, "comma") ||
            EqualsIgnoreCase(delimiterName, "tab") ||
            EqualsIgnoreCase(delimiterName, "pipe");
    }

    _Use_decl_annotations_
    bool MessageCaptureWriter::IsValidFormatName(std::string const& formatName)
    {
        return EqualsIgnoreCase(formatName, "ump") ||
            EqualsIgnoreCase(formatName, "csv") ||
            EqualsIgnoreCase(formatName, "smf") ||
            EqualsIgnoreCase(formatName, "mid");
    }

    _Use_decl_annotations_
    bool MessageCaptureWriter::Open(
        std::string const& fileName,
        std::string const& formatName,
        std::string const& delimiterName,
        bool annotate)
    {
        m_format = ResolveFormat(formatName);
        m_delimiter = ResolveDelimiter(delimiterName);
        m_annotate = annotate;

        auto const wideName = FromUtf8(fileName);

        std::wstring expanded;
        expanded.resize(MAX_PATH * 4);

        auto const expandedLength = ExpandEnvironmentStringsW(
            wideName.c_str(), expanded.data(), static_cast<DWORD>(expanded.size()));

        if (expandedLength > 0 && expandedLength <= expanded.size())
        {
            expanded.resize(expandedLength - 1);
        }
        else
        {
            expanded = wideName;
        }

        m_widePath = expanded;
        m_fileName = ToUtf8(expanded);

        if (m_format == MessageCaptureFormat::StandardMidiFile)
        {
            // Nothing is opened yet. The whole file is built at Close, because each track
            // declares its length in front of itself.
            m_open = true;
            return true;
        }

        // Appending matches the shipping console, so repeated runs build one capture.
        m_file.open(expanded, std::ios::out | std::ios::app);

        m_open = m_file.is_open();

        if (m_open && m_format == MessageCaptureFormat::CommaSeparated && m_file.tellp() == std::streampos{ 0 })
        {
            // Deliberately NOT localized. A script reading this file finds its columns by name,
            // and a column called "Gruppe" on a German machine would break it.
            m_file << "Index,TimestampTicks,OffsetMicroseconds,DeltaMicroseconds,Group,Channel,"
                      "WordCount,Word0,Word1,Word2,Word3,MessageType,MessageName\n";
        }

        return m_open;
    }

    _Use_decl_annotations_
    void MessageCaptureWriter::Write(uint64_t timestamp, uint32_t wordCount, uint32_t const* words)
    {
        if (!m_open || wordCount == 0)
        {
            return;
        }

        switch (m_format)
        {
        case MessageCaptureFormat::CommaSeparated:
            WriteCommaSeparatedLine(timestamp, wordCount, words);
            break;

        case MessageCaptureFormat::StandardMidiFile:
        {
            if (m_held.size() >= MaximumHeldMessages)
            {
                m_messagesSkipped++;
                return;
            }

            HeldMessage held{};

            held.Timestamp = timestamp;
            held.WordCount = static_cast<uint8_t>(wordCount > 4 ? 4 : wordCount);

            for (uint32_t index = 0; index < held.WordCount; index++)
            {
                held.Words[index] = words[index];
            }

            m_held.push_back(held);
            break;
        }

        default:
            WriteUniversalPacketLine(timestamp, wordCount, words);
            break;
        }

        m_messagesWritten++;
    }

    _Use_decl_annotations_
    void MessageCaptureWriter::WriteUniversalPacketLine(
        uint64_t timestamp,
        uint32_t wordCount,
        uint32_t const* words)
    {
        if (m_annotate)
        {
            // # ___time___ ___timestamp___ ___message type___ ___friendly name___
            m_file << "# " << LocalTimeOfDay() << m_delimiter
                << timestamp << m_delimiter
                << MessageTypeName(words[0]) << m_delimiter
                << ToUtf8(midi2msg::MidiMessageHelper::GetMessageDisplayNameFromFirstWord(words[0]))
                << "\n";
        }

        for (uint32_t index = 0; index < wordCount; index++)
        {
            if (index > 0)
            {
                m_file << m_delimiter;
            }

            m_file << fmt::format("0x{:08X}", words[index]);
        }

        m_file << "\n";
    }

    _Use_decl_annotations_
    void MessageCaptureWriter::WriteCommaSeparatedLine(
        uint64_t timestamp,
        uint32_t wordCount,
        uint32_t const* words)
    {
        if (!m_haveOrigin)
        {
            m_originTimestamp = timestamp;
            m_previousTimestamp = timestamp;
            m_haveOrigin = true;
        }

        auto const offsetTicks = timestamp > m_originTimestamp ? timestamp - m_originTimestamp : 0ull;
        auto const deltaTicks = timestamp > m_previousTimestamp ? timestamp - m_previousTimestamp : 0ull;

        m_previousTimestamp = timestamp;

        auto const messageType = midi2msg::MidiMessageHelper::GetMessageTypeFromMessageFirstWord(words[0]);

        std::string group{};
        std::string channel{};

        if (midi2msg::MidiMessageHelper::MessageTypeHasGroupField(messageType))
        {
            group = fmt::format("{}", midi2msg::MidiMessageHelper::GetGroupFromMessageFirstWord(words[0]).DisplayValue());
        }

        if (midi2msg::MidiMessageHelper::MessageTypeHasChannelField(messageType))
        {
            channel = fmt::format("{}", midi2msg::MidiMessageHelper::GetChannelFromMessageFirstWord(words[0]).DisplayValue());
        }

        m_file << m_messagesWritten + 1 << ','
            << timestamp << ','
            << fmt::format("{:.3f}", TicksToMicroseconds(offsetTicks)) << ','
            << fmt::format("{:.3f}", TicksToMicroseconds(deltaTicks)) << ','
            << group << ','
            << channel << ','
            << wordCount << ',';

        for (uint32_t index = 0; index < 4; index++)
        {
            if (index < wordCount)
            {
                m_file << fmt::format("0x{:08X}", words[index]);
            }

            m_file << ',';
        }

        m_file << MessageTypeName(words[0]) << ','
            << CsvField(ToUtf8(midi2msg::MidiMessageHelper::GetMessageDisplayNameFromFirstWord(words[0])))
            << "\n";
    }

    bool MessageCaptureWriter::WriteStandardMidiFile()
    {
        if (m_held.empty())
        {
            return false;
        }

        ::midifile::MidiSequence sequence{};

        sequence.Format = ::midifile::SequenceFormat::MultiTrack;
        sequence.Division.IsSmpte = false;
        sequence.Division.TicksPerQuarterNote = SmfTicksPerQuarterNote;
        sequence.Timing = ::midifile::TimingMode::Musical;

        ::midifile::Track track{};

        track.Name = "MIDI capture";

        sequence.Tracks.push_back(track);

        sequence.TempoMap.push_back(
            ::midifile::TempoChange{ 0, SmfMicrosecondsPerQuarterNote, 0 });

        auto const origin = m_held.front().Timestamp;
        uint32_t previousTick = 0;

        for (auto const& held : m_held)
        {
            auto const elapsed = held.Timestamp > origin ? held.Timestamp - origin : 0ull;

            auto const scaled =
                (TicksToMicroseconds(elapsed) * SmfTicksPerQuarterNote) / SmfMicrosecondsPerQuarterNote;

            auto tick = scaled >= 4294967040.0 ? 0xFFFFFF00u : static_cast<uint32_t>(scaled + 0.5);

            // Arrival order is the truth here, so a tick may never move backwards even if a
            // timestamp does.
            if (tick < previousTick)
            {
                tick = previousTick;
            }

            previousTick = tick;

            ::midifile::SequenceEvent event{};

            event.Tick = tick;
            event.TrackIndex = 0;
            event.Kind = ::midifile::EventKind::UniversalPacket;
            event.Channel = ::midifile::ChannelNone;
            event.ByteOffset = static_cast<uint32_t>(sequence.EventBytes.size());
            event.ByteCount = static_cast<uint32_t>(held.WordCount * sizeof(uint32_t));

            auto const* const bytes = reinterpret_cast<uint8_t const*>(held.Words.data());

            sequence.EventBytes.insert(sequence.EventBytes.end(), bytes, bytes + event.ByteCount);
            sequence.Events.push_back(event);
        }

        sequence.Finalize();

        ::midifile::WriteOptions options{};

        options.SingleTrack = true;

        auto const written = ::midifile::WriteStandardMidiFile(m_widePath, sequence, options);

        m_messagesSkipped += written.SkippedEventCount;

        return written.Succeeded();
    }

    void MessageCaptureWriter::Flush()
    {
        if (m_file.is_open())
        {
            m_file.flush();
        }
    }

    bool MessageCaptureWriter::Close()
    {
        if (!m_open)
        {
            return false;
        }

        m_open = false;

        if (m_format == MessageCaptureFormat::StandardMidiFile)
        {
            auto const written = WriteStandardMidiFile();

            m_held.clear();
            m_held.shrink_to_fit();

            return written;
        }

        m_file.flush();

        auto const good = m_file.good();

        m_file.close();

        return good;
    }
}
