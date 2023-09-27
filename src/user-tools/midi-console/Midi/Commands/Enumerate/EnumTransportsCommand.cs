using Spectre.Console;
using Spectre.Console.Cli;
using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace Microsoft.Devices.Midi2.ConsoleApp
{
    internal class EnumTransportsCommand : Command<EnumTransportsCommand.Settings>
    {
        public sealed class Settings : CommandSettings
        {
        }

        public override int Execute(CommandContext context, Settings settings)
        {
            AnsiConsole.MarkupLine(AnsiMarkupFormatter.FormatError("Enum transports not yet implemented."));

            return (int)MidiConsoleReturnCode.ErrorNotImplemented;
        }
    }
}
