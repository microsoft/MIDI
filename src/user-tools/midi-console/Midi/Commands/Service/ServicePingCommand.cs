using Spectre.Console;
using Spectre.Console.Cli;
using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.ComponentModel.DataAnnotations;
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


    internal class ServicePingCommand : Command<ServicePingCommand.Settings>
    {
        public sealed class Settings : CommandSettings
        {
            // TODO: Consider changing this to a command argument instead of an option
            [LocalizedDescription("ParameterServicePingCount")]
            [CommandOption("-c|--count")]
            [DefaultValue(10)]
            public int Count { get; set; }

            [LocalizedDescription("ParameterServicePingTimeout")]
            [CommandOption("-t|--timeout")]
            [DefaultValue(5000)]
            public int Timeout { get; set; }

            [LocalizedDescription("ParameterServicePingVerbose")]
            [CommandOption("-v|--verbose|--details")]
            [DefaultValue(false)]
            public bool Verbose { get; set; }

        }

        public override Spectre.Console.ValidationResult Validate(CommandContext context, Settings settings)
        {
            if (settings.Timeout < 10)
            {
                return Spectre.Console.ValidationResult.Error(Strings.ValidationErrorPingTimeoutTooLow);
            }

            if (settings.Count < 1)
            {
                return Spectre.Console.ValidationResult.Error(Strings.ValidationErrorPingCountTooLow);
            }

            if (settings.Count > 1000)
            {
                return Spectre.Console.ValidationResult.Error(Strings.ValidationErrorPingCountTooHigh);
            }

            return base.Validate(context, settings);
        }


        public override int Execute(CommandContext context, Settings settings)
        {
            try
            {
                // check to see if the service is running. 
                // NOTE: Equivalent code can't be moved to the SDK due to Desktop/WinRT limitations.

                AnsiConsole.MarkupLine($"Pinging MIDI service {AnsiMarkupFormatter.FormatGeneralNumber(settings.Count)} times...");

                //UInt32 timeout = settings.Timeout.HasValue ? settings.Timeout.Value : 0;

                var pingResult = MidiService.PingService((byte)settings.Count, (UInt32)settings.Timeout);

                // TODO: Display results in a table

                if (pingResult != null)
                {
                    if (pingResult.Success)
                    {
                        // show the summary

                        if (settings.Verbose)
                        {
                            // show table of the ping results

                            var table = new Table();
                            table.Border(TableBorder.Rounded);

                            table.AddColumn(Strings.PingResultTableColumnHeaderPing);
                            table.AddColumn(Strings.PingResultTableColumnHeaderSendTimestamp);
                            table.AddColumn(Strings.PingResultTableColumnHeaderReceiveTimestamp);
                            table.AddColumn(Strings.PingResultTableColumnHeaderRoundTripTicks);
                            table.AddColumn(Strings.PingResultTableColumnHeaderRoundTripMicroseconds);

                            var freq = MidiClock.GetMidiTimestampFrequency();

                            foreach (var response in pingResult.Responses)
                            {
                                double deltaMicroseconds = (double)(response.ClientDeltaTimestamp * 1000000) / freq;

                                table.AddRow(
                                    new Text(response.Index.ToString()),
                                    new Markup(AnsiMarkupFormatter.FormatTimestamp(response.ClientSendMidiTimestamp)),
                                    new Markup(AnsiMarkupFormatter.FormatTimestamp(response.ClientReceiveMidiTimestamp)),
                                    new Markup(AnsiMarkupFormatter.FormatGeneralNumber(response.ClientDeltaTimestamp)),
                                    new Markup(AnsiMarkupFormatter.FormatGeneralNumber(deltaMicroseconds))
                                    );
                            }

                            AnsiConsole.Write(table);
                        }

                        AnsiConsole.MarkupLine($" {Strings.GenericCount}: {AnsiMarkupFormatter.FormatGeneralNumber(pingResult.Responses.Count)} {Strings.GenericResponses}");
                        AnsiConsole.MarkupLine($" {Strings.GenericTotal}: {AnsiMarkupFormatter.FormatGeneralNumber(pingResult.TotalPingRoundTripMidiClock)} {Strings.GenericClockTicks}");
                        AnsiConsole.MarkupLine($" {Strings.GenericAverage}: {AnsiMarkupFormatter.FormatGeneralNumber(pingResult.AveragePingRoundTripMidiClock)} {Strings.GenericClockTicks}");

                    }
                    else
                    {
                        AnsiConsole.Markup(AnsiMarkupFormatter.FormatError($"{Strings.ErrorPingTestFailed}: " + pingResult.FailureReason));
                    }
                }
                else
                {
                    AnsiConsole.Markup(AnsiMarkupFormatter.FormatError($"{Strings.ErrorPingTestFailed}: {Strings.ErrorPingTestFailReasonSummaryNull}"));
                }
                return 0;
            }
            catch (System.TypeInitializationException)
            {
                AnsiConsole.Markup(AnsiMarkupFormatter.FormatError($"{Strings.ErrorPingTestFailed}: {Strings.ErrorGeneralFailReasonWinRTActivation}"));

                return (int)MidiConsoleReturnCode.ErrorWinRTTypeActivationFailure;
            }
            catch (Exception ex)
            {
                AnsiConsole.Markup(AnsiMarkupFormatter.FormatError($"{Strings.ErrorPingTestFailed}: " + ex.Message));

                return (int)MidiConsoleReturnCode.ErrorGeneralFailure;
            }
        }
    }
}
