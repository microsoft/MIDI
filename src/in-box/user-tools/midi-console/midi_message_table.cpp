// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

#include "pch.h"

#include <map>

#include "console_output.h"
#include "endpoint_utility.h"
#include "midi_formatting.h"
#include "midi_message_table.h"
#include "strings.h"

namespace midi2console
{
    namespace
    {
        constexpr const char* VerticalLine = "\u2502";
        constexpr const char* HorizontalLine = "\u2500";
        constexpr const char* Cross = "\u253C";

        const auto headerTextStyle = tableHeaderTextStyle;

        const auto messageTimestampStyle = fmt::fg(fmt::color::dark_sea_green);
        const auto receivedTimestampStyle = fmt::fg(fmt::color::sky_blue);
        const auto offsetValueDefaultStyle = fmt::fg(fmt::color::dark_sea_green);
        const auto offsetValueErrorStyle = fmt::fg(fmt::color::red);
        const auto deltaValueDefaultStyle = fmt::fg(fmt::color::light_sky_blue);
        const auto deltaValueErrorStyle = fmt::fg(fmt::color::red);
        const auto unitsLabelStyle = fmt::fg(fmt::color::gray);
        const auto indexStyle = fmt::fg(fmt::color::gray);
        const auto groupStyle = groupTextStyle;
        const auto channelStyle = channelTextStyle;
        const auto messageTypeStyle = messageTypeTextStyle;
        const auto decodedLabelStyle = fmt::fg(fmt::color::gray);
        const auto decodedValueStyle = fmt::fg(fmt::color::dark_sea_green);

        const std::array<fmt::text_style, 4> dataWordStyles
        {
            fmt::fg(fmt::color::deep_sky_blue),
            fmt::fg(fmt::color::steel_blue),
            fmt::fg(fmt::color::cadet_blue),
            fmt::fg(fmt::color::slate_gray)
        };

        std::string FormatWithThousandsSeparators(_In_ uint64_t value)
        {
            auto digits = fmt::format("{}", value);

            std::string result;
            result.reserve(digits.size() + digits.size() / 3);

            auto const leading = digits.size() % 3;

            for (size_t i = 0; i < digits.size(); i++)
            {
                if (i > 0 && (i % 3) == leading)
                {
                    result.push_back(',');
                }

                result.push_back(digits[i]);
            }

            return result;
        }

        std::string AlignPlain(_In_ std::string_view value, _In_ int width)
        {
            auto const absoluteWidth = static_cast<size_t>(width < 0 ? -width : width);
            auto const currentWidth = DisplayWidth(value);

            if (currentWidth >= absoluteWidth)
            {
                return std::string{ value };
            }

            auto const padding = std::string(absoluteWidth - currentWidth, ' ');

            return width < 0 ? std::string{ value } + padding : padding + std::string{ value };
        }

        std::string FormatByteDecimal(_In_ uint8_t value)
        {
            return fmt::format("{:>3}", static_cast<int>(value));
        }
    }

