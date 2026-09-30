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
    // Carries on with playback paused by Suspend-MidiFilePlayback. Each channel's bank, program
    // and controllers are sent again first, so it resumes on the right sounds.
    [Cmdlet(VerbsLifecycle.Resume, "MidiFilePlayback", SupportsShouldProcess = true)]
    public class CommandResumeMidiFilePlayback : MidiCmdletBase
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

            if (!ShouldProcess(Playback.FilePath, Strings.PlaybackResumeAction))
            {
                return;
            }

            if (!Playback.Resume())
            {
                WriteNonTerminating(
                    new InvalidOperationException(Format(Strings.PlaybackNotPausedFormat, Playback.State)),
                    "MidiFilePlaybackNotPaused",
                    ErrorCategory.InvalidOperation,
                    Playback.FilePath);

                return;
            }

            WriteVerbose(Strings.PlaybackResumed);
        }
    }
}
