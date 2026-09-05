// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

namespace midi2console
{
    // Writes the same capture format the shipping console produces, so files stay interchangeable
    // and can be replayed with 'endpoint send-message-file'.
    class MessageCaptureWriter
    {
    public:
        bool Open(_In_ std::string const& fileName, _In_ std::string const& delimiterName, _In_ bool annotate);

        void Write(
            _In_ uint64_t timestamp,
            _In_ uint32_t wordCount,
            _In_reads_(wordCount) uint32_t const* words);

        void Flush();

        bool IsOpen() const noexcept { return m_file.is_open(); }
        uint64_t MessagesWritten() const noexcept { return m_messagesWritten; }
        std::string const& ResolvedFileName() const noexcept { return m_fileName; }

        static bool IsValidDelimiterName(_In_ std::string const& delimiterName);

    private:
        std::ofstream m_file;
        std::string m_fileName;
        std::string m_delimiter{ " " };
        bool m_annotate{ false };
        uint64_t m_messagesWritten{ 0 };
    };
}
