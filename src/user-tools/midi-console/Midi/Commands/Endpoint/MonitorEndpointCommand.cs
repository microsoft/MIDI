using Spectre.Console;
using Spectre.Console.Cli;
using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

using Windows.Devices.Midi2;
using WinRT;

using Microsoft.Devices.Midi2.ConsoleApp.Resources;

namespace Microsoft.Devices.Midi2.ConsoleApp
{
    // commands to check the health of Windows MIDI Services on this PC
    // will check for MIDI services
    // will attempt a loopback test


    internal class MonitorEndpointCommand : Command<MonitorEndpointCommand.Settings>
    {
        public sealed class Settings : MessageListenerCommandSettings
        {
            [LocalizedDescription("ParameterMonitorEndpointDirectionDescription")]
            [CommandOption("-d|--direction")]
            [DefaultValue(EndpointDirectionInputs.Bidirectional)]
            public EndpointDirectionInputs EndpointDirection { get; set; }


            [LocalizedDescription("ParameterMonitorEndpointSingleMessage")]
            [CommandOption("-s|--single-message")]
            [DefaultValue(false)]
            public bool SingleMessage { get; set; }
        }

        public override int Execute(CommandContext context, Settings settings)
        {
            MidiSession session = null;
            IMidiEndpointConnection connection = null;


            string endpointId = string.Empty;

            if (!string.IsNullOrEmpty(settings.InstanceId))
            {
                endpointId = settings.InstanceId.Trim().ToLower();
            }
            else
            {
                endpointId = UmpEndpointPicker.PickInput();
            }


            AnsiConsole.MarkupLine(Strings.MonitorMonitoringOnEndpointLabel + ": " + AnsiMarkupFormatter.FormatDeviceInstanceId(endpointId));
            AnsiConsole.MarkupLine(Strings.MonitorPressEscapeToStopMonitoringMessage);
            AnsiConsole.WriteLine();

            var table = new Table();



            AnsiConsole.Status()
                .Start(Strings.StatusCreatingSessionAndOpeningEndpoint, ctx =>
                {
                    ctx.Spinner(Spinner.Known.Star);

                    session = MidiSession.CreateSession($"{Strings.AppShortName} - {Strings.MonitorSessionNameSuffix}");

                    if (session != null)
                    {
                        if (settings.EndpointDirection == EndpointDirectionInputs.Bidirectional)
                        {
                            connection = session.ConnectBidirectionalEndpoint(endpointId);
                        }
                        else if (settings.EndpointDirection == EndpointDirectionInputs.In)
                        {
                            connection = session.ConnectInputEndpoint(endpointId);
                        }
                    }
                });

            if (session == null)
            {
                AnsiConsole.WriteLine(Strings.ErrorUnableToCreateSession);
                return (int)MidiConsoleReturnCode.ErrorCreatingSession;
            }
            else if (connection == null)
            {
                AnsiConsole.WriteLine(Strings.ErrorUnableToOpenEndpoint);
                return (int)MidiConsoleReturnCode.ErrorOpeningEndpointConnection;
            }




            // start waiting for messages

            AnsiConsole.Live(table)
                .Start(ctx =>
                {
                    bool firstMessageReceived = false;

                    // set up the event handler
                    IMidiMessageReceivedEventSource eventSource = (IMidiMessageReceivedEventSource)connection;

                    bool continueWaiting = true;

                    eventSource.MessageReceived += (s, e) =>
                    {
                        // TODO: Localize these

                        if (!firstMessageReceived)
                        {
                            table.AddColumn(Strings.TableColumnHeaderCommonTimestamp);
                            table.AddColumn(Strings.MonitorEndpointResultTableColumnHeaderWordsReceived);
                            table.AddColumn(Strings.TableColumnHeaderCommonMessageType);

                            firstMessageReceived = true;
                        }

                        DisplayUmp(e.GetUmp(), table);

                        ctx.Refresh();

                        if (settings.SingleMessage)
                        {
                            continueWaiting = false;
                        }
                    };

                    // open the connection
                    connection.Open();

                    while (continueWaiting)
                    {
                        if (Console.KeyAvailable)
                        {
                            var keyInfo = Console.ReadKey(false);
                            if (keyInfo.Key == ConsoleKey.Escape)
                            {
                                if (!firstMessageReceived)
                                {
                                    // TODO: we should erase the table, otherwise it leaves artifacts.
                                    // may need to rethink how table is created
                                }

                                AnsiConsole.MarkupLine(Strings.MonitorEscapedPressedMessage);
                                continueWaiting = false;
                                break;
                            }
                        }
                        
                        Thread.Sleep(50);

                        // todo: code to allow pressing escape to stop listening and gracefully shut down


                    }

                });

            return 0;
        }

        private void DisplayUmp(IMidiUmp ump, Table table) 
        {
            string data = string.Empty;

            if (ump.UmpPacketType == MidiUmpPacketType.Ump128)
            {
                var ump128 = ump.As<MidiUmp128>();

                data = AnsiMarkupFormatter.FormatMidiWords(ump128.Word0, ump128.Word1, ump128.Word2, ump128.Word3);
            }
            else if (ump.UmpPacketType == MidiUmpPacketType.Ump96)
            {
                var ump96 = ump.As<MidiUmp96>();

                data = AnsiMarkupFormatter.FormatMidiWords(ump96.Word0, ump96.Word1, ump96.Word2);
            }
            else if (ump.UmpPacketType == MidiUmpPacketType.Ump64)
            {
                var ump64 = ump.As<MidiUmp64>();

                data = AnsiMarkupFormatter.FormatMidiWords(ump64.Word0, ump64.Word1);
            }
            else if (ump.UmpPacketType == MidiUmpPacketType.Ump32)
            {
                var ump32 = ump.As<MidiUmp32>();

                data = string.Format("{0:X8}", ump32.Word0);
                data = AnsiMarkupFormatter.FormatMidiWords(ump32.Word0);
            }

            table.AddRow(
                new Markup(AnsiMarkupFormatter.FormatTimestamp(ump.Timestamp)), 
                new Markup(data),
                new Markup(AnsiMarkupFormatter.FormatMessageType(ump.MessageType))
                );

        }


    }
}
