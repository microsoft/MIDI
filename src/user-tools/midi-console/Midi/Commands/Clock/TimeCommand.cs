using Spectre.Console.Cli;
using Spectre.Console;
using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using System.ComponentModel;

using Microsoft.Devices.Midi2.ConsoleApp.Resources;
using Windows.Devices.Midi2;

namespace Microsoft.Devices.Midi2.ConsoleApp
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

            AnsiConsole.MarkupLine("The Windows MIDI Services MIDI Clock is a high-resolution 64 bit system timestamp that is reset when the computer is rebooted. See [darkslategray1]https://aka.ms/miditimestamp[/] for more information.");
            AnsiConsole.WriteLine();

            AnsiConsole.MarkupLine($"Timestamp resolution: [deepskyblue1]{MidiClock.TimestampFrequency.ToString("N0")}[/] ticks per second ([deepskyblue1]{convertedFrequency.ToString("N2")} {units}[/] per tick)");
            AnsiConsole.WriteLine();
            AnsiConsole.MarkupLine($"➡️ 1 microsecond: {AnsiMarkupFormatter.FormatTimestamp(MidiClock.OffsetTimestampByMicroseconds(0, 1))} ticks");
            AnsiConsole.MarkupLine($"➡️ 1 millisecond: {AnsiMarkupFormatter.FormatTimestamp(MidiClock.OffsetTimestampByMilliseconds(0, 1))} ticks");
            AnsiConsole.MarkupLine($"➡️ 1 second:       {AnsiMarkupFormatter.FormatTimestamp(MidiClock.OffsetTimestampByMilliseconds(0, 1000))} ticks");
            AnsiConsole.WriteLine();

            var now = MidiClock.Now;
            var ticksUntilWrap = Int64.MaxValue - now;  // turns out, QPC LARGE_INTEGER is signed, not unsigned.
            double tickWrapTime = 0.0;
            string tickWrapLabel = string.Empty;

            AnsiConsoleOutput.ConvertTicksToFriendlyTimeUnit(ticksUntilWrap, out tickWrapTime, out tickWrapLabel, true); 

            AnsiConsole.MarkupLine($"Current timestamp: {AnsiMarkupFormatter.FormatTimestamp(now)} ticks");
            AnsiConsole.WriteLine();
            AnsiConsole.MarkupLine($"Time until the MIDI Clock wraps back around to zero is {AnsiMarkupFormatter.FormatTimestamp(ticksUntilWrap)} ticks or [deepskyblue1]{tickWrapTime.ToString("N2")} {tickWrapLabel}[/].");

            return (int)MidiConsoleReturnCode.Success;
        }


    }
}
