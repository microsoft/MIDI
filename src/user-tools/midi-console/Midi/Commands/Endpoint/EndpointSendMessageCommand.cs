// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================


namespace Microsoft.Midi.ConsoleApp
{
    internal class EndpointSendMessageCommand : Command<EndpointSendMessageCommand.Settings>
    {
        public sealed class Settings : SendMessageCommandSettings
        {
            // Would be better to remove this and just do an enumeration lookup to see what type of endpoint it is

            [LocalizedDescription("ParameterSendMessageWords")]
            [CommandArgument(1, "<MIDI Words>")]
            public UInt32[]? Words { get; set; }

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
            if (settings.TimestampOffsetMicroseconds > 0 && settings.Timestamp != null)
            {
                // TODO: Localize
                return Spectre.Console.ValidationResult.Error("Please specify a timestamp or an offset, but not both.");
            }

            if (settings.Words == null)
            {
                return Spectre.Console.ValidationResult.Error(Strings.MessageValidationErrorTooFewWords);
            }
            if (settings.Words.Length == 0)
            {
                return Spectre.Console.ValidationResult.Error(Strings.MessageValidationErrorTooFewWords);
            }
            else if (settings.Words.Length > 4)
            {
                return Spectre.Console.ValidationResult.Error(Strings.MessageValidationErrorTooManyWords);
            }
            else if (!ValidateMessage(settings.Words))
            {
                return Spectre.Console.ValidationResult.Error(Strings.MessageValidationErrorInvalidUmp);
            }


            if (settings.Count < 1)
            {
                return Spectre.Console.ValidationResult.Error(Strings.ValidationErrorInvalidMessageCount);
            }

            return base.Validate(context, settings);
        }

        // this should be moved to a base class
        private bool ValidateMessage(UInt32[] words)
        {
            if (words != null && words.Length > 0 && words.Length <= 4)
            {
                // allowed behavior is to cast the packet type to the word count
                return (bool)((int)MidiMessageUtility.GetPacketTypeFromMessageFirstWord(words[0]) == words.Length);
            }
            else
            {
                return false;
            }
        }

        public override int Execute(CommandContext context, Settings settings)
        {
            if (!MidiService.IsAvailable())
            {
                AnsiConsole.MarkupLine(AnsiMarkupFormatter.FormatError("MIDI Service is not available."));
                return (int)MidiConsoleReturnCode.ErrorServiceNotAvailable;
            }


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
                using var session = MidiSession.CreateSession($"{Strings.AppShortName} - {Strings.SendMessageSessionNameSuffix}");

                if (session == null)
                {
                    AnsiConsole.MarkupLine(AnsiMarkupFormatter.FormatError(Strings.ErrorUnableToCreateSession));
                    return (int)MidiConsoleReturnCode.ErrorCreatingSession;
                }

                var connection = session.CreateEndpointConnection(endpointId);
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




                uint messagesSent = 0;
                uint messagesAttempted = 0;
                uint messageFailures = 0;

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
                        if (settings.Words!.Count() > 1 && settings.DebugAutoIncrementLastWord)
                        {
                            settings.Words![settings.Words!.Count() - 1] = (settings.Words![settings.Words!.Count() - 1] + 1) % UInt32.MaxValue;
                        }


                        messagesAttempted++;
                        var sendResult = connection.SendSingleMessageWordArray(timestamp, 0, (byte)settings.Words!.Count(), settings.Words);

                        if (MidiEndpointConnection.SendMessageSucceeded(sendResult))
                        {
                            messagesSent++;

                            if (timestamp > maxTimestampScheduled)
                            {
                                maxTimestampScheduled = timestamp;
                            }

                        }
                        else
                        {
                            messageFailures++;
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


                AnsiConsole.Progress()
                    .Start(ctx =>
                    {
                        //if (settings.DelayBetweenMessages == 0 && (settings.Count * (settings.Words!.Length + 2)) > bufferWarningThreshold)
                        //{
                        //    AnsiConsole.MarkupLine(AnsiMarkupFormatter.FormatWarning(Strings.SendMessageFloodWarning));
                        //    AnsiConsole.WriteLine();
                        //}

                        var sendTask = ctx.AddTask("[white]Sending messages[/]");
                        sendTask.MaxValue = settings.Count;
                        sendTask.Value = 0;

                        messageSenderThread.Start();


                        AnsiConsole.MarkupLine(Strings.SendMessagePressEscapeToStopSendingMessage);
                        AnsiConsole.WriteLine();
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

                            if (stillSending) Thread.Sleep(100);
                        }

                        sendTask.Value = messagesSent;

                    });

                if (messageFailures > 0)
                {
                    // todo: localize
                    AnsiConsole.MarkupLine(AnsiMarkupFormatter.FormatError($"Failed to send {messageFailures} of a planned {settings.Count} message(s)."));
                }
                else
                {
                    // todo: localize
                    if (settings.DelayBetweenMessages > 0)
                    {
                        AnsiConsole.MarkupLine($"Sent [steelblue1]{messagesSent.ToString("N0")}[/] message(s) with a delay of approximately [steelblue1]{settings.DelayBetweenMessages.ToString("N0")} ms[/] between each.");
                    }
                    else
                    {
                        AnsiConsole.MarkupLine($"Sent [steelblue1]{messagesSent.ToString("N0")}[/] message(s).");
                    }
                }

                if (messagesSent > 0 && messageSendingFinishTimestamp >= messageSendingStartTimestamp)
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

                if (maxTimestampScheduled > MidiClock.Now)
                {
                    int sleepMs = (int)Math.Ceiling(MidiClock.ConvertTimestampTicksToMilliseconds(maxTimestampScheduled - MidiClock.Now));

                    sleepMs += 1000;    // we wait an extra second to avoid any timing issues

                    AnsiConsole.MarkupLine($"Keeping connection alive until timestamp : {AnsiMarkupFormatter.FormatTimestamp(maxTimestampScheduled)} (apx {Math.Round(sleepMs / 1000.0, 1)} seconds from now)");

                    Thread.Sleep(sleepMs);
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

    }
}
