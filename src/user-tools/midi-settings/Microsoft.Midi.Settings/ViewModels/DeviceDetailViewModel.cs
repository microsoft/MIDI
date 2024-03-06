using System;
using System.Collections.Generic;
using System.Collections.ObjectModel;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using CommunityToolkit.Mvvm.ComponentModel;
using Microsoft.Midi.Settings.Contracts.ViewModels;
using Microsoft.Midi.Settings.Models;
using Microsoft.UI.Dispatching;
using Windows.Devices.Enumeration;

namespace Microsoft.Midi.Settings.ViewModels
{
    public class DeviceDetailViewModel : ObservableRecipient, INavigationAware
    {

        public MidiEndpointDeviceInformation? DeviceInformation
        {
            get; private set;
        }

        public DeviceInformation? ParentDeviceInformation
        {
            get; private set;
        }


        public void OnNavigatedFrom()
        {
           // throw new NotImplementedException();
        }

        public void OnNavigatedTo(object parameter)
        {
            DeviceInformation = (MidiEndpointDeviceInformation)parameter;

            ParentDeviceInformation = DeviceInformation.GetParentDeviceInformation();
        }
    }
}
