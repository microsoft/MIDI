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
    // Makes any channel a rhythm part, or a melodic one again. Not saved: the content owns this,
    // and a System Reset in a file puts channel 10 back as the only drum channel.
    [Cmdlet(VerbsCommon.Set, "MidiSynthDrumChannel", SupportsShouldProcess = true)]
    public class CommandSetMidiSynthDrumChannel : MidiCmdletBase
    {
        // Zero-based, so channel 10 is index 9.
        [Parameter(Mandatory = true, Position = 0, ValueFromPipeline = true)]
        [ValidateRange(0, 15)]
        public byte ChannelIndex { get; set; }

        [Parameter(Mandatory = true, Position = 1)]
        public bool IsDrumChannel { get; set; }

        protected override void ProcessRecord()
        {
            RequireMidiServices();
            RequireTransport(MidiSynthManager.IsTransportAvailable, Strings.TransportNameSynth);

            var action = IsDrumChannel ? Strings.SynthDrumChannelAction : Strings.SynthMelodicChannelAction;

            if (!ShouldProcess(Format(Strings.SynthChannelTargetFormat, ChannelIndex + 1), action))
            {
                return;
            }

            if (!MidiSynthManager.SetDrumChannel(ChannelIndex, IsDrumChannel))
            {
                WriteNonTerminating(
                    new InvalidOperationException(Strings.SynthDrumChannelFailed),
                    "MidiSynthDrumChannelFailed",
                    ErrorCategory.InvalidOperation,
                    ChannelIndex);
            }
        }
    }
}
