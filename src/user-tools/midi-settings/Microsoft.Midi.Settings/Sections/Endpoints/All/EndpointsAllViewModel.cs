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
using Microsoft.UI.Xaml.Media;
using Microsoft.UI.Xaml.Media.Imaging;
using Microsoft.Windows.Devices.Midi2.Utilities.Metadata;
using System;
using System.Collections.Generic;
using System.Collections.ObjectModel;
using System.Linq;
using System.Net;
using System.Text;
using System.Threading.Tasks;
using System.Windows.Input;

namespace Microsoft.Midi.Settings.ViewModels
{
    public partial class MidiEndpointDisplayItem : ObservableRecipient
    {
        public string Name { get; private set; }
        public string Description { get; private set; }

        public string TransportCode { get; private set; }

        public string Id { get; private set; }

        public string UniqueIdentifier { get; private set; }
        public bool HasUniqueIdentifier { get; private set; }

        public bool IsNativeUmp { get; private set; }
        public bool SupportsMidi2 { get; private set; }

        public ImageSource SmallImage { get; private set; }

        [ObservableProperty]
        private int countMidi1InputPorts;

        [ObservableProperty]
        private int countMidi1OutputPorts;

        public ObservableCollection<MidiEndpointAssociatedPortDeviceInformation> Midi1InputPorts { get; private set; } = [];
        public ObservableCollection<MidiEndpointAssociatedPortDeviceInformation> Midi1OutputPorts { get; private set; } = [];

        public MidiEndpointDisplayItem(MidiEndpointDeviceInformation deviceInformation, IMidiTransportInfoService transportInfoService)
        {
            Name = deviceInformation.Name;
            Id = deviceInformation.EndpointDeviceId;
            TransportCode = deviceInformation.GetTransportSuppliedInfo().TransportCode;

            // description
            if (deviceInformation.GetUserSuppliedInfo().Description != string.Empty)
            {
                Description = deviceInformation.GetUserSuppliedInfo().Description;
            }
            else
            {
                // look up the name of the transport given the transport id
                Description = "A " +
                    transportInfoService.GetTransportForId(deviceInformation.GetTransportSuppliedInfo().TransportId).Name +
                    " endpoint.";
            }

            // unique identifier
            if (deviceInformation.GetDeclaredEndpointInfo().ProductInstanceId != string.Empty)
            {
                UniqueIdentifier = deviceInformation.GetDeclaredEndpointInfo().ProductInstanceId;
            }
            else if (deviceInformation.GetTransportSuppliedInfo().SerialNumber != string.Empty)
            {
                UniqueIdentifier = deviceInformation.GetTransportSuppliedInfo().SerialNumber;
            }
            else
            {
                UniqueIdentifier = string.Empty;
            }

            HasUniqueIdentifier = UniqueIdentifier != string.Empty;


            // small image

            var imagePath = MidiImageAssetHelper.GetSmallImageFullPathForEndpoint(deviceInformation);

            if (imagePath.ToLower().EndsWith(".svg"))
            {
                // SVG requires a specific decoder
                var source = new SvgImageSource(new Uri(imagePath, UriKind.Absolute));
                SmallImage = source;
            }
            else
            {
                // this works with PNG, JPG, etc.
                var source = new BitmapImage(new Uri(imagePath, UriKind.Absolute));
                SmallImage = source;
            }

            // native UMP

            if (deviceInformation.GetTransportSuppliedInfo().NativeDataFormat == MidiEndpointNativeDataFormat.UniversalMidiPacketFormat)
            {
                IsNativeUmp = true;
            }
            else
            {
                IsNativeUmp = false;
            }

            // MIDI 2.0 protocol

            if (deviceInformation.GetDeclaredEndpointInfo().SupportsMidi20Protocol)
            {
                SupportsMidi2 = true;
            }
            else
            {
                SupportsMidi2 = false;
            }

            var uiThreadContext = SynchronizationContext.Current;

            // TODO: These may be a bit expensive to get. TBD

            Task.Run(() =>
            {
                // MIDI 1.0 Input Ports / Sources
                var inputPorts = deviceInformation.FindAllAssociatedMidi1PortsForThisEndpoint(Midi1PortFlow.MidiMessageSource);
                var outputPorts = deviceInformation.FindAllAssociatedMidi1PortsForThisEndpoint(Midi1PortFlow.MidiMessageDestination);


                uiThreadContext.Post(_ =>
                {
                    foreach (var source in inputPorts)
                    {
                        Midi1InputPorts.Add(source);
                    }

                    // MIDI 1.0 Output Ports / Destinations
                    foreach (var destination in outputPorts)
                    {
                        Midi1OutputPorts.Add(destination);
                    }

                    CountMidi1InputPorts = Midi1InputPorts.Count;
                    CountMidi1OutputPorts = Midi1OutputPorts.Count;

                }, null);

            });



        }
    }

    public partial class EndpointsAllViewModel : ObservableRecipient, INavigationAware
    {
        private readonly INavigationService _navigationService;
        private readonly IMidiEndpointEnumerationService _enumerationService;
        private readonly IMidiTransportInfoService _transportInfoService;

        public ICommand ViewDeviceDetailsCommand
        {
            get; private set;
        }

        public DispatcherQueue? DispatcherQueue { get; set; }

        public EndpointsAllViewModel(
            INavigationService navigationService,
            IMidiEndpointEnumerationService enumerationService,
            IMidiTransportInfoService transportInfoService
            )
        {
            _navigationService = navigationService;
            _enumerationService = enumerationService;
            _transportInfoService = transportInfoService;

            ViewDeviceDetailsCommand = new RelayCommand<MidiEndpointDeviceInformation>(
                (param) =>
                {
                    System.Diagnostics.Debug.WriteLine("View Device Details Command exec");

                    _navigationService.NavigateTo(typeof(DeviceDetailViewModel).FullName!, param);

                });
        }

        public ObservableCollection<MidiEndpointDisplayItem> Endpoints { get; } = [];

        [ObservableProperty]
        private string filterString = string.Empty;

        // todo: this should have a sort order and filter
        public void RefreshMidiEndpointDevices()
        {
            var allEndpoints = MidiEndpointDeviceInformation.FindAll(MidiEndpointDeviceInformationSortOrder.Name);

            // todo: sort

            foreach (var endpoint in allEndpoints)
            {
                if (FilterString.Trim() == string.Empty)
                {
                    Endpoints.Add(new MidiEndpointDisplayItem(endpoint, _transportInfoService));
                }
                else
                {
                    // TODO: process with filter

                    if (endpoint.Name.StartsWith(filterString))
                    {
                        Endpoints.Add(new MidiEndpointDisplayItem(endpoint, _transportInfoService));
                    }
                }

            }

        }


        public void OnNavigatedFrom()
        {
        }


        public void OnNavigatedTo(object parameter)
        {
            RefreshMidiEndpointDevices();
        }
    }
}
