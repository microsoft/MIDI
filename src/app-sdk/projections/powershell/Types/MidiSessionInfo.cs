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
    public class MidiSessionInfo
    {
        public string Name => BackingSessionInfo.SessionName;
        public UInt64 ProcessIdentifier => BackingSessionInfo.ProcessId;
        public string ProcessName => BackingSessionInfo.ProcessName;

        public IReadOnlyList<MidiSessionInfoOpenConnection> Connections;


        internal Microsoft.Windows.Devices.Midi2.Reporting.MidiServiceSessionInfo BackingSessionInfo { get; set; }

        public MidiSessionInfo(Microsoft.Windows.Devices.Midi2.Reporting.MidiServiceSessionInfo backingSessionInfo)
        {
            BackingSessionInfo = backingSessionInfo;

            // connections
            var list = new List<MidiSessionInfoOpenConnection>();

            foreach (var conn in backingSessionInfo.Connections)
            {
                var wrap = new MidiSessionInfoOpenConnection(conn);
                list.Add(wrap);
            }

            Connections = list.AsReadOnly();
        }
    }
}
