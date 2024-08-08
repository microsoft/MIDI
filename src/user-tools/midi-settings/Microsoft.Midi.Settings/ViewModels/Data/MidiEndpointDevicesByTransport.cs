using System;
using System.Collections.Generic;
using System.Collections.ObjectModel;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace Microsoft.Midi.Settings.ViewModels
{
    public class MidiEndpointDevicesByTransport
    {
        public MidiEndpointDevicesByTransport(MidiServiceTransportPluginInfo transport)
        {
            Transport = transport;
        }


        public MidiServiceTransportPluginInfo Transport { get; private set; }

        public ObservableCollection<MidiEndpointDeviceListItem> EndpointDevices { get; } = [];

    }
}
