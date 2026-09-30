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
    // The melodic instruments in the synthesizer's sound set, with the bank select and program
    // change that choose each one. Drum kits are chosen by a program change on a drum channel
    // instead, so Get-MidiSynthSoundSet lists those.
    [Cmdlet(VerbsCommon.Get, "MidiSynthInstrument")]
    [OutputType(typeof(MidiSynthInstrumentInfo))]
    public class CommandGetMidiSynthInstrument : MidiCmdletBase
    {
        protected override void ProcessRecord()
        {
            RequireMidiServices();
            RequireTransport(MidiSynthManager.IsTransportAvailable, Strings.TransportNameSynth);

            var instruments = MidiSynthManager.GetMelodicInstruments();

            if (instruments is null)
            {
                WriteNonTerminating(
                    new InvalidOperationException(Strings.SynthInstrumentsUnavailable),
                    "MidiSynthInstrumentsUnavailable",
                    ErrorCategory.ResourceUnavailable);

                return;
            }

            foreach (var instrument in instruments)
            {
                WriteObject(instrument);
            }
        }
    }
}
