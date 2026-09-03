// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

using System.Management.Automation;

using Windows.Devices.Midi2.Reporting;

namespace WindowsMidiServices
{
    // Every MIDI session open on this PC, from any application, not just this one.
    [Cmdlet(VerbsCommon.Get, "MidiSession")]
    [OutputType(typeof(MidiSessionInfo))]
    public class CommandGetMidiSession : MidiCmdletBase
    {
        protected override void ProcessRecord()
        {
            RequireMidiServices();

            var sdkSessions = MidiReporting.GetActiveSessions();

            if (sdkSessions is null)
            {
                return;
            }

            foreach (var sdkSession in sdkSessions)
            {
                WriteObject(new MidiSessionInfo(sdkSession));
            }
        }
    }

}
