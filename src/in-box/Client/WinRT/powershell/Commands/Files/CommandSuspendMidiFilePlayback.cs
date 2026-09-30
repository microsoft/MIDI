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
    // Pauses playback started with Start-MidiFilePlayback -NoWait. Notes which are sounding are
    // silenced. Resume-MidiFilePlayback carries on from the same place.
    [Cmdlet(VerbsLifecycle.Suspend, "MidiFilePlayback", SupportsShouldProcess = true)]
    public class CommandSuspendMidiFilePlayback : MidiCmdletBase
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

            if (!ShouldProcess(Playback.FilePath, Strings.PlaybackPauseAction))
            {
                return;
            }

            if (!Playback.Pause())
            {
                WriteNonTerminating(
                    new InvalidOperationException(Format(Strings.PlaybackNotPlayingFormat, Playback.State)),
                    "MidiFilePlaybackNotPlaying",
                    ErrorCategory.InvalidOperation,
                    Playback.FilePath);

                return;
            }

            WriteVerbose(Strings.PlaybackPaused);
        }
    }
}
