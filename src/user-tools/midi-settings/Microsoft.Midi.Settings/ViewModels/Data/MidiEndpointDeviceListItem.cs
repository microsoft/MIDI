using CommunityToolkit.Mvvm.ComponentModel;
using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace Microsoft.Midi.Settings.ViewModels
{
    public partial class MidiEndpointDeviceListItem : ObservableRecipient
    {
        [ObservableProperty]
        public MidiEndpointDeviceInformation deviceInformation;

        [ObservableProperty]
        public MidiEndpointTransportSuppliedInfo transportSuppliedInfo;

        [ObservableProperty]
        public MidiEndpointUserSuppliedInfo userSuppliedInfo;

        [ObservableProperty]
        public MidiDeclaredEndpointInfo declaredEndpointInfo;

        public MidiEndpointDeviceListItem(MidiEndpointDeviceInformation deviceInformation)
        { 
            DeviceInformation = deviceInformation;

            TransportSuppliedInfo = deviceInformation.GetTransportSuppliedInfo();
            UserSuppliedInfo = deviceInformation.GetUserSuppliedInfo();
            DeclaredEndpointInfo = deviceInformation.GetDeclaredEndpointInfo();
        }


        public string Name => DeviceInformation.Name;

        public string EndpointDeviceId => DeviceInformation.EndpointDeviceId;

        public string UserSuppliedDescription => UserSuppliedInfo.Description;

        public MidiEndpointNativeDataFormat NativeDataFormat => TransportSuppliedInfo.NativeDataFormat;

        public bool SupportsMidi10Protocol => DeclaredEndpointInfo.SupportsMidi10Protocol;
        public bool SupportsMidi20Protocol => DeclaredEndpointInfo.SupportsMidi10Protocol;


        public string TransportSuppliedDescription => TransportSuppliedInfo.Description;

        public string TransportSuppliedSerialNumber => TransportSuppliedInfo.SerialNumber;

        public bool SupportsMultiClient => TransportSuppliedInfo.SupportsMultiClient;


    }
}
