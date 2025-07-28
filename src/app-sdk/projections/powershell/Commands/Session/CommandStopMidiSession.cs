// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

using Microsoft.Windows.Devices.Midi2.Initialization;
using System;
using System.Collections.Generic;
using System.Linq;
using System.Management.Automation;
using System.Text;
using System.Threading.Tasks;

namespace WindowsMidiServices
{

    [Cmdlet(VerbsLifecycle.Stop, "MidiSession")]
    public class CommandStopMidiSession : Cmdlet
    {
        [Parameter(Mandatory = true, Position = 0)]
        public MidiSession Session { get; set; }

        protected override void ProcessRecord()
        {
            if (Session.BackingSession != null)
            {
                WriteVerbose("MIDI Session stopped.");
                Session.BackingSession.Dispose();
            }
            else
            {
                WriteVerbose("MIDI Session was not previously started.");
            }

            Session = null;
        }


    }




}
