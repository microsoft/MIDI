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

    [Cmdlet(VerbsCommon.Get, "MidiEndpointDeviceInfo")]
    public class CommandGetMidiEndpointDeviceInfo : Cmdlet
    {
        [Parameter(Mandatory = true, Position = 0)]
        public string? EndpointDeviceId
        {
            get; set;
        }

        protected override void ProcessRecord()
        {
            if (string.IsNullOrEmpty(EndpointDeviceId))
            {
                throw new ArgumentNullException("Endpoint Device Id is null or empty.");
            }

            var sdkDevice = Microsoft.Windows.Devices.Midi2.MidiEndpointDeviceInformation.CreateFromEndpointDeviceId(EndpointDeviceId);

            var device = new MidiEndpointDeviceInfo(sdkDevice);

            WriteObject(device);
        }
    }

}
