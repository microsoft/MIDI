// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

#pragma warning disable CS8305

namespace WindowsMidiServices
{
    public class MidiClockGenerator
    {
        internal Microsoft.Windows.Devices.Midi2.Utilities.Sequencing.MidiClockGenerator? BackingClockGenerator { get; set; }

        internal bool SendStopMessage { get; set; }

        public MidiClockGenerator(
            Microsoft.Windows.Devices.Midi2.Utilities.Sequencing.MidiClockGenerator backingClockGenerator,
            bool sendStopMessage)
        {
            BackingClockGenerator = backingClockGenerator;
            SendStopMessage = sendStopMessage;
        }

    }
}
