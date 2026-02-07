// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================


using Microsoft.Windows.Devices.Midi2.Endpoints.Loopback;
using Windows.Foundation;

namespace Microsoft.Midi.ConsoleApp
{
    internal class CreateLoopbackCommand : Command<CreateLoopbackCommand.Settings>
    {
        internal class Settings : CommandSettings
        {
            // TODO: Add -r|--root-name as an option to name A/B and make name A/B nullable


            [LocalizedDescription("ParameterCreateLoopbackNameA")]
            [CommandOption("-a|--name-a")]
            public string? NameA { get; set; }

            [LocalizedDescription("ParameterCreateLoopbackNameB")]
            [CommandOption("-b|--name-b")]
            public string? NameB { get; set; }


            [LocalizedDescription("ParameterCreateLoopbackRootName")]
            [CommandOption("-r|--root-name")]
            public string? RootName { get; set; }


            [LocalizedDescription("ParameterCreateLoopbackUniqueIdentifier")]
            [CommandOption("-u|--unique-identifier")]
            public string? UniqueId { get; set; }


            [LocalizedDescription("ParameterCreateLoopbackAssociationId")]
            [CommandOption("-i|--association-id")]
            public Guid? AssociationId { get; set; }
        }

        // these both need to be the same string length
        // they should also be kept in-line with what the settings app creates
        const string LoopbackASuffix = " (A)";
        const string LoopbackBSuffix = " (B)";
        const int MaxWinMMPortNameLength = 32;

        public override ValidationResult Validate(CommandContext context, Settings settings)
        {
            if (!string.IsNullOrEmpty(settings.RootName))
            {
                settings.RootName = settings.RootName.Trim();
                settings.RootName = settings.RootName.Substring(0, Math.Min(settings.RootName.Length, MaxWinMMPortNameLength - LoopbackASuffix.Length));

                // build the names
                settings.NameA = settings.RootName + LoopbackASuffix;
                settings.NameB = settings.RootName + LoopbackBSuffix;
            }


            if (string.IsNullOrEmpty(settings.NameA))
            {
                return ValidationResult.Error("Missing Name A. Please provide the names for both ends of the loopback connection.");
            }

            if (string.IsNullOrEmpty(settings.NameB))
            {
                return ValidationResult.Error("Missing Name B. Please provide the names for both ends of the loopback connection.");
            }

            settings.NameA = settings.NameA.Trim();
            settings.NameB = settings.NameB.Trim();

            settings.NameA = settings.NameA.Substring(0, Math.Min(settings.NameA.Length, MaxWinMMPortNameLength));    // fit WinMM name lengths
            settings.NameB = settings.NameB.Substring(0, Math.Min(settings.NameB.Length, MaxWinMMPortNameLength));    // fit WinMM name lengths

            if (settings.NameA.ToLower() == settings.NameB.ToLower())
            {
                return ValidationResult.Error("The first 32 characters of each name cannot be the same.");
            }



            // TODO: Validate the loopback unique id, if provided, is not already in use

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


            var creationConfig = new MidiLoopbackEndpointCreationConfig();

            var definitionA = new MidiLoopbackEndpointDefinition()
            {
                Name = settings.NameA,
                UniqueId = uniqueId,
            };

            var definitionB = new MidiLoopbackEndpointDefinition()
            {
                Name = settings.NameB,
                UniqueId = uniqueId,
            };



            creationConfig.AssociationId = associationId;
            creationConfig.EndpointDefinitionA = definitionA;
            creationConfig.EndpointDefinitionB = definitionB;

            var creationResult = MidiLoopbackEndpointManager.CreateTransientLoopbackEndpoints(creationConfig);

            if (creationResult.Success)
            {
                // todo: show details of created loopback endpoint, using a table.
                // explain that they will go away with next service restart

                var endpointA = MidiEndpointDeviceInformation.CreateFromEndpointDeviceId(creationResult.EndpointDeviceIdA);
                var endpointB = MidiEndpointDeviceInformation.CreateFromEndpointDeviceId(creationResult.EndpointDeviceIdB);


                AnsiConsole.MarkupLine(AnsiMarkupFormatter.FormatSuccess(Strings.MessageLoopbackCreationSuccess));
                AnsiConsole.WriteLine();
                AnsiConsole.MarkupLine(AnsiMarkupFormatter.FormatGeneralDetailMessage(Strings.MessageLoopbackCreationSuccessDetails));
                AnsiConsole.WriteLine();

                var table = new Table();
                AnsiMarkupFormatter.SetTableBorderStyle(table);

                table.AddColumn("");
                table.AddColumn("");

                table.ShowHeaders = false;

                table.AddRow(AnsiMarkupFormatter.FormatFriendlyPropertyKey(Strings.LabelLoopbackEndpointIdentifierA), AnsiMarkupFormatter.FormatEndpointName(endpointA.Name));
                table.AddRow("", AnsiMarkupFormatter.FormatFullEndpointInterfaceId(endpointA.EndpointDeviceId));
                table.AddEmptyRow();

                table.AddRow(AnsiMarkupFormatter.FormatFriendlyPropertyKey(Strings.LabelLoopbackEndpointIdentifierB), AnsiMarkupFormatter.FormatEndpointName(endpointB.Name));
                table.AddRow("", AnsiMarkupFormatter.FormatFullEndpointInterfaceId(endpointB.EndpointDeviceId));
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
