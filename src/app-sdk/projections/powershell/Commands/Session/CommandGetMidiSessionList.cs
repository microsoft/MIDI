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

    [Cmdlet(VerbsCommon.Get, "MidiSessionList")]
    public class CommandGetMidiSessionList : Cmdlet
    {
        protected override void ProcessRecord()
        {
            var sdkSessions = Microsoft.Windows.Devices.Midi2.Reporting.MidiReporting.GetActiveSessions();

            List<MidiSessionInfo> sessions = [];

            foreach (var sdkSession in sdkSessions)
            {
                sessions.Add(new MidiSessionInfo(sdkSession));
            }

            WriteObject(sessions.AsReadOnly());
        }
    }


}