using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace Microsoft.Midi.Settings.SdkMock.Messages;
public class Ump
{
    public UInt32[] Words;

    public byte MessageType
    {
        get
        {
            return MessageTypeFromFirstWord(Words[0]);
        }
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

    // this doesn't do any checking. It's assumed to be run only on channel voice messages
    private byte GetOpcode()
    {
        return (byte)((Words[0] & 0x00F00000) >> 20);
    }

    // this doesn't do any checking. It's assumed to be run only on channel voice messages
    private byte GetChannel()
    {
        return (byte)((Words[0] & 0x000F0000) >> 16);
    }


    public string FormattedMessageTypeName
    {
        get
        {
            // TODO: Some of this is duplicated in other modules right now.
            // need to normalize this to all be in a single SDK project.

            switch (MessageTypeFromFirstWord(Words[0]))
            {
                case 0x0:   // 32 bit utility
                    return "Utility";
                case 0x1:   // 32 bit System Real Time / Common
                    return "System common";
                case 0x2:   // 32 bit MIDI 1.0 channel voice
                    switch (GetOpcode())
                    {
                        case 0x8:
                            return "M1 Note Off";
                        case 0x9:
                            return "M1 Note On";
                        case 0xA:
                            return "M1 Poly Press";
                        case 0xB:
                            return "M1 CC";
                        case 0xC:
                            return "M1 Prog Change";
                        case 0xD:
                            return "M1 Chan Press";
                        case 0xE:
                            return "M1 Pitch Bend";
                        default:
                            return "M1 CV";
                    }
                //return "MIDI 1 CV";

                case 0x3:   // 64 bit Data messages including SysEx
                    return "Data message";
                case 0x4:   // 64 bit MIDI 2.0 channel voice
                    switch (GetOpcode())
                    {
                        case 0x0:
                            return "M2 Reg Per-note CC";
                        case 0x1:
                            return "M2 Asgn Per-note CC";
                        case 0x4:
                            return "M2 RPN";
                        case 0x5:
                            return "M2 NRPN";
                        case 0x6:
                            return "M2 Per-note Bend";
                        case 0x8:
                            return "M2 Note Off";
                        case 0x9:
                            return "M2 Note On";
                        case 0xA:
                            return "M2 Poly Press";
                        case 0xB:
                            return "M2 CC";
                        case 0xC:
                            return "M2 Prog Change";
                        case 0xD:
                            return "M2 Chan Press";
                        case 0xE:
                            return "M2 Pitch Bend";
                        case 0xF:
                            return "M2 Per-Note mgmt";

                        default:
                            return "MIDI 2 CV";
                    }
                //return "MIDI 2 CV";

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

