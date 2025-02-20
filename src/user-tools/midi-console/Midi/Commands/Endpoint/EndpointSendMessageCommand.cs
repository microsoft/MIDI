// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================


//using Microsoft.Windows.Devices.Midi2.Initialization;
using Microsoft.Windows.Devices.Midi2.Messages;

namespace Microsoft.Midi.ConsoleApp
{
    internal class EndpointSendMessageCommand : Command<EndpointSendMessageCommand.Settings>
    {
        private UInt32[]? _parsedWords;

        public sealed class Settings : SendMessageCommandSettings
        {
            // Would be better to remove this and just do an enumeration lookup to see what type of endpoint it is

            [LocalizedDescription("ParameterSendMessageWords")]
            [CommandArgument(1, "<MIDI Words>")]
            public string[]? Words { get; set; }

            [LocalizedDescription("ParameterSendMessageCount")]
            [CommandOption("-c|--count")]
            [DefaultValue(1)]
            public int Count { get; set; }


            [LocalizedDescription("ParameterSendMessageTimestampOffsetMicroseconds")]
            [CommandOption("-o|--offset-microseconds")]
            [DefaultValue(0L)]
            public long TimestampOffsetMicroseconds { get; set; }

            [LocalizedDescription("ParameterSendMessageTimestamp")]
            [CommandOption("-t|--timestamp")]
            /*[DefaultValue(0UL)]*/
            public ulong? Timestamp { get; set; }

            [LocalizedDescription("ParameterSendMessageAutoIncrementLastWord")]
            [CommandOption("-i|--debug-auto-increment|--increment")]
            [DefaultValue(false)]
            public bool DebugAutoIncrementLastWord { get; set; }

        }

        public override Spectre.Console.ValidationResult Validate(CommandContext context, Settings settings)
        {
            if (settings.Words != null)
            {
                _parsedWords = new UInt32[settings.Words.Length];

                for (int i = 0; i < settings.Words.Length; i++)
                {
                    UInt32 word = 0;

                    string w = settings.Words[i];

                    if (w.ToLower().StartsWith("0x"))
                    {
                        // hex, but the globalization setting doesn't like this prefix
                        w = w.Substring(2);
                        word = UInt32.Parse(w, System.Globalization.NumberStyles.HexNumber);
                        _parsedWords![i] = word;
                    }
                    else if (w.ToLower().StartsWith("0b"))
                    {
                        // binary, but the globalization setting doesn't like this prefix
                        w = w.Substring(2);
                        word = UInt32.Parse(w, System.Globalization.NumberStyles.BinaryNumber);
                        _parsedWords![i] = word;
                    }
                    else if (settings.WordDataFormat == MidiWordDataFormat.Hex)
                    {
                        if (w.ToLower().StartsWith("0x")) w = w.Substring(2);

                        word = UInt32.Parse(w, System.Globalization.NumberStyles.HexNumber);
                        _parsedWords![i] = word;
                    }
                    else if (settings.WordDataFormat == MidiWordDataFormat.Binary)
                    {
                        if (w.ToLower().StartsWith("0b")) w = w.Substring(2);

                        word = UInt32.Parse(w, System.Globalization.NumberStyles.BinaryNumber);
                        _parsedWords![i] = word;
                    }
                    else if (settings.WordDataFormat == MidiWordDataFormat.Decimal)
                    {
                        word = UInt32.Parse(w, System.Globalization.NumberStyles.Number);
                        _parsedWords![i] = word;
                    }

                }

                if (settings.TimestampOffsetMicroseconds > 0 && settings.Timestamp != null)
                {
                    // TODO: Localize
                    return Spectre.Console.ValidationResult.Error("Please specify a timestamp or an offset, but not both.");
                }

                if (_parsedWords!.Length == 0)
                {
                    return Spectre.Console.ValidationResult.Error(Strings.MessageValidationErrorTooFewWords);
                }
                else if (_parsedWords!.Length > 4)
                {
                    return Spectre.Console.ValidationResult.Error(Strings.MessageValidationErrorTooManyWords);
                }
                else if (!ValidateMessage(_parsedWords))
                {
                    return Spectre.Console.ValidationResult.Error(Strings.MessageValidationErrorInvalidUmp);
                }

                if (settings.Count < 1)
                {
                    return Spectre.Console.ValidationResult.Error(Strings.ValidationErrorInvalidMessageCount);
                }
            }
            else
            {
                return Spectre.Console.ValidationResult.Error(Strings.MessageValidationErrorTooFewWords);
            }

            return base.Validate(context, settings);
        }

