// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================
using Microsoft.Windows.Devices.Midi2.Endpoints;
using Microsoft.Windows.Devices.Midi2.Endpoints.BasicLoopback;
using Windows.Foundation;

namespace Microsoft.Midi.ConsoleApp
{
    internal class CreateBasicLoopbackCommand : Command<CreateBasicLoopbackCommand.Settings>
    {
        internal class Settings : CommandSettings
        {
            // TODO: Add -r|--root-name as an option to name A/B and make name A/B nullable


            [LocalizedDescription("ParameterCreateBasicLoopbackName")]
            [CommandOption("-n|--name")]
            public string? Name { get; set; }


            [LocalizedDescription("ParameterCreateBasicLoopbackUniqueIdentifier")]
            [CommandOption("-u|--unique-identifier")]
            public string? UniqueId { get; set; }


            [LocalizedDescription("ParameterCreateBasicLoopbackAssociationId")]
            [CommandOption("-i|--association-id")]
            public Guid? AssociationId { get; set; }
        }

        const int MaxWinMMPortNameLength = 31;

        public override ValidationResult Validate(CommandContext context, Settings settings)
        {
            if (!string.IsNullOrEmpty(settings.Name))
            {
                settings.Name = settings.Name.Trim();
                settings.Name = settings.Name.Substring(0, Math.Min(settings.Name.Length, MaxWinMMPortNameLength));
            }


            if (string.IsNullOrEmpty(settings.Name))
            {
                return ValidationResult.Error("Missing Name. Please provide the name for this basic loopback endpoint.");
            }

            // TODO: Validate the loopback unique id, if provided, is not already in use

            // TODO: Validate the loopback endpoint name is not already in use

            // TODO: validate the association id, if provided, is not already in use

            return ValidationResult.Success();
        }


        private string CleanupUniqueId(string source)
        {
            string result = string.Empty;

            foreach (char ch in source.ToCharArray())
            {
                if (char.IsAsciiLetterOrDigit(ch))
                {
                    result += ch;
                }
            }

            return result;
        }
        public override int Execute(CommandContext context, Settings settings, CancellationToken cancellationToken)
        {
            LoggingService.Current.LogInfo("Enter Execute Command");

            Guid associationId;
            string uniqueId;

            if (settings.AssociationId is null || settings.AssociationId == Guid.Empty)
            {
                associationId = Guid.NewGuid();
            }
            else
            {
                associationId = settings.AssociationId.GetValueOrDefault();
            }

            if (string.IsNullOrEmpty(settings.UniqueId))
            {
                uniqueId = CleanupUniqueId(GuidHelper.CreateNewGuid().ToString());
            }
            else
            {
                uniqueId = CleanupUniqueId(settings.UniqueId);
            }


            var creationConfig = new MidiBasicLoopbackEndpointCreationConfig();

            var definition = new MidiBasicLoopbackEndpointDefinition()
            {
                Name = settings.Name,
                UniqueId = uniqueId,
            };

            creationConfig.AssociationId = associationId;
            creationConfig.EndpointDefinition = definition;

            var creationResult = MidiBasicLoopbackEndpointManager.CreateTransientLoopbackEndpoint(creationConfig);

            if (creationResult.Success)
            {
                // todo: show details of created loopback endpoint, using a table.
                // explain that they will go away with next service restart

                var endpoint = MidiEndpointDeviceInformation.CreateFromEndpointDeviceId(creationResult.EndpointDeviceId);

                AnsiConsole.MarkupLine(AnsiMarkupFormatter.FormatSuccess(Strings.MessageBasicLoopbackCreationSuccess));
                AnsiConsole.WriteLine();
                AnsiConsole.MarkupLine(AnsiMarkupFormatter.FormatGeneralDetailMessage(Strings.MessageBasicLoopbackCreationSuccessDetails));
                AnsiConsole.WriteLine();

                var table = new Table();
                AnsiMarkupFormatter.SetTableBorderStyle(table);

                table.AddColumn("");
                table.AddColumn("");

                table.ShowHeaders = false;

                table.AddRow(AnsiMarkupFormatter.FormatFriendlyPropertyKey(Strings.LabelLoopbackEndpointIdentifier), AnsiMarkupFormatter.FormatEndpointName(endpoint.Name));
                table.AddRow("", AnsiMarkupFormatter.FormatFullEndpointInterfaceId(endpoint.EndpointDeviceId));
                table.AddEmptyRow();

                table.AddRow(AnsiMarkupFormatter.FormatFriendlyPropertyKey(Strings.LabelAssociationId), AnsiMarkupFormatter.FormatEndpointAssociationId(creationResult.AssociationId));

                AnsiConsole.Write(table);

                AnsiConsole.WriteLine();

                return (int)MidiConsoleReturnCode.Success;
            }
            else
            {
                AnsiConsole.WriteLine(AnsiMarkupFormatter.FormatError(Strings.ErrorCreatingLoopback));
                AnsiConsole.WriteLine(AnsiMarkupFormatter.FormatError(creationResult.ErrorInformation));

                return (int)MidiConsoleReturnCode.ErrorGeneralFailure;
            }

        }

    }
}
