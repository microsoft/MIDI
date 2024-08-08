// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================


namespace Microsoft.Midi.ConsoleApp
{
    internal class TimeCommand : Command<TimeCommand.Settings>
    {
        internal class Settings : CommandSettings
        {
        }

        public override int Execute(CommandContext context, Settings settings)
        {

            var frequency = MidiClock.TimestampFrequency;
            double convertedFrequency = 0;
            string units = string.Empty;

            if (frequency >= 100000)
            {
                convertedFrequency = 1.0 / frequency * 1000000000.0;
                units = "ns";
            }
            else if (frequency >= 1000)
            {
                convertedFrequency = 1.0 / frequency * 1000000.0;
                units = "μs";
            }
            else
            {
                // hope we never see this!
                convertedFrequency = 1.0 / frequency * 1000.0;
                units = "ms";
            }


            // TODO: Should right justify and align the numbers shown here

            AnsiConsole.MarkupLine("The Windows MIDI Services MIDI Clock is a high-resolution 64 bit system timestamp that is reset when the computer is rebooted.");
            AnsiConsole.MarkupLine("See [darkslategray1]https://aka.ms/miditimestamp[/] for more information.");
            AnsiConsole.WriteLine();

            AnsiConsole.MarkupLine($"Timestamp resolution: [deepskyblue1]{MidiClock.TimestampFrequency.ToString("N0")}[/] ticks per second ([deepskyblue1]{convertedFrequency.ToString("N2")} {units}[/] per tick)");
            AnsiConsole.WriteLine();
            AnsiConsole.MarkupLine($"➡️ 1 microsecond: {AnsiMarkupFormatter.FormatTimestamp(MidiClock.OffsetTimestampByMicroseconds(0, 1))} ticks");
            AnsiConsole.MarkupLine($"➡️ 1 millisecond: {AnsiMarkupFormatter.FormatTimestamp(MidiClock.OffsetTimestampByMilliseconds(0, 1))} ticks");
            AnsiConsole.MarkupLine($"➡️ 1 second:      {AnsiMarkupFormatter.FormatTimestamp(MidiClock.OffsetTimestampByMilliseconds(0, 1000))} ticks");
            AnsiConsole.WriteLine();

            var now = MidiClock.Now;
            var ticksUntilWrap = Int64.MaxValue - now;  // turns out, QPC LARGE_INTEGER is signed, not unsigned so Int64, not UInt64.
            double tickWrapTime = 0.0;
            string tickWrapLabel = string.Empty;

            AnsiConsoleOutput.ConvertTicksToFriendlyTimeUnit(ticksUntilWrap, out tickWrapTime, out tickWrapLabel, true); 

            AnsiConsole.MarkupLine($"Current timestamp: {AnsiMarkupFormatter.FormatTimestamp(now)} ticks");
            AnsiConsole.WriteLine();
            AnsiConsole.MarkupLine($"Time until the MIDI Clock wraps back around to zero is {AnsiMarkupFormatter.FormatTimestamp(ticksUntilWrap)} ticks or [deepskyblue1]{tickWrapTime.ToString("N2")} {tickWrapLabel}[/].");

            var timerInfo = MidiClock.GetCurrentSystemTimerInfo();

            AnsiConsole.WriteLine();

            if (timerInfo.MaximumIntervalTicks > 0)
            {
                AnsiConsole.MarkupLine($"Windows Timer Intervals (for thread sleep, for example)");
                AnsiConsole.WriteLine();
                AnsiConsole.MarkupLine($"➡️ Minimum:       {AnsiMarkupFormatter.FormatTimestamp(timerInfo.MinimumIntervalTicks)} ticks, ([deepskyblue1]{MidiClock.ConvertTimestampTicksToMilliseconds(timerInfo.MinimumIntervalTicks).ToString("N3")}[/]) milliseconds");
                AnsiConsole.MarkupLine($"➡️ Maximum:       {AnsiMarkupFormatter.FormatTimestamp(timerInfo.MaximumIntervalTicks)} ticks, ([deepskyblue1]{MidiClock.ConvertTimestampTicksToMilliseconds(timerInfo.MaximumIntervalTicks).ToString("N3")}[/]) milliseconds");
                AnsiConsole.MarkupLine($"➡️ Current:       {AnsiMarkupFormatter.FormatTimestamp(timerInfo.CurrentIntervalTicks)} ticks, ([deepskyblue1]{MidiClock.ConvertTimestampTicksToMilliseconds(timerInfo.CurrentIntervalTicks).ToString("N3")}[/]) milliseconds");
                AnsiConsole.WriteLine();
            }
            else
            {
                AnsiConsole.MarkupLine(AnsiMarkupFormatter.FormatError("Windows Timer Intervals (for thread sleep, for example)"));
            }

            return (int)MidiConsoleReturnCode.Success;
        }


    }
}
