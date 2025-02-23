using System;
using System.Collections.Generic;
using System.Linq;
using System.Management.Automation;
using System.Text;
using System.Threading.Tasks;

namespace WindowsMidiServices
{

    [Cmdlet(VerbsCommunications.Send, "MidiMessage")]
    public class CommandSendMidiMessage : Cmdlet
    {
        [Parameter(Mandatory = true, Position = 0)]
        public MidiEndpointConnection Connection { get; set; }

        [Parameter(Mandatory = true, Position = 1)]
        public UInt32[] Words { get; set; }

        [Parameter()]
        public UInt64 Timestamp { get; set; } = 0;

        protected override void ProcessRecord()
        {
            if (Words.Length > 4 || Words.Length < 1)
            {
                throw new ArgumentException("MIDI Words must comprise one and only one valid MIDI UMP message (1-4 32-bit words)");
            }

            // todo: some data validation to ensure the words are a single message only

            // todo: return value
            var result = Connection.BackingConnection.SendSingleMessageWordArray(Timestamp, 0, (byte)Words.Length, Words);

            if (Microsoft.Windows.Devices.Midi2.MidiEndpointConnection.SendMessageSucceeded(result))
            {
                WriteVerbose($"MIDI Message with {Words.Length.ToString()} UMP words sent with timestamp {Timestamp}.");
            }
            else
            {
                WriteVerbose("MIDI Message failed to send");
                WriteObject(result);
            }

        }
    }
}
