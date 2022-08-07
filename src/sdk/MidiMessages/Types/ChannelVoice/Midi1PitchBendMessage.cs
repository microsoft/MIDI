// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License. See LICENSE in the project root for license information.

using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace Microsoft.Windows.Midi.Messages.Types.ChannelVoice
{
    public sealed class Midi1PitchBendMessage : Midi1ChannelVoiceMessage
    {
        public byte NoteNumber { get; set; }

        public byte DataMsb { get; set; }
        public byte DataLsb { get; set; }

        public Int16 CombinedData
        {
            get
            {
                return MidiMessageUtility.Combine7BitMsbAnd7BitLsb(DataMsb, DataLsb);
            }
            set
            {
                DataMsb = (byte)((value >> 8) & 0x7F);
                DataLsb = (byte)(value & 0x7F); 
            }
        }

    }
}
