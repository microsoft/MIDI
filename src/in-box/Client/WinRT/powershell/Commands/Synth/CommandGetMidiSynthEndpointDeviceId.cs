// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

using System.Management.Automation;

using Windows.Devices.Midi2.Transports.Synth;

namespace WindowsMidiServices
{
    // The synthesizer has exactly one endpoint, so it can be named without enumerating anything.
    [Cmdlet(VerbsCommon.Get, "MidiSynthEndpointDeviceId")]
    [OutputType(typeof(string))]
    public class CommandGetMidiSynthEndpointDeviceId : MidiCmdletBase
    {
        protected override void ProcessRecord()
        {
            RequireMidiServices();
            RequireTransport(MidiSynthManager.IsTransportAvailable, Strings.TransportNameSynth);

            var endpointDeviceId = MidiSynthManager.EndpointDeviceId;

            if (string.IsNullOrEmpty(endpointDeviceId))
            {
                // Switched off is the ordinary reason, and it is not an error worth stopping a
                // script over.
                WriteVerbose(Strings.SynthNoEndpoint);

                return;
            }

            WriteObject(endpointDeviceId);
        }
    }
}
