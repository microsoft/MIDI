// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

#include "pch.h"

#include <algorithm>

#include "cmd_enumerate.h"
#include "console_output.h"
#include "console_table.h"
#include "endpoint_utility.h"
#include "midi_formatting.h"
#include "strings.h"

namespace midi2console
{
    namespace
    {
        void WriteFunctionBlocks(_In_ midi2enum::MidiEndpointDeviceInformation const& endpoint)
        {
            auto const blocks = endpoint.GetDeclaredFunctionBlocks();

            if (blocks == nullptr || blocks.Size() == 0)
            {
                return;
            }

            ConsoleTable table{ ResourceString(IDS_ENUM_FUNCTION_BLOCKS_TITLE) };

            table.AddColumn(ResourceString(IDS_LABEL_NUMBER), ColumnAlignment::Right);
            table.AddColumn(ResourceString(IDS_LABEL_NAME));
            table.SetLastColumnShrinkable();
            table.AddColumn(ResourceString(IDS_LABEL_DIRECTION));
            table.AddColumn(ResourceString(IDS_LABEL_GROUPS));
            table.AddColumn(ResourceString(IDS_LABEL_ACTIVE));

            for (auto const& block : blocks)
            {
                table.BeginRow();
                table.AddCell(fmt::format("{}", static_cast<int>(block.Number())), numberTextStyle);
                table.AddCell(ToUtf8(block.Name()), endpointNameTextStyle);
                table.AddCell(FormatFunctionBlockDirection(block.Direction()));
                table.AddCell(FormatGroupSpan(block.FirstGroup().Index(), block.GroupCount()));
                table.AddCell(FormatBoolean(block.IsActive()), BooleanStyle(block.IsActive()));
            }

            table.Render();
        }

        void WriteGroupTerminalBlocks(_In_ midi2enum::MidiEndpointDeviceInformation const& endpoint)
        {
            auto const blocks = endpoint.GetGroupTerminalBlocks();

            if (blocks == nullptr || blocks.Size() == 0)
            {
                return;
            }

            ConsoleTable table{ ResourceString(IDS_ENUM_GROUP_TERMINAL_BLOCKS_TITLE) };

            table.AddColumn(ResourceString(IDS_LABEL_NUMBER), ColumnAlignment::Right);
            table.AddColumn(ResourceString(IDS_LABEL_NAME));
            table.SetLastColumnShrinkable();
            table.AddColumn(ResourceString(IDS_LABEL_DIRECTION));
            table.AddColumn(ResourceString(IDS_LABEL_GROUPS));

            for (auto const& block : blocks)
            {
                table.BeginRow();
                table.AddCell(fmt::format("{}", static_cast<int>(block.Number())), numberTextStyle);
                table.AddCell(ToUtf8(block.Name()), endpointNameTextStyle);
                table.AddCell(FormatGroupTerminalBlockDirection(block.Direction()));
                table.AddCell(FormatGroupSpan(block.FirstGroup().Index(), block.GroupCount()));
            }

            table.Render();
        }
    }

