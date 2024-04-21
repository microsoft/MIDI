// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================


namespace Microsoft.Midi.ConsoleApp
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
            [DefaultValue(20)]
            public int Count { get; set; }

            [LocalizedDescription("ParameterServicePingTimeout")]
            [CommandOption("-t|--timeout")]
            [DefaultValue(10000)]
            public int Timeout { get; set; }

            [LocalizedDescription("ParameterServicePingVerbose")]
            [CommandOption("-v|--verbose")]
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

            if (settings.Count > 255)
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
                        var freq = MidiClock.TimestampFrequency;

                        if (settings.Verbose)
                        {
                            // show table of the ping results

                            var table = new Table();
                            AnsiMarkupFormatter.SetTableBorderStyle(table);

                            table.AddColumn(AnsiMarkupFormatter.FormatTableColumnHeading(Strings.PingResultTableColumnHeaderPing));
                            table.AddColumn(AnsiMarkupFormatter.FormatTableColumnHeading(Strings.PingResultTableColumnHeaderSendTimestamp));
                            table.AddColumn(AnsiMarkupFormatter.FormatTableColumnHeading(Strings.PingResultTableColumnHeaderServiceTimestamp));
                            table.AddColumn(AnsiMarkupFormatter.FormatTableColumnHeading(Strings.PingResultTableColumnHeaderReceiveTimestamp));
                            table.AddColumn(AnsiMarkupFormatter.FormatTableColumnHeading(Strings.PingResultTableColumnHeaderServiceBreakdown));
                            table.AddColumn(AnsiMarkupFormatter.FormatTableColumnHeading(Strings.PingResultTableColumnHeaderRoundTripTicks));
                            table.AddColumn(AnsiMarkupFormatter.FormatTableColumnHeading(Strings.PingResultTableColumnHeaderRoundTripMicroseconds));
                            table.AddColumn(AnsiMarkupFormatter.FormatTableColumnHeading(Strings.PingResultTableColumnHeaderRoundTripMilliseconds));


                            foreach (var response in pingResult.Responses)
                            {
                                double deltaMicroseconds = (double)(response.ClientDeltaTimestamp * 1000000) / freq;
                                double deltaMilliseconds = (double)(response.ClientDeltaTimestamp * 1000) / freq;

                                UInt64 sendToServiceTicks = response.ServiceReportedMidiTimestamp - response.ClientSendMidiTimestamp;
                                UInt64 receiveFromServiceTicks = response.ClientReceiveMidiTimestamp - response.ServiceReportedMidiTimestamp;

                                table.AddRow(
                                    new Text((response.Index + 1).ToString()),
                                    new Markup(AnsiMarkupFormatter.FormatTimestamp(response.ClientSendMidiTimestamp)),
                                    new Markup(AnsiMarkupFormatter.FormatTimestamp(response.ServiceReportedMidiTimestamp)),
                                    new Markup(AnsiMarkupFormatter.FormatTimestamp(response.ClientReceiveMidiTimestamp)),
                                    new Markup(AnsiMarkupFormatter.FormatGeneralNumber(sendToServiceTicks) + " / " + AnsiMarkupFormatter.FormatGeneralNumber(receiveFromServiceTicks)),
                                    new Markup(AnsiMarkupFormatter.FormatGeneralNumber(response.ClientDeltaTimestamp)),
                                    new Markup(AnsiMarkupFormatter.FormatGeneralNumber(deltaMicroseconds)),
                                    new Markup(AnsiMarkupFormatter.FormatGeneralNumber(deltaMilliseconds))
                                    );
                            }

                            AnsiConsole.Write(table);
                        }

                        double averageMicroseconds = (double)(pingResult.AveragePingRoundTripMidiClock * 1000000) / freq;
                        double averageMilliseconds = (double)(pingResult.AveragePingRoundTripMidiClock * 1000) / freq;

                        var summaryTable = new Table();
                        summaryTable.Border(TableBorder.None);

                        summaryTable.AddColumn("").LeftAligned();
                        summaryTable.AddColumn("").RightAligned();
                        summaryTable.AddColumn("").LeftAligned();

                        summaryTable.AddRow(Strings.GenericCount, AnsiMarkupFormatter.FormatGeneralNumber(pingResult.Responses.Count), Strings.GenericResponses);
                        summaryTable.AddRow(Strings.GenericTotal, AnsiMarkupFormatter.FormatGeneralNumber(pingResult.TotalPingRoundTripMidiClock), Strings.GenericClockTicks);
                        summaryTable.AddRow(Strings.GenericAverage, AnsiMarkupFormatter.FormatGeneralNumber(pingResult.AveragePingRoundTripMidiClock), Strings.GenericClockTicks);
                        summaryTable.AddRow(Strings.GenericAverage, AnsiMarkupFormatter.FormatGeneralNumber(averageMicroseconds), Strings.GenericMicroseconds);
                        summaryTable.AddRow(Strings.GenericAverage, AnsiMarkupFormatter.FormatGeneralNumber(averageMilliseconds), Strings.GenericMilliseconds);

                        AnsiConsole.Write(summaryTable);
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
