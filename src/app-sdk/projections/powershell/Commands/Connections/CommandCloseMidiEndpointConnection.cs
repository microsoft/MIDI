using System;
using System.Collections.Generic;
using System.Linq;
using System.Management.Automation;
using System.Text;
using System.Threading.Tasks;

namespace WindowsMidiServices
{

    [Cmdlet(VerbsCommon.Close, "MidiEndpointConnection")]
    public class CommandCloseMidiEndpointConnection : Cmdlet
    {
        [Parameter(Mandatory = true, Position = 0)]
        public MidiSession Session
        {
            get; set;
        }

        [Parameter(Mandatory = true, Position = 1)]
        public MidiEndpointConnection Connection
        {
            get; set;
        }

        protected override void ProcessRecord()
        {
            if (Session.BackingSession == null)
            {
                // todo: throw
                return;
            }

            Guid id = Connection.ConnectionId;

            Session.BackingSession.DisconnectEndpointConnection(id);

            WriteVerbose($"MIDI Endpoint Connection {id} closed.");
        }
    }


}