    MidiMessageTable::MidiMessageTable(
        _In_ bool expectMidi2Data,
        _In_ bool includeTimestamps,
        _In_ bool decodeMessages,
        _In_ bool verbose) :
        m_expectMidi2Data(expectMidi2Data),
        m_verbose(verbose),
        m_includeTimestamps(includeTimestamps),
        m_decodeMessages(decodeMessages)
    {
        AddColumn(ParameterIndexColumn, ResourceString(IDS_EP_MONITOR_HEADER_INDEX), 8, true, indexStyle);

        if (verbose || includeTimestamps)
        {
            AddColumn(ParameterMessageTimestamp, ResourceString(IDS_EP_MONITOR_HEADER_MESSAGE_TIMESTAMP), 19, false, messageTimestampStyle);
            AddColumn(ParameterMessageTimestampDelta, ResourceString(IDS_EP_MONITOR_HEADER_FROM_LAST), TimestampOffsetValueColumnWidth, false, offsetValueDefaultStyle);
            AddColumn(ParameterMessageTimestampDeltaUnits, "", -2, true, unitsLabelStyle);
        }

        if (verbose)
        {
            AddColumn(ParameterReceivedTimestamp, ResourceString(IDS_EP_MONITOR_HEADER_RECEIVED_TIMESTAMP), 19, false, receivedTimestampStyle);
            AddColumn(ParameterReceivedTimestampDelta, ResourceString(IDS_EP_MONITOR_HEADER_RECEIVE_DELTA), TimestampOffsetValueColumnWidth, false, deltaValueDefaultStyle);
            AddColumn(ParameterReceivedTimestampDeltaUnits, "", -2, true, unitsLabelStyle);
        }

        AddColumn(ParameterDataWord0, ResourceString(IDS_EP_MONITOR_HEADER_DATA), 8, false, dataWordStyles[0]);
        AddColumn(ParameterDataWord1, "", 8, true, dataWordStyles[1]);

        if (m_expectMidi2Data)
        {
            AddColumn(ParameterDataWord2, "", 8, true, dataWordStyles[2]);
            AddColumn(ParameterDataWord3, "", 8, true, dataWordStyles[3]);
        }

        if (verbose || decodeMessages)
        {
            AddColumn(ParameterDecodedGroup, ResourceString(IDS_EP_MONITOR_HEADER_GROUP_SHORT), 2, false, groupStyle);
            AddColumn(ParameterDecodedChannel, ResourceString(IDS_EP_MONITOR_HEADER_CHANNEL_SHORT), 2, true, channelStyle);
            AddColumn(ParameterDecodedMessageType, ResourceString(IDS_EP_MONITOR_HEADER_MESSAGE_TYPE), -DetailedMessageTypeTextWidth, false, messageTypeStyle);
            AddColumn(ParameterDecodedData, ResourceString(IDS_EP_MONITOR_HEADER_DECODED_DATA), -DecodedDataTextWidth, false, decodedValueStyle);
        }

        BuildHeaderAndSeparator();
    }

    void MidiMessageTable::AddColumn(
        _In_ int parameterIndex,
        _In_ std::string headerText,
        _In_ int width,
        _In_ bool noLeftSeparator,
        _In_ fmt::text_style const& style)
    {
        m_columns.push_back(Column{ parameterIndex, std::move(headerText), width, noLeftSeparator, style });
    }

    void MidiMessageTable::BuildHeaderAndSeparator()
    {
        std::string headerPlain;

        bool first = true;

        for (auto const& column : m_columns)
        {
            auto const columnWidth = static_cast<size_t>(column.Width < 0 ? -column.Width : column.Width);

            if (!first && !column.NoLeftSeparator)
            {
                headerPlain += " ";
                m_headerLine += " ";
                m_separatorLine += HorizontalLine;

                headerPlain += VerticalLine;
                m_headerLine += fmt::format("{}", Styled(VerticalLine, separatorTextStyle));
                m_separatorLine += Cross;

                headerPlain += " ";
                m_headerLine += " ";
                m_separatorLine += HorizontalLine;
            }
            else if (column.NoLeftSeparator && !first)
            {
                headerPlain += " ";
                m_headerLine += " ";
                m_separatorLine += HorizontalLine;
            }

            auto const headerText = AlignPlain(column.HeaderText, -static_cast<int>(columnWidth));

            headerPlain += headerText;
            m_headerLine += fmt::format("{}", Styled(headerText, headerTextStyle));

            for (size_t i = 0; i < columnWidth; i++)
            {
                m_separatorLine += HorizontalLine;
            }

            first = false;
        }

        m_totalWidth = DisplayWidth(headerPlain);
        m_separatorLine = fmt::format("{}", Styled(m_separatorLine, separatorTextStyle));
    }

    void MidiMessageTable::OutputSeparatorLine()
    {
        WriteLine(m_separatorLine);
    }

    void MidiMessageTable::OutputHeader()
    {
        WriteLine(m_headerLine);
        OutputSeparatorLine();
    }

    void MidiMessageTable::OutputComment(_In_ std::string_view comment)
    {
        // Deliberately not padded to the table width: trailing spaces buy nothing without a
        // background color and force an extra wrapped line on a narrow console.
        WriteLine(fmt::format("{}", Styled(fmt::format("\u25B6 {}", comment), commentTextStyle)));
    }

