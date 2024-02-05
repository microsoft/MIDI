using Spectre.Console;
using Spectre.Console.Cli;
using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.Linq;
using System.Net;
using System.Text;
using System.Threading.Tasks;

using Windows.Devices.Midi2;

namespace Microsoft.Devices.Midi2.ConsoleApp
{
    internal class EnumActiveSessionsCommand : Command<EnumActiveSessionsCommand.Settings>
    {
        public sealed class Settings : CommandSettings
        {
        }

        public override int Execute(CommandContext context, Settings settings)
        {
            AnsiConsole.Status()
            .Start("Enumerating endpoints...", ctx =>
            {
                ctx.Spinner(Spinner.Known.Star);

                var table = new Table();

                AnsiMarkupFormatter.SetTableBorderStyle(table);


                table.AddColumn(AnsiMarkupFormatter.FormatTableColumnHeading("Processes"));
                table.AddColumn(AnsiMarkupFormatter.FormatTableColumnHeading("Sessions"));


                var sessionList = MidiService.GetActiveSessions();


                if (sessionList.Count > 0)
                {
                    foreach (var session in sessionList)
                    {
                        table.AddRow(
                            $"{AnsiMarkupFormatter.FormatProcessName(session.ProcessName)} [[{AnsiMarkupFormatter.FormatProcessId(session.ProcessId)}]]",
                            $"Session {AnsiMarkupFormatter.FormatSessionName(session.SessionName)} open since {AnsiMarkupFormatter.FormatLongDateTime(session.StartTime)}"
                            );

                        if (session.Connections.Count > 0)
                        {
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

                                var di = MidiEndpointDeviceInformation.CreateFromId(connection.EndpointDeviceId);

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
                            table.AddRow("", "No open endpoint connections.");
                        }

                        table.AddEmptyRow();
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