        // this should be moved to a base class
        private bool ValidateMessage(UInt32[] words)
        {
            if (words != null && words.Length > 0 && words.Length <= 4)
            {
                // allowed behavior is to cast the packet type to the word count
                return (bool)((int)MidiMessageHelper.GetPacketTypeFromMessageFirstWord(words[0]) == words.Length);
            }
            else
            {
                return false;
            }
        }

        public override int Execute(CommandContext context, Settings settings)
        {
            //if (!MidiServicesInitializer.EnsureServiceAvailable())
            //{
            //    AnsiConsole.MarkupLine(AnsiMarkupFormatter.FormatError("MIDI Service is not available."));
            //    return (int)MidiConsoleReturnCode.ErrorServiceNotAvailable;
            //}

            // we'll use this info when sleeping
            var systemTimerInfo = MidiClock.GetCurrentSystemTimerInfo();


            string endpointId = string.Empty;

            if (!string.IsNullOrEmpty(settings.EndpointDeviceId))
            {
                endpointId = settings.EndpointDeviceId.Trim();
            }
            else
            {
                endpointId = UmpEndpointPicker.PickEndpoint();
            }

            if (!string.IsNullOrEmpty(endpointId))
            {
                string endpointName = EndpointUtility.GetEndpointNameFromEndpointInterfaceId(endpointId);

                AnsiConsole.Markup(Strings.SendMessageSendingThroughEndpointLabel);
                AnsiConsole.MarkupLine(" " + AnsiMarkupFormatter.FormatEndpointName(endpointName));
                AnsiConsole.MarkupLine(AnsiMarkupFormatter.FormatFullEndpointInterfaceId(endpointId));
                AnsiConsole.WriteLine();

                bool openSuccess = false;

                // when this goes out of scope, it will dispose of the session, which closes the connections
                using var session = MidiSession.Create($"{Strings.AppShortName} - {Strings.SendMessageSessionNameSuffix}");

                if (session == null)
                {
                    AnsiConsole.MarkupLine(AnsiMarkupFormatter.FormatError(Strings.ErrorUnableToCreateSession));
                    return (int)MidiConsoleReturnCode.ErrorCreatingSession;
                }

                var connection = session.CreateEndpointConnection(endpointId, true);
                if (connection != null)
                {
                    openSuccess = connection.Open();
                }
                else
                {
                    AnsiConsole.MarkupLine(AnsiMarkupFormatter.FormatError(Strings.ErrorUnableToCreateEndpointConnection));
                    return (int)MidiConsoleReturnCode.ErrorCreatingEndpointConnection;
                }

                if (!openSuccess)
                {
                    AnsiConsole.MarkupLine(AnsiMarkupFormatter.FormatError(Strings.ErrorUnableToOpenEndpoint));
                    return (int)MidiConsoleReturnCode.ErrorOpeningEndpointConnection;
                }


                connection.EndpointDeviceDisconnected += Connection_EndpointDeviceDisconnected;
                connection.EndpointDeviceReconnected += Connection_EndpointDeviceReconnected;


                uint messagesSent = 0;
                uint messagesAttempted = 0;
                uint messageFailures = 0;
                uint totalMessageRetries = 0;

                UInt64 maxTimestampScheduled = 0;


                using AutoResetEvent m_messageDispatcherThreadWakeup = new AutoResetEvent(false);

                bool stillSending = true;

                UInt64 messageSendingStartTimestamp = 0;
                UInt64 messageSendingFinishTimestamp = 0;

                // todo: we need to break messaging sending out to a bg thread so
                // we can make it as fast as possible
                var messageSenderThread = new Thread(() =>
                {
                    messageSendingStartTimestamp = MidiClock.Now;

                    while (stillSending && messagesAttempted < settings.Count)
                    {
                        if (_disconnected)
                        {
                            // if we disconnect, we just have a short delay and then skip the rest of the loop
                            Thread.Sleep(200);
                            continue;
                        }

                        UInt64 baseTimestamp = MidiClock.Now;
                        UInt64 timestamp = 0;

                        if (settings.TimestampOffsetMicroseconds > 0)
                        {
                            timestamp = MidiClock.OffsetTimestampByMicroseconds(baseTimestamp, settings.TimestampOffsetMicroseconds);
                        }
                        else if (settings.Timestamp != null)
                        {
                            timestamp = (ulong)(settings.Timestamp);
                        }
                        else
                        {
                            timestamp = MidiClock.Now;
                        }


                        // the debug auto increment
                        if (_parsedWords!.Count() > 1 && settings.DebugAutoIncrementLastWord)
                        {
                            _parsedWords![_parsedWords!.Count() - 1] = (_parsedWords![_parsedWords!.Count() - 1] + 1) % UInt32.MaxValue;
                        }

                        messagesAttempted++;

                        bool continueToRetry = true;
                        uint retryAttempts = 0;
                        uint maxRetryAttempts = 500;
                        while (continueToRetry)
                        {
                            var sendResult = connection.SendSingleMessageWordArray(timestamp, 0, (byte)_parsedWords!.Count(), _parsedWords);

                            if (MidiEndpointConnection.SendMessageSucceeded(sendResult))
                            {
                                messagesSent++;
                                continueToRetry = false;    // message was sent, so no need to retry

                                if (timestamp > maxTimestampScheduled)
                                {
                                    maxTimestampScheduled = timestamp;
                                }
                            }
                            else
                            {
                                continueToRetry = false;

                                // if the problem is that the buffer is full, we will retry
                                // we should sleep in this loop, but the sleep resolution is not
                                // low enough to make this a reasonable thing to do.
                                if ((sendResult & MidiSendMessageResults.BufferFull) == MidiSendMessageResults.BufferFull)
                                {
                                    if (retryAttempts < maxRetryAttempts)
                                    {
                                        continueToRetry = true;
                                        retryAttempts++;
                                        totalMessageRetries++;
                                    }
                                    else
                                    {
                                        messageFailures++;
                                    }
                                }
                                else
                                {
                                    messageFailures++;
                                }
                            }
                        }




                        if (settings.DelayBetweenMessages > 0)
                        {
                            m_messageDispatcherThreadWakeup.WaitOne(settings.DelayBetweenMessages);
                        }
                        else
                        {
                            // we just tight loop
                        }
                    }

                    stillSending = false;

                    messageSendingFinishTimestamp = MidiClock.Now;
                });


                //    const uint bufferWarningThreshold = 10000;

                if (settings.Count == 1)
                {
                    messageSenderThread.Start();

                    while (messagesAttempted < 1)
                    {
                        Thread.Sleep(1);
                    }
                }
                else
                {
                    AnsiConsole.Progress()
                        .Start(ctx =>
                        {
                            var sendTask = ctx.AddTask("[white]Sending messages to MIDI service[/]");
                            sendTask.MaxValue = settings.Count;
                            sendTask.Value = 0;

                            messageSenderThread.Start();

                            if (settings.Count > 1)
                            {
                                AnsiConsole.MarkupLine(Strings.SendMessagePressEscapeToStopSendingMessage);
                                AnsiConsole.WriteLine();
                            }

                            while (stillSending)
                            {
                                // check for input

                                if (Console.KeyAvailable)
                                {
                                    var keyInfo = Console.ReadKey(true);

                                    if (keyInfo.Key == ConsoleKey.Escape)
                                    {
                                        stillSending = false;

                                        // wake up the threads so they terminate
                                        m_messageDispatcherThreadWakeup.Set();

                                        AnsiConsole.WriteLine();
                                        AnsiConsole.MarkupLine("🛑 " + Strings.SendMessageEscapePressedMessage);
                                    }

                                }

                                sendTask.Value = messagesSent;
                                ctx.Refresh();

                                if (stillSending) Thread.Sleep(1);
                            }

                            sendTask.Value = messagesSent;

                        });
                }

                if (messageFailures > 0)
                {
                    // todo: localize
                    AnsiConsole.MarkupLine(AnsiMarkupFormatter.FormatError($"{messageFailures} failed message sending attempts."));
                }
                else
                {
                    if (settings.Count > 1)
                    {
                        // todo: localize
                        if (settings.DelayBetweenMessages > 0)
                        {
                            double actualDelayMilliseconds = Math.Max((double)settings.DelayBetweenMessages, MidiClock.ConvertTimestampTicksToMilliseconds(systemTimerInfo.CurrentIntervalTicks));

                            AnsiConsole.MarkupLine($"Sent [steelblue1]{messagesSent.ToString("N0")}[/] message(s) with a delay of approximately [steelblue1]{actualDelayMilliseconds.ToString("N2")} ms[/] between each.");
                        }
                        else
                        {
                            AnsiConsole.MarkupLine($"Sent [steelblue1]{messagesSent.ToString("N0")}[/] message(s) as fast as possible.");
                        }
                    }
                    else
                    {
                        AnsiConsole.MarkupLine($"Sent [steelblue1]1[/] message.");
                    }
                }

                if (messagesSent > 1 && messageSendingFinishTimestamp >= messageSendingStartTimestamp)
                {
                    // calculate total send time, not total display time

                    UInt64 totalTicks = messageSendingFinishTimestamp - messageSendingStartTimestamp;
                    double totalTime = 0.0;
                    string totalTimeLabel = string.Empty;

                    AnsiConsoleOutput.ConvertTicksToFriendlyTimeUnit(totalTicks, out totalTime, out totalTimeLabel);

                    UInt64 averageTicks = totalTicks / messagesSent;   // yes, this will truncate. That's ok
                    double averageTime = 0.0;
                    string averageTimeLabel = string.Empty;

                    AnsiConsoleOutput.ConvertTicksToFriendlyTimeUnit(averageTicks, out averageTime, out averageTimeLabel);

                    AnsiConsole.MarkupLine($"Total send time [steelblue1]{totalTime.ToString("N2")} {totalTimeLabel}[/], averaging [steelblue1]{averageTime.ToString("N2")} {averageTimeLabel}[/] per message.");
                }

                if (totalMessageRetries > 0)
                {
                    AnsiConsole.MarkupLine(AnsiMarkupFormatter.FormatWarning($"{totalMessageRetries} retried send operations."));
                }


                // for scheduled messages, we wait
                if (maxTimestampScheduled > MidiClock.Now)
                {
                    int sleepMs = (int)Math.Ceiling(MidiClock.ConvertTimestampTicksToMilliseconds(maxTimestampScheduled - MidiClock.Now));

                    sleepMs += 1000;    // we wait an extra second to avoid any timing issues

                    AnsiConsole.MarkupLine($"Keeping connection alive until timestamp : {AnsiMarkupFormatter.FormatTimestamp(maxTimestampScheduled)} (apx {Math.Round(sleepMs / 1000.0, 1)} seconds from now)");

                    Thread.Sleep(sleepMs);
                }


                if (!settings.NoWait)
                {
                    AnsiConsole.MarkupLine(Strings.SendMessagePressEscapeToCloseConnectionMessage);
                    AnsiConsole.WriteLine();

                    bool continueWaiting = true;

                    while (continueWaiting)
                    {
                        if (Console.KeyAvailable)
                        {
                            var keyInfo = Console.ReadKey(true);

                            if (keyInfo.Key == ConsoleKey.Escape)
                            {
                                continueWaiting = false;

                                AnsiConsole.WriteLine();
                                AnsiConsole.MarkupLine("🛑 " + Strings.SendMessageEscapePressedMessage);
                            }
                        }
                        else
                        {
                            Thread.Sleep(500);
                        }
                    }
                }

                session.DisconnectEndpointConnection(connection.ConnectionId);

                return (int)MidiConsoleReturnCode.Success;
            }
            else
            {
                // no endpoint

                return (int)MidiConsoleReturnCode.ErrorGeneralFailure;
            }
        }


        bool _disconnected = false;

        // Auto-reconnect takes care of the internals. We're just reporting here
        private void Connection_EndpointDeviceReconnected(IMidiEndpointConnectionSource sender, object args)
        {
            AnsiConsole.MarkupLine(AnsiMarkupFormatter.FormatSuccess(Strings.EndpointReconnected));

            _disconnected = false;
        }

        // Auto-reconnect takes care of the internals. We're just reporting here
        private void Connection_EndpointDeviceDisconnected(IMidiEndpointConnectionSource sender, object args)
        {
            AnsiConsole.MarkupLine(AnsiMarkupFormatter.FormatError(Strings.EndpointDisconnected));

            _disconnected = true;
        }



    }
}
