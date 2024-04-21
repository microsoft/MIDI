// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================



using Windows.Devices.Enumeration;
using Windows.Devices.Midi;

namespace Microsoft.Midi.ConsoleApp
{
    internal sealed class EnumLegacyEndpointsCommand : Command<EnumLegacyEndpointsCommand.Settings>
    {
        public sealed class Settings : CommandSettings
        {
            [LocalizedDescription("ParameterEnumLegacyEndpointsDirection")]
            [CommandOption("-d|--direction")]
            [DefaultValue(LegacyEndpointDirection.All)]
            public LegacyEndpointDirection EndpointDirection { get; init; }

            [LocalizedDescription("ParameterEnumLegacyEndpointsIncludeEndpointId")]
            [CommandOption("-i|--include-endpoint-id")]
            [DefaultValue(true)]
            public bool IncludeId { get; init; }
        }

        public override int Execute(CommandContext context, Settings settings)
        {
            bool atLeastOneEndpointFound = false;

            var table = new Table();

            AnsiMarkupFormatter.SetTableBorderStyle(table);

            table.AddColumn("MIDI 1.0 Byte Stream Endpoints for older MIDI APIs");

            // Input endpoints

            if (settings.EndpointDirection == LegacyEndpointDirection.All || settings.EndpointDirection == LegacyEndpointDirection.In)
            {
                var endpoints = DeviceInformation.FindAllAsync(MidiInPort.GetDeviceSelector())
                .GetAwaiter().GetResult();

                if (endpoints.Count > 0)
                {
                    atLeastOneEndpointFound = true;

                    foreach (var endpointInfo in endpoints)
                    {
                        DisplayEndpointInformationFormatted(table, settings, endpointInfo, "MIDI 1.0 In");
                    }
                }
                else
                {
                    table.AddRow("No input endpoints.");
                }
            }

            // Output endpoints

            if (settings.EndpointDirection == LegacyEndpointDirection.All || settings.EndpointDirection == LegacyEndpointDirection.Out)
            {
                var endpoints = DeviceInformation.FindAllAsync(MidiOutPort.GetDeviceSelector())
                .GetAwaiter().GetResult();

                if (endpoints.Count > 0)
                {
                    atLeastOneEndpointFound = true;

                    foreach (var endpointInfo in endpoints)
                    {
                        DisplayEndpointInformationFormatted(table, settings, endpointInfo, "MIDI 1.0 Out");
                    }
                }
                else
                {
                    table.AddRow("No output endpoints.");
                }
            }

            AnsiConsole.Write(table);

            if (atLeastOneEndpointFound)
            {
                return 0;
            }
            else
            {
                // TODO: Formalize error codes.
                return 1;
            }
        }


        private void DisplayEndpointInformationFormatted(Table table, Settings settings, DeviceInformation endpointInfo, string endpointType)
        {

            table.AddRow(new Markup(AnsiMarkupFormatter.FormatEndpointName(endpointInfo.Name)));

            table.AddRow(endpointType);

            if (settings.IncludeId)
            {
                table.AddRow(new Markup(AnsiMarkupFormatter.FormatDeviceInstanceId(endpointInfo.Id)));
            }

            table.AddEmptyRow();

        }

    }
}
