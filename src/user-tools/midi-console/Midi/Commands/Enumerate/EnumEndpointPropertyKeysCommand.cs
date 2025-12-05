// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

using Microsoft.Windows.Devices.Midi2.Endpoints.Network;

namespace Microsoft.Midi.ConsoleApp
{
    internal class EnumEndpointPropertyKeysCommand : Command<EnumEndpointPropertyKeysCommand.Settings>
    {
        public sealed class Settings : CommandSettings
        {
        }

        public override int Execute(CommandContext context, Settings settings, CancellationToken cancellationToken)
        {
            var properties = MidiEndpointDevicePropertyHelper.GetAllMidiProperties();

            Table table = new Table();

            // TODO: Localize
            table.AddColumn("Property Key");
            table.AddColumn("Name");

            AnsiMarkupFormatter.SetTableBorderStyle(table);

            foreach (var property in properties)
            {
                table.AddRow(AnsiMarkupFormatter.FormatUnrecognizedPropertyKey(property.Key), AnsiMarkupFormatter.FormatFriendlyPropertyKey(property.Value));
            }

            AnsiConsole.Write(table);

            return 0;
        }
    }
}
