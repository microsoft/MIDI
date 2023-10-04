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
using Windows.ApplicationModel.UserDataTasks;
using System.Diagnostics.Eventing.Reader;

namespace Microsoft.Devices.Midi2.ConsoleApp
{

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


            [LocalizedDescription("ParameterMonitorEndpointVerbose")]
            [CommandOption("-v|--verbose|--details")]
            [DefaultValue(false)]
            public bool Verbose { get; set; }
        }


        public override Spectre.Console.ValidationResult Validate(CommandContext context, Settings settings)
        {
            // TODO: Validate endpoint 

            return ValidationResult.Success();
        }


        public override int Execute(CommandContext context, Settings settings)
        {
            IMidiEndpointConnection? connection = null;


            string endpointId = string.Empty;

            if (!string.IsNullOrEmpty(settings.EndpointDeviceId))
            {
                endpointId = settings.EndpointDeviceId.Trim().ToLower();
            }
            else
            {
                endpointId = UmpEndpointPicker.PickInput();
            }


            AnsiConsole.MarkupLine(Strings.MonitorMonitoringOnEndpointLabel + ": " + AnsiMarkupFormatter.FormatDeviceInstanceId(endpointId));
            AnsiConsole.MarkupLine(Strings.MonitorPressEscapeToStopMonitoringMessage);
            AnsiConsole.WriteLine();

            var table = new Table();

            // when this goes out of scope, it will dispose of the session, which closes the connections
            using var session = MidiSession.CreateSession($"{Strings.AppShortName} - {Strings.MonitorSessionNameSuffix}");

            AnsiConsole.Status()
                .Start(Strings.StatusCreatingSessionAndOpeningEndpoint, ctx =>
                {
                    ctx.Spinner(Spinner.Known.Star);


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


            if (settings.Verbose)
            {
                // start waiting for messages

                AnsiConsole.Live(table)
                    .Start(ctx =>
                    {
                        UInt32 index = 0;

                        bool firstMessageReceived = false;

                        // set up the event handler
                        IMidiMessageReceivedEventSource eventSource = (IMidiMessageReceivedEventSource)connection;

                        bool continueWaiting = true;

                        eventSource.MessageReceived += (s, e) =>
                        {
                            index++;

                            if (!firstMessageReceived)
                            {
                                table.AddColumn(Strings.CommonTableHeaderIndex);
                                table.AddColumn(Strings.TableColumnHeaderCommonTimestamp);
                                table.AddColumn(Strings.MonitorEndpointResultTableColumnHeaderWordsReceived);
                                table.AddColumn(Strings.TableColumnHeaderCommonMessageType);
                                table.AddColumn(Strings.TableColumnHeaderCommonDetailedMessageType);

                                firstMessageReceived = true;
                            }

                            DisplayUmp(index, e.GetMessagePacket(), table);

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

                            Thread.Sleep(0);

                            // todo: code to allow pressing escape to stop listening and gracefully shut down


                        }

                    });


            }
            else
            {
                // show count only

                // start waiting for messages

                // TODO: May want to create some different column types for counts by message type etc.

                AnsiConsole.Status()
                    .Spinner(Spinner.Known.Arc)
                    /*.Spinner(Spinner.Known.Grenade) */
                    .Start("Waiting for messages...", ctx =>
                    {
                        UInt32 index = 0;

                        // set up the event handler
                        IMidiMessageReceivedEventSource eventSource = (IMidiMessageReceivedEventSource)connection;

                        bool continueWaiting = true;

                        eventSource.MessageReceived += (s, e) =>
                        {
                            index++;

                            if (index > 1)
                                ctx.Status($"Received {index} messages");
                            else
                                ctx.Status($"Received 1 message");

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
                                    AnsiConsole.MarkupLine(Strings.MonitorEscapedPressedMessage);
                                    continueWaiting = false;
                                    break;
                                }
                            }

                            Thread.Sleep(0);
                        }

                    });


            }

            return 0;
        }

        private void DisplayUmp(UInt32 index, IMidiUniversalPacket ump, Table table) 
        {
            string data = string.Empty;

            if (ump.PacketType == MidiPacketType.UniversalMidiPacket128)
            {
                var ump128 = ump.As<MidiMessage128>();

                data = AnsiMarkupFormatter.FormatMidiWords(ump128.Word0, ump128.Word1, ump128.Word2, ump128.Word3);
            }
            else if (ump.PacketType == MidiPacketType.UniversalMidiPacket96)
            {
                var ump96 = ump.As<MidiMessage96>();

                data = AnsiMarkupFormatter.FormatMidiWords(ump96.Word0, ump96.Word1, ump96.Word2);
            }
            else if (ump.PacketType == MidiPacketType.UniversalMidiPacket64)
            {
                var ump64 = ump.As<MidiMessage64>();

                data = AnsiMarkupFormatter.FormatMidiWords(ump64.Word0, ump64.Word1);
            }
            else if (ump.PacketType == MidiPacketType.UniversalMidiPacket32)
            {
                var ump32 = ump.As<MidiMessage32>();

                data = string.Format("{0:X8}", ump32.Word0);
                data = AnsiMarkupFormatter.FormatMidiWords(ump32.Word0);
            }

            string detailedMessageType = MidiMessageUtility.GetMessageFriendlyNameFromFirstWord(ump.PeekFirstWord());

            table.AddRow(
                new Markup(AnsiMarkupFormatter.FormatRowIndex(index)),
                new Markup(AnsiMarkupFormatter.FormatTimestamp(ump.Timestamp)), 
                new Markup(data),
                new Markup(AnsiMarkupFormatter.FormatMessageType(ump.MessageType)), 
                new Markup(AnsiMarkupFormatter.FormatDetailedMessageType(detailedMessageType))
                );

        }


    }
}
