using System;
using System.Collections.Generic;
using System.Linq;
using System.Management.Automation;
using System.Text;
using System.Threading.Tasks;

namespace WindowsMidiServices
{
    [Cmdlet(VerbsCommon.Get, "MidiMessageInfo")]
    public class CommandGetMidiMessageInfo : Cmdlet
    {
        [Parameter(Mandatory = true, Position = 0)]
        public UInt32[] Words { get; set; }
        protected override void ProcessRecord()
        {
            if (Words.Length > 4 || Words.Length < 1)
            {
                throw new ArgumentException("MIDI Words must comprise one and only one valid MIDI UMP message (1-4 32-bit words)");
            }

            var info = new MidiMessageInfo();

            info.MessageType = Microsoft.Windows.Devices.Midi2.Messages.MidiMessageHelper.GetMessageTypeFromMessageFirstWord(Words[0]);
            info.PacketType = Microsoft.Windows.Devices.Midi2.Messages.MidiMessageHelper.GetPacketTypeFromMessageFirstWord(Words[0]);

            info.MessageTypeHasGroupField = Microsoft.Windows.Devices.Midi2.Messages.MidiMessageHelper.MessageTypeHasGroupField(info.MessageType);
            info.MessageTypeHasChannelField = Microsoft.Windows.Devices.Midi2.Messages.MidiMessageHelper.MessageTypeHasChannelField(info.MessageType);

            if (info.MessageTypeHasGroupField)
            {
                info.Group = Microsoft.Windows.Devices.Midi2.Messages.MidiMessageHelper.GetGroupFromMessageFirstWord(Words[0]);
            }

            if (info.MessageTypeHasChannelField)
            {
                info.Channel = Microsoft.Windows.Devices.Midi2.Messages.MidiMessageHelper.GetChannelFromMessageFirstWord(Words[0]);
            }

            info.MessageName = Microsoft.Windows.Devices.Midi2.Messages.MidiMessageHelper.GetMessageDisplayNameFromFirstWord(Words[0]);

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
