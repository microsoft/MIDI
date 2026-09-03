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


    [Cmdlet(VerbsLifecycle.Start, "MidiSession")]
    [OutputType(typeof(MidiSession))]
    public class CommandStartMidiSession : MidiCmdletBase
    {
        [Parameter(Mandatory = true, Position = 0)]
        [ValidateNotNullOrWhiteSpace]
        public string Name { get; set; } = string.Empty;

        protected override void ProcessRecord()
        {
            RequireMidiServices();

            var backingSession = Windows.Devices.Midi2.MidiSession.Create(Name);

            if (backingSession is null)
            {
                ThrowTerminating(
                    new InvalidOperationException($"Unable to create the MIDI session \"{Name}\"."),
                    "MidiSessionCreationFailed",
                    ErrorCategory.ResourceUnavailable,
                    Name);

                return;
            }

            WriteVerbose("MIDI session started.");
            WriteObject(new MidiSession(backingSession));
        }
    }


}
