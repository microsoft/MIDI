// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================



using Microsoft.Windows.Devices.Midi2.Initialization;

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
            if (!MidiServicesInitializer.EnsureServiceAvailable())
            {
                AnsiConsole.MarkupLine(AnsiMarkupFormatter.FormatError("MIDI Service is not available."));
                return (int)MidiConsoleReturnCode.ErrorServiceNotAvailable;
            }


            // TODO: This code should be moved to the SDK (as C++) so the UI and apps can use it
            // Either that, or just share an assembly

            // Keep private information out, so no machine names, IP addresses, user names, etc.


            // To avoid using this to exploit, do not overwrite existing files, ever


            // OS Information ==============================================================

            // OS Name

            // OS Version

            // Nothing else for OS. If there's more desired, an app like MSINFO will provide it



            // MIDI Information ============================================================

            // Version of Windows MIDI Services installed

            // Ids and names of all MIDI devices recognized by the API and their driver information

            // Ids and names of all MIDI 1.0 API devices









            return 0;
        }
    }
}