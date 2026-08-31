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
    internal class ListLoopbackCommand : Command<ListLoopbackCommand.Settings>
    {
        internal class Settings : CommandSettings
        {
        }

        public override int Execute(CommandContext context, Settings settings, CancellationToken cancellationToken)
        {
            LoggingService.Current.LogInfo("Enter Execute Command");

            if (!MidiLoopbackManager.IsTransportAvailable)
            {
                AnsiConsole.MarkupLine(AnsiMarkupFormatter.FormatError(Strings.ErrorLoopbackTransportNotAvailable));

                return (int)MidiConsoleReturnCode.ErrorGeneralFailure;
            }

            // An older service may not answer this command, and asking first gives a clear
            // message instead of an empty list which reads as "you have no loopbacks".
            if (!MidiServiceTransportPluginConfigManager.QueryCapability(
                    MidiLoopbackManager.TransportId,
                    MidiServiceTransportCommonCommands.ListEntries))
            {
                AnsiConsole.MarkupLine(AnsiMarkupFormatter.FormatError(Strings.ErrorLoopbackListNotSupported));

                return (int)MidiConsoleReturnCode.ErrorGeneralFailure;
            }

            var entries = MidiLoopbackManager.GetActiveLoopbackEntries();

            if (entries == null || entries.Count == 0)
            {
                AnsiConsole.MarkupLine(AnsiMarkupFormatter.FormatGeneralDetailMessage(Strings.MessageNoLoopbacksFound));

                return (int)MidiConsoleReturnCode.Success;
            }

            var savedIds = LoopbackConfigReader.GetSavedAssociationIds(MidiLoopbackManager.TransportId);

            var table = new Table();
            AnsiMarkupFormatter.SetTableBorderStyle(table);

            table.AddColumn(AnsiMarkupFormatter.FormatTableColumnHeading(Strings.LabelAssociationId));
            table.AddColumn(AnsiMarkupFormatter.FormatTableColumnHeading("A"));
            table.AddColumn(AnsiMarkupFormatter.FormatTableColumnHeading("B"));
            table.AddColumn(AnsiMarkupFormatter.FormatTableColumnHeading("Muted"));
            table.AddColumn(AnsiMarkupFormatter.FormatTableColumnHeading("Saved"));

            foreach (var entry in entries)
            {
                table.AddRow(
                    AnsiMarkupFormatter.FormatEndpointAssociationId(entry.AssociationId),
                    AnsiMarkupFormatter.FormatEndpointName(entry.EndpointA.Name),
                    AnsiMarkupFormatter.FormatEndpointName(entry.EndpointB.Name),
                    entry.IsMuted ? "yes" : "no",
                    savedIds.Contains(entry.AssociationId) ? "[green]yes[/]" : "no");
            }

            AnsiConsole.Write(table);
            AnsiConsole.WriteLine();

            AnsiConsole.MarkupLine(AnsiMarkupFormatter.FormatGeneralDetailMessage(Strings.MessageLoopbackListDetails));

            return (int)MidiConsoleReturnCode.Success;
        }
    }


    internal class ListBasicLoopbackCommand : Command<ListBasicLoopbackCommand.Settings>
    {
        internal class Settings : CommandSettings
        {
        }

        public override int Execute(CommandContext context, Settings settings, CancellationToken cancellationToken)
        {
            LoggingService.Current.LogInfo("Enter Execute Command");

            if (!MidiBasicLoopbackManager.IsTransportAvailable)
            {
                AnsiConsole.MarkupLine(AnsiMarkupFormatter.FormatError(Strings.ErrorLoopbackTransportNotAvailable));

                return (int)MidiConsoleReturnCode.ErrorGeneralFailure;
            }

            if (!MidiServiceTransportPluginConfigManager.QueryCapability(
                    MidiBasicLoopbackManager.TransportId,
                    MidiServiceTransportCommonCommands.ListEntries))
            {
                AnsiConsole.MarkupLine(AnsiMarkupFormatter.FormatError(Strings.ErrorLoopbackListNotSupported));

                return (int)MidiConsoleReturnCode.ErrorGeneralFailure;
            }

            var entries = MidiBasicLoopbackManager.GetActiveLoopbackEntries();

            if (entries == null || entries.Count == 0)
            {
                AnsiConsole.MarkupLine(AnsiMarkupFormatter.FormatGeneralDetailMessage(Strings.MessageNoLoopbacksFound));

                return (int)MidiConsoleReturnCode.Success;
            }

            var savedIds = LoopbackConfigReader.GetSavedAssociationIds(MidiBasicLoopbackManager.TransportId);

            var table = new Table();
            AnsiMarkupFormatter.SetTableBorderStyle(table);

            table.AddColumn(AnsiMarkupFormatter.FormatTableColumnHeading(Strings.LabelAssociationId));
            table.AddColumn(AnsiMarkupFormatter.FormatTableColumnHeading("Name"));
            table.AddColumn(AnsiMarkupFormatter.FormatTableColumnHeading("Muted"));
            table.AddColumn(AnsiMarkupFormatter.FormatTableColumnHeading("Saved"));

            foreach (var entry in entries)
            {
                table.AddRow(
                    AnsiMarkupFormatter.FormatEndpointAssociationId(entry.AssociationId),
                    AnsiMarkupFormatter.FormatEndpointName(entry.Name),
                    entry.IsMuted ? "yes" : "no",
                    savedIds.Contains(entry.AssociationId) ? "[green]yes[/]" : "no");
            }

            AnsiConsole.Write(table);
            AnsiConsole.WriteLine();

            AnsiConsole.MarkupLine(AnsiMarkupFormatter.FormatGeneralDetailMessage(Strings.MessageLoopbackListDetails));

            return (int)MidiConsoleReturnCode.Success;
        }
    }
}
