// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================


using Microsoft.Windows.Devices.Midi2;
using Microsoft.Windows.Devices.Midi2.Utilities.Sequencing;
using System;
using System.Collections.Generic;
using System.Linq;
using System.Management.Automation;
using System.Text;
using System.Threading.Tasks;

#pragma warning disable CS8305

namespace WindowsMidiServices
{
    [Cmdlet(VerbsLifecycle.Stop, "MidiBeatClock")]
    public class CommandStopMidiBeatClock : Cmdlet
    {
        [Parameter(Mandatory = true, Position = 0)]
        public WindowsMidiServices.MidiClockGenerator? ClockGenerator { get; set; }


        protected override void ProcessRecord()
        {
            if (ClockGenerator != null)
            {
                if (ClockGenerator.BackingClockGenerator != null)
                {
                    ClockGenerator.BackingClockGenerator.Stop(ClockGenerator.SendStopMessage);
                }
            }
        }

    }
}

