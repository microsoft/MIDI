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

    [Cmdlet(VerbsCommon.Get, "MidiEndpointGroups")]
    public class CommandGetMidiEndpointGroups : Cmdlet
    {
        protected override void ProcessRecord()
        {
            // take in the id, return all the active groups with their GTB or function block names


        }
    }

}
