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


            [LocalizedDescription("VERBOSE_OPTION_TODO")]
            [CommandOption("-v|--verbose|--details")]
            [DefaultValue(false)]
            public bool Verbose { get; set; }
        }

        public override Spectre.Console.ValidationResult Validate(CommandContext context, Settings settings)
        {
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
                return (bool)((int)MidiMessageUtility.GetPacketTypeFromFirstMessageWord(words[0]) == words.Length);
            }
            else
            {
                return false;
            }
        }

        private void SendSingleMessage(IMidiOutputConnection endpoint, UInt32[] words)
        {

        }

        public override int Execute(CommandContext context, Settings settings)
        {
            IMidiOutputConnection? connection = null;

            string endpointId = string.Empty;

            if (!string.IsNullOrEmpty(settings.EndpointDeviceId))
            {
                endpointId = settings.EndpointDeviceId.Trim();
            }
            else
            {
                endpointId = UmpEndpointPicker.PickOutput();
            }

            AnsiConsole.MarkupLine(Strings.SendMessageSendingThroughEndpointLabel + ": " + AnsiMarkupFormatter.FormatDeviceInstanceId(endpointId));
            AnsiConsole.WriteLine();

            bool openSuccess = false;

            // todo: update loc strings
            using var session = MidiSession.CreateSession($"{Strings.AppShortName} - {Strings.SendMessageSessionNameSuffix}");


            AnsiConsole.Status()
                .Start(Strings.StatusCreatingSessionAndOpeningEndpoint, ctx =>
                {
                    ctx.Spinner(Spinner.Known.Star);

                    if (session != null)
                    {
                        var endpointDirection = EndpointUtility.GetUmpEndpointTypeFromInstanceId(endpointId);

                        if (endpointDirection == EndpointDirection.Bidirectional)
                        {
                            connection = session.ConnectBidirectionalEndpoint(endpointId);
                        }
                        else if (endpointDirection == EndpointDirection.Out)
                        {
                            connection = session.ConnectOutputEndpoint(endpointId);
                        }
                    }

                    if (connection != null)
                    {
                        openSuccess = false;

                        if (connection is MidiBidirectionalEndpointConnection)
                        {
                            openSuccess = ((MidiBidirectionalEndpointConnection)(connection)).Open();
                        }
                        else if (connection is MidiOutputEndpointConnection)
                        {
                            openSuccess = ((MidiOutputEndpointConnection)(connection)).Open();
                        }

                    }
                });

            if (session == null)
            {
                AnsiConsole.MarkupLine(AnsiMarkupFormatter.FormatError(Strings.ErrorUnableToCreateSession));
                return (int)MidiConsoleReturnCode.ErrorCreatingSession;
            }
            else if (connection == null)
            {
                AnsiConsole.MarkupLine(AnsiMarkupFormatter.FormatError(Strings.ErrorUnableToCreateEndpointConnection));
                return (int)MidiConsoleReturnCode.ErrorCreatingEndpointConnection;
            }
            else if (!openSuccess)
            {
                AnsiConsole.MarkupLine(AnsiMarkupFormatter.FormatError(Strings.ErrorUnableToOpenEndpoint));
                return (int)MidiConsoleReturnCode.ErrorOpeningEndpointConnection;
            }


            if (settings.Verbose)
            {
                var table = new Table();

                table.AddColumn(Strings.TableColumnHeaderCommonTimestamp);
                table.AddColumn(Strings.SendMessageResultTableColumnHeaderWordsSent);
                table.AddColumn(Strings.TableColumnHeaderCommonMessageType);

                AnsiConsole.Live(table)
                    .Start(ctx =>
                    {
                        if (settings.Words != null)
                        {
                            for (uint i = 0; i < settings.Count; i++)
                            {
                                UInt64 timestamp = MidiClock.GetMidiTimestamp();
                                var sendResult = connection.SendMessageWordArray(timestamp, settings.Words, 0, (byte)settings.Words.Count());

                                // TODO: check for error or other result


                                table.AddRow(
                                    AnsiMarkupFormatter.FormatTimestamp(timestamp),
                                    AnsiMarkupFormatter.FormatMidiWords(settings.Words),
                                    AnsiMarkupFormatter.FormatMessageType(MidiMessageUtility.GetMessageTypeFromFirstMessageWord(settings.Words[0]))
                                    );

                                ctx.Refresh();

                                Thread.Sleep(settings.DelayBetweenMessages);
                            }
                        }
                    });

                return 0;
            }
            else
            {
                // not verbose, so just show a counter

                AnsiConsole.Progress()
                    .Start(ctx =>
                    {
                        var sendTask = ctx.AddTask("[green]Sending messages[/]");
                        sendTask.MaxValue = settings.Count;
                        sendTask.Value = 0;

                        uint messagesSent = 0;

                        while (messagesSent < settings.Count)
                        {
                            UInt64 timestamp = MidiClock.GetMidiTimestamp();
                            connection.SendMessageWordArray(timestamp, settings.Words, 0, (byte)settings.Words.Count());

                            messagesSent++;
                            sendTask.Value = messagesSent;

                            ctx.Refresh();

                            Thread.Sleep(settings.DelayBetweenMessages);
                        }
                    });

                return 0;
            }
        }

    }
}
