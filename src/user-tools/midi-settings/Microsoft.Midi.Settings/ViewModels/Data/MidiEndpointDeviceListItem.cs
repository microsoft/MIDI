using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace Microsoft.Midi.Settings.ViewModels
{
    public class MidiEndpointDeviceListItem
    {
        private MidiEndpointDeviceInformation _deviceInformation;

        private MidiEndpointTransportSuppliedInfo _transportSuppliedInfo;

        private MidiEndpointUserSuppliedInfo _userSuppliedInfo;

        private MidiDeclaredEndpointInfo _declaredEndpointInfo;

        public MidiEndpointDeviceListItem(MidiEndpointDeviceInformation deviceInformation)
        { 
            _deviceInformation = deviceInformation;

            _transportSuppliedInfo = deviceInformation.GetTransportSuppliedInfo();
            _userSuppliedInfo = deviceInformation.GetUserSuppliedInfo();
            _declaredEndpointInfo = deviceInformation.GetDeclaredEndpointInfo();
        }


        public string Name => _deviceInformation.Name;

        public string EndpointDeviceId => _deviceInformation.EndpointDeviceId;

        public string UserSuppliedDescription => _userSuppliedInfo.Description;

        public MidiEndpointNativeDataFormat NativeDataFormat => _transportSuppliedInfo.NativeDataFormat;

        public bool SupportsMidi10Protocol => _declaredEndpointInfo.SupportsMidi10Protocol;
        public bool SupportsMidi20Protocol => _declaredEndpointInfo.SupportsMidi10Protocol;


        public string TransportSuppliedDescription => _transportSuppliedInfo.Description;

        public string TransportSuppliedSerialNumber => _transportSuppliedInfo.SerialNumber;

        public bool SupportsMultiClient => _transportSuppliedInfo.SupportsMultiClient;

        public MidiEndpointDeviceInformation DeviceInformation => _deviceInformation;

    }
}