    int RunEnumEndpointsCommand(_In_ EnumEndpointsOptions const& options)
    {
        auto const filters = BuildEndpointFilters(options.IncludeDiagnosticLoopback, options.IncludeAll);

        auto const endpoints = EnumerateEndpoints(filters);

        if (endpoints == nullptr || endpoints.Size() == 0)
        {
            WriteWarningLine(ResourceString(IDS_ERROR_NO_ENDPOINTS_FOUND));
            return 0;
        }

        ConsoleTable table{ ResourceString(IDS_ENUM_ENDPOINTS_TABLE_TITLE) };

        table.AddColumn(ResourceString(IDS_LABEL_NAME), ColumnAlignment::Left, endpointNameTextStyle);
        table.SetLastColumnShrinkable();
        table.AddColumn(ResourceString(IDS_LABEL_TRANSPORT), ColumnAlignment::Left, transportCodeTextStyle);
        table.AddColumn(ResourceString(IDS_LABEL_MIDI2_PROTOCOL));
        table.AddColumn(ResourceString(IDS_LABEL_MANUFACTURER), ColumnAlignment::Left, manufacturerTextStyle);
        table.SetLastColumnShrinkable();

        for (auto const& endpoint : endpoints)
        {
            auto const transportInfo = endpoint.GetTransportSuppliedInfo();
            auto const declaredInfo = endpoint.GetDeclaredEndpointInfo();

            auto const supportsMidi2 = declaredInfo != nullptr && declaredInfo.SupportsMidi20Protocol();

            table.BeginRow();
            table.AddCell(GetEndpointIcon(endpoint) + " " + ToUtf8(endpoint.Name()));
            table.AddCell(ToUtf8(transportInfo.TransportCode()));
            table.AddCell(supportsMidi2 ? ResourceString(IDS_LABEL_YES) : std::string{},
                BooleanStyle(supportsMidi2));

            auto const manufacturer = ToUtf8(transportInfo.ManufacturerName());

            table.AddCell(manufacturer == "Microsoft" ? std::string{} : manufacturer);
        }

        table.Render();

        WriteLine(fmt::format("{}",
            Styled(FormatResourceString(IDS_ENUM_ENDPOINTS_COUNT, endpoints.Size()), infoTextStyle)));

        if (options.ShowEndpointId || options.Verbose)
        {
            for (auto const& endpoint : endpoints)
            {
                WriteBlankLine();
                WriteLine(fmt::format("{} {}",
                    Styled(GetEndpointIcon(endpoint), normalTextStyle),
                    Styled(ToUtf8(endpoint.Name()), endpointNameTextStyle)));

                WriteField(ResourceString(IDS_LABEL_ID), ToUtf8(endpoint.EndpointDeviceId()), endpointIdTextStyle);

                if (options.Verbose)
                {
                    auto const transportInfo = endpoint.GetTransportSuppliedInfo();
                    auto const userInfo = endpoint.GetUserSuppliedInfo();

                    auto const transportDescription = ToUtf8(transportInfo.Description());

                    if (!transportDescription.empty())
                    {
                        WriteField(ResourceString(IDS_EP_PROP_TRANSPORT_DESCRIPTION), transportDescription);
                    }

                    if (userInfo != nullptr)
                    {
                        auto const userDescription = ToUtf8(userInfo.Description());

                        if (!userDescription.empty())
                        {
                            WriteField(ResourceString(IDS_EP_PROP_USER_DESCRIPTION), userDescription);
                        }
                    }

                    auto const blocks = endpoint.GetDeclaredFunctionBlocks();

                    if (blocks != nullptr && blocks.Size() > 0)
                    {
                        WriteFunctionBlocks(endpoint);
                    }
                    else
                    {
                        WriteGroupTerminalBlocks(endpoint);
                    }
                }
            }
        }

        return 0;
    }

