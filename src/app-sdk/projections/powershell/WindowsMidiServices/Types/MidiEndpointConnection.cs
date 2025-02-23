using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace WindowsMidiServices
{
    public class MidiEndpointConnection
    {
        public Guid ConnectionId => BackingConnection.ConnectionId;
        public string EndpointDeviceId => BackingConnection.ConnectedEndpointDeviceId;

        internal Microsoft.Windows.Devices.Midi2.MidiEndpointConnection? BackingConnection { get; set; }

        public MidiEndpointConnection(Microsoft.Windows.Devices.Midi2.MidiEndpointConnection backingConnection)
        {
            BackingConnection = backingConnection;
        }

        ~MidiEndpointConnection()
        {
            if (BackingConnection != null)
            {
                BackingConnection = null;
            }
        }

    }
}
