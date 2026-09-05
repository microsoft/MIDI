// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

#include "pch.h"

#include <algorithm>
#include <fstream>
#include <thread>

#include "cmd_endpoint.h"
#include "console_output.h"
#include "console_table.h"
#include "endpoint_picker.h"
#include "endpoint_utility.h"
#include "midi_formatting.h"
#include "pickers.h"
#include "strings.h"
#include "word_parsing.h"

namespace midi2console
{
    namespace
    {
        constexpr int KeyEscape = 27;
        constexpr int SendRetryLimit = 500;

        // Every sending command needs the same session + connection dance, so it lives here once.
        struct EndpointSession
        {
            midi2::MidiSession Session{ nullptr };
            midi2::MidiEndpointConnection Connection{ nullptr };
            std::string EndpointDeviceId;
            std::string EndpointName;
            int FailureCode{ 0 };

            bool IsValid() const noexcept { return Connection != nullptr; }
        };

        EndpointSession OpenEndpoint(
            _In_ std::string endpointDeviceId,
            _In_ std::wstring_view sessionName,
            _In_ bool autoReconnect = false)
        {
            EndpointSession result;

            std::string endpointName;

            if (!ResolveEndpointDeviceId(endpointDeviceId, endpointName))
            {
                result.FailureCode = 2;
                return result;
            }

            result.EndpointDeviceId = endpointDeviceId;
            result.EndpointName = endpointName;

            result.Session = midi2::MidiSession::Create(winrt::hstring{ sessionName });

            if (result.Session == nullptr)
            {
                WriteErrorLine(ResourceString(IDS_ERROR_CREATING_SESSION));
                result.FailureCode = 1;
                return result;
            }

            midi2::MidiEndpointConnectionSettings const settings{ false, autoReconnect };

            auto connection = result.Session.CreateEndpointConnection(
                winrt::hstring{ FromUtf8(endpointDeviceId) }, settings);

            if (connection == nullptr)
            {
                WriteErrorLine(ResourceString(IDS_ERROR_CREATING_CONNECTION));
                result.FailureCode = 1;
                return result;
            }

            if (!connection.Open())
            {
                WriteErrorLine(ResourceString(IDS_ERROR_OPENING_CONNECTION));
                result.FailureCode = 1;
                return result;
            }

            result.Connection = connection;

            WriteLine(fmt::format("{} {}",
                Styled(ResourceString(IDS_EP_SENDING_TO), infoTextStyle),
                Styled(endpointName.empty() ? endpointDeviceId : endpointName, endpointNameTextStyle)));

            return result;
        }

        // Buffer-full is a transient condition; everything else is fatal for that message.
        bool SendWordsWithRetry(
            _In_ midi2::MidiEndpointConnection const& connection,
            _In_ uint64_t timestamp,
            _In_ std::vector<uint32_t>& words)
        {
            for (int attempt = 0; attempt < SendRetryLimit; attempt++)
            {
                auto const result = connection.SendSingleMessageWordArray(
                    timestamp, 0, static_cast<uint8_t>(words.size()), words);

                if (midi2::MidiEndpointConnection::SendMessageSucceeded(result))
                {
                    return true;
                }

                if ((result & midi2::MidiSendMessageResults::BufferFull) != midi2::MidiSendMessageResults::BufferFull)
                {
                    return false;
                }

                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            }

            return false;
        }

        void WaitForKeyIfRequested(_In_ bool noWait)
        {
            if (noWait || !CanShowInteractiveUI())
            {
                return;
            }

            WriteInfoLine(ResourceString(IDS_PROMPT_PRESS_KEY_TO_CLOSE));

            _getch();
        }

        bool EscapePressed()
        {
            while (_kbhit())
            {
                if (_getch() == KeyEscape)
                {
                    return true;
                }
            }

            return false;
        }

        int SendSingleStreamMessage(
            _In_ std::string endpointDeviceId,
            _In_ midi2::IMidiUniversalPacket const& message)
        {
            auto session = OpenEndpoint(std::move(endpointDeviceId), L"MIDI Console - Request");

            if (!session.IsValid())
            {
                return session.FailureCode;
            }

            auto const result = session.Connection.SendSingleMessagePacket(message);

            if (!midi2::MidiEndpointConnection::SendMessageSucceeded(result))
            {
                WriteErrorLine(ResourceString(IDS_ERROR_SEND_FAILED));
                return 1;
            }

            WriteSuccessLine(ResourceString(IDS_EP_REQUEST_SENT));

            return 0;
        }
    }

