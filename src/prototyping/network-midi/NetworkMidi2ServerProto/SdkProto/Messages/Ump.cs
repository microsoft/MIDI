using System;
using System.Collections.Generic;
using System.Linq;
using System.Reflection.Emit;
using System.Reflection.Metadata;
using System.Text;
using System.Threading.Tasks;
using Windows.Devices.Midi;

namespace NetworkMidi2ServerProto.SdkProto
{
    public class Ump
    {
        public UInt32[] Words;

        public byte MessageType
        {
            get { return MessageTypeFromFirstWord(Words[0]); }
        }

        public static byte MessageTypeFromFirstWord(UInt32 firstMidiWord)
        {
            byte messageType = (byte)((firstMidiWord & 0xF0000000) >> 28);
            return messageType;
        }

        public static byte NumberOfMessageWordsFromFirstWord(UInt32 firstMidiWord)
        {
            switch (MessageTypeFromFirstWord(firstMidiWord))
            {
                case 0x0:   // 32 bit utility
                case 0x1:   // 32 bit System Real Time / Common
                case 0x2:   // 32 bit MIDI 1.0 channel voice
                    return 1;
                case 0x3:   // 64 bit Data messages including SysEx
                case 0x4:   // 64 bit MIDI 2.0 channel voice
                    return 2;
                case 0x5:   // 128 bit Data messages
                    return 4;
                case 0x6:   // 32 bit reserved
                case 0x7:   // 32 bit reserved
                    return 1;
                case 0x8:   // 64 bit reserved
                case 0x9:   // 64 bit reserved   
                case 0xA:   // 64 bit reserved
                    return 2;
                case 0xB:   // 96 bit reserved
                case 0xC:   // 96 bit reserved
                    return 3;
                case 0xD:   // 128 bit Flex Data
                case 0xE:   // 128 bit reserved
                case 0xF:   // 128 bit UMP Stream messages
                    return 4;
            }

            // it's a 4-bit value, but C# doesn't know about those, so this is required
            return 0;
        }

        public void Alloc(byte numberOfMidiWords)
        {
            Words = new UInt32[numberOfMidiWords];
        }


        public string FormattedMessageTypeName
        {
            get
            {
                // TODO: Localize

                switch (MessageTypeFromFirstWord(Words[0]))
                {
                    case 0x0:   // 32 bit utility
                        return "Utility";
                    case 0x1:   // 32 bit System Real Time / Common
                        return "System common";
                    case 0x2:   // 32 bit MIDI 1.0 channel voice
                        //switch (Opcode)
                        //{
                        //    case 0x8:
                        //        return "MIDI 1 Note Off";
                        //    case 0x9:
                        //        return "MIDI 1 Note On";
                        //    case 0xA:
                        //        return "MIDI 1 Poly Pressure";
                        //    case 0xB:
                        //        return "MIDI 1 CC";
                        //    case 0xC:
                        //        return "MIDI 1 Program Change";
                        //    case 0xD:
                        //        return "MIDI 1 Channel Pressure";
                        //    case 0xE:
                        //        return "MIDI 1 Pitch Bend";
                        //    default:
                        //        return "MIDI 1 Channel Voice";
                        //}
                        return "MIDI 1 Channel Voice";

                    case 0x3:   // 64 bit Data messages including SysEx
                        return "Data message";
                    case 0x4:   // 64 bit MIDI 2.0 channel voice
                        //switch (Opcode)
                        //{
                        //    case 0x0:
                        //        return "MIDI 2 Reg Per-note CC";
                        //    case 0x1:
                        //        return "MIDI 2 Asgn Per-note CC";
                        //    case 0x4:
                        //        return "MIDI 2 RPN";
                        //    case 0x5:
                        //        return "MIDI 2 NRPN";
                        //    case 0x6:
                        //        return "MIDI 2 Per-note Bend";
                        //    case 0x8:
                        //        return "MIDI 2 Note Off";
                        //    case 0x9:
                        //        return "MIDI 2 Note On";
                        //    case 0xA:
                        //        return "MIDI 2 Poly Pressure";
                        //    case 0xB:
                        //        return "MIDI 2 CC";
                        //    case 0xC:
                        //        return "MIDI 2 Program Change";
                        //    case 0xD:
                        //        return "MIDI 2 Channel Pressure";
                        //    case 0xE:
                        //        return "MIDI 2 Pitch Bend";
                        //    case 0xF:
                        //        return "MIDI 2 Per-Note mgmt";

                        //    default:
                        //        return "MIDI 2 Channel Voice";
                        //}
                        return "MIDI 2 Channel Voice";

                    case 0x5:   // 128 bit Data messages
                        return "Data Message";

                    case 0xD:   // 128 bit Flex Data
                        return "Flex data";

                    case 0xF:   // 128 bit UMP Stream messages
                        return "Stream";

                    default: return "Undefined";
                }
            }
        }

        public override string ToString()
        {
            if (Words != null)
            {
                var s = string.Format("0x{0:X1}:({1})", MessageType, FormattedMessageTypeName);

                s = s.PadRight(20, ' ');

                for (int i = 0; i < Words.Length; i++)
                {
                    s += string.Format("0x{0:X8} ", Words[i]);
                }

                return s;
            }
            else
            {
                return "Empty UMP";
            }
        }

    }
}
