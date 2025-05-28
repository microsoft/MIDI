using System;
using System.Collections.Generic;
using System.Linq;
using System.Management.Automation;
using System.Text;
using System.Threading.Tasks;

namespace WindowsMidiServices
{


    [Cmdlet(VerbsCommon.Open, "MidiEndpointConnection")]
    public class CommandOpenMidiEndpointConnection : Cmdlet
    {
        [Parameter(Mandatory = true, Position = 0)]
        public MidiSession Session
        {
            get;set;
        }

        [Parameter(Mandatory = true, Position = 1)]
        public string EndpointDeviceId
        {
            get; set;
        }


        protected override void ProcessRecord()
        {
            if (Session == null || !Session.IsValid)
            {
                // invalid
                return;
            }

            if (string.IsNullOrWhiteSpace(EndpointDeviceId))
            {
                // need an endpoint id
                return;
            }

            var backingConnection = Session.BackingSession!.CreateEndpointConnection(EndpointDeviceId);

            if (backingConnection == null)
            {
                // failed to create connection
                return;
            }

            // do this here to make sure the event handler is wired up before we open
            var conn = new MidiEndpointConnection(backingConnection);

            if (backingConnection.Open())
            {
                WriteObject(conn);
            }
            else
            {
                // unable to open session
            }



        }


    }



}