    void MidiMessageTable::OutputRow(_In_ ReceivedMidiMessage const& message)
    {
        std::map<int, Cell> values;

        auto setPlain = [&values](int parameterIndex, std::string value)
        {
            values[parameterIndex] = Cell{ value, value, false };
        };

        setPlain(ParameterIndexColumn, fmt::format("{}", message.Index));

        setPlain(ParameterDataWord0, fmt::format("{:08X}", message.Word0));
        setPlain(ParameterDataWord1, message.NumWords >= 2 ? fmt::format("{:08X}", message.Word1) : std::string{});
        setPlain(ParameterDataWord2, message.NumWords >= 3 ? fmt::format("{:08X}", message.Word2) : std::string{});
        setPlain(ParameterDataWord3, message.NumWords >= 4 ? fmt::format("{:08X}", message.Word3) : std::string{});

        auto const messageType = midi2msg::MidiMessageHelper::GetMessageTypeFromMessageFirstWord(message.Word0);

        if (m_verbose || m_decodeMessages)
        {
            auto detailedMessageType =
                ToUtf8(midi2msg::MidiMessageHelper::GetMessageDisplayNameFromFirstWord(message.Word0));

            if (DisplayWidth(detailedMessageType) > DetailedMessageTypeTextWidth)
            {
                detailedMessageType = detailedMessageType.substr(0, DetailedMessageTypeTextWidth);
            }

            setPlain(ParameterDecodedMessageType, detailedMessageType);

            std::string groupText;
            std::string channelText;

            if (midi2msg::MidiMessageHelper::MessageTypeHasGroupField(messageType))
            {
                groupText = fmt::format("{:>2}",
                    midi2msg::MidiMessageHelper::GetGroupFromMessageFirstWord(message.Word0).DisplayValue());
            }

            if (midi2msg::MidiMessageHelper::MessageTypeHasChannelField(messageType))
            {
                channelText = fmt::format("{:>2}",
                    midi2msg::MidiMessageHelper::GetChannelFromMessageFirstWord(message.Word0).DisplayValue());
            }

            setPlain(ParameterDecodedGroup, groupText);
            setPlain(ParameterDecodedChannel, channelText);

            values[ParameterDecodedData] = BuildDecodedDataCell(message);
        }

        if (m_verbose || m_includeTimestamps)
        {
            setPlain(ParameterMessageTimestamp, FormatWithThousandsSeparators(message.MessageTimestamp));

            auto const offset = ConvertTicksToFriendlyTimeUnit(message.ReceivedOffsetFromLastMessage);

            auto offsetValueText = fmt::format("{:.2f}", offset.Value);
            auto offsetUnitLabel = offset.UnitLabel;

            // 0 is the magic "send now" value. It only comes back this way on a loopback.
            std::string deltaValueText{ "--" };
            std::string deltaUnitLabel;

            if (message.MessageTimestamp != 0)
            {
                FriendlyTimeUnit delta{};

                if (message.ReceivedTimestamp >= message.MessageTimestamp)
                {
                    delta = ConvertTicksToFriendlyTimeUnit(message.ReceivedTimestamp - message.MessageTimestamp);
                }
                else
                {
                    delta = ConvertTicksToFriendlyTimeUnit(message.MessageTimestamp - message.ReceivedTimestamp);
                    delta.Value = -delta.Value;
                }

                deltaValueText = fmt::format("{:.2f}", delta.Value);
                deltaUnitLabel = delta.UnitLabel;
            }

            for (auto& column : m_columns)
            {
                if (column.ParameterIndex == ParameterMessageTimestampDelta)
                {
                    if (DisplayWidth(deltaValueText) > TimestampOffsetValueColumnWidth)
                    {
                        deltaValueText = "000000";
                        deltaUnitLabel.clear();
                        column.DataStyle = deltaValueErrorStyle;
                    }
                    else
                    {
                        column.DataStyle = deltaValueDefaultStyle;
                    }
                }
                else if (column.ParameterIndex == ParameterReceivedTimestampDelta)
                {
                    if (DisplayWidth(offsetValueText) > TimestampOffsetValueColumnWidth)
                    {
                        offsetValueText = "000000";
                        offsetUnitLabel.clear();
                        column.DataStyle = offsetValueErrorStyle;
                    }
                    else
                    {
                        column.DataStyle = offsetValueDefaultStyle;
                    }
                }
            }

            setPlain(ParameterMessageTimestampDelta, deltaValueText);
            setPlain(ParameterMessageTimestampDeltaUnits, deltaUnitLabel);
            setPlain(ParameterReceivedTimestamp, FormatWithThousandsSeparators(message.ReceivedTimestamp));
            setPlain(ParameterReceivedTimestampDelta, offsetValueText);
            setPlain(ParameterReceivedTimestampDeltaUnits, offsetUnitLabel);
        }

        auto const errorBackground = fmt::bg(fmt::color::dark_red);

        std::string line;

        bool first = true;

        for (auto const& column : m_columns)
        {
            if (!first && !column.NoLeftSeparator)
            {
                line += fmt::format(" {} ", Styled(VerticalLine, separatorTextStyle));
            }
            else if (column.NoLeftSeparator && !first)
            {
                line += " ";
            }

            auto const found = values.find(column.ParameterIndex);
            auto const& cell = found != values.end() ? found->second : Cell{};

            auto const style = message.HasError ? (column.DataStyle | errorBackground) : column.DataStyle;

            if (cell.HasOwnStyling)
            {
                // Already carries escape sequences, so only the trailing pad can be added here.
                auto const padWidth = static_cast<size_t>(column.Width < 0 ? -column.Width : column.Width);
                auto const currentWidth = DisplayWidth(cell.Plain);

                line += cell.Rendered;

                if (currentWidth < padWidth)
                {
                    line += std::string(padWidth - currentWidth, ' ');
                }
            }
            else
            {
                line += fmt::format("{}", Styled(AlignPlain(cell.Plain, column.Width), style));
            }

            first = false;
        }

        if (message.HasError)
        {
            line += fmt::format("{}", Styled(ResourceString(IDS_EP_MONITOR_POSSIBLE_ERROR),
                fmt::fg(fmt::color::white) | errorBackground));
        }

        WriteLine(line);
    }

