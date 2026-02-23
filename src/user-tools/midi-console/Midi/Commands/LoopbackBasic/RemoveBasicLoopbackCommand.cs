// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================


using Microsoft.Windows.Devices.Midi2.Endpoints.BasicLoopback;
using Windows.Foundation;

namespace Microsoft.Midi.ConsoleApp
{
    internal class RemoveBasicLoopbackCommand : Command<RemoveBasicLoopbackCommand.Settings>
    {
        internal class Settings : CommandSettings
        {
            [LocalizedDescription("ParameterRemoveLoopbackAssociationId")]
            [CommandOption("-i|--association-id")]
            public Guid AssociationId { get; set; }
        }

        public override ValidationResult Validate(CommandContext context, Settings settings)
        {

            return ValidationResult.Success();
        }


        public override int Execute(CommandContext context, Settings settings, CancellationToken cancellationToken)
        {
            LoggingService.Current.LogInfo("Enter Execute Command");

            var config = new MidiBasicLoopbackEndpointRemovalConfig(settings.AssociationId);

            var removalSuccess = MidiBasicLoopbackEndpointManager.RemoveTransientLoopbackEndpoint(config);

            if (removalSuccess)
            {
                AnsiConsole.MarkupLine(AnsiMarkupFormatter.FormatSuccess(Strings.MessageLoopbackRemovalSuccess));
                AnsiConsole.WriteLine();
                return (int)MidiConsoleReturnCode.Success;
            }
            else
            {
                AnsiConsole.WriteLine(AnsiMarkupFormatter.FormatError(Strings.ErrorRemovingLoopback));

                return (int)MidiConsoleReturnCode.ErrorGeneralFailure;
            }

        }

    }
}
