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
    // commands to check the health of Windows MIDI Services on this PC
    // will check for MIDI services
    // will attempt a loopback test


    internal class SendMessageCommand : Command<SendMessageCommand.Settings>
    {
        public sealed class Settings : CommandSettings
        {
            [LocalizedDescription("ParameterCommonInstanceIdDescription")]
            [CommandOption("-i|--instance-id")]
            public string InstanceId { get; init; }

            [LocalizedDescription("ParameterSendMessageEndpointDirectionDescription")]
            [CommandOption("-d|--direction")]
            [DefaultValue(EndpointDirectionOutputs.Bidirectional)]
            public EndpointDirectionOutputs EndpointDirection { get; init; }


            [LocalizedDescription("ParameterSendMessageCount")]
            [CommandOption("-c|--count")]
            [DefaultValue(1)]
            public UInt32 Count { get; init; }

            [LocalizedDescription("ParameterSendMessageDelayBetweenMessages")]
            [CommandOption("-p|--pause")]
            [DefaultValue(1000)]
            public int DelayBetweenMessages { get; init; }

            // TODO: Need to validate this is > 0 and <= 4 words
            [LocalizedDescription("ParameterSendMessageWords")]
            [CommandOption("-w|--word")]
            public UInt32[] Words { get; init; }

        }

        public override int Execute(CommandContext context, Settings settings)
        {
            MidiSession session = null;
            IMidiOutputConnection connection = null;

            string endpointId = settings.InstanceId.Trim().ToUpper();

            AnsiConsole.Status()
                .Start(Strings.StatusCreatingSessionAndOpeningEndpoint, ctx =>
                {
                    ctx.Spinner(Spinner.Known.Star);

                    session = MidiSession.CreateSession("MIDI Console - Sender");

                    if (session != null)
                    {
                        if (settings.EndpointDirection == EndpointDirectionOutputs.Bidirectional)
                        {
                            connection = session.ConnectBidirectionalEndpoint(endpointId);
                        }
                        else if (settings.EndpointDirection == EndpointDirectionOutputs.Out)
                        {
                            connection = session.ConnectOutputEndpoint(endpointId);
                        }
                    }

                    if (connection != null)
                    {
                        if (connection is MidiBidirectionalEndpointConnection)
                        {
                            ((MidiBidirectionalEndpointConnection)(connection)).Open();
                        }
                        else if (connection is MidiOutputEndpointConnection)
                        {
                            ((MidiOutputEndpointConnection)(connection)).Open();
                        }
                    }
                });

            if (session == null)
            {
                AnsiConsole.WriteLine(Strings.ErrorUnableToCreateSession);
                return 1;
            }
            else if (connection == null)
            {
                AnsiConsole.WriteLine(Strings.ErrorUnableToOpenEndpoint);
                return 1;
            }

            var table = new Table();

            // TODO: Localize these
            table.AddColumn("Timestamp");
            table.AddColumn("MIDI Words Sent");

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
