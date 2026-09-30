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
    // What the synthesizer's sound set contains: its name and version, how many instruments it
    // has, and its drum kits. The melodic instruments themselves come from Get-MidiSynthInstrument.
    [Cmdlet(VerbsCommon.Get, "MidiSynthSoundSet")]
    [OutputType(typeof(MidiSynthSoundSetInfo))]
    public class CommandGetMidiSynthSoundSet : MidiCmdletBase
    {
        protected override void ProcessRecord()
        {
            RequireMidiServices();
            RequireTransport(MidiSynthManager.IsTransportAvailable, Strings.TransportNameSynth);

            var soundSet = MidiSynthManager.GetSoundSetInfo();

            if (soundSet is null)
            {
                WriteNonTerminating(
                    new InvalidOperationException(Strings.SynthSoundSetUnavailable),
                    "MidiSynthSoundSetUnavailable",
                    ErrorCategory.ResourceUnavailable);

                return;
            }

            WriteObject(soundSet);
        }
    }
}
