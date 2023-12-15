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

            // gap in milliseconds before restarting offset calculation
            [LocalizedDescription("TODO ParameterMonitorEndpointGap")]
            [CommandOption("-g|--gap")]
            [DefaultValue(1000)]
            public int Gap { get; set; }


            [LocalizedDescription("ParameterMonitorEndpointVerbose")]
            [CommandOption("-v|--verbose")]
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
                UInt64 lastTimestamp = 0;

                MidiMessageStruct msg;

                UInt32 index = 0;

                bool continueWaiting = true;

                connection.MessageReceived += (s, e) =>
                {
                //    AnsiConsole.WriteLine("Message received.");

                    // helps prevent any race conditions with main loop and its output
                    if (!continueWaiting) return;

                    if (startTimestamp == 0)
                    {
                        // gets timestamp of first message we receive and uses that so all others are an offset
                        startTimestamp = e.Timestamp;    
                    }

                    if (lastTimestamp == 0)
                    {
                        // gets timestamp of first message we receive and uses that so all others are an offset
                        lastTimestamp = e.Timestamp;
                    }

                    if (index == 0)
                    {
                        AnsiConsoleOutput.DisplayMidiMessageTableHeaders();
                    }

                    //Console.WriteLine("DEBUG: MessageReceived");
                    index++;

                    var numWords = e.FillMessageStruct(out msg);

                    double offsetMicroseconds = 0.0;

                    if (settings.SingleMessage)
                    {
                        continueWaiting = false;
                    }
                    else
                    {
                        // calculate offset from the last message received
                        //offsetMilliseconds = MidiClock.ConvertTimestampToMilliseconds(e.Timestamp - startTimestamp);
                        offsetMicroseconds = MidiClock.ConvertTimestampToMicroseconds(e.Timestamp - lastTimestamp);
                    }

                    AnsiConsoleOutput.DisplayMidiMessageTableRow(msg, numWords, offsetMicroseconds, e.Timestamp, index);

                    lastTimestamp = e.Timestamp;
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
                                continueWaiting = false;

                                // leading space is because the "E" in "Escape" is often lost in the output for some reason.
                                AnsiConsole.MarkupLine("-" + Strings.MonitorEscapePressedMessage);
                                break;
                            }
                        }

                        Thread.Sleep(0);
                    }
                }
                else
                {
                    AnsiConsole.MarkupLine(AnsiMarkupFormatter.FormatError(Strings.ErrorUnableToOpenEndpoint));
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
                        if (connection.Open())
                        {

                            while (continueWaiting)
                            {
                                if (Console.KeyAvailable)
                                {
                                    var keyInfo = Console.ReadKey(false);
                                    if (keyInfo.Key == ConsoleKey.Escape)
                                    {
                                        AnsiConsole.MarkupLine(Strings.MonitorEscapePressedMessage);
                                        continueWaiting = false;
                                        break;
                                    }
                                }

                                Thread.Sleep(0);
                            }
                        }
                        else
                        {
                            AnsiConsole.MarkupLine(AnsiMarkupFormatter.FormatError(Strings.ErrorUnableToOpenEndpoint));
                            //return (int)MidiConsoleReturnCode.ErrorOpeningEndpointConnection;
                        }
                    });


            }

            return 0;
        }

    }
}
