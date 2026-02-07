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
            [LocalizedDescription("ParameterCreateLoopbackNameA")]
            [CommandOption("-a|--name-a")]
            public string? NameA { get; set; }

            [LocalizedDescription("ParameterCreateLoopbackNameB")]
            [CommandOption("-b|--name-b")]
            public string? NameB { get; set; }


            [LocalizedDescription("ParameterCreateLoopbackUniqueIdentifier")]
            [CommandOption("-u|--unique-identifier")]
            public string? UniqueId { get; set; }


            [LocalizedDescription("ParameterCreateLoopbackAssociationId")]
            [CommandOption("-a|--association-id")]
            public Guid? AssociationId { get; set; }
        }

        public override ValidationResult Validate(CommandContext context, Settings settings)
        {
            if (string.IsNullOrEmpty(settings.NameA))
            {
                return ValidationResult.Error("Missing Name A. Please provide the names for both ends of the loopback connection.");
            }

            if (string.IsNullOrEmpty(settings.NameB))
            {
                return ValidationResult.Error("Missing Name B. Please provide the names for both ends of the loopback connection.");
            }

            settings.NameA = settings.NameA.Trim().Substring(0, 32);    // fit WinMM name lengths
            settings.NameB = settings.NameB.Trim().Substring(0, 32);    // fit WinMM name lengths

            if (settings.NameA.ToLower() == settings.NameB.ToLower())
            {
                return ValidationResult.Error("The first 32 characters of each name cannot be the same.");
            }

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


            var creationConfig = new MidiLoopbackEndpointCreationConfig();

            var definitionA = new MidiLoopbackEndpointDefinition()
            {
                Name = settings.NameA,
            };

            var definitionB = new MidiLoopbackEndpointDefinition()
            {
                Name = settings.NameB,                              
            };

            creationConfig.AssociationId = associationId;
            creationConfig.EndpointDefinitionA = definitionA;
            creationConfig.EndpointDefinitionB = definitionB;

            var creationResult = MidiLoopbackEndpointManager.CreateTransientLoopbackEndpoints(creationConfig);

            if (creationResult.Success)
            {
                // todo: show details of created loopback endpoint, using a table.

                Console.WriteLine("TEMP: loopback created.");


                return (int)MidiConsoleReturnCode.Success;
            }
            else
            {
                Console.WriteLine("TEMP: Error creating loopback.");

                // todo: display error

                return (int)MidiConsoleReturnCode.ErrorGeneralFailure;
            }

        }

    }
}
