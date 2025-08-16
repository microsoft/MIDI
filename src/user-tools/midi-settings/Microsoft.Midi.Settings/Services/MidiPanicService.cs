using Microsoft.Midi.Settings.Contracts.Services;
using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Text.RegularExpressions;
using System.Threading.Tasks;

namespace Microsoft.Midi.Settings.Services;

public class MidiPanicService : IMidiPanicService
{
    public static readonly List<MidiGroup> AllGroups = [];
    public static readonly List<MidiChannel> AllChannels = [];

    private readonly IMidiSessionService _sessionService;

    public MidiPanicService(IMidiSessionService sessionService)
    {
        _sessionService = sessionService;

        for (int i = 0; i < 16; i++)
        {
            AllGroups.Add(new MidiGroup((byte)i));
            AllChannels.Add(new MidiChannel((byte)i));
        }
    }


    public bool SendMidiPanic(string endpointDeviceId)
    {
        return SendMidiPanic(endpointDeviceId, AllGroups);
    }

    public bool SendMidiPanic(string endpointDeviceId, IList<MidiGroup> groups)
    {
        return SendMidiPanic(endpointDeviceId, groups, AllChannels);
    }


    public bool SendMidiPanic(string endpointDeviceId, IList<MidiGroup> groups, IList<MidiChannel> channels)
    {
        var connection = _sessionService.GetConnection(endpointDeviceId.Trim().ToLower());

        if (connection == null)
        {
            return false;
        }

        return SendMidiPanic(connection, groups, channels);
    }


    public bool SendMidiPanic(MidiEndpointConnection connection)
    {
        return SendMidiPanic(connection, AllGroups);
    }

    public bool SendMidiPanic(MidiEndpointConnection connection, IList<MidiGroup> groups)
    {
        return SendMidiPanic(connection, groups, AllChannels);
    }


    public bool SendMidiPanic(MidiEndpointConnection connection, IList<MidiGroup> groups, IList<MidiChannel> channels)
    {
        if (!connection.IsOpen)
        {
            return false;
        }

        // create the set of messages 
        List<UInt32> panicMessages = [];

        const byte allNotesOffCC = 123;
        const byte allSoundOffCC = 120;
        const byte resetAllControllersCC = 121;

        foreach (var group in groups)
        {
            foreach (var channel in channels)
            {
                // all notes off
                panicMessages.Add(MidiMessageBuilder.BuildMidi1ChannelVoiceMessage(
                    MidiClock.TimestampConstantSendImmediately,
                    group,
                    Midi1ChannelVoiceMessageStatus.ControlChange,
                    channel,
                    allNotesOffCC,
                    0).Word0);

                // all sound off
                panicMessages.Add(MidiMessageBuilder.BuildMidi1ChannelVoiceMessage(
                    MidiClock.TimestampConstantSendImmediately,
                    group,
                    Midi1ChannelVoiceMessageStatus.ControlChange,
                    channel,
                    allSoundOffCC,
                    0).Word0);

                // reset all controllers
                panicMessages.Add(MidiMessageBuilder.BuildMidi1ChannelVoiceMessage(
                    MidiClock.TimestampConstantSendImmediately,
                    group,
                    Midi1ChannelVoiceMessageStatus.ControlChange,
                    channel,
                    resetAllControllersCC,
                    0).Word0);


            }
        }

        var sendResult = connection.SendMultipleMessagesWordList(MidiClock.TimestampConstantSendImmediately, panicMessages);

        return MidiEndpointConnection.SendMessageSucceeded(sendResult);

    }


}
