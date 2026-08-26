// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

using Windows.Devices.Midi2.ServiceConfig;

namespace Microsoft.Midi.ConsoleApp
{
    internal static class ConfigFileSaver
    {
        // Sending a change and saving it are separate operations, so a command applies the change
        // first and then persists it only if the customer asked for that.
        internal static void ReportSave(IMidiServiceTransportPluginConfig config)
        {
            var response = MidiServiceTransportPluginConfigManager.SaveUpdate(config);

            if (response.Success)
            {
                AnsiConsole.MarkupLine(AnsiMarkupFormatter.FormatSuccess("Saved to the configuration file."));

                if (!string.IsNullOrEmpty(response.BackupFilePath))
                {
                    AnsiConsole.MarkupLine(AnsiMarkupFormatter.FormatGeneralDetailMessage(
                        $"A backup of the previous configuration was written to {AnsiMarkupFormatter.EscapeString(response.BackupFilePath)}."));
                }

                return;
            }

            AnsiConsole.MarkupLine(AnsiMarkupFormatter.FormatWarning(
                AnsiMarkupFormatter.EscapeString(response.ErrorMessage)));

            AnsiConsole.MarkupLine(AnsiMarkupFormatter.FormatGeneralDetailMessage(
                "The change was applied, but will be lost when the service restarts."));
        }
    }
}
