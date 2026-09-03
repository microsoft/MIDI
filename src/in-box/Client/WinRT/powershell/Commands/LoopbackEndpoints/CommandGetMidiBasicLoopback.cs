// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

using System.Management.Automation;

using Windows.Devices.Midi2.Transports.BasicLoopback;

namespace WindowsMidiServices
{

    [Cmdlet(VerbsCommon.Get, "MidiBasicLoopback")]
    [OutputType(typeof(MidiBasicLoopbackEntry))]
    public class CommandGetMidiBasicLoopback : MidiCmdletBase
    {
        [Parameter(Mandatory = false, Position = 0, ValueFromPipelineByPropertyName = true)]
        public Guid AssociationId { get; set; }

        protected override void ProcessRecord()
        {
            RequireMidiServices();
            RequireTransport(MidiBasicLoopbackManager.IsTransportAvailable, "MIDI 1.0 basic loopback");

            var entries = MidiBasicLoopbackManager.GetActiveLoopbackEntries();

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