    int RunEndpointPropertiesCommand(_In_ EndpointPropertiesOptions const& options)
    {
        auto endpointDeviceId = options.EndpointDeviceId;
        std::string endpointName;

        if (!ResolveEndpointDeviceId(endpointDeviceId, endpointName))
        {
            return 2;
        }

        auto const device = midi2enum::MidiEndpointDeviceInformation::CreateFromEndpointDeviceId(
            winrt::hstring{ FromUtf8(endpointDeviceId) });

        if (device == nullptr)
        {
            WriteErrorLine(ResourceString(IDS_ERROR_ENDPOINT_NOT_FOUND));
            return 1;
        }

        auto const transportInfo = device.GetTransportSuppliedInfo();
        auto const declaredInfo = device.GetDeclaredEndpointInfo();
        auto const userInfo = device.GetUserSuppliedInfo();

        auto const isUmpNative = transportInfo != nullptr &&
            transportInfo.NativeDataFormat() == midi2enum::MidiEndpointNativeDataFormat::UniversalMidiPacketFormat;

        // ---- identification

        WriteSectionHeading(ResourceString(IDS_EP_SECTION_IDENTIFICATION));

        WriteField(ResourceString(IDS_LABEL_NAME), ToUtf8(device.Name()), endpointNameTextStyle);
        WriteField(ResourceString(IDS_LABEL_ID), ToUtf8(device.EndpointDeviceId()), endpointIdTextStyle);
        WriteField(ResourceString(IDS_LABEL_PURPOSE), FormatEndpointPurpose(device.EndpointPurpose()));

        if (transportInfo != nullptr)
        {
            WriteField(ResourceString(IDS_LABEL_TRANSPORT), ToUtf8(transportInfo.TransportCode()), transportCodeTextStyle);
            WriteField(ResourceString(IDS_LABEL_SERIAL_NUMBER), ToUtf8(transportInfo.SerialNumber()));
            WriteField(ResourceString(IDS_LABEL_MANUFACTURER), ToUtf8(transportInfo.ManufacturerName()));

            if (transportInfo.VendorId() != 0 || transportInfo.ProductId() != 0)
            {
                WriteField(ResourceString(IDS_LABEL_VID_PID),
                    fmt::format("0x{:04X} / 0x{:04X}", transportInfo.VendorId(), transportInfo.ProductId()));
            }

            auto const description = ToUtf8(transportInfo.Description());

            if (!description.empty())
            {
                WriteField(ResourceString(IDS_EP_PROP_TRANSPORT_DESCRIPTION), description);
            }

            WriteField(ResourceString(IDS_LABEL_MULTICLIENT),
                FormatBoolean(transportInfo.SupportsMultiClient()), BooleanStyle(transportInfo.SupportsMultiClient()));

            WriteField(ResourceString(IDS_LABEL_NATIVE_DATA_FORMAT),
                FormatNativeDataFormat(transportInfo.NativeDataFormat()));
        }

        // ---- endpoint metadata (verbose, UMP only)

        if (options.Verbose && isUmpNative && declaredInfo != nullptr)
        {
            WriteSectionHeading(ResourceString(IDS_EP_SECTION_ENDPOINT_METADATA));

            WriteField(ResourceString(IDS_EP_PROP_ENDPOINT_SUPPLIED_NAME), ToUtf8(declaredInfo.Name()));
            WriteField(ResourceString(IDS_EP_PROP_PRODUCT_INSTANCE_ID), ToUtf8(declaredInfo.ProductInstanceId()));
            WriteField(ResourceString(IDS_LABEL_UMP_VERSION),
                fmt::format("{}.{}", declaredInfo.SpecificationVersionMajor(), declaredInfo.SpecificationVersionMinor()));

            auto const identity = device.GetDeclaredDeviceIdentity();

            if (identity != nullptr)
            {
                auto const systemExclusiveId = identity.SystemExclusiveId();

                std::string sysExText;

                for (auto const value : systemExclusiveId)
                {
                    sysExText += fmt::format("{:02X} ", value);
                }

                WriteField(ResourceString(IDS_EP_PROP_SYSTEM_EXCLUSIVE_ID), TrimCopy(sysExText));

                WriteField(ResourceString(IDS_EP_PROP_DEVICE_FAMILY),
                    fmt::format("{:02X} {:02X}", identity.DeviceFamilyLsb(), identity.DeviceFamilyMsb()));

                auto const softwareRevision = identity.SoftwareRevisionLevel();

                std::string revisionText;

                for (auto const value : softwareRevision)
                {
                    revisionText += fmt::format("{:02X} ", value);
                }

                WriteField(ResourceString(IDS_EP_PROP_SOFTWARE_REVISION), TrimCopy(revisionText));
            }
        }

        // ---- user data

        WriteSectionHeading(ResourceString(IDS_EP_SECTION_USER_DATA));

        if (userInfo != nullptr)
        {
            WriteField(ResourceString(IDS_EP_PROP_USER_SUPPLIED_NAME), ToUtf8(userInfo.Name()));
            WriteField(ResourceString(IDS_EP_PROP_USER_DESCRIPTION), ToUtf8(userInfo.Description()));
            WriteField(ResourceString(IDS_LABEL_IMAGE_FILE_NAME), ToUtf8(userInfo.ImageFileName()));
        }

        // ---- active configuration and declared capabilities (UMP only)

        if (isUmpNative)
        {
            auto const streamConfiguration = device.GetDeclaredStreamConfiguration();

            if (streamConfiguration != nullptr)
            {
                WriteSectionHeading(ResourceString(IDS_EP_SECTION_ACTIVE_CONFIGURATION));

                WriteField(ResourceString(IDS_LABEL_PROTOCOL), FormatProtocol(streamConfiguration.Protocol()));
                WriteField(ResourceString(IDS_EP_PROP_SEND_JR_TIMESTAMPS),
                    FormatBoolean(streamConfiguration.SendJitterReductionTimestamps()),
                    BooleanStyle(streamConfiguration.SendJitterReductionTimestamps()));
                WriteField(ResourceString(IDS_EP_PROP_RECEIVE_JR_TIMESTAMPS),
                    FormatBoolean(streamConfiguration.ReceiveJitterReductionTimestamps()),
                    BooleanStyle(streamConfiguration.ReceiveJitterReductionTimestamps()));
            }

            if (declaredInfo != nullptr)
            {
                WriteSectionHeading(ResourceString(IDS_EP_SECTION_DECLARED_CAPABILITIES));

                WriteField(ResourceString(IDS_EP_PROP_SUPPORTS_MIDI1_PROTOCOL),
                    FormatBoolean(declaredInfo.SupportsMidi10Protocol()), BooleanStyle(declaredInfo.SupportsMidi10Protocol()));
                WriteField(ResourceString(IDS_EP_PROP_SUPPORTS_MIDI2_PROTOCOL),
                    FormatBoolean(declaredInfo.SupportsMidi20Protocol()), BooleanStyle(declaredInfo.SupportsMidi20Protocol()));

                if (options.Verbose)
                {
                    WriteField(ResourceString(IDS_EP_PROP_SUPPORTS_SENDING_JR),
                        FormatBoolean(declaredInfo.SupportsSendingJitterReductionTimestamps()),
                        BooleanStyle(declaredInfo.SupportsSendingJitterReductionTimestamps()));
                    WriteField(ResourceString(IDS_EP_PROP_SUPPORTS_RECEIVING_JR),
                        FormatBoolean(declaredInfo.SupportsReceivingJitterReductionTimestamps()),
                        BooleanStyle(declaredInfo.SupportsReceivingJitterReductionTimestamps()));
                }
            }
        }

        // ---- function blocks

        auto const functionBlocks = device.GetDeclaredFunctionBlocks();

        if (isUmpNative && functionBlocks != nullptr && functionBlocks.Size() > 0)
        {
            WriteSectionHeading(ResourceString(IDS_EP_SECTION_FUNCTION_BLOCKS));

            if (declaredInfo != nullptr)
            {
                WriteField(ResourceString(IDS_EP_PROP_HAS_STATIC_FUNCTION_BLOCKS),
                    FormatBoolean(declaredInfo.HasStaticFunctionBlocks()),
                    BooleanStyle(declaredInfo.HasStaticFunctionBlocks()));
                WriteField(ResourceString(IDS_EP_PROP_FUNCTION_BLOCK_COUNT),
                    fmt::format("{}", declaredInfo.DeclaredFunctionBlockCount()), numberTextStyle);
            }

            for (auto const& block : functionBlocks)
            {
                auto const label = fmt::format("{} {}",
                    Styled(fmt::format("{:2}", static_cast<int>(block.Number())), portNumberTextStyle),
                    Styled(ToUtf8(block.Name()), endpointNameTextStyle));

                auto value = fmt::format("{} {} ({} {}), {} {}, {} {}, {} {}",
                    Styled(ResourceString(IDS_LABEL_GROUP), inlineLabelTextStyle),
                    block.FirstGroup().DisplayValue(),
                    Styled(ResourceString(IDS_LABEL_INDEX), inlineLabelTextStyle),
                    block.FirstGroup().Index(),
                    Styled(ResourceString(IDS_LABEL_MIDI), inlineLabelTextStyle),
                    FormatRepresentsMidi10Connection(block.RepresentsMidi10Connection()),
                    Styled(ResourceString(IDS_LABEL_DIRECTION), inlineLabelTextStyle),
                    FormatFunctionBlockDirection(block.Direction()),
                    Styled(ResourceString(IDS_LABEL_UI_HINT), inlineLabelTextStyle),
                    FormatFunctionBlockUIHint(block.UIHint()));

                if (!block.IsActive())
                {
                    value += fmt::format(", {}", Styled(ResourceString(IDS_LABEL_INACTIVE), warningTextStyle));
                }

                WriteField(label, value, {});

                if (options.Verbose)
                {
                    WriteField("    Max System Exclusive 8 Streams",
                        fmt::format("{}", block.MaxSystemExclusive8Streams()), numberTextStyle);
                    WriteField("    MIDI CI Message Version Format",
                        fmt::format("{}", block.MidiCIMessageVersionFormat()), numberTextStyle);
                }
            }
        }

        // ---- group terminal blocks

        auto const groupTerminalBlocks = device.GetGroupTerminalBlocks();

        if (groupTerminalBlocks != nullptr && groupTerminalBlocks.Size() > 0 &&
            (options.Verbose || functionBlocks == nullptr || functionBlocks.Size() == 0))
        {
            WriteSectionHeading(ResourceString(IDS_EP_SECTION_GROUP_TERMINAL_BLOCKS));

            for (auto const& block : groupTerminalBlocks)
            {
                auto const label = fmt::format("{} {}",
                    Styled(fmt::format("{:2}", static_cast<int>(block.Number())), portNumberTextStyle),
                    Styled(ToUtf8(block.Name()), endpointNameTextStyle));

                auto const value = fmt::format("{} {} ({} {}), {} {}",
                    Styled(ResourceString(IDS_LABEL_GROUP), inlineLabelTextStyle),
                    block.FirstGroup().DisplayValue(),
                    Styled(ResourceString(IDS_LABEL_INDEX), inlineLabelTextStyle),
                    block.FirstGroup().Index(),
                    Styled(ResourceString(IDS_LABEL_DIRECTION), inlineLabelTextStyle),
                    FormatGroupTerminalBlockDirection(block.Direction()));

                WriteField(label, value, {});

                if (options.Verbose)
                {
                    WriteField("    Max Input Bandwidth",
                        fmt::format("{}", block.CalculatedMaxDeviceInputBandwidthBitsPerSecond()), numberTextStyle);
                    WriteField("    Max Output Bandwidth",
                        fmt::format("{}", block.CalculatedMaxDeviceOutputBandwidthBitsPerSecond()), numberTextStyle);
                }
            }
        }

        // ---- MIDI 1.0 ports

        WriteSectionHeading(ResourceString(IDS_EP_SECTION_MIDI1_PORTS));

        WriteField(ResourceString(IDS_EP_PROP_PORT_NAMING_APPROACH),
            FormatPortNamingApproach(device.Midi1PortNamingApproach()));

        WriteBlankLine();

        for (auto const flow : { midi2enum::Midi1PortFlow::MidiMessageDestination,
                                 midi2enum::Midi1PortFlow::MidiMessageSource })
        {
            auto const ports = midi2legacy::MidiLegacyPortDeviceInformation::FindAllForAssociatedEndpoint(
                device.EndpointDeviceId(), flow);

            if (ports == nullptr || ports.Size() == 0)
            {
                continue;
            }

            for (auto const& port : ports)
            {
                // The WinMM port number is what an older application actually sees.
                auto const label = fmt::format("{} {}",
                    Styled(fmt::format("{:3}", port.Number()), portNumberTextStyle),
                    Styled(ToUtf8(port.Name()), endpointNameTextStyle));

                auto value = fmt::format("{} {} ({} {}), {} {}",
                    Styled(ResourceString(IDS_LABEL_GROUP), inlineLabelTextStyle),
                    port.Group().DisplayValue(),
                    Styled(ResourceString(IDS_LABEL_INDEX), inlineLabelTextStyle),
                    port.Group().Index(),
                    Styled(ResourceString(IDS_LABEL_DIRECTION), inlineLabelTextStyle),
                    FormatPortFlow(flow));

                WriteField(label, value, {});

                if (options.Verbose)
                {
                    WriteField("    " + ResourceString(IDS_LABEL_ID), ToUtf8(port.PortDeviceId()), endpointIdTextStyle);
                }
            }
        }

        // ---- name table

        if (options.IncludeNameTable)
        {
            auto const nameTable = device.GetNameTable();

            if (nameTable != nullptr && nameTable.Size() > 0)
            {
                WriteSectionHeading(ResourceString(IDS_EP_SECTION_NAME_TABLE));

                ConsoleTable table;

                table.AddColumn(ResourceString(IDS_LABEL_GROUP), ColumnAlignment::Right, numberTextStyle);
                table.AddColumn(ResourceString(IDS_LABEL_DIRECTION));
                table.AddColumn(ResourceString(IDS_EP_NAME_TABLE_CUSTOM));
                table.SetLastColumnShrinkable();
                table.AddColumn(ResourceString(IDS_EP_NAME_TABLE_LEGACY_COMPATIBLE));
                table.SetLastColumnShrinkable();
                table.AddColumn(ResourceString(IDS_EP_NAME_TABLE_NEW_STYLE));
                table.SetLastColumnShrinkable();

                for (auto const& entry : nameTable)
                {
                    table.BeginRow();
                    table.AddCell(fmt::format("{}", entry.Group().DisplayValue()));
                    table.AddCell(FormatPortFlow(entry.Flow()));
                    table.AddCell(ToUtf8(entry.CustomName()));
                    table.AddCell(ToUtf8(entry.LegacyCompatibleName()));
                    table.AddCell(ToUtf8(entry.NewStyleName()));
                }

                table.Render();
            }
        }

        // ---- raw properties

        if (options.IncludeRawProperties)
        {
            WriteSectionHeading(ResourceString(IDS_EP_SECTION_RAW_PROPERTIES));

            auto const properties = device.Properties();

            if (properties != nullptr)
            {
                std::vector<std::pair<std::string, foundation::IInspectable>> entries;

                for (auto const& property : properties)
                {
                    entries.emplace_back(ToUtf8(property.Key()), property.Value());
                }

                std::sort(entries.begin(), entries.end(),
                    [](auto const& left, auto const& right) { return left.first < right.first; });

                for (auto const& [key, value] : entries)
                {
                    auto const friendlyName = midi2enum::MidiEndpointDevicePropertyHelper::
                        GetMidiPropertyNameFromPropertyKey(winrt::hstring{ FromUtf8(key) });

                    auto const label = friendlyName.empty() ? key : ToUtf8(friendlyName);

                    WriteLine(fmt::format("  {}", Styled(label, propertyKeyTextStyle)));

                    if (label != key)
                    {
                        WriteLine(fmt::format("    {}", Styled(key, separatorTextStyle)));
                    }

                    // The value may already carry its own styling, so it is not restyled here.
                    WriteLine(fmt::format("    {}", FormatPropertyValue(value)));
                    WriteBlankLine();
                }
            }
        }

        // ---- parent device

        auto const parent = device.GetParentDeviceInformation();

        if (parent != nullptr)
        {
            WriteSectionHeading(ResourceString(IDS_EP_SECTION_PARENT));

            WriteField(ResourceString(IDS_LABEL_NAME), ToUtf8(parent.Name()));
            WriteField(ResourceString(IDS_LABEL_ID), ToUtf8(parent.Id()), deviceInstanceIdTextStyle);
        }

        WriteBlankLine();

        return 0;
    }

