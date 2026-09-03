// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

using System.Management.Automation;

using Windows.Devices.Midi2.Enumeration;
using Windows.Devices.Midi2.Enumeration.Legacy;

namespace WindowsMidiServices
{
    // The MIDI 1.0 ports the older Windows MIDI APIs (WinMM and WinRT MIDI 1.0) see. These are
    // created by Windows MIDI Services alongside the UMP endpoints they belong to.
    [Cmdlet(VerbsCommon.Get, "MidiLegacyPort", DefaultParameterSetName = AllParameterSet)]
    [OutputType(typeof(MidiLegacyPortDeviceInformation))]
    public class CommandGetMidiLegacyPort : MidiCmdletBase
    {
        private const string AllParameterSet = "All";
        private const string EndpointParameterSet = "Endpoint";
        private const string NameParameterSet = "Name";
        private const string ContainerParameterSet = "Container";
        private const string PortDeviceIdParameterSet = "PortDeviceId";

        [Parameter(Mandatory = true, Position = 0, ValueFromPipelineByPropertyName = true, ParameterSetName = EndpointParameterSet)]
        [ValidateNotNullOrWhiteSpace]
        public string EndpointDeviceId { get; set; } = string.Empty;

        [Parameter(Mandatory = true, Position = 0, ParameterSetName = NameParameterSet)]
        [ValidateNotNullOrWhiteSpace]
        public string Name { get; set; } = string.Empty;

        [Parameter(Mandatory = true, Position = 0, ValueFromPipelineByPropertyName = true, ParameterSetName = ContainerParameterSet)]
        public Guid ContainerId { get; set; }

        [Parameter(Mandatory = true, Position = 0, ValueFromPipelineByPropertyName = true, ParameterSetName = PortDeviceIdParameterSet)]
        [ValidateNotNullOrWhiteSpace]
        public string PortDeviceId { get; set; } = string.Empty;

        // Only the whole list and the per-endpoint list can be filtered by direction, which is
        // what the underlying enumeration offers.
        [Parameter(ParameterSetName = AllParameterSet)]
        [Parameter(ParameterSetName = EndpointParameterSet)]
        public Midi1PortFlow? Flow { get; set; }

        protected override void ProcessRecord()
        {
            RequireMidiServices();

            if (ParameterSetName == PortDeviceIdParameterSet)
            {
                var port = MidiLegacyPortDeviceInformation.CreateFromPortDeviceId(PortDeviceId);

                if (port is null)
                {
                    WriteNonTerminating(
                        new ItemNotFoundException($"No MIDI 1.0 port was found with the identifier \"{PortDeviceId}\"."),
                        "MidiLegacyPortNotFound",
                        ErrorCategory.ObjectNotFound,
                        PortDeviceId);

                    return;
                }

                WriteObject(port);
                return;
            }

            var ports = ParameterSetName switch
            {
                EndpointParameterSet => Flow.HasValue
                    ? MidiLegacyPortDeviceInformation.FindAllForAssociatedEndpoint(EndpointDeviceId, Flow.Value)
                    : MidiLegacyPortDeviceInformation.FindAllForAssociatedEndpoint(EndpointDeviceId),

                NameParameterSet => MidiLegacyPortDeviceInformation.FindAllForName(Name),

                ContainerParameterSet => MidiLegacyPortDeviceInformation.FindAllForContainer(ContainerId),

                _ => Flow.HasValue
                    ? MidiLegacyPortDeviceInformation.FindAll(Flow.Value)
                    : MidiLegacyPortDeviceInformation.FindAll(),
            };

            if (ports is null)
            {
                return;
            }

            foreach (var port in ports)
            {
                WriteObject(port);
            }
        }
    }

}
