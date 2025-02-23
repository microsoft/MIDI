using System;
using System.Collections.Generic;
using System.Linq;
using System.Management.Automation;
using System.Text;
using System.Threading.Tasks;

namespace WindowsMidiServices
{


    [Cmdlet(VerbsCommon.Open, "MidiConnection")]
    public class ConnectToEndpointCommand : Cmdlet
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

            if (backingConnection.Open())
            {
                var conn = new MidiEndpointConnection(backingConnection);

                WriteObject(conn);
            }
            else
            {
                // unable to open session
            }



        }


    }


    [Cmdlet(VerbsCommon.Close, "MidiEndpoint")]
    public class DisconnectFromEndpointCommand : Cmdlet
    {


    }


}
