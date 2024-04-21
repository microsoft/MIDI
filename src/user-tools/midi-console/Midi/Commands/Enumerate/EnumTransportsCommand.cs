// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================



namespace Microsoft.Midi.ConsoleApp
{
    internal class EnumTransportsCommand : Command<EnumTransportsCommand.Settings>
    {
        public sealed class Settings : CommandSettings
        {
            [LocalizedDescription("ParameterEnumTransportsVerbose")]
            [CommandOption("-v|--verbose")]
            [DefaultValue(false)]
            public bool Verbose { get; set; }
        }


        public override int Execute(CommandContext context, Settings settings)
        {
            AnsiConsole.Status()
            .Start("Enumerating sessions...", ctx =>
            {
                ctx.Spinner(Spinner.Known.Star);

                var table = new Table();

                AnsiMarkupFormatter.SetTableBorderStyle(table);


                table.AddColumn(AnsiMarkupFormatter.FormatTableColumnHeading("Transport Plugins"));

                var transportList = MidiService.GetInstalledTransportPlugins();

                if (transportList.Count > 0)
                {
                    foreach (var transport in transportList)
                    {
                        table.AddRow(
                            $"{AnsiMarkupFormatter.FormatTransportName(transport.Name)} ({AnsiMarkupFormatter.FormatTransportMnemonic(transport.Mnemonic)}) Version {AnsiMarkupFormatter.FormatTransportVersion(transport.Version)}"
                            );

                        table.AddRow(
                            $"by {AnsiMarkupFormatter.FormatTransportAuthor(transport.Author)}"
                            );

                        table.AddRow(
                            $"{AnsiMarkupFormatter.FormatTransportDescription(transport.Description)}"
                            );

                        if (settings.Verbose)
                        {
                            table.AddRow(
                            $"Transport Id:                  [mediumturquoise]{AnsiMarkupFormatter.FormatTransportId(transport.Id)}[/]"
                            );

                            table.AddRow(
                            $"Small Image Path:              [mediumturquoise]{transport.SmallImagePath}[/]"
                            );

                            table.AddRow(
                            $"Is API Creatable:              [mediumturquoise]{transport.IsRuntimeCreatableByApps}[/]"
                            );

                            table.AddRow(
                            $"Is Config Creatable:           [mediumturquoise]{transport.IsRuntimeCreatableBySettings}[/]"
                            );

                            table.AddRow(
                            $"Is System Managed:             [mediumturquoise]{transport.IsSystemManaged}[/]"
                            );

                            table.AddRow(
                            $"Is Client Configurable:        [mediumturquoise]{transport.IsClientConfigurable}[/]"
                            );

                            table.AddRow(
                            $"Client Configuration Assembly: [mediumturquoise]{transport.ClientConfigurationAssemblyName}[/]"
                            );

                        }

                        table.AddEmptyRow();
                    }
                }
                else
                {
                    table.AddRow("No transports available. This is not normal.", "");
                }

                AnsiConsole.Write(table);
            });

            return (int)MidiConsoleReturnCode.Success;
        }

    }
}
