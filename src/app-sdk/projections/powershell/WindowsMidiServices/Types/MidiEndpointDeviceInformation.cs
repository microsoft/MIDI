using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace WindowsMidiServices
{
    public class MidiEndpointDeviceInformation
    {
        public string Name => BackingDeviceInformation.Name;
        public string EndpointDeviceId => BackingDeviceInformation.EndpointDeviceId;
      //  public string DeviceInstanceId => BackingDeviceInformation.DeviceInstanceId;
        public string TransportCode => BackingDeviceInformation.GetTransportSuppliedInfo().TransportCode;
        public Microsoft.Windows.Devices.Midi2.MidiEndpointNativeDataFormat NativeDataFormat => BackingDeviceInformation.GetTransportSuppliedInfo().NativeDataFormat;

        internal Microsoft.Windows.Devices.Midi2.MidiEndpointDeviceInformation BackingDeviceInformation { get; set; }

        public MidiEndpointDeviceInformation(Microsoft.Windows.Devices.Midi2.MidiEndpointDeviceInformation backingDeviceInformation)
        {
            BackingDeviceInformation = backingDeviceInformation;
        }

    }
}