    int RunEnumLegacyCommand(_In_ EnumLegacyOptions const& options)
    {
        std::vector<midi2legacy::MidiLegacyPortDeviceInformation> ports;

        auto const appendAll = [&ports](collections::IVectorView<midi2legacy::MidiLegacyPortDeviceInformation> const& found)
        {
            if (found == nullptr)
            {
                return;
            }

            for (auto const& port : found)
            {
                // Only ports midisrv owns. This deliberately drops what the older WinRT MIDI 1.0
                // enumerator also returns - the GS Wavetable Synth and its own Bluetooth ports -
                // which have no WinMM port number here. Use midi1enum for the full WinMM view.
                if (port.AssociatedEndpointDeviceId().empty())
                {
                    continue;
                }

                ports.push_back(port);
            }
        };

        if (EqualsIgnoreCase(options.Direction, "source") || EqualsIgnoreCase(options.Direction, "input"))
        {
            appendAll(midi2legacy::MidiLegacyPortDeviceInformation::FindAll(
                midi2enum::Midi1PortFlow::MidiMessageSource));
        }
        else if (EqualsIgnoreCase(options.Direction, "destination") || EqualsIgnoreCase(options.Direction, "output"))
        {
            appendAll(midi2legacy::MidiLegacyPortDeviceInformation::FindAll(
                midi2enum::Midi1PortFlow::MidiMessageDestination));
        }
        else
        {
            appendAll(midi2legacy::MidiLegacyPortDeviceInformation::FindAll(
                midi2enum::Midi1PortFlow::MidiMessageSource));
            appendAll(midi2legacy::MidiLegacyPortDeviceInformation::FindAll(
                midi2enum::Midi1PortFlow::MidiMessageDestination));
        }

        if (ports.empty())
        {
            WriteWarningLine(ResourceString(IDS_ERROR_NO_ENDPOINTS_FOUND));
            return 0;
        }

        // Match the shipping console: grouped by direction, then port number.
        std::stable_sort(ports.begin(), ports.end(),
            [](auto const& left, auto const& right)
            {
                if (left.Flow() != right.Flow())
                {
                    return static_cast<int>(left.Flow()) < static_cast<int>(right.Flow());
                }

                return left.Number() < right.Number();
            });

        ConsoleTable table{ ResourceString(IDS_ENUM_LEGACY_TABLE_TITLE) };

        table.AddColumn(ResourceString(IDS_LABEL_PORT_NUMBER), ColumnAlignment::Right, portNumberTextStyle);
        table.AddColumn(ResourceString(IDS_LABEL_PORT_NAME), ColumnAlignment::Left, endpointNameTextStyle);
        table.SetLastColumnShrinkable();
        table.AddColumn(ResourceString(IDS_LABEL_DIRECTION));

        for (auto const& port : ports)
        {
            table.BeginRow();
            table.AddCell(fmt::format("{}", port.Number()));
            table.AddCell(ToUtf8(port.Name()));
            table.AddCell(FormatPortFlow(port.Flow()));

            if (options.IncludePortId)
            {
                table.AddRowDetail(ToUtf8(port.PortDeviceId()), endpointIdTextStyle);
            }
        }

        table.Render();

        WriteBlankLine();
        WriteLine(fmt::format("{}", Styled(ResourceString(IDS_ENUM_LEGACY_FOOTNOTE), separatorTextStyle)));

        return 0;
    }

    int RunEnumSessionsCommand(_In_ EnumSessionsOptions const& options)
    {
        auto const sessions = midi2report::MidiReporting::GetActiveSessions();

        if (sessions == nullptr || sessions.Size() == 0)
        {
            WriteWarningLine(ResourceString(IDS_ENUM_SESSIONS_NONE));
            return 0;
        }

        WriteLine(fmt::format("{}",
            Styled(ResourceString(IDS_ENUM_SESSIONS_TABLE_TITLE), tableTitleTextStyle)));

        size_t shown{ 0 };

        for (auto const& session : sessions)
        {
            auto const connections = session.Connections();
            auto const connectionCount = connections == nullptr ? 0u : connections.Size();

            if (!options.All && connectionCount == 0)
            {
                continue;
            }

            shown++;

            WriteBlankLine();

            WriteLine(fmt::format("{} {}",
                Styled(ToUtf8(session.ProcessName()), processNameTextStyle),
                Styled(fmt::format("[{}]", session.ProcessId()), numberTextStyle)));

            WriteField(ResourceString(IDS_LABEL_NAME), ToUtf8(session.SessionName()));
            WriteField(ResourceString(IDS_ENUM_SESSIONS_STARTED), FormatDateTime(session.StartTime()));

            if (options.Verbose)
            {
                WriteField(ResourceString(IDS_LABEL_ID), FormatGuid(session.SessionId()), guidTextStyle);
            }

            WriteLine(fmt::format("  {}",
                Styled(FormatResourceString(IDS_ENUM_SESSIONS_CONNECTION_COUNT, connectionCount), infoTextStyle)));

            if (connections != nullptr)
            {
                for (auto const& connection : connections)
                {
                    auto const deviceId = ToUtf8(connection.EndpointOrPortDeviceId());
                    auto const name = GetEndpointNameFromEndpointDeviceId(deviceId);

                    WriteLine(fmt::format("    {} {}",
                        Styled(name.empty() ? deviceId : name, endpointNameTextStyle),
                        Styled(fmt::format("x{}", connection.InstanceCount()), numberTextStyle)));

                    if (options.Verbose)
                    {
                        WriteLine(fmt::format("      {}", Styled(deviceId, endpointIdTextStyle)));
                    }
                }
            }
        }

        if (shown == 0)
        {
            WriteWarningLine(ResourceString(IDS_ENUM_SESSIONS_NONE));
        }

        return 0;
    }

