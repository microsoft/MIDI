using System;
using System.Collections.Generic;
using System.Collections.ObjectModel;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using System.Windows.Input;
using CommunityToolkit.Mvvm.ComponentModel;
using CommunityToolkit.Mvvm.Input;
using Microsoft.Midi.Settings.Contracts.Services;
using Microsoft.Midi.Settings.Contracts.ViewModels;
using Microsoft.Midi.Settings.Models;
using Microsoft.UI.Dispatching;
using Microsoft.UI.Xaml;
using Windows.Devices.Enumeration;



//using Microsoft.Midi.Settings.SdkMock;

using Windows.Devices.Midi2;

namespace Microsoft.Midi.Settings.ViewModels
{
    public class DevicesViewModel : ObservableRecipient, INavigationAware
    {

        private DispatcherQueue _dispatcherQueue;


        private INavigationService _navigationService;

        public ICommand ViewDeviceDetailsCommand
        {
            get; private set;
        }

        public DevicesViewModel(INavigationService navigationService)
        {
            _navigationService = navigationService;

            _dispatcherQueue = DispatcherQueue.GetForCurrentThread();

            ViewDeviceDetailsCommand = new RelayCommand<MidiEndpointDeviceInformation>(
                async (param) =>
                {
                    System.Diagnostics.Debug.WriteLine("View Device Details Command exec");

                    _navigationService.NavigateTo(typeof(DeviceDetailViewModel).FullName!, param);

                });
        }

        public ObservableCollection<MidiEndpointDevicesByTransport> MidiEndpointDevicesByTransport { get; } = [];


        // CollectionViewSource grouped by transport



        public void RefreshDeviceCollection(bool showDiagnosticsEndpoints = false)
        {
            ObservableCollection<MidiEndpointDevicesByTransport> tempCollection = [];

            // go through devices in AppState and group by parent

            // pre-populate with transports

            foreach (var transport in MidiService.GetInstalledTransportPlugins())
            {
                var t = new MidiEndpointDevicesByTransport();
                t.Transport = transport;

                tempCollection.Add(t);
            }

            // now get all the endpoint devices and put them in groups by transport

            foreach (var endpointDevice in AppState.Current.MidiEndpointDeviceWatcher.EnumeratedEndpointDevices.Values)
            {
                // Get the transport

                var transportId = endpointDevice.TransportId;

                var parentTransport = tempCollection.Where(x => x.Transport.Id == transportId).FirstOrDefault();

                // add this device to the transport's collection
                if (parentTransport != null)
                {
                    parentTransport.EndpointDevices.Add(endpointDevice);
                }

            }


            // Only show relevant transports. Either they have children, or support
            // creating a runtime through the settings application

            MidiEndpointDevicesByTransport.Clear();

            foreach (var item in tempCollection.OrderBy(x => x.Transport.Name))
            {
                // TODO: this is a hack. Probably shouldn't be using the mnemonic directly
                // Instead, need a transport properly for purpose like we have with endpoints
                if (item.Transport.Mnemonic == "DIAG")
                {
                    if (showDiagnosticsEndpoints)
                    {
                        MidiEndpointDevicesByTransport.Add(item);
                    }
                }
                else if (item.EndpointDevices.Count > 0 || item.Transport.IsRuntimeCreatableBySettings)
                {
                    MidiEndpointDevicesByTransport.Add(item);
                }
            }




        }



        public void OnNavigatedFrom()
        {
        }


        public void OnNavigatedTo(object parameter)
        {
            RefreshDeviceCollection();
        }
    }
}
