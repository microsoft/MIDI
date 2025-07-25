﻿// Copyright (c) Microsoft Corporation.
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
    public partial class SingleTransportEndpointViewModelBase : ObservableRecipient
    {
        private readonly INavigationService _navigationService;
        protected readonly IMidiEndpointEnumerationService _enumerationService;

        protected readonly string _transportCode;

        public ICommand ViewDeviceDetailsCommand
        {
            get; private set;
        }

        public DispatcherQueue? DispatcherQueue { get; set; }

        public SingleTransportEndpointViewModelBase(string transportCode, 
            INavigationService navigationService,
            IMidiEndpointEnumerationService enumerationService)
        {
            _transportCode = transportCode.ToUpper().Trim();

            _navigationService = navigationService;
            _enumerationService = enumerationService;

            ViewDeviceDetailsCommand = new RelayCommand<MidiEndpointDeviceInformation>(
                (param) =>
                {
                    System.Diagnostics.Debug.WriteLine("View Device Details Command exec");

                    _navigationService.NavigateTo(typeof(DeviceDetailViewModel).FullName!, param);

                });
        }

        [ObservableProperty]
        public MidiServiceTransportPluginInfo transport;

        public ObservableCollection<MidiEndpointDeviceListItem> MidiEndpointDevices { get; } = [];


        public void RefreshDeviceCollection()
        {
            if (DispatcherQueue == null) return;

            DispatcherQueue.TryEnqueue(DispatcherQueuePriority.Normal, () =>
            {
                System.Diagnostics.Debug.WriteLine("Begin RefreshDeviceCollection");

                foreach (var transport in MidiReporting.GetInstalledTransportPlugins())
                {
                    if (transport.TransportCode.ToUpper() == _transportCode)
                    {
                        Transport = transport;
                        break;
                    }
                }

                MidiEndpointDevices.Clear();

                // now get all the endpoint devices and put them in groups by transport

                var enumeratedDevices = _enumerationService.MidiEndpointDeviceWatcher.EnumeratedEndpointDevices.Values.OrderBy(x=>x.Name);

                foreach (var endpointDevice in enumeratedDevices)
                {
                    if (endpointDevice != null && endpointDevice.GetTransportSuppliedInfo().TransportCode == _transportCode)
                    {
                        MidiEndpointDevices.Add(new MidiEndpointDeviceListItem(endpointDevice));
                    }
                }

                System.Diagnostics.Debug.WriteLine("Completed RefreshDeviceCollection");

            });

        }

    }
}
