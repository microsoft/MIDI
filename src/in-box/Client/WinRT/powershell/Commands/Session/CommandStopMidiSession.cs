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

    [Cmdlet(VerbsLifecycle.Stop, "MidiSession", SupportsShouldProcess = true)]
    public class CommandStopMidiSession : MidiCmdletBase
    {
        [Parameter(Mandatory = true, Position = 0, ValueFromPipeline = true)]
        public MidiSession? Session { get; set; }

        protected override void ProcessRecord()
        {
            if (Session is null)
            {
                ThrowTerminating(
                    new ArgumentNullException(nameof(Session)),
                    "MidiSessionRequired",
                    ErrorCategory.InvalidArgument);

                return;
            }

            if (Session.BackingSession is null)
            {
                WriteVerbose(Strings.SessionAlreadyStopped);
                return;
            }

            if (!ShouldProcess(Session.Name, Strings.SessionStopAction))
            {
                return;
            }

            // Closing the session also closes every endpoint connection opened from it.
            Session.BackingSession.Dispose();
            Session.BackingSession = null;

            WriteVerbose(Strings.SessionStopped);
        }


    }




}
