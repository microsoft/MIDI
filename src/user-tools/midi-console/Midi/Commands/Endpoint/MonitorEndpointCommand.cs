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
            string endpointId = string.Empty;

            if (!string.IsNullOrEmpty(settings.EndpointDeviceId))
            {
                endpointId = settings.EndpointDeviceId.Trim().ToLower();
            }
            else
            {
                endpointId = UmpEndpointPicker.PickEndpoint();
            }


            AnsiConsole.MarkupLine(Strings.MonitorMonitoringOnEndpointLabel + ": " + AnsiMarkupFormatter.FormatDeviceInstanceId(endpointId));
            AnsiConsole.MarkupLine(Strings.MonitorPressEscapeToStopMonitoringMessage);
            AnsiConsole.WriteLine();

            var table = new Table();

            // when this goes out of scope, it will dispose of the session, which closes the connections
            using var session = MidiSession.CreateSession($"{Strings.AppShortName} - {Strings.MonitorSessionNameSuffix}");
            if (session == null)
            {
                AnsiConsole.WriteLine(Strings.ErrorUnableToCreateSession);
                return (int)MidiConsoleReturnCode.ErrorCreatingSession;
            }

            using var connection = session.CreateEndpointConnection(endpointId);
            if (connection == null)
            {
                AnsiConsole.WriteLine(Strings.ErrorUnableToCreateEndpointConnection);
                return (int)MidiConsoleReturnCode.ErrorCreatingEndpointConnection;
            }


            if (settings.Verbose)
            {
                UInt64 startTimestamp = 0;

                MidiMessageStruct msg;

                UInt32 index = 0;

                //bool firstMessageReceived = false;

                bool continueWaiting = true;

                connection.MessageReceived += (s, e) =>
                {
                    if (startTimestamp == 0)
                    {
                        // gets timestamp of first message we receive and uses that so all others are an offset
                        startTimestamp = e.Timestamp;    
                    }

                    //Console.WriteLine("DEBUG: MessageReceived");
                    index++;

                    var numWords = e.FillMessageStruct(out msg);

                    //AnsiConsoleOutput.DisplayMidiMessage(msg, numWords, e.Timestamp - startTimestamp, index);
                    AnsiConsoleOutput.DisplayMidiMessage(msg, numWords, e.Timestamp, index);

                    if (settings.SingleMessage)
                    {
                        continueWaiting = false;
                    }
                };


                // open the connection
                if (connection.Open())
                {
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
                }
                else
                {
                    AnsiConsole.WriteLine(Strings.ErrorUnableToOpenEndpoint);
                    return (int)MidiConsoleReturnCode.ErrorOpeningEndpointConnection;
                }

                session.DisconnectEndpointConnection(connection.ConnectionId);
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

        //private void DisplayUmp(UInt32 index, MidiMessageStruct ump, byte numWords, MidiMessageType messageType, UInt64 timestamp, Table table) 
        //{
        //    string data = string.Empty;

        //    if (numWords == 4)
        //    {
        //        data = AnsiMarkupFormatter.FormatMidiWords(ump.Word0, ump.Word1, ump.Word2, ump.Word3);
        //    }
        //    else if (numWords == 3)
        //    {
        //        data = AnsiMarkupFormatter.FormatMidiWords(ump.Word0, ump.Word1, ump.Word2);
        //    }
        //    else if (numWords == 2)
        //    {
        //        data = AnsiMarkupFormatter.FormatMidiWords(ump.Word0, ump.Word1);
        //    }
        //    else if (numWords == 1)
        //    {
        //        data = AnsiMarkupFormatter.FormatMidiWords(ump.Word0);
        //    }

        //    string detailedMessageType = MidiMessageUtility.GetMessageFriendlyNameFromFirstWord(ump.Word0);



        //    table.AddRow(
        //        new Markup(AnsiMarkupFormatter.FormatRowIndex(index)),
        //        new Markup(AnsiMarkupFormatter.FormatTimestamp(timestamp)), 
        //        new Markup(data),
        //        new Markup(AnsiMarkupFormatter.FormatMessageType(messageType)), 
        //        new Markup(AnsiMarkupFormatter.FormatDetailedMessageType(detailedMessageType))
        //        );

        //}


    }
}
