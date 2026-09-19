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
    // Everything the built-in synthesizer is set to right now. The endpoint id is empty while it
    // is switched off, because then the endpoint does not exist.
    [Cmdlet(VerbsCommon.Get, "MidiSynth")]
    [OutputType(typeof(MidiSynthStatus))]
    public class CommandGetMidiSynth : MidiCmdletBase
    {
        protected override void ProcessRecord()
        {
            RequireMidiServices();
            RequireTransport(MidiSynthManager.IsTransportAvailable, "General MIDI synthesizer");

            var status = MidiSynthManager.GetStatus();

            if (status is null)
            {
                WriteNonTerminating(
                    new InvalidOperationException("The synthesizer did not report its status."),
                    "MidiSynthStatusUnavailable",
                    ErrorCategory.ResourceUnavailable);

                return;
            }

            WriteObject(status);
        }
    }
}
