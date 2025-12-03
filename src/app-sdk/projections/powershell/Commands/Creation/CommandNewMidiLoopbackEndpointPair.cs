// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================


using System.Management.Automation;

namespace WindowsMidiServices
{

    [Cmdlet(VerbsCommon.New, "MidiLoopbackEndpointPair")]
    public class CommandNewMidiLoopbackEndpointPair : Cmdlet
    {
        [Parameter(Mandatory = true, Position = 0)]
        public required string LoopbackBaseName
        {
            get; set;
        }

        [Parameter(Mandatory = true, Position = 1)]
        public required string UniqueIdentifier
        {
            get; set;
        }

        [Parameter(Mandatory = false, Position = 2)]
        public string? Description
        {
            get; set;
        }

        protected override void ProcessRecord()
        {
            var loopbackA = new Microsoft.Windows.Devices.Midi2.Endpoints.Loopback.MidiLoopbackEndpointDefinition($"{LoopbackBaseName} (A)", UniqueIdentifier, Description);
            var loopbackB = new Microsoft.Windows.Devices.Midi2.Endpoints.Loopback.MidiLoopbackEndpointDefinition($"{LoopbackBaseName} (B)", UniqueIdentifier, Description);

            var guid = Guid.NewGuid();
            var creationConfig = new Microsoft.Windows.Devices.Midi2.Endpoints.Loopback.MidiLoopbackEndpointCreationConfig(guid, loopbackA, loopbackB);
            
            var createdEndpoint = Microsoft.Windows.Devices.Midi2.Endpoints.Loopback.MidiLoopbackEndpointManager.CreateTransientLoopbackEndpoints(creationConfig);
            
            WriteObject($"Created new endpoint pair: {LoopbackBaseName} (A), {LoopbackBaseName} (B)");
        }
    }


}
