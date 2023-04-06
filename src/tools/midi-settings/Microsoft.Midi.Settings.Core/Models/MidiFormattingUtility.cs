using System;
using System.Collections.Generic;
using System.Linq;
using System.Reflection.Emit;
using System.Text;
using System.Threading.Tasks;

namespace Microsoft.Midi.Settings.Core.Models;
public  class MidiFormattingUtility
{
    public static string Midi1ChannelVoiceMessageNameFromOpcode(byte opcode)
    {
        switch (opcode)
        {
            case 0x8: return "MIDI 1.0 Note Off";
            case 0x9: return "MIDI 1.0 Note On";
            case 0xA: return "MIDI 1.0 Poly Pressure";
            case 0xB: return "MIDI 1.0 Control Change";
            case 0xC: return "MIDI 1.0 Program Change";
            case 0xD: return "MIDI 1.0 Channel Pressure";
            case 0xE: return "MIDI 1.0 Pitch Bend";
            default: return "Unknown";
        }
    }

    public static string Midi2ChannelVoiceMessageNameFromOpcode(byte opcode)
    {
        switch (opcode)
        {
            case 0x0: return "MIDI 2.0 Registered Per-Note Controller";
            case 0x1: return "MIDI 2.0 Assignable Per-Note Controller";
            case 0x2: return "MIDI 2.0 Registered Controller";
            case 0x3: return "MIDI 2.0 Assignable Controller";
            case 0x4: return "MIDI 2.0 Relative Registered Controller";
            case 0x5: return "MIDI 2.0 Relative Assignable Controller";
            case 0x6: return "MIDI 2.0 Per-Note Pitch Bend";

            case 0x8: return "MIDI 2.0 Note Off";
            case 0x9: return "MIDI 2.0 Note On";
            case 0xA: return "MIDI 2.0 Poly Pressure";
            case 0xB: return "MIDI 2.0 Control Change";
            case 0xC: return "MIDI 2.0 Program Change";
            case 0xD: return "MIDI 2.0 Channel Pressure";
            case 0xE: return "MIDI 2.0 Pitch Bend";
            case 0xF: return "MIDI 2.0 Per-Note Management";

            default: return "Unknown";
        }
    }

    public static string NoteNameFromMidi1StyleNoteNumber(byte noteNumber)
    {
        // octaves go from -1 to 9 for normal MIDI notes, and start with C

        var noteNames = new string[] {"C", "C#/Db", "D", "D#/Eb", "E", "F", "F#/Gb", "G", "G#/Ab", "A", "A#/Bb", "B" };
        var octave = noteNumber / 12 - 1;

        return noteNames[noteNumber % 12] + " " + octave;
    }


    public static string MessageTypeNameFromMessageType(byte messageType)
    {
        // TODO: Localize

        switch (messageType)
        {
            case 0x0:   // 32 bit utility
                return "Utility";
            case 0x1:   // 32 bit System Real Time / Common
                return "System common";

            case 0x2:   // 32 bit MIDI 1.0 channel voice
                return "MIDI 1.0 Channel Voice";

            case 0x3:   // 64 bit Data messages including SysEx
                return "Data message";

            case 0x4:   // 64 bit MIDI 2.0 channel voice
                return "MIDI 2.0 Channel Voice";

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
