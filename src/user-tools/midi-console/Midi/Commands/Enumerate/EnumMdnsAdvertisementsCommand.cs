// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

using Microsoft.Windows.Devices.Midi2.Endpoints.Network;

namespace Microsoft.Midi.ConsoleApp
{
    internal class EnumMdnsAdvertisementsCommand : Command<EnumMdnsAdvertisementsCommand.Settings>
    {
        public sealed class Settings : CommandSettings
        {
        }

        public override int Execute(CommandContext context, Settings settings, CancellationToken cancellationToken)
        {



            return 0;
        }
    }
}
