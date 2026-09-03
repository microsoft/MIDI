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
    // Connections this PC makes out to remote hosts. An entry is reported even when it is not
    // currently connected, so EntryState is what says whether it is usable.
    [Cmdlet(VerbsCommon.Get, "MidiNetworkConfiguredClient")]
    [OutputType(typeof(MidiNetworkConfiguredClient))]
    public class CommandGetMidiNetworkConfiguredClient : MidiCmdletBase
    {
        [Parameter(Mandatory = false, Position = 0, ValueFromPipelineByPropertyName = true)]
        public Guid ClientId { get; set; }

        protected override void ProcessRecord()
        {
            RequireMidiServices();
            RequireTransport(MidiNetworkTransportManager.IsTransportAvailable, "Network MIDI 2.0");

            var clients = MidiNetworkTransportManager.GetConfiguredClients();

            if (clients is null)
            {
                return;
            }

            foreach (var client in clients)
            {
                if (ClientId != Guid.Empty && client.ClientId != ClientId)
                {
                    continue;
                }

                WriteObject(client);
            }
        }
    }

}
