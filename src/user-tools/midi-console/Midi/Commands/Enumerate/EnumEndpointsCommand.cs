// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================



namespace Microsoft.Midi.ConsoleApp
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

            [LocalizedDescription("ParameterEnumEndpointsIncludeAll")]
            [CommandOption("-a|--all")]
            [DefaultValue(false)]
            public bool IncludeAll{ get; set; }


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

                    AnsiMarkupFormatter.SetTableBorderStyle(table);

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

                    MidiEndpointDeviceInformationFilters filter =
                        MidiEndpointDeviceInformationFilters.IncludeClientByteStreamNative |
                        MidiEndpointDeviceInformationFilters.IncludeClientUmpNative;

                    if (settings.IncludeAll)
                    {
                        filter =
                            MidiEndpointDeviceInformationFilters.IncludeClientByteStreamNative |
                            MidiEndpointDeviceInformationFilters.IncludeClientUmpNative |
                            MidiEndpointDeviceInformationFilters.IncludeDiagnosticLoopback |
                            MidiEndpointDeviceInformationFilters.IncludeDiagnosticPing |
                            MidiEndpointDeviceInformationFilters.IncludeVirtualDeviceResponder;
                    }
                    else if (settings.IncludeDiagnosticLoopback)
                    {
                        filter |= MidiEndpointDeviceInformationFilters.IncludeDiagnosticLoopback;
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
                if (!string.IsNullOrEmpty(endpointInfo.TransportSuppliedDescription))
                {
                    table.AddRow(new Markup(AnsiMarkupFormatter.EscapeString(endpointInfo.TransportSuppliedDescription)));
                }

                if (!string.IsNullOrEmpty(endpointInfo.UserSuppliedDescription))
                {
                    table.AddRow(new Markup(AnsiMarkupFormatter.EscapeString(endpointInfo.UserSuppliedDescription)));
                }

            }

            table.AddEmptyRow();

        }

    }
}
