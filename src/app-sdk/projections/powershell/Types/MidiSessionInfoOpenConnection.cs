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
    public class MidiSessionInfoOpenConnection
    {
        public string EndpointDeviceId => BackingSessionConnectionInfo.EndpointDeviceId;
        public UInt16 InstanceCount => BackingSessionConnectionInfo.InstanceCount;

        internal Microsoft.Windows.Devices.Midi2.Reporting.MidiServiceSessionConnectionInfo BackingSessionConnectionInfo { get; set; }

        public MidiSessionInfoOpenConnection(Microsoft.Windows.Devices.Midi2.Reporting.MidiServiceSessionConnectionInfo backingSessionConnectionInfo)
        {
            BackingSessionConnectionInfo = backingSessionConnectionInfo;
        }
    }
}