    int RunEnumTransportsCommand(_In_ EnumTransportsOptions const& options)
    {
        auto const transports = midi2report::MidiReporting::GetInstalledTransportPlugins();

        if (transports == nullptr || transports.Size() == 0)
        {
            WriteWarningLine(ResourceString(IDS_LABEL_NONE));
            return 0;
        }

        ConsoleTable table{ ResourceString(IDS_ENUM_TRANSPORTS_TABLE_TITLE) };

        table.AddColumn(ResourceString(IDS_LABEL_TRANSPORT), ColumnAlignment::Left, transportCodeTextStyle);
        table.AddColumn(ResourceString(IDS_LABEL_NAME), ColumnAlignment::Left, endpointNameTextStyle);
        table.SetLastColumnShrinkable();
        table.AddColumn(ResourceString(IDS_LABEL_VERSION));
        table.AddColumn(ResourceString(IDS_LABEL_AUTHOR));
        table.SetLastColumnShrinkable();

        for (auto const& transport : transports)
        {
            table.BeginRow();
            table.AddCell(ToUtf8(transport.TransportCode()));
            table.AddCell(ToUtf8(transport.Name()));
            table.AddCell(ToUtf8(transport.Version()));
            table.AddCell(ToUtf8(transport.Author()));
        }

        table.Render();

        for (auto const& transport : transports)
        {
            WriteBlankLine();

            WriteLine(fmt::format("{} {}",
                Styled(ToUtf8(transport.TransportCode()), transportCodeTextStyle),
                Styled(ToUtf8(transport.Name()), endpointNameTextStyle)));

            auto const description = ToUtf8(transport.Description());

            if (!description.empty())
            {
                WriteField(ResourceString(IDS_LABEL_DESCRIPTION), description);
            }

            if (options.Verbose)
            {
                WriteField(ResourceString(IDS_LABEL_ID), FormatGuid(transport.TransportId()), guidTextStyle);
                WriteField(ResourceString(IDS_LABEL_IMAGE_FILE_NAME), ToUtf8(transport.ImageFileName()));
                WriteField(ResourceString(IDS_ENUM_TRANSPORT_IS_API_CREATABLE),
                    FormatBoolean(transport.IsRuntimeCreatableByApps()), BooleanStyle(transport.IsRuntimeCreatableByApps()));
                WriteField(ResourceString(IDS_ENUM_TRANSPORT_IS_CONFIG_CREATABLE),
                    FormatBoolean(transport.IsRuntimeCreatableBySettings()), BooleanStyle(transport.IsRuntimeCreatableBySettings()));
                WriteField(ResourceString(IDS_ENUM_TRANSPORT_IS_SYSTEM_MANAGED),
                    FormatBoolean(transport.IsSystemManaged()), BooleanStyle(transport.IsSystemManaged()));
            }
        }

        return 0;
    }

    int RunEnumPropertyKeysCommand()
    {
        auto const properties = midi2enum::MidiEndpointDevicePropertyHelper::GetAllMidiProperties();

        if (properties == nullptr || properties.Size() == 0)
        {
            WriteWarningLine(ResourceString(IDS_LABEL_NONE));
            return 0;
        }

        ConsoleTable table{ ResourceString(IDS_ENUM_PROPERTY_KEYS_TABLE_TITLE) };

        table.AddColumn(ResourceString(IDS_LABEL_PROPERTY_KEY), ColumnAlignment::Left, propertyKeyTextStyle);
        table.SetLastColumnShrinkable();
        table.AddColumn(ResourceString(IDS_LABEL_NAME));
        table.SetLastColumnShrinkable();

        std::vector<std::pair<std::string, std::string>> entries;

        for (auto const& entry : properties)
        {
            entries.emplace_back(ToUtf8(entry.Key()), ToUtf8(entry.Value()));
        }

        std::sort(entries.begin(), entries.end(),
            [](auto const& left, auto const& right) { return left.second < right.second; });

        for (auto const& [key, name] : entries)
        {
            table.BeginRow();
            table.AddCell(key);
            table.AddCell(name);
        }

        table.Render();

        return 0;
    }
}
