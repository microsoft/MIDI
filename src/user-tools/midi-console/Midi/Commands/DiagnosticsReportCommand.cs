// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================



//using Microsoft.Windows.Devices.Midi2.Initialization;

namespace Microsoft.Midi.ConsoleApp
{
    // commands to check the health of Windows MIDI Services on this PC
    // will check for MIDI services
    // will attempt a loopback test


    internal class DiagnosticsReportCommand : Command<DiagnosticsReportCommand.Settings>
    {
        public sealed class Settings : CommandSettings
        {
            [LocalizedDescription("ParameterDiagnosticsReportOutputFile")]
            [CommandArgument(0, "<Output File>")]
            public string? OutputFile { get; set; }
        }

        public override ValidationResult Validate(CommandContext context, Settings settings)
        {
            // we won't overwrite existing files. Intent is to prevent misuse of this app
            if (System.IO.File.Exists(settings.OutputFile))
            {
                return ValidationResult.Error($"File already exists {settings.OutputFile}. Please choose a different name.");
            }

            return base.Validate(context, settings);
        }

        public override int Execute(CommandContext context, Settings settings)
        {
            //if (!MidiServicesInitializer.EnsureServiceAvailable())
            //{
            //    AnsiConsole.MarkupLine(AnsiMarkupFormatter.FormatError("MIDI Service is not available."));
            //    return (int)MidiConsoleReturnCode.ErrorServiceNotAvailable;
            //}


            // TODO: this should just shell out and run mididiag.exe



            return 0;
        }
    }
}