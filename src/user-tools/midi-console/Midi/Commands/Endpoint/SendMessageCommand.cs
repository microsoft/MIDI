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
            public UInt32[] Words { get; init; }

            [LocalizedDescription("ParameterSendMessageCount")]
            [CommandOption("-c|--count")]
            [DefaultValue(1)]
            public int Count { get; init; }
        }

        public override Spectre.Console.ValidationResult Validate(CommandContext context, Settings settings)
        {
            if (settings.Words.Length == 0)
            {
                return Spectre.Console.ValidationResult.Error(Strings.SendMessageValidationErrorTooFewWords);
            }
            else if (settings.Words.Length > 4)
            {
                return Spectre.Console.ValidationResult.Error(Strings.SendMessageValidationErrorTooManyWords);
            }



            return base.Validate(context, settings);
        }

        private void ValidateMessage(UInt32[] words)
        {

        }

        private void SendSingleMessage(IMidiOutputConnection endpoint, UInt32[] words)
        {

        }

        public override int Execute(CommandContext context, Settings settings)
        {
            MidiSession? session = null;
            IMidiOutputConnection? connection = null;

            string endpointId = string.Empty;

            if (!string.IsNullOrEmpty(settings.InstanceId))
            {
                endpointId = settings.InstanceId.Trim().ToLower();
            }
            else
            {
                endpointId = UmpEndpointPicker.PickOutput();
            }

            AnsiConsole.MarkupLine(Strings.SendMessageSendingThroughEndpointLabel + ": " + AnsiMarkupFormatter.FormatDeviceInstanceId(endpointId));
            AnsiConsole.WriteLine();

            bool openSuccess = false;

            AnsiConsole.Status()
                .Start(Strings.StatusCreatingSessionAndOpeningEndpoint, ctx =>
                {
                    ctx.Spinner(Spinner.Known.Star);

                    session = MidiSession.CreateSession($"{Strings.AppShortName} - {Strings.SendMessageSessionNameSuffix}");


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


            var table = new Table();

            table.AddColumn(Strings.SendMessageResultTableColumnHeaderTimestamp);
            table.AddColumn(Strings.SendMessageResultTableColumnHeaderWordsSent);

            AnsiConsole.Live(table)
                .Start(ctx =>
                {
                    for (uint i = 0; i < settings.Count; i++)
                    {
                        UInt64 timestamp = MidiClock.GetMidiTimestamp();
                        connection.SendUmpWordArray(timestamp, settings.Words, (UInt32)settings.Words.Count());

                        table.AddRow(AnsiMarkupFormatter.FormatTimestamp(timestamp), AnsiMarkupFormatter.FormatMidiWords(settings.Words));

                        ctx.Refresh();

                        Thread.Sleep(settings.DelayBetweenMessages);
                    }
                });

            return 0;
        }

    }
}
