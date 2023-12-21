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
using System.Reflection;

namespace Microsoft.Devices.Midi2.ConsoleApp
{
    public struct ReceivedMidiMessage
    {
        public UInt32 Index;
        public UInt64 ReceivedTimestamp;
        public UInt64 MessageTimestamp;
        public byte NumWords;
        public UInt32 Word0;
        public UInt32 Word1;
        public UInt32 Word2;
        public UInt32 Word3;
    }


    internal class MonitorEndpointCommand : Command<MonitorEndpointCommand.Settings>
    {
        // we have this struct so we can separate the relatively fast received processing
        // and its calculations from the comparatively slow displays processing
        private Queue<ReceivedMidiMessage> m_receivedMessagesQueue = new Queue<ReceivedMidiMessage>();



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
                UInt64 lastReceivedTimestamp = 0;

                MidiMessageStruct msg;

                UInt32 index = 0;

                bool continueWaiting = true;


                var messageListener = new Thread(() => 
                {
                    connection.MessageReceived += (s, e) =>
                    {
                        // helps prevent any race conditions with main loop and its output
                        if (!continueWaiting) return;

                        //Console.WriteLine("DEBUG: MessageReceived");
                        index++;

                        var receivedMessage = new ReceivedMidiMessage()
                        {
                            Index = index,
                            ReceivedTimestamp = MidiClock.Now,
                            MessageTimestamp = e.Timestamp
                        };

                        receivedMessage.NumWords = e.FillWords(out receivedMessage.Word0, out receivedMessage.Word1, out receivedMessage.Word2, out receivedMessage.Word3);

                        m_receivedMessagesQueue.Enqueue(receivedMessage);

                        if (settings.SingleMessage)
                        {
                            continueWaiting = false;
                        }
                    };

                    while (continueWaiting)
                    {
                    }

                });

                messageListener.Start();


                // open the connection
                if (connection.Open())
                {
                    while (continueWaiting)
                    {
                        if (Console.KeyAvailable)
                        {
                            var keyInfo = Console.ReadKey(true);

                            if (keyInfo.Key == ConsoleKey.Escape)
                            {
                                continueWaiting = false;

                                // leading space is because the "E" in "Escape" is often lost in the output for some reason.
                                AnsiConsole.MarkupLine(Strings.MonitorEscapePressedMessage);
                                break;
                            }
                        }

                        if (m_receivedMessagesQueue.Count > 0)
                        {
                            lock (m_receivedMessagesQueue)
                            {
                                var message = m_receivedMessagesQueue.Dequeue();

                                if (startTimestamp == 0)
                                {
                                    // gets timestamp of first message we receive and uses that so all others are an offset
                                    startTimestamp = message.ReceivedTimestamp;
                                }

                                if (lastReceivedTimestamp == 0)
                                {
                                    // gets timestamp of first message we receive and uses that so all others are an offset from previous message
                                    lastReceivedTimestamp = message.ReceivedTimestamp;
                                }

                                // TODO: Maybe re-display the header if it's been a while since last header, and it is off-screen (index delta > some number)
                                if (message.Index == 1)
                                {
                                    AnsiConsoleOutput.DisplayMidiMessageTableHeaders();
                                }

                                // calculate offset from the last message received
                                //offsetMilliseconds = MidiClock.ConvertTimestampToMilliseconds(e.Timestamp - startTimestamp);
                                var offsetMicroseconds = MidiClock.ConvertTimestampToMicroseconds(message.ReceivedTimestamp - lastReceivedTimestamp);

                                AnsiConsoleOutput.DisplayMidiMessageTableRow(message, offsetMicroseconds);

                                // set our last received so we can calculate offsets
                                lastReceivedTimestamp = message.ReceivedTimestamp;
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
                                    var keyInfo = Console.ReadKey(true);
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
