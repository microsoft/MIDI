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
        public UInt64 ReceivedOffsetFromLastMessage;
        public UInt64 MessageTimestamp;
        public byte NumWords;
        public UInt32 Word0;
        public UInt32 Word1;
        public UInt32 Word2;
        public UInt32 Word3;

        public bool HasError;
    }


    internal class EndpointMonitorCommand : Command<EndpointMonitorCommand.Settings>
    {
        // we have this struct so we can separate the relatively fast received processing
        // and its calculations from the comparatively slow displays processing
        private Queue<ReceivedMidiMessage> m_receivedMessagesQueue = new Queue<ReceivedMidiMessage>(1000);
        private Queue<ReceivedMidiMessage> m_displayMessageQueue = new Queue<ReceivedMidiMessage>(1000);
        private Queue<ReceivedMidiMessage> m_fileWriterMessagesQueue = new Queue<ReceivedMidiMessage>(1000);



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

            [EnumLocalizedDescription("ParameterCaptureMessagesFieldDebugWarnSkippedIncrement", typeof(CaptureFieldDelimiter))]
            [CommandOption("-w|--debug-warn-skipped-increment")]
            [DefaultValue(false)]
            public bool WarnIfSkippedIncrementMessage { get; set; }

            //[EnumLocalizedDescription("ParameterMonitorEndpointSkipToKeepUp", typeof(CaptureFieldDelimiter))]
            //[CommandOption("-k|--keep-up-display")]
            //[DefaultValue(false)]
            //public bool SkipToKeepUp { get; set; }


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
                    MidiEndpointDeviceInformationFilters.IncludeDiagnosticLoopback |
                    MidiEndpointDeviceInformationFilters.IncludeClientUmpNative |
                    MidiEndpointDeviceInformationFilters.IncludeVirtualDeviceResponder |
                    MidiEndpointDeviceInformationFilters.IncludeClientByteStreamNative
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
            using AutoResetEvent m_displayMessageThreadWakeup = new AutoResetEvent(false);
            using AutoResetEvent m_fileMessageThreadWakeup = new AutoResetEvent(false);
            using AutoResetEvent m_messageDispatcherThreadWakeup = new AutoResetEvent(false);

            using AutoResetEvent m_terminateMessageListenerThread = new AutoResetEvent(false);


            MidiMessageTable displayTable = new MidiMessageTable(settings.Verbose);

        //    bool capturingToFile = false;

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

                AnsiConsole.MarkupLine(Strings.MonitorMonitoringOnEndpointLabel + ": " + AnsiMarkupFormatter.FormatFullEndpointInterfaceId(endpointId));

                //var table = new Table();

                // when this goes out of scope, it will dispose of the session, which closes the connections
                using var session = MidiSession.CreateSession($"{Strings.AppShortName} - {Strings.MonitorSessionNameSuffix}");
                if (session == null)
                {
                    AnsiConsole.WriteLine(Strings.ErrorUnableToCreateSession);
                    return (int)MidiConsoleReturnCode.ErrorCreatingSession;
                }

                var connection = session.CreateEndpointConnection(endpointId);
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

                    //capturingToFile = true;
                }
                else
                {
                    //captureWriter = null;
                }


                UInt64 startTimestamp = 0;
                //UInt64 lastReceivedEventTimestamp = 0;
                UInt32 index = 0;

                UInt64 firstMessageReceivedEventTimestamp = 0;
                UInt64 lastMessageReceivedEventTimestamp = 0;
                UInt32 countMessagesReceived = 0;


                MonitorEndpointConnectionStatusInTheBackground(endpointId);

                bool continueWaiting = true;
                
                UInt64 outOfTimestampOrderMessageCount = 0;

                uint numberOfSkippedDebugMessages = 0;

                // open the connection
                if (connection.Open())
                {
                    // Main message listener background thread ---------------------------------------------------------

                    var messageListener = new Thread(() =>
                    {

                        UInt64 lastMessageTimestamp = 0;

                        UInt32 lastReceivedDebugWord = 0;
                        UInt32 currentDebugWord = 0;

                        connection.MessageReceived += (s, e) =>
                        {
                            countMessagesReceived++;

                            // this captures stats to tell us how long it takes to receive and queue messages
                            UInt64 thisMessageReceivedEventTimestamp = MidiClock.Now;

                            if (firstMessageReceivedEventTimestamp == 0)
                            {
                                firstMessageReceivedEventTimestamp = thisMessageReceivedEventTimestamp;
                                lastMessageReceivedEventTimestamp = thisMessageReceivedEventTimestamp;
                            }

                            // this captures incoming message timestamps, not actual received event timestamp
                            if (startTimestamp == 0)
                            {
                                startTimestamp = e.Timestamp;
                                lastMessageTimestamp = e.Timestamp;
                            }

                            // helps prevent any race conditions with main loop and its output
                            //if (!continueWaiting) return;

                            index++;

                            var receivedMessage = new ReceivedMidiMessage()
                            {
                                Index = index,
                                ReceivedTimestamp = thisMessageReceivedEventTimestamp,
                                ReceivedOffsetFromLastMessage = thisMessageReceivedEventTimestamp - lastMessageReceivedEventTimestamp,
                                MessageTimestamp = e.Timestamp
                            };


                            if (index > 1 && e.Timestamp < lastMessageTimestamp)
                            {
                                outOfTimestampOrderMessageCount++;
                                receivedMessage.HasError = true;
                            }
                            lastMessageTimestamp = e.Timestamp;


                            receivedMessage.NumWords = e.FillWords(
                                out receivedMessage.Word0,
                                out receivedMessage.Word1,
                                out receivedMessage.Word2,
                                out receivedMessage.Word3);


                            if (settings.WarnIfSkippedIncrementMessage)
                            {
                                switch (receivedMessage.NumWords)
                                {
                                    case 1: break; // 1 word messages don't auto-increment

                                    case 2:
                                        currentDebugWord = receivedMessage.Word1;
                                        break;

                                    case 3:
                                        currentDebugWord = receivedMessage.Word2;
                                        break;

                                    case 4:
                                        currentDebugWord = receivedMessage.Word3;
                                        break;
                                }

                                if (index > 1 && (currentDebugWord > lastReceivedDebugWord + 1))
                                {
                                    numberOfSkippedDebugMessages += (currentDebugWord - lastReceivedDebugWord - 1);
                                    receivedMessage.HasError = true;
                                }

                                lastReceivedDebugWord = currentDebugWord;
                            }

                            // add it to the queue so it can be processed
                            lock (m_receivedMessagesQueue)
                            {
                                m_receivedMessagesQueue.Enqueue(receivedMessage);
                            }
                            m_messageDispatcherThreadWakeup.Set();


                            if (settings.SingleMessage)
                            {
                                continueWaiting = false;
                            }

                            lastMessageReceivedEventTimestamp = thisMessageReceivedEventTimestamp;
                        };

                        m_terminateMessageListenerThread.WaitOne();
                    });
                    messageListener.Start();


                    // Console display background thread ---------------------------------------------------------------
                    var messageConsoleDisplay = new Thread(() =>
                    {
                        while (continueWaiting)
                        {
                            if (m_displayMessageQueue.Count == 0) m_displayMessageThreadWakeup.WaitOne(5000);

                            if (continueWaiting && m_displayMessageQueue.Count > 0)
                            {
                                ReceivedMidiMessage message;

                                lock (m_displayMessageQueue)
                                {
                                    message = m_displayMessageQueue.Dequeue();
                                }

                                if (message.Index == 1)
                                {
                                    displayTable.OutputHeader();
                                }

                                displayTable.OutputRow(message);
                            }
                        }
                    });
                    messageConsoleDisplay.Start();


                    var messageDispatcherThread = new Thread(() =>
                    {
                        while (continueWaiting)
                        {
                            if (m_receivedMessagesQueue.Count == 0) m_messageDispatcherThreadWakeup.WaitOne(5000);

                            if (continueWaiting && m_receivedMessagesQueue.Count > 0)
                            {
                                // we load up the various queues here

                                ReceivedMidiMessage message;

                                // pull from the incoming messages queue
                                lock (m_receivedMessagesQueue)
                                {
                                    message = m_receivedMessagesQueue.Dequeue();
                                }

                                // add to the display queue
                                lock (m_displayMessageQueue)
                                {
                                    m_displayMessageQueue.Enqueue(message);
                                }
                                m_displayMessageThreadWakeup.Set();


                                // add to the file writer queue if we're capturing to file
                                if (captureWriter != null)
                                {
                                    lock (m_fileWriterMessagesQueue)
                                    {
                                        m_fileWriterMessagesQueue.Enqueue(message);
                                    }
                                    m_fileMessageThreadWakeup.Set();
                                }
                            }
                        }

                    });
                    messageDispatcherThread.Start();


                    // File writing background thread ------------------------------------------------------------------
                    if (captureWriter != null)
                    {
                        var messageFileWriter = new Thread(() =>
                        {
                            while (continueWaiting)
                            {
                                if (m_fileWriterMessagesQueue.Count == 0) m_fileMessageThreadWakeup.WaitOne(5000);

                                if (continueWaiting && m_fileWriterMessagesQueue.Count > 0)
                                {
                                    ReceivedMidiMessage message;

                                    lock (m_fileWriterMessagesQueue)
                                    {
                                        message = m_fileWriterMessagesQueue.Dequeue();
                                    }


                                    // write header if first message
                                    if (message.Index == 1)
                                    {
                                        captureWriter.WriteLine("#");
                                        captureWriter.WriteLine($"# Windows MIDI Services Console Capture {DateTime.Now.ToLongDateString()}");
                                        captureWriter.WriteLine($"# Endpoint:   {endpointId.ToString()}");
                                        captureWriter.WriteLine($"# Annotation: {settings.AnnotateCaptureFile.ToString()}");
                                        captureWriter.WriteLine($"# Delimiter:  {settings.FieldDelimiter.ToString()}");
                                        captureWriter.WriteLine("#");
                                    }

                                    WriteMessageToFile(settings, captureWriter, message);
                                }

                                //Thread.Sleep(0);
                            }
                        });
                        messageFileWriter.Start();
                    }


                    // Main UI loop ------------------------------------------------------------------------------------
                    AnsiConsole.MarkupLine(Strings.MonitorPressEscapeToStopMonitoringMessage);
                    AnsiConsole.WriteLine();

                    while (continueWaiting)
                    {
                        if (Console.KeyAvailable)
                        {
                            var keyInfo = Console.ReadKey(true);

                            if (keyInfo.Key == ConsoleKey.Escape)
                            {
                                continueWaiting = false;

                                // wake up the threads so they terminate
                                m_terminateMessageListenerThread.Set();
                                m_fileMessageThreadWakeup.Set();
                                m_displayMessageThreadWakeup.Set();
                                m_messageDispatcherThreadWakeup.Set();

                                AnsiConsole.WriteLine();
                                AnsiConsole.MarkupLine("🛑 " + Strings.MonitorEscapePressedMessage);
                            }
                            
                        }

                        if (_hasEndpointDisconnected)
                        {
                            continueWaiting = false;
                            m_terminateMessageListenerThread.Set();
                            m_displayMessageThreadWakeup.Set();
                            m_fileMessageThreadWakeup.Set();
                            m_messageDispatcherThreadWakeup.Set();

                            AnsiConsole.MarkupLine("❎ " + AnsiMarkupFormatter.FormatError(Strings.EndpointDisconnected));
                        }
                       
                        if (continueWaiting) Thread.Sleep(100); 
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

                // Summary information ---------------------------------------------------------------------------------

                {
                    string message = "➡️ ";

                    if (countMessagesReceived == 0)
                    {
                        message += "[yellow]No messages received[/]";
                    }
                    else if (countMessagesReceived == 1)
                    {
                        message += "[steelblue1]One message received[/]";
                    }
                    else
                    {
                        message += $"[steelblue1]{countMessagesReceived.ToString("N0")} messages received[/]";
                    }

                    if (countMessagesReceived > 0 && lastMessageReceivedEventTimestamp >= firstMessageReceivedEventTimestamp)
                    {
                        // calculate total receive time, not total display time

                        UInt64 totalTicks = lastMessageReceivedEventTimestamp - firstMessageReceivedEventTimestamp;
                        double totalTime = 0.0;
                        string totalTimeLabel = string.Empty;

                        AnsiConsoleOutput.ConvertTicksToFriendlyTimeUnit(totalTicks, out totalTime, out totalTimeLabel);

                        UInt64 averageTicks = totalTicks / countMessagesReceived;   // yes, this will truncate. That's ok
                        double averageTime = 0.0;
                        string averageTimeLabel = string.Empty;

                        AnsiConsoleOutput.ConvertTicksToFriendlyTimeUnit(averageTicks, out averageTime, out averageTimeLabel);

                        message += $" over [steelblue1]{totalTime.ToString("N2")} {totalTimeLabel}[/], ";
                        message += $"averaging [steelblue1]{averageTime.ToString("N2")} {averageTimeLabel}[/] per message. (Total display time may be longer)";

                    }

                    AnsiConsole.MarkupLine(message);
                }

                if (countMessagesReceived > 0)
                {
                    if (outOfTimestampOrderMessageCount > 0)
                    {
                        string message = "❎ " + outOfTimestampOrderMessageCount.ToString();

                        if (outOfTimestampOrderMessageCount > 1)
                        {
                            // multiple messages out of order
                            message += " messages received out of sent timestamp order.";
                        }
                        else
                        {
                            // single message out of order
                            message += " message received out of sent timestamp order.";
                        }

                        AnsiConsole.MarkupLine(AnsiMarkupFormatter.FormatError(message));
                    }
                    else
                    {
                        AnsiConsole.MarkupLine("✅ No messages received out of expected timestamp order.");
                    }
                }

                if (countMessagesReceived > 0 && settings.WarnIfSkippedIncrementMessage)
                {
                    if (numberOfSkippedDebugMessages > 0)
                    {
                        string message = "❎ " + numberOfSkippedDebugMessages.ToString();

                        if (numberOfSkippedDebugMessages > 1)
                        {
                            // multiple messages out of order
                            message = AnsiMarkupFormatter.FormatError(message + " debug messages skipped");
                        }
                        else
                        {
                            // single message out of order
                            message = AnsiMarkupFormatter.FormatError(message + " debug message skipped");
                        }

                        message += " (assuming ordered debug words). This does not include any missing from the end or beginning.";

                        AnsiConsole.MarkupLine(message);
                    }
                    else
                    {
                        AnsiConsole.MarkupLine("✅ No debug messages skipped. This does not include any missing from the end or beginning.");
                    }
                }



                //      AnsiConsole.MarkupLine($"{MidiClock.ConvertTimestampToMilliseconds(displayTable.TotalRenderingTicks).ToString("N4")} milliseconds spent just on output rendering.");

                if (captureWriter != null)
                {
                    AnsiConsole.MarkupLine("✅ Messages written to " + AnsiMarkupFormatter.FormatFileName(fileName));
                    captureWriter.Close();
                }

            }

            return (int)MidiConsoleReturnCode.Success;
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
