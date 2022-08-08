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
    public sealed class MidiMessageUtility
    {
        public static short Combine7BitMsbAnd7BitLsb(byte msb, byte lsb)
        {
            return (short)((msb << 8) | lsb);
        }

        public static byte GetMsb(short value)
        {
            return (byte)((value >> 8) & 0x7F);
        }

        public static byte GetLsb(short value)
        {
            return (byte)(value & 0x7F);
        }

        public static byte Byte8ToByte7(byte value)
        {
            return (byte)(value & 0x7F);
        }




        /// <summary>
        /// Builds a status byte by shifting the status nibble to the left 4 digits and then 
        /// adding in the channel nibble to the lower 4 digits
        /// </summary>
        public static byte BuildStatusByte(byte statusNibble, byte channelNibble)
        {
            return (byte)((statusNibble << 4) | (channelNibble & 0x0F));
        }

        public static byte BuildMessageTypeByte(MidiMessageType messageType, byte groupNibble)
        {
            return BuildMessageTypeByte((byte)messageType, groupNibble);
        }

        [DefaultOverload]
        public static byte BuildMessageTypeByte(byte messageTypeNibble, byte groupNibble)
        {
            return (byte)((messageTypeNibble << 4) | (groupNibble & 0x0F));
        }

    }
}