    int RunEndpointSendMessageCommand(_In_ EndpointSendMessageOptions const& options)
    {
        // Both positionals land in one list because CLI11 cannot express "optional single, then
        // variadic". Anything that is not a valid MIDI word is taken as the endpoint id, which
        // is unambiguous: endpoint ids are SWD interface ids, never numbers.
        auto endpointDeviceId = options.EndpointDeviceId;
        std::vector<std::string> wordText;

        for (auto const& argument : options.Words)
        {
            uint32_t parsed{ 0 };

            if (TryParseMidiWord(argument, options.WordDataFormat, parsed))
            {
                wordText.push_back(argument);
            }
            else if (endpointDeviceId.empty())
            {
                endpointDeviceId = argument;
            }
            else
            {
                WriteErrorLine(FormatResourceString(IDS_ERROR_INVALID_WORD_VALUE, argument));
                return 1;
            }
        }

        if (wordText.empty())
        {
            WriteErrorLine(ResourceString(IDS_ERROR_TOO_FEW_WORDS));
            return 1;
        }

        if (wordText.size() > 4)
        {
            WriteErrorLine(ResourceString(IDS_ERROR_TOO_MANY_WORDS));
            return 1;
        }

        if (options.Count < 1)
        {
            WriteErrorLine(ResourceString(IDS_ERROR_COUNT_TOO_LOW));
            return 1;
        }

        if (options.HasTimestamp && options.TimestampOffsetMicroseconds != 0)
        {
            WriteErrorLine(ResourceString(IDS_ERROR_TIMESTAMP_AND_OFFSET));
            return 1;
        }

        std::vector<uint32_t> words;

        for (auto const& text : wordText)
        {
            uint32_t value{ 0 };

            if (!TryParseMidiWord(text, options.WordDataFormat, value))
            {
                WriteErrorLine(FormatResourceString(IDS_ERROR_INVALID_WORD_VALUE, text));
                return 1;
            }

            words.push_back(value);
        }

        auto session = OpenEndpoint(endpointDeviceId, L"MIDI Console - Send Message");

        if (!session.IsValid())
        {
            return session.FailureCode;
        }

        uint32_t sent{ 0 };
        uint32_t failed{ 0 };
        uint64_t maximumScheduledTimestamp{ 0 };

        for (int i = 0; i < options.Count; i++)
        {
            uint64_t timestamp{ 0 };

            if (options.HasTimestamp)
            {
                timestamp = options.Timestamp;
            }
            else if (options.TimestampOffsetMicroseconds != 0)
            {
                timestamp = midi2::MidiClock::OffsetTimestampByMicroseconds(
                    midi2::MidiClock::Now(), options.TimestampOffsetMicroseconds);
            }
            else
            {
                timestamp = midi2::MidiClock::Now();
            }

            maximumScheduledTimestamp = std::max(maximumScheduledTimestamp, timestamp);

            if (SendWordsWithRetry(session.Connection, timestamp, words))
            {
                sent++;
            }
            else
            {
                failed++;
            }

            if (options.DebugAutoIncrementLastWord && words.size() >= 2)
            {
                words.back() = words.back() + 1;
            }

            if (options.DelayBetweenMessages > 0 && i + 1 < options.Count)
            {
                std::this_thread::sleep_for(std::chrono::milliseconds(options.DelayBetweenMessages));
            }

            if (EscapePressed())
            {
                break;
            }
        }

        WriteBlankLine();
        WriteField(ResourceString(IDS_EP_SEND_MESSAGES_SENT), fmt::format("{}", sent), successTextStyle);

        if (failed > 0)
        {
            WriteField(ResourceString(IDS_EP_SEND_MESSAGES_FAILED), fmt::format("{}", failed), errorTextStyle);
        }

        // Scheduled messages have not left the service yet; closing now would discard them.
        auto const now = midi2::MidiClock::Now();

        if (maximumScheduledTimestamp > now)
        {
            auto const remaining = midi2::MidiClock::ConvertTimestampTicksToMilliseconds(
                maximumScheduledTimestamp - now);

            std::this_thread::sleep_for(
                std::chrono::milliseconds(static_cast<long long>(remaining) + 1000));
        }

        WaitForKeyIfRequested(options.NoWait);

        return failed > 0 ? 1 : 0;
    }

