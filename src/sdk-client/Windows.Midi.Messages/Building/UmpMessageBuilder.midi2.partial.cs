// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License. See LICENSE in the project root for license information.

using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using Microsoft.Windows.Midi.Messages.Packet;
using Windows.Foundation.Metadata;
using Microsoft.Windows.Midi.Messages.Types.ChannelVoice;

namespace Microsoft.Windows.Midi.Messages
{
    public sealed partial class UmpMessageBuilder
    {

        public static Ump64 BuildMidi2NoteOffMessage(byte group, byte channel, byte noteNumber, Midi2NoteOnOffAttributeType attributeType, UInt16 velocity, UInt16 attributeData)
        {
            throw new NotImplementedException();
        }
        public static Ump64 BuildMidi2NoteOnMessage(byte group, byte channel, byte noteNumber, Midi2NoteOnOffAttributeType attributeType, UInt16 velocity, UInt16 attributeData)
        {
            throw new NotImplementedException();
        }

        // consider dedicated method for pitch 7.9



        public static Ump64 BuildMidi2PolyPressureMessage(byte group, byte channel, byte noteNumber, UInt32 data)
        {
            throw new NotImplementedException();
        }
        public static Ump64 BuildMidi2RegisteredPerNoteControllerMessage(byte group, byte channel, byte noteNumber, byte index, UInt32 data)
        {
            throw new NotImplementedException();
        }
        public static Ump64 BuildMidi2AssignablePerNoteControllerMessage(byte group, byte channel, byte noteNumber, byte index, UInt32 data)
        {
            throw new NotImplementedException();
        }
        public static Ump64 BuildMidi2PerNoteManagementMessage(byte group, byte channel, byte noteNumber, byte flags)
        {
            throw new NotImplementedException();
        }
        public static Ump64 BuildMidi2ControlChangeMessage(byte group, byte channel, byte index, UInt32 data)
        {
            throw new NotImplementedException();
        }

        // RPN and NRPN
        public static Ump64 BuildMidi2RegisteredControllerMessage(byte group, byte channel, byte bank, byte index, UInt32 data)
        {
            throw new NotImplementedException();
        }
        public static Ump64 BuildMidi2AssignableControllerMessage(byte group, byte channel, byte bank, byte index, UInt32 data)
        {
            throw new NotImplementedException();
        }
        public static Ump64 BuildMidi2RelativeRegisteredControllerMessage(byte group, byte channel, byte bank, byte index, UInt32 data)
        {
            throw new NotImplementedException();
        }
        public static Ump64 BuildMidi2RelativeAssignableControllerMessage(byte group, byte channel, byte bank, byte index, UInt32 data)
        {
            throw new NotImplementedException();
        }

        public static Ump64 BuildMidi2ProgramChangeMessage(byte group, byte channel, byte index, byte program, byte bankMsb, byte bankLsb)
        {
            throw new NotImplementedException();
        }
        public static Ump64 BuildMidi2ProgramChangeMessage(byte group, byte channel, byte index, byte program, UInt16 bank)
        {
            throw new NotImplementedException();
        }

        public static Ump64 BuildMidi2ChannelPressureMessage(byte group, byte channel, UInt32 data)
        {
            throw new NotImplementedException();
        }
        public static Ump64 BuildMidi2PitchBendMessage(byte group, byte channel, UInt32 data)
        {
            throw new NotImplementedException();
        }
        public static Ump64 BuildMidi2PerNotePitchBendMessage(byte group, byte channel, byte noteNumber, UInt32 data)
        {
            throw new NotImplementedException();
        }


        // TODO: System Common

        // TODO: Sysex

    }
}
