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


        public event EventHandler<MessageReceivedEventArgs>? MessageReceived;



        public MidiEndpointConnection(Microsoft.Windows.Devices.Midi2.MidiEndpointConnection backingConnection)
        {
            BackingConnection = backingConnection;

            // ideally, we wouldn't wire this up unless a command told us to
            BackingConnection.MessageReceived += BackingConnection_MessageReceived;
        }

        // todo: May need to wrap the event args as well
        private void BackingConnection_MessageReceived(Microsoft.Windows.Devices.Midi2.IMidiMessageReceivedEventSource sender, Microsoft.Windows.Devices.Midi2.MidiMessageReceivedEventArgs args)
        {
            if (MessageReceived != null)
            {
                List<UInt32> words = [];
                args.AppendWordsToList(words);

                var newArgs = new MessageReceivedEventArgs(args.Timestamp, words.ToArray());

                MessageReceived(this, newArgs);
            }
        }

        ~MidiEndpointConnection()
        {
            if (BackingConnection != null)
            {
                BackingConnection.MessageReceived -= BackingConnection_MessageReceived;
                BackingConnection = null;
            }
        }

    }
}
