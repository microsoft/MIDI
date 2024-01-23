using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

using midi2 = Windows.Devices.Midi2;

namespace MidiSample.AppToAppMidi
{

    public class Note
    {
        public midi2.MidiEndpointConnection Connection { get; set; }
        public byte NoteNumber { get; set; }

        public byte GroupIndex { get; set; }

        public byte ChannelIndex { get; set; }

        public void NoteOn() => Connection.SendMessagePacket(
            midi2.MidiMessageBuilder.BuildMidi2ChannelVoiceMessage(
                0, 
                GroupIndex, 
                midi2.Midi2ChannelVoiceMessageStatus.NoteOn, 
                ChannelIndex, 
                (ushort)((ushort)NoteNumber << 8), 
                1000));
        public void NoteOff() => Connection.SendMessagePacket(
            midi2.MidiMessageBuilder.BuildMidi2ChannelVoiceMessage(
                0, 
                GroupIndex, 
                midi2.Midi2ChannelVoiceMessageStatus.NoteOff, 
                ChannelIndex,
                (ushort)((ushort)NoteNumber << 8), 
                0));
    }


}
