using System;
using System.Collections.Generic;
using System.Linq;
using System.Management.Automation;
using System.Text;
using System.Threading.Tasks;

namespace Microsoft.Midi.ConsoleApp.Commands
{
    namespace WindowsMidiServices
    {
        [Cmdlet(VerbsCommon.Open, "MidiConnection")]
        public class ConnectToEndpointCommand : Cmdlet
        {


        }


        [Cmdlet(VerbsCommon.Close, "MidiEndpoint")]
        public class DisconnectFromEndpointCommand : Cmdlet
        {


        }


        [Cmdlet(VerbsCommunications.Send, "MidiMessage")]
        public class SendMidiMessageCommand : Cmdlet
        {


        }


        [Cmdlet(VerbsCommunications.Receive, "MidiMessage")]
        public class ReceiveMidiMessageCommand : Cmdlet
        {


        }


    }
}
