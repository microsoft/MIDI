// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

using System.Management.Automation;

using Windows.Devices.Midi2.Utilities.Messages;

namespace WindowsMidiServices
{
    [Cmdlet(VerbsCommon.Get, "MidiMessageInfo")]
    [OutputType(typeof(MidiMessageInfo))]
    public class CommandGetMidiMessageInfo : MidiCmdletBase
    {
        [Parameter(Mandatory = true, Position = 0, ValueFromPipeline = true)]
        [ValidateCount(1, 4)]
        public UInt32[] Words { get; set; } = [];

        protected override void ProcessRecord()
        {
            var info = new MidiMessageInfo();

            info.MessageType = MidiMessageHelper.GetMessageTypeFromMessageFirstWord(Words[0]);
            info.PacketType = MidiMessageHelper.GetPacketTypeFromMessageFirstWord(Words[0]);

            info.MessageTypeHasGroupField = MidiMessageHelper.MessageTypeHasGroupField(info.MessageType);
            info.MessageTypeHasChannelField = MidiMessageHelper.MessageTypeHasChannelField(info.MessageType);

            if (info.MessageTypeHasGroupField)
            {
                info.Group = MidiMessageHelper.GetGroupFromMessageFirstWord(Words[0]);
            }

            if (info.MessageTypeHasChannelField)
            {
                info.Channel = MidiMessageHelper.GetChannelFromMessageFirstWord(Words[0]);
            }

            info.MessageName = MidiMessageHelper.GetMessageDisplayNameFromFirstWord(Words[0]);

            info.ExpectedWordCount = (UInt16)info.PacketType;

            info.Words = Words;

            string wordsHex = string.Empty;

            foreach (var word in Words)
            {
                wordsHex += word.ToString("X8") + " ";
            }

            info.WordsHex = wordsHex.Trim();

            WriteObject(info);
        }
    }
}
