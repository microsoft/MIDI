using Spectre.Console.Cli;
using Spectre.Console;
using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using System.ComponentModel;

namespace Microsoft.Devices.Midi2.ConsoleApp
{
    internal class CaptureMessagesCommand : Command<CaptureMessagesCommand.Settings>
    {
        internal class Settings : EndpointCommandSettings
        {
            [LocalizedDescription("ParameterCaptureMessagesOutputFile")]
            [CommandArgument(0, "<Output File>")]
            public string OutputFile { get; set; }

            [LocalizedDescription("ParameterCaptureMessagesEcho")]
            [CommandOption("-e|--echo")]
            [DefaultValue(false)]
            public bool Echo { get; set; }

            [LocalizedDescription("ParameterCaptureMessagesAnnotate")]
            [CommandOption("-a|--annotate")]
            [DefaultValue(false)]
            public bool Annotate { get; set; }

            [EnumLocalizedDescription("ParameterCaptureMessagesFilter", typeof(CaptureMessageTypeFilter))]
            [CommandOption("-f|--filter")]
            [DefaultValue(CaptureMessageTypeFilter.All)]
            public CaptureMessageTypeFilter Filter { get; set; }

            [LocalizedDescription("ParameterCaptureMessagesIncludeTimestamps")]
            [CommandOption("-t|--timestamps")]
            [DefaultValue(false)]
            public bool IncludeTimestamps { get; set; }

            [EnumLocalizedDescription("ParameterCaptureMessagesFieldDelimiter", typeof(CaptureFieldDelimiter))]
            [CommandOption("-d|--delimiter")]
            [DefaultValue(CaptureFieldDelimiter.Space)]
            public CaptureFieldDelimiter FieldDelimiter { get; set; }

            [EnumLocalizedDescription("ParameterCaptureMessagesFieldFormat", typeof(CaptureFieldFormat))]
            [CommandOption("--format")]
            [DefaultValue(CaptureFieldFormat.HexWithPrefix)]
            public CaptureFieldFormat FieldFormat { get; set; }
        }

        public override int Execute(CommandContext context, Settings settings)
        {
            AnsiConsole.MarkupLine(AnsiMarkupFormatter.FormatError("Capture messages not yet implemented."));

            return (int)MidiConsoleReturnCode.ErrorNotImplemented;
        }





    }
}
