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
    struct ReceivedMidiMessage
    {
        uint32_t Index{ 0 };
        uint64_t ReceivedTimestamp{ 0 };
        uint64_t ReceivedOffsetFromLastMessage{ 0 };
        uint64_t MessageTimestamp{ 0 };
        uint8_t NumWords{ 0 };
        uint32_t Word0{ 0 };
        uint32_t Word1{ 0 };
        uint32_t Word2{ 0 };
        uint32_t Word3{ 0 };
        bool HasError{ false };
    };

    // Hand-rolled rather than a general table library, for the same reason the C# console has its
    // own: a general table cannot keep up with endpoint monitoring throughput.
    class MidiMessageTable
    {
    public:
        MidiMessageTable(
            _In_ bool expectMidi2Data,
            _In_ bool includeTimestamps,
            _In_ bool decodeMessages,
            _In_ bool verbose);

        void OutputHeader();
        void OutputSeparatorLine();
        void OutputRow(_In_ ReceivedMidiMessage const& message);

        // A full-width annotation row, used for the in-monitor comments.
        void OutputComment(_In_ std::string_view comment);

        size_t TotalWidth() const noexcept { return m_totalWidth; }

    private:
        static constexpr int ParameterIndexColumn = 0;
        static constexpr int ParameterMessageTimestamp = 1;
        static constexpr int ParameterMessageTimestampDelta = 2;
        static constexpr int ParameterMessageTimestampDeltaUnits = 3;
        static constexpr int ParameterReceivedTimestamp = 4;
        static constexpr int ParameterReceivedTimestampDelta = 5;
        static constexpr int ParameterReceivedTimestampDeltaUnits = 6;
        static constexpr int ParameterDataWord0 = 7;
        static constexpr int ParameterDataWord1 = 8;
        static constexpr int ParameterDataWord2 = 9;
        static constexpr int ParameterDataWord3 = 10;
        static constexpr int ParameterDecodedMessageType = 11;
        static constexpr int ParameterDecodedGroup = 12;
        static constexpr int ParameterDecodedChannel = 13;
        static constexpr int ParameterDecodedData = 14;

        static constexpr int TimestampOffsetValueColumnWidth = 9;
        static constexpr int DetailedMessageTypeTextWidth = 35;
        static constexpr int DecodedDataTextWidth = 35;

        struct Column
        {
            int ParameterIndex{ 0 };
            std::string HeaderText;
            int Width{ 0 };                 // negative means left-aligned
            bool NoLeftSeparator{ false };
            fmt::text_style DataStyle;
        };

        // Cells carry both a plain form (for width) and a rendered form (which may hold escape
        // sequences), because escape sequences have length but no display width.
        struct Cell
        {
            std::string Plain;
            std::string Rendered;
            bool HasOwnStyling{ false };
        };

        void AddColumn(
            _In_ int parameterIndex,
            _In_ std::string headerText,
            _In_ int width,
            _In_ bool noLeftSeparator,
            _In_ fmt::text_style const& style);

        void BuildHeaderAndSeparator();

        Cell BuildDecodedDataCell(_In_ ReceivedMidiMessage const& message) const;
        Cell BuildDecodedMidi1ChannelVoiceCell(_In_ ReceivedMidiMessage const& message) const;
        Cell BuildDecodedMidi2ChannelVoiceCell(_In_ ReceivedMidiMessage const& message) const;

        std::vector<Column> m_columns;
        std::string m_headerLine;
        std::string m_separatorLine;
        size_t m_totalWidth{ 0 };

        bool m_expectMidi2Data{ true };
        bool m_verbose{ false };
        bool m_includeTimestamps{ false };
        bool m_decodeMessages{ false };
    };
}
