// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License. See LICENSE in the project root for license information.

using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using Windows.Devices.Midi;
using Windows.Foundation.Metadata;

namespace Microsoft.Windows.Midi.Messages
{
    public sealed partial class UmpMessageBuilder
    {



        /// <summary>
        /// You can safely cast the return value to an int to get the number of words
        /// in the UMP, based on the message type
        /// </summary>
        /// <param name="firstWord">The first word in the UMP, including the message type nibble</param>
        public static UmpType GetUmpLengthFromFirstWord(UInt32 firstWord)
        {
            switch (MidiMessageUtilityInternal.UmpLengthFromFirstWord(firstWord))
            {
                case 1: return UmpType.Ump32;
                case 2: return UmpType.Ump64;
                case 3: return UmpType.Ump96;
                case 4: return UmpType.Ump128;
                default: return UmpType.Unknown;
            }
        }




        // Utility Message first 32 bits:
        // 4 bit message type = 0
        // 4 bit group
        // 4 bit status
        // 20 bit data
        public static UInt32 BuildUtilityMessageFirstWord(byte group, byte status, UInt32 data)
        {
            throw new NotImplementedException();
        }

        // System Message first 32 bits:
        // 4 bit message type = 1
        // 4 bit group
        // 8 bit status
        // 16 bit data
        public static UInt32 BuildSystemMessageFirstWord(byte group, byte status, UInt16 data)
        {
            throw new NotImplementedException();
        }

        // MIDI 1 Channel voice first 32 bits:
        // 4 bit message type = 2
        // 4 bit group
        // 8 bit status + channel
        // 8 bit data1
        // 8 bit data2
        [DefaultOverload]
        public static UInt32 BuildMidi1ChannelVoiceFirstWord(byte group, byte status, byte data1, byte data2)
        {
            // TODO: Use a bunch of shifts, or use a BitVector32

            throw new NotImplementedException();
        }

        // MIDI 1 Channel voice first 32 bits:
        // 4 bit message type = 2
        // 4 bit group
        // 8 bit status + channel
        // 8 bits data1
        // 8 bits of zero
        public static UInt32 BuildMidi1ChannelVoiceFirstWord(byte group, byte status, byte data1)
        {
            throw new NotImplementedException();
        }



        // MIDI 2 Channel voice first 32 bits:
        // 4 bit message type = 4
        // 4 bit group
        // 8 bit status
        // 16 bit index
        public static UInt32 BuildMidi2ChannelVoiceFirstWord(byte group, byte status, UInt16 index)
        {
            throw new NotImplementedException();
        }

    }
}
