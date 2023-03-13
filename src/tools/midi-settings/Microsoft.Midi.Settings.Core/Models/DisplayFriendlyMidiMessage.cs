using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace Microsoft.Midi.Settings.Core.Models;

// this class is specifically to enable binding to the display controls in the settings app

public class DisplayFriendlyMidiMessage
{
    
    
    // TODO: Also need incoming message timestamp

    public byte[] AllData {
        get; private set;
    }    // bytes are more convenient for display 

    public DisplayFriendlyMidiMessage(long timestamp, byte[] data, byte messageSizeInBytes, string sourceDeviceName)
    {
        AllData = new byte[messageSizeInBytes];

        Array.Copy(data, AllData, messageSizeInBytes);

        SourceDeviceName = sourceDeviceName;
        Timestamp = timestamp;
    }

    public uint MessageSizeInBits => (uint)(AllData.Length * 8);


    public byte MessageType => MostSignificantNibble(AllData[0]);

    public byte Group => LeastSignificantNibble(AllData[0]);

    public byte Opcode => MostSignificantNibble(AllData[1]);

    public byte Channel => LeastSignificantNibble(AllData[1]);

    public string SourceDeviceName { get; set; }
    public long Timestamp { get; set; }

    public string FormattedMessageTypeName
    {
        get
        {
            // TODO: Localize

            switch (MessageType)
            {
                case 0x0:   // 32 bit utility
                    return "Utility";
                case 0x1:   // 32 bit System Real Time / Common
                    return "System common";
                case 0x2:   // 32 bit MIDI 1.0 channel voice
                    switch (Opcode)
                    {
                        case 0x8:
                            return "MIDI 1 Note Off";
                        case 0x9:
                            return "MIDI 1 Note On";
                        case 0xA:
                            return "MIDI 1 Poly Pressure";
                        case 0xB:
                            return "MIDI 1 CC";
                        case 0xC:
                            return "MIDI 1 Program Change";
                        case 0xD:
                            return "MIDI 1 Channel Pressure";
                        case 0xE:
                            return "MIDI 1 Pitch Bend";
                        default:
                            return "MIDI 1 Channel Voice";
                    }

                case 0x3:   // 64 bit Data messages including SysEx
                    return "Data message";
                case 0x4:   // 64 bit MIDI 2.0 channel voice
                    switch (Opcode)
                    {
                        case 0x0:
                            return "MIDI 2 Reg Per-note CC";
                        case 0x1:
                            return "MIDI 2 Asgn Per-note CC";
                        case 0x4:
                            return "MIDI 2 RPN";
                        case 0x5:
                            return "MIDI 2 NRPN";
                        case 0x6:
                            return "MIDI 2 Per-note Bend";
                        case 0x8:
                            return "MIDI 2 Note Off";
                        case 0x9:
                            return "MIDI 2 Note On";
                        case 0xA:
                            return "MIDI 2 Poly Pressure";
                        case 0xB:
                            return "MIDI 2 CC";
                        case 0xC:
                            return "MIDI 2 Program Change";
                        case 0xD:
                            return "MIDI 2 Channel Pressure";
                        case 0xE:
                            return "MIDI 2 Pitch Bend";
                        case 0xF:
                            return "MIDI 2 Per-Note mgmt";

                        default:
                            return "MIDI 2 Channel Voice";
                    }

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

    public bool HasGroup
    {
        get
        {
            switch (MessageType)
            {
                case 0x0:   // 32 bit utility
                    return false;
                case 0x1:   // 32 bit System Real Time / Common
                    return true;
                case 0x2:   // 32 bit MIDI 1.0 channel voice
                    return true;
                case 0x3:   // 64 bit Data messages including SysEx
                    return true;
                case 0x4:   // 64 bit MIDI 2.0 channel voice
                    return true;
                case 0x5:   // 128 bit Data messages
                    return true;

                case 0xD:   // 128 bit Flex Data
                    return true;

                case 0xF:   // 128 bit UMP Stream messages
                    return false;

                default: return false;
            }
        }
    }

    public bool HasChannel
    {
        get
        {
            // TODO: Double-check these assumptions

            switch (MessageType)
            {
                case 0x0:   // 32 bit utility
                    return false;
                case 0x1:   // 32 bit System Real Time / Common
                    return true;
                case 0x2:   // 32 bit MIDI 1.0 channel voice
                    return true;
                case 0x3:   // 64 bit Data messages including SysEx
                    return true;
                case 0x4:   // 64 bit MIDI 2.0 channel voice
                    return true;
                case 0x5:   // 128 bit Data messages
                    return true;

                case 0xD:   // 128 bit Flex Data
                    return true;

                case 0xF:   // 128 bit UMP Stream messages
                    return false;

                default: return false;
            }
        }
    }

    public bool HasOpcode
    {
        get
        {
            // TODO: Double-check these assumptions

            switch (MessageType)
            {
                case 0x0:   // 32 bit utility
                    return true;
                case 0x1:   // 32 bit System Real Time / Common
                    return true;
                case 0x2:   // 32 bit MIDI 1.0 channel voice
                    return true;
                case 0x3:   // 64 bit Data messages including SysEx
                    return true;
                case 0x4:   // 64 bit MIDI 2.0 channel voice
                    return true;
                case 0x5:   // 128 bit Data messages
                    return true;

                case 0xD:   // 128 bit Flex Data
                    return true;

                case 0xF:   // 128 bit UMP Stream messages
                    return false;

                default: return false;
            }
        }
    }


    public string FormattedMessageType => FormatNibbleHex(MessageType);
    public string FormattedGroup => FormatNibbleDecimal((byte)(Group + 1));
    public string FormattedOpcode => FormatNibbleBinary(Opcode);
    public string FormattedChannel => FormatNibbleDecimal((byte)(Channel + 1));
    public string FormattedTimestamp => Timestamp.ToString();






    private UInt16 UInt16FromBytes(byte msb, byte lsb)
    {
        return (UInt16)(msb << 8 | lsb);
    }

    private UInt32 UInt32FromBytes(byte msb, byte innerMsb1, byte innerLsb1, byte lsb)
    {
        return (UInt16)(msb << 24 | innerMsb1 << 16 | innerLsb1 << 8 | lsb);
    }

    // A crumb is 2 bits. Learned something new today.
    private byte MostSignificantCrumbFromNibble(byte value)
    {
        return (byte)((value >> 2) & 0x07);
    }

    private byte LeastSignificantCrumbFromNibble(byte value)
    {
        return (byte)(value & 0x07);
    }



    private byte MostSignificantNibble(byte value)
    {
        return (byte)((value >> 4) & 0x0F);
    }

    private byte LeastSignificantNibble(byte value)
    {
        return (byte)(value & 0x0F);
    }


    private string FormatNibbleDecimal(byte value)
    {
        return string.Format("{0:D2}", value);
    }

    private string FormatNibbleHex(byte value)
    {
        return string.Format("0x{0:X1}", value);
    }

    private string FormatNibbleBinary(byte value)
    {
        return "b" + Convert.ToString(value, 2).PadLeft(4, '0');
    }


    private string FormatByteHex(byte value)
    {
        return string.Format("0x{0:X2}", value);
    }

    private string FormatUInt16Hex(UInt16 value)
    {
        return string.Format("0x{0:X4}", value);
    }

    private string FormatUInt32Hex(UInt32 value)
    {
        return string.Format("0x{0:X8}", value);
    }


}
