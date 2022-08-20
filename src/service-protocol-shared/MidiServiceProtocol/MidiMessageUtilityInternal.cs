// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License. See LICENSE in the project root for license information.

using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace Microsoft.Windows.Midi.Internal.ServiceProtocol
{
    public class MidiMessageUtilityInternal
    {

        /// <summary>
        /// Calculates the number of 32 bit words in the packet based on the message type.
        /// </summary>
        /// <param name="word"></param>
        /// <returns>The number of MIDI Words that should be in the packet, including the first one</returns>
        /// <exception cref="InvalidDataException"></exception>
        public static int UmpLengthFromFirstWord(MidiWord word)
        { 
            // See section 2.1.4 of the UMP protocol specification
            // message type is the first 4 most significant bits of the first word
            //
            byte messageType = (byte)((word >> 28) & 0x0F); // the mask isn't necessary, but it makes me feel better inside

            switch (messageType)
            {
                case 0x0:   // Utility message
                case 0x1:   // system real time and system common
                case 0x2:   // MIDI 1.0 channel voice
                    return 1;

                case 0x3:   // data message
                case 0x4:   // MIDI 2.0 channel voice
                    return 2;

                case 0x5:   // Data
                    return 4;


                // future reserved message types --------

                case 0x6:   // reserved
                case 0x7:   // reserved
                    return 1;
                case 0x8:   // reserved
                case 0x9:   // reserved
                case 0xA:   // reserved
                    return 2;
                case 0xB:   // reserved
                case 0xC:   // reserved
                    return 3;
                case 0xD:   // reserved
                case 0xE:   // reserved
                case 0xF:   // reserved
                    return 4;
                default:
                    throw new InvalidDataException($"Next word contains invalid messageType {messageType}");
            }

        }
    }
}
