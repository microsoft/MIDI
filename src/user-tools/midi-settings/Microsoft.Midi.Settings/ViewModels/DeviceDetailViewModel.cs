using System;
using System.Collections.Generic;
using System.Collections.ObjectModel;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using CommunityToolkit.Mvvm.ComponentModel;
using Microsoft.Midi.Settings.Contracts.ViewModels;
using Microsoft.Midi.Settings.Models;
using Microsoft.Midi.Settings.ViewModels.Data;
using Microsoft.UI.Dispatching;
using Windows.Devices.Enumeration;

namespace Microsoft.Midi.Settings.ViewModels
{
    public class DeviceDetailViewModel : ObservableRecipient, INavigationAware
    {
        public EndpointUserMetadata UserMetadata { get; private set; } = new EndpointUserMetadata();

        public MidiEndpointDeviceInformation DeviceInformation
        {
            get; private set;
        }
        public IReadOnlyList<MidiFunctionBlock> FunctionBlocks => DeviceInformation.GetDeclaredFunctionBlocks();

        public MidiEndpointTransportSuppliedInfo TransportSuppliedInfo => DeviceInformation.GetTransportSuppliedInfo();

        public MidiEndpointUserSuppliedInfo UserSuppliedInfo => DeviceInformation.GetUserSuppliedInfo();

        public MidiDeclaredDeviceIdentity DeviceIdentity => DeviceInformation.GetDeclaredDeviceIdentity();

        public MidiDeclaredStreamConfiguration StreamConfiguration => DeviceInformation.GetDeclaredStreamConfiguration();

        public MidiDeclaredEndpointInfo EndpointInfo => DeviceInformation.GetDeclaredEndpointInfo();

        public DeviceInformation ParentDeviceInformation => DeviceInformation.GetParentDeviceInformation();


        public void OnNavigatedFrom()
        {
           // throw new NotImplementedException();
        }

        public void OnNavigatedTo(object parameter)
        {
            DeviceInformation = (MidiEndpointDeviceInformation)parameter;
        }
    }
}
