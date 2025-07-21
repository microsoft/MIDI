// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

using CommunityToolkit.Mvvm.ComponentModel;
using CommunityToolkit.Mvvm.Input;
using Microsoft.Midi.Settings.Contracts.Services;
using Microsoft.Midi.Settings.Contracts.ViewModels;
using Microsoft.Midi.Settings.Models;
using Microsoft.UI.Dispatching;
using System;
using System.Collections.Generic;
using System.Collections.ObjectModel;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using System.Windows.Input;

namespace Microsoft.Midi.Settings.ViewModels
{
    public class EndpointsAllViewModel : ObservableRecipient, INavigationAware
    {
        private readonly INavigationService _navigationService;
        private readonly IMidiEndpointEnumerationService _enumerationService;

        public ICommand ViewDeviceDetailsCommand
        {
            get; private set;
        }

        public DispatcherQueue? DispatcherQueue { get; set; }

        public EndpointsAllViewModel(
            INavigationService navigationService,
            IMidiEndpointEnumerationService enumerationService
            )
        {
            _navigationService = navigationService;
            _enumerationService = enumerationService;

            ViewDeviceDetailsCommand = new RelayCommand<MidiEndpointDeviceInformation>(
                (param) =>
                {
                    System.Diagnostics.Debug.WriteLine("View Device Details Command exec");

                    _navigationService.NavigateTo(typeof(DeviceDetailViewModel).FullName!, param);

                });
        }

        public ObservableCollection<MidiEndpointDevicesByTransport> MidiEndpointDevicesByTransport { get; } = [];


        public void RefreshDeviceCollection(bool showDiagnosticsEndpoints = false)
        {
            if (DispatcherQueue == null) return;

            DispatcherQueue.TryEnqueue(DispatcherQueuePriority.Normal, () =>
            {
                System.Diagnostics.Debug.WriteLine("Begin RefreshDeviceCollection");

                ObservableCollection<MidiEndpointDevicesByTransport> tempCollection = [];

                // go through devices in AppState and group by parent

                // pre-populate with transports

                foreach (var transport in MidiReporting.GetInstalledTransportPlugins())
                {
                    var t = new MidiEndpointDevicesByTransport(transport);

                    tempCollection.Add(t);
                }

                // now get all the endpoint devices and put them in groups by transport

                var enumeratedDevices = _enumerationService.MidiEndpointDeviceWatcher.EnumeratedEndpointDevices;

                foreach (var endpointDevice in enumeratedDevices.Values)
                {
                    if (endpointDevice != null)
                    {
                        // Get the transport

                        var transportInfo = endpointDevice.GetTransportSuppliedInfo();
                        var transportId = transportInfo.TransportId;

                        var parentTransport = tempCollection.Where(x => x.Transport.Id == transportId).FirstOrDefault();

                        // add this device to the transport's collection
                        if (parentTransport != null)
                        {
                            parentTransport.EndpointDevices.Add(new MidiEndpointDeviceListItem(endpointDevice));
                        }
                    }
                }


                // Only show relevant transports. Either they have children, or support
                // creating a runtime through the settings application

                MidiEndpointDevicesByTransport.Clear();

                foreach (var item in tempCollection.OrderBy(x => x.Transport.Name))
                {
                    // TODO: this is a hack. Probably shouldn't be using the transport code directly
                    // Instead, need a transport property for purpose like we have with endpoints
                    if (item.Transport.TransportCode == "DIAG")
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


                System.Diagnostics.Debug.WriteLine("Completed RefreshDeviceCollection");



            });

        }



        public void OnNavigatedFrom()
        {
        }


        public void OnNavigatedTo(object parameter)
        {
        }
    }
}
