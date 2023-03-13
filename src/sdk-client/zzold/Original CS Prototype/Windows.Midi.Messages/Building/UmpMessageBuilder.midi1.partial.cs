// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License. See LICENSE in the project root for license information.

using Microsoft.Windows.Midi.Messages.Packet;
using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using Windows.Foundation.Metadata;

namespace Microsoft.Windows.Midi.Messages
{
    public sealed partial class UmpMessageBuilder
    {
        // MIDI 1.0 three-byte messages
        public static Ump32 BuildMidi1NoteOffMessage(byte group, byte channel, byte noteNumber, byte velocity)
        {
            throw new NotImplementedException();
        }

        public static Ump32 BuildMidi1NoteOnMessage(byte group, byte channel, byte noteNumber, byte velocity)
        {
            throw new NotImplementedException();
        }

        public static Ump32 BuildMidi1PolyPressureMessage(byte group, byte channel, byte noteNumber, byte data)
        {
            throw new NotImplementedException();
        }

        public static Ump32 BuildMidi1ControlChangeMessage(byte group, byte channel, byte index, byte data)
        {
            throw new NotImplementedException();
        }

        public static Ump32 BuildMidi1PitchBendMessage(byte group, byte channel, byte lsbData, byte msbData)
        {
            throw new NotImplementedException();
        }

        [DefaultOverload]
        public static Ump32 BuildMidi1PitchBendMessage(byte group, byte channel, UInt16 data)
        {
            throw new NotImplementedException();
        }

        // MIDI 1.0 two-byte messages
        public static Ump32 BuildMidi1ProgramChangeMessage(byte group, byte channel, byte program)
        {
            throw new NotImplementedException();
        }
        public static Ump32 BuildMidi1ChannelPressureMessage(byte group, byte channel, byte data)
        {
            throw new NotImplementedException();
        }

        // MIDI 1.0 for users who want to send over the bytes without any parsing
        public static Ump32 BuildMidi1MessageDirect(byte group, byte status, byte data1, byte data2)
        {
            throw new NotImplementedException();
        }
        public static Ump32 BuildMidi1MessageDirect(byte group, byte status, byte data1)
        {
            throw new NotImplementedException();
        }
        //static Ump32 BuildMidi1MessageDirect(byte group, byte[] bytes)



    }
}
