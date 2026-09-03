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
    // there's an initialize verb, but no corresponding
    // terminate or shutdown, so we'll just use start/stop

    [Cmdlet(VerbsLifecycle.Start, "Midi")]
    public class CommandStartMidi : MidiCmdletBase
    {
        protected override void ProcessRecord()
        {
            RequireMidiServices();

            WriteVerbose("Windows MIDI Services is available.");
        }

    }

}
