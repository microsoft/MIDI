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

namespace WindowsMidiServices
{
    public class MidiSession
    {
        public Guid SessionId => BackingSession.SessionId;
        public string Name => BackingSession.Name;

        public bool IsValid => (bool)(BackingSession != null && BackingSession.IsOpen == true);

        internal Microsoft.Windows.Devices.Midi2.MidiSession? BackingSession { get; set; }

        public MidiSession(Microsoft.Windows.Devices.Midi2.MidiSession backingSession)
        {
            BackingSession = backingSession;
        }

        ~MidiSession()
        {
            if (BackingSession != null)
            {
                //BackingSession.Dispose();
                BackingSession = null;
            }
        }
    }
}
