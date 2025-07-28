// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

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
                // this could be more efficient.

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
