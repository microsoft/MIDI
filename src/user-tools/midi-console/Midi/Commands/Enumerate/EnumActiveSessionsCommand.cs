﻿// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

//using Microsoft.Windows.Devices.Midi2.Initialization;
using Microsoft.Windows.Devices.Midi2.Reporting;

namespace Microsoft.Midi.ConsoleApp
{
    internal class EnumActiveSessionsCommand : Command<EnumActiveSessionsCommand.Settings>
    {
        public sealed class Settings : CommandSettings
        {
            [LocalizedDescription("ParameterEnumSessionsIncludeAll")]
            [CommandOption("-a|--all")]
            [DefaultValue(false)]
            public bool All { get; set; }
        }

        public override int Execute(CommandContext context, Settings settings)
        {
            //if (!MidiServicesInitializer.EnsureServiceAvailable())
            //{
            //    AnsiConsole.MarkupLine(AnsiMarkupFormatter.FormatError("MIDI Service is not available."));
            //    return (int)MidiConsoleReturnCode.ErrorServiceNotAvailable;
            //}


            AnsiConsole.Status()
            .Start("Enumerating sessions...", ctx =>
            {
                ctx.Spinner(Spinner.Known.Star);

                var table = new Table();

                AnsiMarkupFormatter.SetTableBorderStyle(table);


                table.AddColumn(AnsiMarkupFormatter.FormatTableColumnHeading("Processes"));
                table.AddColumn(AnsiMarkupFormatter.FormatTableColumnHeading("Sessions"));


                var sessionList = MidiReporting.GetActiveSessions();

                uint reportedSessionCount = 0;

                if (sessionList.Count > 0)
                {
                    foreach (var session in sessionList)
                    {
                        // if session has no open connections, and we haven't specified to include all, then skip
                        if (!settings.All && session.Connections.Count == 0)
                        {
                            continue;
                        }

                        table.AddRow(
                            $"{AnsiMarkupFormatter.FormatProcessName(session.ProcessName)} [[{AnsiMarkupFormatter.FormatProcessId(session.ProcessId)}]]",
                            $"Session {AnsiMarkupFormatter.FormatSessionName(session.SessionName)} open since {AnsiMarkupFormatter.FormatLongDateTime(session.StartTime)}"
                            );

                        if (session.Connections.Count > 0)
                        {
                            reportedSessionCount++;

                            //table.AddEmptyRow();

                            string connectionCountMessage = string.Empty;

                            if (session.Connections.Count == 1)
                            {
                                connectionCountMessage = "Connected to 1 endpoint";
                            }
                            else
                            {
                                connectionCountMessage = $"Connected to {session.Connections.Count} endpoints";
                            }

                            //table.AddEmptyRow();
                            table.AddRow("", $"{connectionCountMessage}:");
                            table.AddEmptyRow();

                            foreach (var connection in session.Connections)
                            {
                                // TODO: Display the endpoint name, type, etc. here as well and use the standard emoji icon for the type
                                string connectionInfoString = string.Empty; // = "🔗";

                                if (connection.InstanceCount > 1)
                                {
                                    // highlight that the process has more than one connection to the same endpoint
                                    connectionInfoString += $" [paleturquoise1]x{connection.InstanceCount}[/] ";
                                }

                                var di = MidiEndpointDeviceInformation.CreateFromEndpointDeviceId(connection.EndpointDeviceId);

                                string epName;
                                string icon;

                                if (di != null)
                                {
                                    epName = di.Name;
                                    icon = AnsiMarkupFormatter.GetEndpointIcon(di.EndpointPurpose);
                                }
                                else
                                {
                                    epName = "Unknown";
                                    icon = AnsiMarkupFormatter.GetEndpointIcon(MidiEndpointDevicePurpose.NormalMessageEndpoint);
                                }

                                if (epName != string.Empty)
                                {
                                    table.AddRow("", $"{icon} {AnsiMarkupFormatter.FormatEndpointName(epName)}");

                                }

                                table.AddRow("", $"{connectionInfoString} {AnsiMarkupFormatter.FormatFullEndpointInterfaceId(connection.EndpointDeviceId)}");
                                table.AddEmptyRow();
                            }

                        }
                        else
                        {
                            table.AddRow("", "No open endpoint connections");
                        }

                        table.AddEmptyRow();
                    }

                    if (reportedSessionCount == 0)
                    {
                        table.AddRow("", "No active sessions with open connections (use --all to see all sessions)");
                    }



                }
                else
                {
                    table.AddRow("No active sessions.", "");
                }

                AnsiConsole.Write(table);
            });

            return (int)MidiConsoleReturnCode.Success;
        }

    }
}
