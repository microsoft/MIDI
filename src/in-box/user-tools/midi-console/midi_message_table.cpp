// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

#include "pch.h"

#include <map>

#include <midi_ump_message_defs.h>
#include <ump_helpers.h>

#include "console_output.h"
#include "endpoint_utility.h"
#include "midi_formatting.h"
#include "midi_message_table.h"
#include "strings.h"

namespace internal = ::WindowsMidiServicesInternal;

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

        constexpr uint8_t UtilityStatusJitterReductionClock = 0x1;
        constexpr uint8_t UtilityStatusJitterReductionTimestamp = 0x2;
        constexpr uint8_t UtilityStatusDeltaClockstampTicksPerQuarterNote = 0x3;
        constexpr uint8_t UtilityStatusDeltaClockstampTicksSinceLast = 0x4;

        // Two seven bit bytes, least significant first, as the identity fields are carried.
        uint16_t FormatTwoByteValue(_In_ uint8_t leastSignificant, _In_ uint8_t mostSignificant)
        {
            return static_cast<uint16_t>((static_cast<uint16_t>(mostSignificant) << 7) | leastSignificant);
        }

        std::string FormatStreamProtocol(_In_ uint8_t protocol)
        {
            switch (protocol)
            {
            case 1:  return "MIDI 1.0";
            case 2:  return "MIDI 2.0";
            default: return fmt::format("{}", protocol);
            }
        }

        // Compact letters rather than words, because several of these share one narrow column.
        std::string FormatEndpointDiscoveryFilter(_In_ uint8_t filter)
        {
            if (filter == 0)
            {
                return "nothing";
            }

            std::string result;

            auto const append = [&result](_In_ std::string_view value)
                {
                    if (!result.empty())
                    {
                        result += ",";
                    }

                    result += value;
                };

            if (internal::EndpointDiscoveryFilterRequestsEndpointInfoNotification(filter))          append("Info");
            if (internal::EndpointDiscoveryFilterRequestsDeviceIdentityNotification(filter))        append("Identity");
            if (internal::EndpointDiscoveryFilterRequestsEndpointNameNotification(filter))          append("Name");
            if (internal::EndpointDiscoveryFilterRequestsProductInstanceIdNotification(filter))     append("ProductId");
            if (internal::EndpointDiscoveryFilterRequestsStreamConfigurationNotification(filter))   append("Config");

            return result;
        }

        std::string FormatFunctionBlockDiscoveryFilter(_In_ uint8_t filter)
        {
            bool const wantsInfo = (filter & 0x01) != 0;
            bool const wantsName = (filter & 0x02) != 0;

            if (wantsInfo && wantsName) return "Info,Name";
            if (wantsInfo)              return "Info";
            if (wantsName)              return "Name";

            return "nothing";
        }

        std::string FormatFunctionBlockRequestNumber(_In_ uint8_t functionBlockNumber)
        {
            return functionBlockNumber == MIDI_STREAM_MESSAGE_FUNCTION_BLOCK_REQUEST_ALL_FUNCTION_BLOCKS
                ? std::string{ "all" }
                : fmt::format("{}", functionBlockNumber);
        }

        std::string FormatFunctionBlockInfoDirection(_In_ uint8_t direction)
        {
            switch (direction)
            {
            case 0x01: return "Input";
            case 0x02: return "Output";
            case 0x03: return "Bidi";
            default:   return "Undefined";
            }
        }

        // Group numbers, not indexes, because this is customer-facing.
        std::string FormatGroupSpanFromFirstGroupIndex(_In_ uint8_t firstGroupIndex, _In_ uint8_t groupCount)
        {
            auto const first = static_cast<int>(firstGroupIndex) + 1;

            if (groupCount > 1)
            {
                return fmt::format("Gr {}-{}", first, first + groupCount - 1);
            }

            return fmt::format("Gr {}", first);
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

        case midi2::MidiMessageType::UtilityMessage32:
            return BuildDecodedUtilityCell(message);

        case midi2::MidiMessageType::Stream128:
            return BuildDecodedStreamCell(message);

        // SysEx7 carries six payload bytes after the type/group and status/count bytes.
        case midi2::MidiMessageType::DataMessage64:
            return BuildDecodedDataBytesCell(message, 2,
                midi2msg::MidiMessageHelper::GetNumberOfBytesFromDataMessage64FirstWord(message.Word0), 6);

        // SysEx8 spends a third byte on the stream id, leaving thirteen for the payload.
        case midi2::MidiMessageType::DataMessage128:
            return BuildDecodedDataBytesCell(message, 3,
                midi2msg::MidiMessageHelper::GetNumberOfBytesFromDataMessage128FirstWord(message.Word0), 13);

        default:
            return {};
        }
    }

    MidiMessageTable::Cell MidiMessageTable::BuildCellFromParts(
        _In_ std::vector<std::pair<std::string, fmt::text_style>> const& parts) const
    {
        if (parts.empty())
        {
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

    MidiMessageTable::Cell MidiMessageTable::BuildDecodedUtilityCell(_In_ ReceivedMidiMessage const& message) const
    {
        auto const status = static_cast<uint8_t>((message.Word0 >> 20) & 0x0F);

        std::vector<std::pair<std::string, fmt::text_style>> parts;

        switch (status)
        {
        case UtilityStatusJitterReductionClock:
        case UtilityStatusJitterReductionTimestamp:
            parts.emplace_back("Ticks ", decodedLabelStyle);
            parts.emplace_back(fmt::format("{}", static_cast<uint16_t>(message.Word0 & 0xFFFF)), decodedValueStyle);
            break;

        case UtilityStatusDeltaClockstampTicksPerQuarterNote:
            parts.emplace_back("Ticks per quarter note ", decodedLabelStyle);
            parts.emplace_back(fmt::format("{}", static_cast<uint16_t>(message.Word0 & 0xFFFF)), decodedValueStyle);
            break;

        // This one carries a 20 bit value, unlike the 16 bit fields above.
        case UtilityStatusDeltaClockstampTicksSinceLast:
            parts.emplace_back("Ticks since last event ", decodedLabelStyle);
            parts.emplace_back(fmt::format("{}", message.Word0 & 0x000FFFFF), decodedValueStyle);
            break;

        default:
            return {};
        }

        return BuildCellFromParts(parts);
    }

    MidiMessageTable::Cell MidiMessageTable::BuildDecodedStreamCell(_In_ ReceivedMidiMessage const& message) const
    {
        auto const status = internal::GetStatusFromStreamMessageFirstWord(message.Word0);

        std::vector<std::pair<std::string, fmt::text_style>> parts;

        auto const appendFlag = [&parts](_In_ std::string_view label, _In_ bool value)
            {
                parts.emplace_back(std::string{ label }, decodedLabelStyle);
                parts.emplace_back(value ? "Y" : "n", value ? booleanTrueTextStyle : booleanFalseTextStyle);
            };

        switch (status)
        {
        case MIDI_STREAM_MESSAGE_STATUS_ENDPOINT_DISCOVERY:
            parts.emplace_back("UMP ", decodedLabelStyle);
            parts.emplace_back(fmt::format("{}.{}",
                internal::GetEndpointInfoNotificationUmpVersionMajorFirstWord(message.Word0),
                internal::GetEndpointInfoNotificationUmpVersionMinorFirstWord(message.Word0)), decodedValueStyle);
            parts.emplace_back(" Asks for ", decodedLabelStyle);
            parts.emplace_back(FormatEndpointDiscoveryFilter(
                static_cast<uint8_t>(message.Word1 & 0xFF)), decodedValueStyle);
            break;

        case MIDI_STREAM_MESSAGE_STATUS_ENDPOINT_INFO_NOTIFICATION:
            parts.emplace_back("UMP ", decodedLabelStyle);
            parts.emplace_back(fmt::format("{}.{}",
                internal::GetEndpointInfoNotificationUmpVersionMajorFirstWord(message.Word0),
                internal::GetEndpointInfoNotificationUmpVersionMinorFirstWord(message.Word0)), decodedValueStyle);
            parts.emplace_back(" FBs ", decodedLabelStyle);
            parts.emplace_back(fmt::format("{}",
                internal::GetEndpointInfoNotificationNumberOfFunctionBlocksFromSecondWord(message.Word1)), decodedValueStyle);

            if (internal::GetEndpointInfoNotificationStaticFunctionBlocksFlagFromSecondWord(message.Word1))
            {
                parts.emplace_back(" static", decodedLabelStyle);
            }

            appendFlag(" MIDI1 ", internal::GetEndpointInfoNotificationMidi1ProtocolCapabilityFromSecondWord(message.Word1));
            appendFlag(" MIDI2 ", internal::GetEndpointInfoNotificationMidi2ProtocolCapabilityFromSecondWord(message.Word1));
            appendFlag(" RxJR ", internal::GetEndpointInfoNotificationReceiveJRTimestampCapabilityFromSecondWord(message.Word1));
            appendFlag(" TxJR ", internal::GetEndpointInfoNotificationTransmitJRTimestampCapabilityFromSecondWord(message.Word1));
            break;

        case MIDI_STREAM_MESSAGE_STATUS_DEVICE_IDENTITY_NOTIFICATION:
            parts.emplace_back("Mfg ", decodedLabelStyle);
            parts.emplace_back(fmt::format("{:02X} {:02X} {:02X}",
                static_cast<uint8_t>((message.Word1 >> 16) & 0x7F),
                static_cast<uint8_t>((message.Word1 >> 8) & 0x7F),
                static_cast<uint8_t>(message.Word1 & 0x7F)), decodedValueStyle);
            parts.emplace_back(" Family ", decodedLabelStyle);
            parts.emplace_back(fmt::format("{}", FormatTwoByteValue(
                static_cast<uint8_t>((message.Word2 >> 24) & 0x7F),
                static_cast<uint8_t>((message.Word2 >> 16) & 0x7F))), decodedValueStyle);
            parts.emplace_back(" Model ", decodedLabelStyle);
            parts.emplace_back(fmt::format("{}", FormatTwoByteValue(
                static_cast<uint8_t>((message.Word2 >> 8) & 0x7F),
                static_cast<uint8_t>(message.Word2 & 0x7F))), decodedValueStyle);
            parts.emplace_back(" Rev ", decodedLabelStyle);
            parts.emplace_back(fmt::format("{:02X} {:02X} {:02X} {:02X}",
                static_cast<uint8_t>((message.Word3 >> 24) & 0x7F),
                static_cast<uint8_t>((message.Word3 >> 16) & 0x7F),
                static_cast<uint8_t>((message.Word3 >> 8) & 0x7F),
                static_cast<uint8_t>(message.Word3 & 0x7F)), decodedValueStyle);
            break;

        case MIDI_STREAM_MESSAGE_STATUS_STREAM_CONFIGURATION_REQUEST:
        case MIDI_STREAM_MESSAGE_STATUS_STREAM_CONFIGURATION_NOTIFICATION:
            parts.emplace_back("Protocol ", decodedLabelStyle);
            parts.emplace_back(FormatStreamProtocol(
                internal::GetStreamConfigurationNotificationProtocolFromFirstWord(message.Word0)), decodedValueStyle);
            appendFlag(" RxJR ", internal::GetStreamConfigurationNotificationReceiveJRFromFirstWord(message.Word0));
            appendFlag(" TxJR ", internal::GetStreamConfigurationNotificationTransmitJRFromFirstWord(message.Word0));
            break;

        case MIDI_STREAM_MESSAGE_STATUS_FUNCTION_BLOCK_DISCOVERY:
            parts.emplace_back("FB ", decodedLabelStyle);
            parts.emplace_back(FormatFunctionBlockRequestNumber(
                static_cast<uint8_t>((message.Word0 >> 8) & 0xFF)), decodedValueStyle);
            parts.emplace_back(" Asks for ", decodedLabelStyle);
            parts.emplace_back(FormatFunctionBlockDiscoveryFilter(
                static_cast<uint8_t>(message.Word0 & 0xFF)), decodedValueStyle);
            break;

        case MIDI_STREAM_MESSAGE_STATUS_FUNCTION_BLOCK_INFO_NOTIFICATION:
            parts.emplace_back("FB ", decodedLabelStyle);
            parts.emplace_back(fmt::format("{}",
                internal::GetFunctionBlockNumberFromInfoNotificationFirstWord(message.Word0)), decodedValueStyle);

            if (!internal::GetFunctionBlockActiveFlagFromInfoNotificationFirstWord(message.Word0))
            {
                parts.emplace_back(" inactive", warningTextStyle);
            }

            parts.emplace_back(" ", decodedLabelStyle);
            parts.emplace_back(FormatFunctionBlockInfoDirection(
                internal::GetFunctionBlockDirectionFromInfoNotificationFirstWord(message.Word0)), decodedValueStyle);
            parts.emplace_back(" ", decodedLabelStyle);
            parts.emplace_back(FormatGroupSpanFromFirstGroupIndex(
                internal::GetFunctionBlockFirstGroupFromInfoNotificationSecondWord(message.Word1),
                internal::GetFunctionBlockNumberOfGroupsFromInfoNotificationSecondWord(message.Word1)), groupTextStyle);
            parts.emplace_back(" MIDI-CI ", decodedLabelStyle);
            parts.emplace_back(fmt::format("{}",
                internal::GetFunctionBlockMidiCIVersionFromInfoNotificationSecondWord(message.Word1)), decodedValueStyle);
            break;

        // Endpoint name, product instance id and function block name are UTF-8 split across
        // several messages, so a single message holds a fragment that cannot be read on its own.
        default:
            return {};
        }

        return BuildCellFromParts(parts);
    }

    _Use_decl_annotations_
    MidiMessageTable::Cell MidiMessageTable::BuildDecodedDataBytesCell(
        ReceivedMidiMessage const& message,
        uint8_t payloadByteOffset,
        uint8_t declaredByteCount,
        uint8_t maximumByteCount) const
    {
        std::array<uint32_t, 4> const words{ message.Word0, message.Word1, message.Word2, message.Word3 };

        auto const availableBytes = static_cast<uint8_t>(std::min<uint8_t>(message.NumWords, 4) * 4);

        if (payloadByteOffset >= availableBytes)
        {
            return {};
        }

        // A device is free to declare a byte count the message cannot hold, so it is clamped
        // both to what the message type allows and to what actually arrived.
        auto const byteCount = std::min<uint8_t>(
            std::min<uint8_t>(declaredByteCount, maximumByteCount),
            static_cast<uint8_t>(availableBytes - payloadByteOffset));

        if (byteCount == 0)
        {
            return {};
        }

        std::array<uint8_t, 16> bytes{};

        for (uint8_t i = 0; i < std::min<uint8_t>(message.NumWords, 4); i++)
        {
            bytes[(i * 4) + 0] = static_cast<uint8_t>((words[i] >> 24) & 0xFF);
            bytes[(i * 4) + 1] = static_cast<uint8_t>((words[i] >> 16) & 0xFF);
            bytes[(i * 4) + 2] = static_cast<uint8_t>((words[i] >> 8) & 0xFF);
            bytes[(i * 4) + 3] = static_cast<uint8_t>(words[i] & 0xFF);
        }

        Cell cell;
        cell.Plain.reserve(static_cast<size_t>(byteCount) * 3);

        for (uint8_t i = 0; i < byteCount; i++)
        {
            if (i > 0)
            {
                cell.Plain += ' ';
            }

            cell.Plain += fmt::format("{:02X}", bytes[payloadByteOffset + i]);
        }

        return cell;
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

        // RPN and NRPN. In MIDI 2.0 these are whole messages rather than the multi-message
        // control change sequence MIDI 1.0 uses, so bank, index and value all read from here.
        case midi2msg::Midi2ChannelVoiceMessageStatus::RegisteredController:
        case midi2msg::Midi2ChannelVoiceMessageStatus::AssignableController:
        case midi2msg::Midi2ChannelVoiceMessageStatus::RelativeRegisteredController:
        case midi2msg::Midi2ChannelVoiceMessageStatus::RelativeAssignableController:
            parts.emplace_back("Bank ", decodedLabelStyle);
            parts.emplace_back(fmt::format("{}", dataByte1 & 0x7F), decodedValueStyle);
            parts.emplace_back(" Index ", decodedLabelStyle);
            parts.emplace_back(fmt::format("{}", static_cast<uint8_t>(message.Word0 & 0x7F)), decodedValueStyle);
            parts.emplace_back(" Value ", decodedLabelStyle);
            parts.emplace_back(fmt::format("{}", message.Word1), decodedValueStyle);
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
