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


    internal class EndpointMonitorCommand : Command<EndpointMonitorCommand.Settings>
    {
        // we have this struct so we can separate the relatively fast received processing
        // and its calculations from the comparatively slow displays processing
        private Queue<ReceivedMidiMessage> m_receivedMessagesQueue = new Queue<ReceivedMidiMessage>();



        public sealed class Settings : MessageListenerCommandSettings
        {
            //[LocalizedDescription("ParameterMonitorEndpointDirectionDescription")]
            //[CommandOption("-d|--direction")]
            //[DefaultValue(EndpointDirectionInputs.Bidirectional)]
            //public EndpointDirectionInputs EndpointDirection { get; set; }


            [LocalizedDescription("ParameterMonitorEndpointSingleMessage")]
            [CommandOption("-s|--single-message")]
            [DefaultValue(false)]
            public bool SingleMessage { get; set; }

            //// gap in milliseconds before restarting offset calculation
            //[LocalizedDescription("TODO ParameterMonitorEndpointGap")]
            //[CommandOption("-g|--gap")]
            //[DefaultValue(5000)]
            //public int Gap { get; set; }


            [LocalizedDescription("ParameterMonitorEndpointVerbose")]
            [CommandOption("-v|--verbose")]
            [DefaultValue(false)]
            public bool Verbose { get; set; }


            [LocalizedDescription("ParameterCaptureMessagesOutputFile")]
            [CommandOption("-c|--capture-to-file")]
            public string? OutputFile { get; set; }

            [LocalizedDescription("ParameterCaptureMessagesAnnotate")]
            [CommandOption("-a|--annotate-capture")]
            [DefaultValue(false)]
            public bool AnnotateCaptureFile { get; set; }

            [EnumLocalizedDescription("ParameterCaptureMessagesFieldDelimiter", typeof(CaptureFieldDelimiter))]
            [CommandOption("-l|--capture-field-delimiter")]
            [DefaultValue(CaptureFieldDelimiter.Space)]
            public CaptureFieldDelimiter FieldDelimiter { get; set; }


        }


        public override Spectre.Console.ValidationResult Validate(CommandContext context, Settings settings)
        {
            // TODO: Validate endpoint 

            if (settings.OutputFile != null)
            {
                if (Path.GetExtension(settings.OutputFile) == string.Empty)
                {
                    settings.OutputFile = Path.ChangeExtension(settings.OutputFile, ".midi2");
                }
            }

            

            return ValidationResult.Success();
        }

        bool _hasEndpointDisconnected = false;
        bool _continueWatchingDevice = true;

        private void MonitorEndpointConnectionStatusInTheBackground(string endpointId)
        {
            var deviceWatcherThread = new Thread(() =>
            {
                var watcher = MidiEndpointDeviceWatcher.CreateWatcher(
                    MidiEndpointDeviceInformationFilter.IncludeDiagnosticLoopback |
                    MidiEndpointDeviceInformationFilter.IncludeClientUmpNative |
                    MidiEndpointDeviceInformationFilter.IncludeVirtualDeviceResponder |
                    MidiEndpointDeviceInformationFilter.IncludeClientByteStreamNative
                    );


                watcher.Removed += (s, e) =>
                {
                    if (e.Id.ToLower() == endpointId.ToLower())
                    {
                        _hasEndpointDisconnected = true;
                        _continueWatchingDevice = false;

                        watcher.Stop();
                    }
                };

                watcher.Start();

                while (_continueWatchingDevice)
                {
                    Thread.Sleep(100);
                }

            });

            deviceWatcherThread.Start();
        }

        public override int Execute(CommandContext context, Settings settings)
        {
            MidiMessageTable displayTable = new MidiMessageTable(settings.Verbose);

            bool capturingToFile = false;

            string endpointId = string.Empty;

            if (!string.IsNullOrEmpty(settings.EndpointDeviceId))
            {
                endpointId = settings.EndpointDeviceId.Trim().ToLower();
            }
            else
            {
                endpointId = UmpEndpointPicker.PickEndpoint();
            }

            if (!string.IsNullOrEmpty(endpointId))
            {

                AnsiConsole.MarkupLine(Strings.MonitorMonitoringOnEndpointLabel + ": " + AnsiMarkupFormatter.FormatDeviceInstanceId(endpointId));

                //var table = new Table();

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

                string fileName = string.Empty;
                StreamWriter? captureWriter = null;

                if (settings.OutputFile != null && settings.OutputFile.Trim() != string.Empty)
                {
                    // todo: some cleanup on this

                    fileName = Environment.ExpandEnvironmentVariables(settings.OutputFile.Trim());

                    // open the file and append to it if it already exists
                    //captureWriter = new StreamWriter(fileName, true);
                    captureWriter = System.IO.File.AppendText(fileName);

                    AnsiConsole.MarkupLine("Capturing to " + AnsiMarkupFormatter.FormatFileName(fileName));

                    capturingToFile = true;
                }
                else
                {
                    //captureWriter = null;
                }

                AnsiConsole.MarkupLine(Strings.MonitorPressEscapeToStopMonitoringMessage);
                AnsiConsole.WriteLine();

                UInt64 startTimestamp = 0;
                UInt64 lastReceivedTimestamp = 0;

                //MidiMessageStruct msg;

                UInt32 index = 0;


                MonitorEndpointConnectionStatusInTheBackground(endpointId);

                bool continueWaiting = true;
                UInt64 outOfOrderMessageCount = 0;

                var messageListener = new Thread(() =>
                {
                    UInt64 lastMessageTimestamp = 0;

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

                        if (e.Timestamp <  lastMessageTimestamp)
                        {
                            outOfOrderMessageCount++;
                        }

                        lastMessageTimestamp = e.Timestamp;


                        receivedMessage.NumWords = e.FillWords(out receivedMessage.Word0, out receivedMessage.Word1, out receivedMessage.Word2, out receivedMessage.Word3);

                        m_receivedMessagesQueue.Enqueue(receivedMessage);

                        if (settings.SingleMessage)
                        {
                            continueWaiting = false;
                        }
                    };

                    while (continueWaiting && !_hasEndpointDisconnected)
                    {
                        Thread.Sleep(0);
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

                                AnsiConsole.WriteLine();
                                AnsiConsole.MarkupLine(Strings.MonitorEscapePressedMessage);
                            }
                            
                        }


                        if (_hasEndpointDisconnected)
                        {
                            continueWaiting = false;
                            AnsiConsole.MarkupLine(AnsiMarkupFormatter.FormatError(Strings.EndpointDisconnected));
                        }
                        else if (continueWaiting && m_receivedMessagesQueue.Count > 0)
                        {
                            lock (m_receivedMessagesQueue)
                            {
                                int iter = 0;

                                while (m_receivedMessagesQueue.Count > 0 && iter < 10)
                                {
                                    iter++; // we need to take a break if we're being spammed, so this tracks iterations

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
                                        displayTable.OutputHeader();


                                        if (captureWriter != null && capturingToFile)
                                        {
                                            captureWriter.WriteLine("#");
                                            captureWriter.WriteLine($"# Windows MIDI Services Console Capture {DateTime.Now.ToLongDateString()}");
                                            captureWriter.WriteLine($"# Endpoint:   {endpointId.ToString()}");
                                            captureWriter.WriteLine($"# Annotation: {settings.AnnotateCaptureFile.ToString()}");
                                            captureWriter.WriteLine($"# Delimiter:  {settings.FieldDelimiter.ToString()}");
                                            captureWriter.WriteLine("#");
                                        }

                                    }

                                    // calculate offset from the last message received
                                    var offsetMicroseconds = MidiClock.ConvertTimestampToMicroseconds(message.ReceivedTimestamp - lastReceivedTimestamp);

                                    displayTable.OutputRow(message, offsetMicroseconds);

                                    if (captureWriter != null)
                                    {
                                        WriteMessageToFile(settings, captureWriter, message);
                                    }


                                    // set our last received so we can calculate offsets
                                    lastReceivedTimestamp = message.ReceivedTimestamp;

                                }
                            }
                        }

                        // we don't need to update the display more often. A tight loop really ties up the CPU.
                        // actual message receive timings are based on the worker thread
                        if (continueWaiting) Thread.Sleep(125); 
                    }
                }
                else
                {
                    AnsiConsole.MarkupLine(AnsiMarkupFormatter.FormatError(Strings.ErrorUnableToOpenEndpoint));
                    return (int)MidiConsoleReturnCode.ErrorOpeningEndpointConnection;
                }

                _continueWatchingDevice = false;

                if (!_hasEndpointDisconnected)
                {
                    session.DisconnectEndpointConnection(connection.ConnectionId);
                }

                if (outOfOrderMessageCount > 0)
                {
                    AnsiConsole.MarkupLine(AnsiMarkupFormatter.FormatError(outOfOrderMessageCount.ToString() + " messages received out of sent timestamp order."));
                }
                else
                {
                    AnsiConsole.MarkupLine("No messages received out of expected timestamp order.");
                }

                if (captureWriter != null)
                {
                    AnsiConsole.MarkupLine("Messages written to " + AnsiMarkupFormatter.FormatFileName(fileName));
                    captureWriter.Close();
                }

            }

            return 0;
        }


        private void WriteMessageToFile(Settings settings, StreamWriter writer, ReceivedMidiMessage message)
        {
            if (settings.AnnotateCaptureFile)
            {
                writer.WriteLine(BuildAnnotationLine(settings, message.MessageTimestamp, message.ReceivedTimestamp, message.Word0));
            }

            string messageLine = string.Empty;

            if (message.NumWords >= 1)
            {
                messageLine += string.Format("0x{0:X8}", message.Word0);
            }

            if (message.NumWords >= 2)
            {
                messageLine += GetConfiguredDelimiter(settings);
                messageLine += string.Format("0x{0:X8}", message.Word1);
            }

            if (message.NumWords >= 3)
            {
                messageLine += GetConfiguredDelimiter(settings);
                messageLine += string.Format("0x{0:X8}", message.Word2);
            }

            if (message.NumWords == 4)
            {
                messageLine += GetConfiguredDelimiter(settings);
                messageLine += string.Format("0x{0:X8}", message.Word3);
            }

            writer.WriteLine(messageLine);

        }


        private string BuildAnnotationLine(Settings settings, UInt64 messageTimestamp, UInt64 receivedTimestamp, UInt32 word0)
        {
            // format here is
            // # ___time___ ___timestamp___ ___message type___ ___friendly name___

            string messageType = MidiMessageUtility.GetMessageTypeFromMessageFirstWord(word0).ToString();
            string fullMessageType = MidiMessageUtility.GetMessageFriendlyNameFromFirstWord(word0);

            string delimiter = GetConfiguredDelimiter(settings);

            //return $"# {DateTime.Now.ToString("HH:mm:ss.fff")}{delimiter}{messageTimestamp.ToString()}{delimiter}{receivedTimestamp.ToString()}{delimiter}{messageType}{delimiter}{fullMessageType}";
            return $"# {DateTime.Now.ToString("HH:mm:ss.fff")}{delimiter}{messageTimestamp.ToString()}{delimiter}{messageType}{delimiter}{fullMessageType}";
        }

        private string GetConfiguredDelimiter(Settings settings)
        {
            switch (settings.FieldDelimiter)
            {
                case CaptureFieldDelimiter.Space:
                    return " ";
                case CaptureFieldDelimiter.Comma:
                    return ",";
                case CaptureFieldDelimiter.Tab:
                    return "\t";
                case CaptureFieldDelimiter.Pipe:
                    return "|";
                default:
                    return ",";
            }
        }

    }
}