    MidiMessageTable::Cell MidiMessageTable::BuildDecodedDataCell(_In_ ReceivedMidiMessage const& message) const
    {
        switch (midi2msg::MidiMessageHelper::GetMessageTypeFromMessageFirstWord(message.Word0))
        {
        case midi2::MidiMessageType::Midi1ChannelVoice32:
            return BuildDecodedMidi1ChannelVoiceCell(message);

        case midi2::MidiMessageType::Midi2ChannelVoice64:
            return BuildDecodedMidi2ChannelVoiceCell(message);

        default:
            return {};
        }
    }

    MidiMessageTable::Cell MidiMessageTable::BuildDecodedMidi1ChannelVoiceCell(_In_ ReceivedMidiMessage const& message) const
    {
        auto const status = midi2msg::MidiMessageHelper::GetStatusFromMidi1ChannelVoiceMessage(message.Word0);

        auto const dataByte1 = static_cast<uint8_t>((message.Word0 >> 8) & 0xFF);
        auto const dataByte2 = static_cast<uint8_t>(message.Word0 & 0xFF);

        std::vector<std::pair<std::string, fmt::text_style>> parts;

        switch (status)
        {
        case midi2msg::Midi1ChannelVoiceMessageStatus::NoteOn:
        case midi2msg::Midi1ChannelVoiceMessageStatus::NoteOff:
        {
            auto const noteName = ToUtf8(midi2msg::MidiMessageHelper::GetNoteDisplayNameFromNoteIndex(dataByte1));
            auto const octave = midi2msg::MidiMessageHelper::GetNoteOctaveFromNoteIndex(dataByte1);
            auto const noteInfo = fmt::format("({}{})", noteName, octave);

            parts.emplace_back("Note ", decodedLabelStyle);
            parts.emplace_back(FormatByteDecimal(dataByte1), decodedValueStyle);
            parts.emplace_back(fmt::format(" {:<9}", noteInfo), messageTypeStyle);
            parts.emplace_back(" Vel ", decodedLabelStyle);
            parts.emplace_back(FormatByteDecimal(dataByte2), decodedValueStyle);
            break;
        }

        case midi2msg::Midi1ChannelVoiceMessageStatus::ControlChange:
            parts.emplace_back("Controller ", decodedLabelStyle);
            parts.emplace_back(FormatByteDecimal(dataByte1), decodedValueStyle);
            parts.emplace_back(", Value ", decodedLabelStyle);
            parts.emplace_back(FormatByteDecimal(dataByte2), decodedValueStyle);
            break;

        case midi2msg::Midi1ChannelVoiceMessageStatus::PitchBend:
            parts.emplace_back("Fine ", decodedLabelStyle);
            parts.emplace_back(FormatByteDecimal(dataByte1), decodedValueStyle);
            parts.emplace_back(", Coarse ", decodedLabelStyle);
            parts.emplace_back(FormatByteDecimal(dataByte2), decodedValueStyle);
            break;

        case midi2msg::Midi1ChannelVoiceMessageStatus::PolyPressure:
            parts.emplace_back("Key ", decodedLabelStyle);
            parts.emplace_back(FormatByteDecimal(dataByte1), decodedValueStyle);
            parts.emplace_back(", Value ", decodedLabelStyle);
            parts.emplace_back(FormatByteDecimal(dataByte2), decodedValueStyle);
            break;

        case midi2msg::Midi1ChannelVoiceMessageStatus::ChannelPressure:
        case midi2msg::Midi1ChannelVoiceMessageStatus::ProgramChange:
            parts.emplace_back("Value ", decodedLabelStyle);
            parts.emplace_back(FormatByteDecimal(dataByte1), decodedValueStyle);
            break;

        default:
            return {};
        }

        Cell cell;
        cell.HasOwnStyling = true;

        for (auto const& [text, style] : parts)
        {
            cell.Plain += text;
            cell.Rendered += fmt::format("{}", Styled(text, style));
        }

        return cell;
    }

