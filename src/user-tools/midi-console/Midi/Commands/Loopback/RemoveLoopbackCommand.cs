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
    internal class RemoveLoopbackCommand : Command<RemoveLoopbackCommand.Settings>
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
            var config = new MidiLoopbackEndpointRemovalConfig(settings.AssociationId);

            var removalSuccess = MidiLoopbackEndpointManager.RemoveTransientLoopbackEndpoints(config);

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
