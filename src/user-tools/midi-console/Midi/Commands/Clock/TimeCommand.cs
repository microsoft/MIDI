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
            var now = MidiClock.Now;

            AnsiConsole.MarkupLine("Current timestamp:    " + AnsiMarkupFormatter.FormatTimestamp(now) + " ticks");
            AnsiConsole.MarkupLine("Timestamp resolution: " + AnsiMarkupFormatter.FormatTimestamp(MidiClock.TimestampFrequency) + " ticks per second");
            AnsiConsole.MarkupLine("Two seconds in ticks: " + AnsiMarkupFormatter.FormatTimestamp(MidiClock.OffsetTimestampByMilliseconds(0, 2000)) + " ticks");

            ;

            return (int)MidiConsoleReturnCode.Success;
        }


    }
}