    int RunEndpointSendMessageFileCommand(_In_ EndpointSendMessageFileOptions const& options)
    {
        auto const path = ExpandEnvironmentPath(options.InputFile);

        std::ifstream file{ FromUtf8(path) };

        if (!file.is_open())
        {
            WriteErrorLine(FormatResourceString(IDS_ERROR_FILE_NOT_FOUND, path));
            return 1;
        }

        auto session = OpenEndpoint(options.EndpointDeviceId, L"MIDI Console - Send Message File");

        if (!session.IsValid())
        {
            return session.FailureCode;
        }

        uint32_t sent{ 0 };
        uint32_t failed{ 0 };
        uint32_t skipped{ 0 };
        uint32_t lineNumber{ 0 };

        std::string line;

        while (std::getline(file, line))
        {
            lineNumber++;

            auto const trimmed = TrimCopy(line);

            if (trimmed.empty() || trimmed[0] == '#')
            {
                continue;
            }

            auto const fields = SplitDelimitedLine(trimmed, options.FieldDelimiter);

            if (fields.empty() || fields.size() > 4)
            {
                skipped++;

                if (options.Verbose)
                {
                    WriteWarningLine(fmt::format("{}: {}", lineNumber, ResourceString(IDS_ERROR_INVALID_UMP)));
                }

                continue;
            }

            std::vector<uint32_t> words;

            bool parsed{ true };

            for (auto const& field : fields)
            {
                uint32_t value{ 0 };

                if (!TryParseMidiWord(field, options.WordDataFormat, value))
                {
                    parsed = false;
                    break;
                }

                words.push_back(value);
            }

            if (!parsed)
            {
                skipped++;

                if (options.Verbose)
                {
                    WriteWarningLine(fmt::format("{}: {}", lineNumber, ResourceString(IDS_ERROR_INVALID_WORD_VALUE)));
                }

                continue;
            }

            if (options.HasNewGroupIndex)
            {
                auto const messageType = midi2msg::MidiMessageHelper::GetMessageTypeFromMessageFirstWord(words[0]);

                if (midi2msg::MidiMessageHelper::MessageTypeHasGroupField(messageType))
                {
                    words[0] = midi2msg::MidiMessageHelper::ReplaceGroupInMessageFirstWord(
                        words[0], midi2::MidiGroup{ static_cast<uint8_t>(options.NewGroupIndex) });
                }
            }

            if (SendWordsWithRetry(session.Connection, midi2::MidiClock::Now(), words))
            {
                sent++;

                if (options.Verbose)
                {
                    WriteLine(fmt::format("  {}", Styled(trimmed, dataWordTextStyle)));
                }
            }
            else
            {
                failed++;
                WriteWarningLine(fmt::format("{}: {}", lineNumber, ResourceString(IDS_ERROR_SEND_FAILED)));
            }

            if (options.DelayBetweenMessages > 0)
            {
                std::this_thread::sleep_for(std::chrono::milliseconds(options.DelayBetweenMessages));
            }

            if (EscapePressed())
            {
                break;
            }
        }

        WriteBlankLine();
        WriteField(ResourceString(IDS_EP_SEND_MESSAGES_SENT), fmt::format("{}", sent), successTextStyle);

        if (skipped > 0)
        {
            WriteField(ResourceString(IDS_EP_SEND_LINES_SKIPPED), fmt::format("{}", skipped), warningTextStyle);
        }

        if (failed > 0)
        {
            WriteField(ResourceString(IDS_EP_SEND_MESSAGES_FAILED), fmt::format("{}", failed), errorTextStyle);
        }

        WaitForKeyIfRequested(options.NoWait);

        return failed > 0 ? 1 : 0;
    }

