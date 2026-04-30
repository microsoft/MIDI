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
            [LocalizedDescription("ParameterBridgeEndpointsEndpointIdA")]
            [CommandOption("-a|--endpoint-id-a")]
            public string EndpointIdA{ get; set; }

            [LocalizedDescription("ParameterBridgeEndpointsEndpointIdB")]
            [CommandOption("-b|--endpoint-id-b")]
            public string EndpointIdB { get; set; }

            [LocalizedDescription("ParameterBridgeEndpointsQuiet")]
            [CommandOption("-q|--quiet")]
            [DefaultValue(false)]
            public bool Quiet { get; set; }


            // TODO: This isn't going to be very useful unless we can provide a way to map the groups
            // from each side. LIke an array of A and B groups
            // or we can maybe provide just a way to map a single group to a single group and have
            // just the A and B side groups specified. That will work better for MIDI 1-style connections

            // consider if we should allow mapping to the same endpoint, but different groups. Think through
            // how that would work


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
