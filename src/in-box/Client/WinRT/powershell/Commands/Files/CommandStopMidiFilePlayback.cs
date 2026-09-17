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
    // Ends playback started with Start-MidiFilePlayback -NoWait, silencing the instrument on the
    // way out.
    [Cmdlet(VerbsLifecycle.Stop, "MidiFilePlayback", SupportsShouldProcess = true)]
    public class CommandStopMidiFilePlayback : MidiCmdletBase
    {
        [Parameter(Mandatory = true, Position = 0, ValueFromPipeline = true)]
        public MidiFilePlayback? Playback { get; set; }

        protected override void ProcessRecord()
        {
            if (Playback is null)
            {
                ThrowTerminating(
                    new ArgumentNullException(nameof(Playback)),
                    "MidiFilePlaybackRequired",
                    ErrorCategory.InvalidArgument);

                return;
            }

            if (Playback.BackingPlayer is null)
            {
                WriteVerbose("Playback had already stopped.");
                return;
            }

            if (!ShouldProcess(Playback.FilePath, "Stop MIDI file playback"))
            {
                return;
            }

            Playback.StopAndRelease();
            GC.SuppressFinalize(Playback);

            WriteVerbose("MIDI file playback stopped.");
        }
    }
}