    int RunEndpointPlayNotesCommand(_In_ EndpointPlayNotesOptions const& options)
    {
        if (options.NoteIndexes.empty())
        {
            WriteErrorLine(ResourceString(IDS_ERROR_NO_NOTES_SUPPLIED));
            return 1;
        }

        if (options.GroupNumber < 1 || options.GroupNumber > 16)
        {
            WriteErrorLine(ResourceString(IDS_ERROR_INVALID_GROUP));
            return 1;
        }

        if (options.ChannelNumber < 1 || options.ChannelNumber > 16)
        {
            WriteErrorLine(ResourceString(IDS_ERROR_INVALID_CHANNEL));
            return 1;
        }

        if (options.Velocity < 1.0 || options.Velocity > 100.0)
        {
            WriteErrorLine(ResourceString(IDS_ERROR_INVALID_VELOCITY));
            return 1;
        }

        std::vector<uint8_t> notes;

        for (auto const& text : options.NoteIndexes)
        {
            auto const trimmed = TrimCopy(text);

            uint32_t parsed{ 0 };

            if (!TryParseMidiWord(trimmed, "Decimal", parsed) || parsed > 127)
            {
                WriteErrorLine(FormatResourceString(IDS_ERROR_INVALID_NOTE, trimmed));
                return 1;
            }

            notes.push_back(static_cast<uint8_t>(parsed));
        }

        auto session = OpenEndpoint(options.EndpointDeviceId, L"MIDI Console - Play Notes", options.AutoReconnect);

        if (!session.IsValid())
        {
            return session.FailureCode;
        }

        auto const group = midi2::MidiGroup{ static_cast<uint8_t>(options.GroupNumber - 1) };
        auto const channel = midi2::MidiChannel{ static_cast<uint8_t>(options.ChannelNumber - 1) };

        auto const midi1Velocity = static_cast<uint8_t>(options.Velocity / 100.0 * 127.0);
        auto const midi2Velocity = static_cast<uint32_t>(options.Velocity / 100.0 * 65535.0) << 16;

        WriteInfoLine(ResourceString(IDS_EP_PLAY_PLAYING));

        bool keepGoing{ true };

        do
        {
            for (auto const note : notes)
            {
                auto const sendNote = [&](bool noteOn)
                {
                    if (options.Midi2)
                    {
                        auto const message = midi2msg::MidiMessageBuilder::BuildMidi2ChannelVoiceMessage(
                            midi2::MidiClock::TimestampConstantSendImmediately(),
                            group,
                            noteOn ? midi2msg::Midi2ChannelVoiceMessageStatus::NoteOn
                                   : midi2msg::Midi2ChannelVoiceMessageStatus::NoteOff,
                            channel,
                            static_cast<uint16_t>(note << 8),
                            noteOn ? midi2Velocity : 0u);

                        session.Connection.SendSingleMessagePacket(message);
                    }
                    else
                    {
                        auto const message = midi2msg::MidiMessageBuilder::BuildMidi1ChannelVoiceMessage(
                            midi2::MidiClock::TimestampConstantSendImmediately(),
                            group,
                            noteOn ? midi2msg::Midi1ChannelVoiceMessageStatus::NoteOn
                                   : midi2msg::Midi1ChannelVoiceMessageStatus::NoteOff,
                            channel,
                            note,
                            noteOn ? midi1Velocity : static_cast<uint8_t>(0));

                        session.Connection.SendSingleMessagePacket(message);
                    }
                };

                WriteLine(fmt::format("  {} {}",
                    Styled(ResourceString(IDS_EP_DECODE_NOTE), fieldLabelTextStyle),
                    Styled(fmt::format("{:>3}", static_cast<int>(note)), numberTextStyle)));

                sendNote(true);

                std::this_thread::sleep_for(std::chrono::milliseconds(std::max(30, options.Length)));

                sendNote(false);

                if (options.Rest > 0)
                {
                    std::this_thread::sleep_for(std::chrono::milliseconds(options.Rest));
                }

                if (EscapePressed())
                {
                    keepGoing = false;
                    break;
                }
            }
        } while (options.Forever && keepGoing);

        return 0;
    }

