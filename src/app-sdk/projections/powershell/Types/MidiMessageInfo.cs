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
    public class MidiMessageInfo
    {
        public string MessageName { get; internal set; }

        public Microsoft.Windows.Devices.Midi2.MidiMessageType MessageType { get; internal set; }

        public Microsoft.Windows.Devices.Midi2.MidiPacketType PacketType { get; internal set; }


        public UInt16 ExpectedWordCount { get; internal set; }

        public UInt32[] Words { get; internal set; }

        public string WordsHex { get; internal set; }

        public bool MessageTypeHasGroupField { get; internal set; }
        public bool MessageTypeHasChannelField { get; internal set; }

        public Microsoft.Windows.Devices.Midi2.MidiGroup? Group { get; internal set; } = null;
        public Microsoft.Windows.Devices.Midi2.MidiChannel? Channel { get; internal set; } = null;
    }
}
