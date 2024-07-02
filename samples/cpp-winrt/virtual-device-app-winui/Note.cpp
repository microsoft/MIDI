#include "pch.h"
#include "Note.h"
#include "Note.g.cpp"

namespace winrt::virtual_device_app_winui::implementation
{
    void Note::NoteOn()
    {
        uint16_t index = NoteNumber();

        index <<= 8;

        uint16_t velocity = 0xFFFF;

        uint16_t word1 = (uint32_t)velocity << 16;

        if (midi2::MidiEndpointConnection::SendMessageSucceeded(Connection().SendSingleMessagePacket(
            msgs::MidiMessageBuilder::BuildMidi2ChannelVoiceMessage(
                0,
                midi2::MidiGroup(GroupIndex()),
                msgs::Midi2ChannelVoiceMessageStatus::NoteOn,
                midi2::MidiChannel(ChannelIndex()),
                index,
                word1))))
        {
            //System.Diagnostics.Debug.WriteLine(" - sent");
        }
    }


    void Note::NoteOff()
    {
        uint16_t index = NoteNumber();
        index <<= 8;

        if (midi2::MidiEndpointConnection::SendMessageSucceeded(Connection().SendSingleMessagePacket(
            msgs::MidiMessageBuilder::BuildMidi2ChannelVoiceMessage(
                0,
                midi2::MidiGroup(GroupIndex()),
                msgs::Midi2ChannelVoiceMessageStatus::NoteOff,
                midi2::MidiChannel(ChannelIndex()),
                index,
                0))))
        {
            //System.Diagnostics.Debug.WriteLine(" - sent");
        }
    }
}
