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

            AnsiConsole.MarkupLine("Current timestamp:    " + AnsiMarkupFormatter.FormatTimestamp(now));
            AnsiConsole.MarkupLine("Timestamp resolution: " + AnsiMarkupFormatter.FormatTimestamp(MidiClock.TimestampFrequency) + "hz");

            return (int)MidiConsoleReturnCode.Success;
        }


    }
}
