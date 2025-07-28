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

namespace WindowsMidiServices
{
    [Cmdlet(VerbsLifecycle.Start, "MidiBeatClock")]
    public class CommandStartMidiBeatClock : Cmdlet
    {
        [Parameter(Mandatory = true, Position = 0)]
        public MidiEndpointConnection Connection { get; set; }


        [Parameter(Mandatory = true, Position = 1)]
        public byte[] GroupNumbers { get; set; }


        [Parameter(Mandatory = false, Position = 2)]
        public double Tempo { get; set; }


        [Parameter(Mandatory = false, Position = 3)]
        public bool SendStartMessage { get; set; }

        [Parameter(Mandatory = false, Position = 4)]
        public bool SendStopMessage { get; set; }



        [Parameter(Mandatory = false, Position = 5)]
        public ushort PulsesPerQuarterNote { get; set; }


        public CommandStartMidiBeatClock()
        {
            PulsesPerQuarterNote = 24;
            Tempo = 120;
            SendStartMessage = false; 
            SendStopMessage = false;
        }

        protected override void ProcessRecord()
        {
            // todo: return value

            List<MidiGroup> groups = new List<MidiGroup>();

            foreach (var number in GroupNumbers)
            {
                if (MidiGroup.IsValidIndex(Convert.ToByte(number -1)))
                {
                    groups.Add(new MidiGroup(Convert.ToByte(number - 1)));
                }
                else
                {
                    throw new ArgumentException($"Invalid group number '{number}'. Group numbers must be between 1 and 16, inclusive.");
                }
            }

            var destinations = new List<MidiClockDestination>();
            destinations.Add(new MidiClockDestination(Connection.BackingConnection, groups));

            var backingClock = new Microsoft.Windows.Devices.Midi2.Utilities.Sequencing.MidiClockGenerator(destinations, Tempo, PulsesPerQuarterNote);
            var result = new MidiClockGenerator(backingClock, SendStopMessage);

            backingClock.Start(SendStartMessage);

            WriteObject(result);
        }

    }
}