    int RunEndpointRequestFunctionBlocksCommand(_In_ EndpointRequestFunctionBlocksOptions const& options)
    {
        if (!options.RequestInfo && !options.RequestName)
        {
            WriteErrorLine(ResourceString(IDS_ERROR_NO_REQUEST_FLAGS));
            return 1;
        }

        if (!options.RequestAll && (options.FunctionBlockNumber < 0 || options.FunctionBlockNumber > 31))
        {
            WriteErrorLine(ResourceString(IDS_ERROR_FUNCTION_BLOCK_NUMBER));
            return 1;
        }

        auto requests = midi2msg::MidiFunctionBlockDiscoveryRequests::None;

        if (options.RequestInfo)
        {
            requests |= midi2msg::MidiFunctionBlockDiscoveryRequests::RequestFunctionBlockInfo;
        }

        if (options.RequestName)
        {
            requests |= midi2msg::MidiFunctionBlockDiscoveryRequests::RequestFunctionBlockName;
        }

        auto const blockNumber = static_cast<uint8_t>(options.RequestAll ? 0xFF : options.FunctionBlockNumber);

        auto const message = midi2msg::MidiStreamMessageBuilder::BuildFunctionBlockDiscoveryMessage(
            midi2::MidiClock::TimestampConstantSendImmediately(), blockNumber, requests);

        return SendSingleStreamMessage(options.EndpointDeviceId, message);
    }

