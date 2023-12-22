using Spectre.Console;
using Spectre.Console.Cli;
using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using Windows.Devices.Midi2;

using Microsoft.Devices.Midi2.ConsoleApp.Resources;
using System.Diagnostics.Eventing.Reader;

namespace Microsoft.Devices.Midi2.ConsoleApp
{
    internal class SendMessageCommand : Command<SendMessageCommand.Settings>
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
                AnsiConsole.MarkupLine(Strings.SendMessageSendingThroughEndpointLabel + ": " + AnsiMarkupFormatter.FormatDeviceInstanceId(endpointId));
                AnsiConsole.WriteLine();

                bool openSuccess = false;

                // when this goes out of scope, it will dispose of the session, which closes the connections
                using var session = MidiSession.CreateSession($"{Strings.AppShortName} - {Strings.SendMessageSessionNameSuffix}");

                var bidiOpenOptions = new MidiEndpointConnectionOptions();

                if (session == null)
                {
                    AnsiConsole.MarkupLine(AnsiMarkupFormatter.FormatError(Strings.ErrorUnableToCreateSession));
                    return (int)MidiConsoleReturnCode.ErrorCreatingSession;
                }

                using var connection = session.CreateEndpointConnection(endpointId, bidiOpenOptions);
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


                UInt64 maxTimestampScheduled = 0;

                AnsiConsole.Progress()
                    .Start(ctx =>
                    {
                        var sendTask = ctx.AddTask("[white]Sending messages[/]");
                        sendTask.MaxValue = settings.Count;
                        sendTask.Value = 0;

                        uint messagesSent = 0;
                        uint messagesAttempted = 0;
                        uint messageFailures = 0;

                        while (messagesAttempted < settings.Count)
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


                            //Console.WriteLine($"Clock Frequency  : {MidiClock.TimestampFrequency}");
                            //Console.WriteLine($"Current Timestamp: {baseTimestamp}");
                            //Console.WriteLine($"Target Timestamp : {timestamp}");

                            messagesAttempted++;
                            var sendResult = connection.SendMessageWordArray(timestamp, settings.Words, 0, (byte)settings.Words.Count());

                            if (MidiEndpointConnection.SendMessageSucceeded(sendResult))
                            {

                                messagesSent++;
                                sendTask.Value = messagesSent;

                                ctx.Refresh();

                                if (timestamp > maxTimestampScheduled)
                                {
                                    maxTimestampScheduled = timestamp;
                                }

                            }
                            else
                            {
                                messageFailures ++;
                            }

                            Thread.Sleep(settings.DelayBetweenMessages);
                        }

                        if (messageFailures > 0)
                        {
                            // todo: localize
                            AnsiConsole.MarkupLine(AnsiMarkupFormatter.FormatError($"Failed to send {messageFailures} of a planned {settings.Count} message(s)."));
                        }
                        else
                        {
                            // todo: localize
                            AnsiConsole.MarkupLine($"Sent {messagesSent} message(s).");
                        }

                    });

                if (maxTimestampScheduled > MidiClock.Now)
                {
                    int sleepMs = (int)Math.Ceiling(MidiClock.ConvertTimestampToMilliseconds(maxTimestampScheduled - MidiClock.Now));

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
