// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

using System.Management.Automation;

using Windows.Devices.Midi2.Transports.Loopback;

namespace WindowsMidiServices
{

    [Cmdlet(VerbsCommon.Get, "MidiLoopback")]
    [OutputType(typeof(MidiLoopbackEntry))]
    public class CommandGetMidiLoopback : MidiCmdletBase
    {
        [Parameter(Mandatory = false, Position = 0, ValueFromPipelineByPropertyName = true)]
        public Guid AssociationId { get; set; }

        protected override void ProcessRecord()
        {
            RequireMidiServices();
            RequireTransport(MidiLoopbackManager.IsTransportAvailable, "MIDI 2.0 loopback");

            var entries = MidiLoopbackManager.GetActiveLoopbackEntries();

            if (entries is null)
            {
                return;
            }

            foreach (var entry in entries)
            {
                if (AssociationId != Guid.Empty && entry.AssociationId != AssociationId)
                {
                    continue;
                }

                WriteObject(entry);
            }
        }
    }

}
