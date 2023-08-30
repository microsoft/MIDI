using Spectre.Console;
using Spectre.Console.Cli;
using Spectre.Console.Rendering;
using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.Linq;
using System.Net;
using System.Text;
using System.Threading.Tasks;

using Windows.Devices.Enumeration;
using Windows.Devices.Midi2;

namespace Microsoft.Devices.Midi2.ConsoleApp
{
    internal sealed class EnumEndpointsCommand : Command<EnumEndpointsCommand.Settings>
    {
        public sealed class Settings : CommandSettings
        {
            [LocalizedDescription("ParameterEnumEndpointsDirection")]
            [CommandOption("-d|--direction")]
            [DefaultValue(EndpointDirection.All)]
            public EndpointDirection EndpointDirection { get; init; }

            [LocalizedDescription("ParameterEnumEndpointsIncludeInstanceId")]
            [CommandOption("-i|--include-instance-id")]
            [DefaultValue(true)]
            public bool IncludeId { get; init; }
        }

        public override int Execute(CommandContext context, Settings settings)
        {
            bool atLeastOneEndpointFound = false;

            AnsiConsole.Status()
                .Start("Enumerating endpoints...", ctx =>
                {
                    ctx.Spinner(Spinner.Known.Star);


                    var table = new Table();

                    table.Border(TableBorder.HeavyHead);

                    table.AddColumn("UMP Endpoints for Windows MIDI Services");

                    // Bidirectional endpoints

                    if (settings.EndpointDirection == EndpointDirection.All || settings.EndpointDirection == EndpointDirection.Bidirectional)
                    {
                        var endpoints = DeviceInformation.FindAllAsync(MidiBidirectionalEndpointConnection.GetDeviceSelector())
                            .GetAwaiter().GetResult();

                        if (endpoints.Count > 0)
                        {
                            atLeastOneEndpointFound = true;

                            foreach (var endpointInfo in endpoints)
                            {
                                DisplayEndpointInformationFormatted(table, settings, endpointInfo, "UMP MIDI 2.0 Bidirectional");
                            }
                        }
                        else
                        {
                            table.AddRow("No bidirectional endpoints.");
                        }
                    }

                    // Input endpoints

                    if (settings.EndpointDirection == EndpointDirection.All || settings.EndpointDirection == EndpointDirection.In)
                    {
                        var endpoints = DeviceInformation.FindAllAsync(MidiInputEndpointConnection.GetDeviceSelector())
                        .GetAwaiter().GetResult();

                        if (endpoints.Count > 0)
                        {
                            atLeastOneEndpointFound = true;

                            foreach (var endpointInfo in endpoints)
                            {
                                DisplayEndpointInformationFormatted(table, settings, endpointInfo, "UMP MIDI In");
                            }
                        }
                        else
                        {
                            table.AddRow("No input endpoints.");
                        }
                    }

                    // Output endpoints

                    if (settings.EndpointDirection == EndpointDirection.All || settings.EndpointDirection == EndpointDirection.Out)
                    {
                        var endpoints = DeviceInformation.FindAllAsync(MidiOutputEndpointConnection.GetDeviceSelector())
                        .GetAwaiter().GetResult();

                        if (endpoints.Count > 0)
                        {
                            atLeastOneEndpointFound = true;

                            foreach (var endpointInfo in endpoints)
                            {
                                DisplayEndpointInformationFormatted(table, settings, endpointInfo, "UMP MIDI Out");
                            }
                        }
                        else
                        {
                            table.AddRow("No output endpoints.");
                        }
                    }

                    AnsiConsole.Write(table);
                });

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
            //var rowData = new List<IRenderable>();

            //rowData.Add(new Text(endpointInfo.Name));


            //table.AddRow(rowData);

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