    MidiMessageTable::Cell MidiMessageTable::BuildDecodedMidi2ChannelVoiceCell(_In_ ReceivedMidiMessage const& message) const
    {
        auto const status = midi2msg::MidiMessageHelper::GetStatusFromMidi2ChannelVoiceMessageFirstWord(message.Word0);

        auto const dataByte1 = static_cast<uint8_t>((message.Word0 >> 8) & 0xFF);

        std::vector<std::pair<std::string, fmt::text_style>> parts;

        switch (status)
        {
        case midi2msg::Midi2ChannelVoiceMessageStatus::NoteOn:
        case midi2msg::Midi2ChannelVoiceMessageStatus::NoteOff:
        {
            auto const noteName = ToUtf8(midi2msg::MidiMessageHelper::GetNoteDisplayNameFromNoteIndex(dataByte1));
            auto const octave = midi2msg::MidiMessageHelper::GetNoteOctaveFromNoteIndex(dataByte1);
            auto const noteInfo = fmt::format("({}{})", noteName, octave);

            auto const velocity = static_cast<uint16_t>((message.Word1 >> 16) & 0xFFFF);

            parts.emplace_back("Note ", decodedLabelStyle);
            parts.emplace_back(FormatByteDecimal(dataByte1), decodedValueStyle);
            parts.emplace_back(fmt::format(" {:<9}", noteInfo), messageTypeStyle);
            parts.emplace_back(" Vel ", decodedLabelStyle);
            parts.emplace_back(fmt::format("{:>5}", velocity), decodedValueStyle);
            break;
        }

        case midi2msg::Midi2ChannelVoiceMessageStatus::ControlChange:
            parts.emplace_back("Controller ", decodedLabelStyle);
            parts.emplace_back(FormatByteDecimal(dataByte1), decodedValueStyle);
            parts.emplace_back(", Value ", decodedLabelStyle);
            parts.emplace_back(fmt::format("{:>10}", message.Word1), decodedValueStyle);
            break;

        case midi2msg::Midi2ChannelVoiceMessageStatus::PitchBend:
            parts.emplace_back("Value ", decodedLabelStyle);
            parts.emplace_back(fmt::format("{:>10}", message.Word1), decodedValueStyle);
            break;

        default:
            return {};
        }

        Cell cell;
        cell.HasOwnStyling = true;

        for (auto const& [text, style] : parts)
        {
            cell.Plain += text;
            cell.Rendered += fmt::format("{}", Styled(text, style));
        }

        return cell;
    }
}
