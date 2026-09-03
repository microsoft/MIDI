// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

using System.Management.Automation;

using Windows.Devices.Midi2.Transports.Network;

namespace WindowsMidiServices
{
    // The remote hosts this PC can currently see advertised on the network. A host which is
    // powered off or on another subnet simply does not appear.
    [Cmdlet(VerbsCommon.Get, "MidiNetworkAdvertisedHost")]
    [OutputType(typeof(MidiNetworkAdvertisedHost))]
    public class CommandGetMidiNetworkAdvertisedHost : MidiCmdletBase
    {
        protected override void ProcessRecord()
        {
            RequireMidiServices();
            RequireTransport(MidiNetworkTransportManager.IsTransportAvailable, "Network MIDI 2.0");

            var hosts = MidiNetworkTransportManager.GetAdvertisedHosts();

            if (hosts is null)
            {
                return;
            }

            foreach (var host in hosts)
            {
                WriteObject(host);
            }
        }
    }

}
