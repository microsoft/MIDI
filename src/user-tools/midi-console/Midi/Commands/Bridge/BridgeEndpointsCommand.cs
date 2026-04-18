// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================


namespace Microsoft.Midi.ConsoleApp
{
    internal class BridgeEndpointsCommand : Command<BridgeEndpointsCommand.Settings>
    {
        internal class Settings : CommandSettings
        {




        }


        public override Spectre.Console.ValidationResult Validate(CommandContext context, Settings settings)
        {

            return ValidationResult.Success();
        }

        public override int Execute(CommandContext context, Settings settings, CancellationToken cancellationToken)
        {


            return (int)MidiConsoleReturnCode.Success;
        }

    }
}
