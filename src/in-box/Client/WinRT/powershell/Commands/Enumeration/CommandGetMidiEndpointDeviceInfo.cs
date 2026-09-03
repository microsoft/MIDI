// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================


using System.Management.Automation;

using Windows.Devices.Midi2.Enumeration;

namespace WindowsMidiServices
{

    [Cmdlet(VerbsCommon.Get, "MidiEndpointDeviceInfo")]
    [OutputType(typeof(MidiEndpointDeviceInfo))]
    public class CommandGetMidiEndpointDeviceInfo : MidiCmdletBase
    {
        // Omitting this returns every endpoint on the PC.
        [Parameter(Mandatory = false, Position = 0, ValueFromPipelineByPropertyName = true)]
        public string? EndpointDeviceId
        {
            get; set;
        }

        protected override void ProcessRecord()
        {
            RequireMidiServices();

            if (string.IsNullOrWhiteSpace(EndpointDeviceId))
            {
                foreach (var sdkDevice in MidiEndpointDeviceInformation.FindAll())
                {
                    WriteObject(new MidiEndpointDeviceInfo(sdkDevice));
                }

                return;
            }

            var device = MidiEndpointDeviceInformation.CreateFromEndpointDeviceId(EndpointDeviceId);

            if (device is null)
            {
                WriteNonTerminating(
                    new ItemNotFoundException($"No MIDI endpoint was found with the identifier \"{EndpointDeviceId}\"."),
                    "MidiEndpointNotFound",
                    ErrorCategory.ObjectNotFound,
                    EndpointDeviceId);

                return;
            }

            WriteObject(new MidiEndpointDeviceInfo(device));
        }
    }

}
