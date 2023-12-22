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

using Microsoft.Devices.Midi2.ConsoleApp.Resources;

namespace Microsoft.Devices.Midi2.ConsoleApp
{
    internal sealed class EnumEndpointsCommand : Command<EnumEndpointsCommand.Settings>
    {
        public sealed class Settings : CommandSettings
        {
            [LocalizedDescription("ParameterEnumEndpointsIncludeEndpointId")]
            [CommandOption("-i|--include-endpoint-id")]
            [DefaultValue(true)]
            public bool IncludeId { get; set; }

            [LocalizedDescription("ParameterEnumEndpointsIncludeLoopbackEndpoints")]
            [CommandOption("-l|--include-loopback")]
            [DefaultValue(false)]
            public bool IncludeDiagnosticLoopback { get; set; }

            [LocalizedDescription("ParameterEnumEndpointsVerboseOutput")]
            [CommandOption("-v|--verbose")]
            [DefaultValue(false)]
            public bool Verbose { get; set; }

        }

        public override int Execute(CommandContext context, Settings settings)
        {
            bool atLeastOneEndpointFound = false;

            AnsiConsole.Status()
                .Start("Enumerating endpoints...", ctx =>
                {
                    ctx.Spinner(Spinner.Known.Star);


                    var table = new Table();

                    table.Border(TableBorder.Rounded);

                    table.AddColumn(AnsiMarkupFormatter.FormatTableColumnHeading("UMP Endpoints for Windows MIDI Services"));

                    // Bidirectional endpoints

                    string selector = string.Empty;

                    try
                    {
                        selector = MidiEndpointConnection.GetDeviceSelector();
                    }
                    catch (System.TypeInitializationException)
                    {
                        AnsiConsole.Markup(AnsiMarkupFormatter.FormatError($"{Strings.ErrorEnumEndpointsFailed}: {Strings.ErrorGeneralFailReasonWinRTActivation}"));
                        return;
                    }

                    MidiEndpointDeviceInformationFilter filter = 
                        MidiEndpointDeviceInformationFilter.IncludeClientByteStreamNative | 
                        MidiEndpointDeviceInformationFilter.IncludeClientUmpNative;

                    if (settings.IncludeDiagnosticLoopback)
                    {
                        filter |= MidiEndpointDeviceInformationFilter.IncludeDiagnosticLoopback;
                    }

                    var endpoints = MidiEndpointDeviceInformation.FindAll(
                        MidiEndpointDeviceInformationSortOrder.Name,
                        filter
                        );

                    if (endpoints.Count > 0)
                    {
                        atLeastOneEndpointFound = true;

                        foreach (var endpointInfo in endpoints)
                        {
                            DisplayEndpointInformationFormatted(table, settings, endpointInfo, "UMP Bidirectional");
                        }
                    }
                    else
                    {
                        table.AddRow("No matching endpoints found.");
                    }


                    AnsiConsole.Write(table);
                });

            if (atLeastOneEndpointFound)
            {
                return (int)MidiConsoleReturnCode.Success;
            }
            else
            {
                return (int)MidiConsoleReturnCode.ErrorNoEndpointsFound;
            }
        }


        private void DisplayEndpointInformationFormatted(Table table, Settings settings, MidiEndpointDeviceInformation endpointInfo, string endpointType)
        {
            table.AddRow(new Markup(AnsiMarkupFormatter.GetEndpointIcon(endpointInfo.EndpointPurpose) + " " + AnsiMarkupFormatter.FormatEndpointName(endpointInfo.Name)));

            if (settings.IncludeId)
            {
                table.AddRow(new Markup(AnsiMarkupFormatter.FormatFullEndpointInterfaceId(endpointInfo.Id)));
            }

            if (settings.Verbose)
            {
                if (!string.IsNullOrEmpty(endpointInfo.Description))
                {
                    table.AddRow(new Markup(AnsiMarkupFormatter.EscapeString(endpointInfo.Description)));
                }

            }

            table.AddEmptyRow();

        }

    }
}
