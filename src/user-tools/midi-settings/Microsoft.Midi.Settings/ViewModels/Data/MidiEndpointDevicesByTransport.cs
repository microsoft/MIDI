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
        public MidiServiceTransportPluginInfo Transport { get; set; }

        public ObservableCollection<MidiEndpointDeviceInformation> EndpointDevices { get; } = [];

    }
}
