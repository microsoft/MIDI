// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

#include "midi_file_sequence.h"

namespace midi2console
{
    enum class MessageCaptureFormat
    {
        // The shipping console format: one line of Universal MIDI Packet words per message, with
        // optional annotation lines in front of them. 'endpoint send-message-file' replays it.
        UniversalPackets = 0,

        // One row per message under a fixed English heading, for a script or a spreadsheet.
        CommaSeparated = 1,

        // A Standard MIDI File, for a sequencer. MIDI 1.0 only, so anything MIDI 1.0 cannot
        // express is counted and left out.
        StandardMidiFile = 2
    };

    // Writes the same capture format the shipping console produces, so files stay interchangeable
    // and can be replayed with 'endpoint send-message-file', plus two formats for taking a
    // capture somewhere else.
    class MessageCaptureWriter
    {
    public:
        ~MessageCaptureWriter();

        bool Open(
            _In_ std::string const& fileName,
            _In_ std::string const& formatName,
            _In_ std::string const& delimiterName,
            _In_ bool annotate);

        void Write(
            _In_ uint64_t timestamp,
            _In_ uint32_t wordCount,
            _In_reads_(wordCount) uint32_t const* words);

        void Flush();

        // Finishes the file and reports whether it was written. A Standard MIDI File declares
        // each track's length in front of the track, so it cannot be written a message at a time
        // and is produced here instead.
        bool Close();

        bool IsOpen() const noexcept { return m_open; }
        uint64_t MessagesWritten() const noexcept { return m_messagesWritten; }
        uint64_t MessagesSkipped() const noexcept { return m_messagesSkipped; }
        std::string const& ResolvedFileName() const noexcept { return m_fileName; }
        MessageCaptureFormat Format() const noexcept { return m_format; }

        static bool IsValidDelimiterName(_In_ std::string const& delimiterName);
        static bool IsValidFormatName(_In_ std::string const& formatName);

    private:
        struct HeldMessage
        {
            uint64_t Timestamp{ 0 };
            uint8_t WordCount{ 0 };
            std::array<uint32_t, 4> Words{};
        };

        // A held capture costs memory, so it has a ceiling. Generous: a busy endpoint would take
        // most of an hour to reach it.
        static constexpr size_t MaximumHeldMessages = 1000000;

        void WriteUniversalPacketLine(
            _In_ uint64_t timestamp,
            _In_ uint32_t wordCount,
            _In_reads_(wordCount) uint32_t const* words);

        void WriteCommaSeparatedLine(
            _In_ uint64_t timestamp,
            _In_ uint32_t wordCount,
            _In_reads_(wordCount) uint32_t const* words);

        bool WriteStandardMidiFile();

        std::ofstream m_file;
        std::wstring m_widePath;
        std::string m_fileName;
        std::string m_delimiter{ " " };
        MessageCaptureFormat m_format{ MessageCaptureFormat::UniversalPackets };
        bool m_annotate{ false };
        bool m_open{ false };
        uint64_t m_messagesWritten{ 0 };
        uint64_t m_messagesSkipped{ 0 };
        uint64_t m_originTimestamp{ 0 };
        uint64_t m_previousTimestamp{ 0 };
        bool m_haveOrigin{ false };

        std::vector<HeldMessage> m_held{};
    };
}