    int RunEndpointRequestEndpointInfoCommand(_In_ EndpointRequestEndpointInfoOptions const& options)
    {
        auto requests = midi2msg::MidiEndpointDiscoveryRequests::None;

        if (options.RequestAll)
        {
            requests =
                midi2msg::MidiEndpointDiscoveryRequests::RequestEndpointInfo |
                midi2msg::MidiEndpointDiscoveryRequests::RequestDeviceIdentity |
                midi2msg::MidiEndpointDiscoveryRequests::RequestEndpointName |
                midi2msg::MidiEndpointDiscoveryRequests::RequestProductInstanceId |
                midi2msg::MidiEndpointDiscoveryRequests::RequestStreamConfiguration;
        }
        else
        {
            if (options.RequestEndpointInfo)
            {
                requests |= midi2msg::MidiEndpointDiscoveryRequests::RequestEndpointInfo;
            }

            if (options.RequestDeviceIdentity)
            {
                requests |= midi2msg::MidiEndpointDiscoveryRequests::RequestDeviceIdentity;
            }

            if (options.RequestEndpointName)
            {
                requests |= midi2msg::MidiEndpointDiscoveryRequests::RequestEndpointName;
            }

            if (options.RequestProductInstanceId)
            {
                requests |= midi2msg::MidiEndpointDiscoveryRequests::RequestProductInstanceId;
            }

            if (options.RequestStreamConfiguration)
            {
                requests |= midi2msg::MidiEndpointDiscoveryRequests::RequestStreamConfiguration;
            }
        }

        if (requests == midi2msg::MidiEndpointDiscoveryRequests::None)
        {
            WriteErrorLine(ResourceString(IDS_ERROR_NO_REQUEST_FLAGS));
            return 1;
        }

        auto const message = midi2msg::MidiStreamMessageBuilder::BuildEndpointDiscoveryMessage(
            midi2::MidiClock::TimestampConstantSendImmediately(),
            static_cast<uint8_t>(options.UmpVersionMajor),
            static_cast<uint8_t>(options.UmpVersionMinor),
            requests);

        return SendSingleStreamMessage(options.EndpointDeviceId, message);
    }
}
