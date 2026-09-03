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
    // Hosts this PC runs, which remote devices connect to.
    [Cmdlet(VerbsCommon.Get, "MidiNetworkConfiguredHost")]
    [OutputType(typeof(MidiNetworkConfiguredHost))]
    public class CommandGetMidiNetworkConfiguredHost : MidiCmdletBase
    {
        [Parameter(Mandatory = false, Position = 0, ValueFromPipelineByPropertyName = true)]
        public Guid HostId { get; set; }

        protected override void ProcessRecord()
        {
            RequireMidiServices();
            RequireTransport(MidiNetworkTransportManager.IsTransportAvailable, "Network MIDI 2.0");

            var hosts = MidiNetworkTransportManager.GetConfiguredHosts();

            if (hosts is null)
            {
                return;
            }

            foreach (var host in hosts)
            {
                if (HostId != Guid.Empty && host.HostId != HostId)
                {
                    continue;
                }

                WriteObject(host);
            }
        }
    }

}
