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
using Microsoft.UI.Xaml.Media;
using Microsoft.UI.Xaml.Media.Imaging;
using Microsoft.Windows.Devices.Midi2.Utilities.Metadata;
using System;
using System.Collections.Generic;
using System.Collections.ObjectModel;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using System.Windows.Input;

namespace Microsoft.Midi.Settings.ViewModels
{
    public partial class MidiEndpointWrapper : ObservableRecipient
    {
        private readonly INavigationService _navigationService;
        private readonly ISynchronizationContextService _synchronizationContextService;
        private readonly IMidiPanicService _panicService;
        private readonly IMidiTransportInfoService _transportInfoService;

        public ICommand ViewDeviceDetailsCommand
        {
            get; private set;
        }

        public ICommand SendPanicCommand
        {
            get; private set;
        }


        public string ManufacturerName { get; private set; }
        public bool HasManufacturerName { get; private set; }
        public string Name { get; private set; }
        public string Description { get; private set; }

        public string TransportCode { get; private set; }

        public Guid TransportId { get; private set; }

        public string Id { get; private set; }

        public string UniqueIdentifier { get; private set; }
        public bool HasUniqueIdentifier { get; private set; }

        public bool IsMultiClient { get; private set; }
        public bool IsNativeUmp { get; private set; }
        public bool SupportsMidi2 { get; private set; }

        public ImageSource Image { get; private set; }

        [ObservableProperty]
        private bool hasSingleInputPort;

        [ObservableProperty]
        private bool hasSingleOutputPort;

        [ObservableProperty]
        private string singleInputPortName;

        [ObservableProperty]
        private string singleOutputPortName;


        [ObservableProperty]
        private int countMidi1InputPorts;

        [ObservableProperty]
        private int countMidi1OutputPorts;

        [ObservableProperty]
        private MidiEndpointDeviceInformation deviceInformation;

        public ObservableCollection<MidiEndpointAssociatedPortDeviceInformation> Midi1InputPorts { get; private set; } = [];
        public ObservableCollection<MidiEndpointAssociatedPortDeviceInformation> Midi1OutputPorts { get; private set; } = [];

        public void RefreshData(MidiEndpointDeviceInformation deviceInformation)
        {
            DeviceInformation = deviceInformation;

            RefreshData();
        }

        public void RefreshData()
        {
            // Property updates need to happen on UI thread
            var context = _synchronizationContextService.GetUIContext();

            if (context == null)
            {
                context = SynchronizationContext.Current;
            }

            context!.Post(_ =>
            {
                Name = DeviceInformation.Name;
                Id = DeviceInformation.EndpointDeviceId;
                TransportCode = DeviceInformation.GetTransportSuppliedInfo().TransportCode;
                TransportId = DeviceInformation.GetTransportSuppliedInfo().TransportId;

                ManufacturerName = DeviceInformation.GetTransportSuppliedInfo().ManufacturerName.Trim();

                HasManufacturerName = ManufacturerName != string.Empty;

                // native UMP

                if (DeviceInformation.GetTransportSuppliedInfo().NativeDataFormat == MidiEndpointNativeDataFormat.UniversalMidiPacketFormat)
                {
                    IsNativeUmp = true;
                }
                else
                {
                    IsNativeUmp = false;
                }

                // MIDI 2.0 protocol

                if (DeviceInformation.GetDeclaredEndpointInfo().SupportsMidi20Protocol)
                {
                    SupportsMidi2 = true;
                }
                else
                {
                    SupportsMidi2 = false;
                }

                // description
                if (DeviceInformation.GetUserSuppliedInfo().Description != string.Empty)
                {
                    Description = DeviceInformation.GetUserSuppliedInfo().Description;
                }
                else
                {
                    // look up the name of the transport given the transport id
                    Description = "A " +
                        _transportInfoService.GetTransportForId(DeviceInformation.GetTransportSuppliedInfo().TransportId).Name +
                        " endpoint";

                    if (SupportsMidi2)
                    {
                        Description += " which natively supports the MIDI 2.0 protocol.";
                    }
                    else
                    {
                        Description += ".";
                    }
                }

                // unique identifier
                if (DeviceInformation.GetDeclaredEndpointInfo().ProductInstanceId != string.Empty)
                {
                    UniqueIdentifier = DeviceInformation.GetDeclaredEndpointInfo().ProductInstanceId;
                }
                else if (DeviceInformation.GetTransportSuppliedInfo().SerialNumber != string.Empty)
                {
                    UniqueIdentifier = DeviceInformation.GetTransportSuppliedInfo().SerialNumber;
                }
                else
                {
                    UniqueIdentifier = string.Empty;
                }

                HasUniqueIdentifier = UniqueIdentifier != string.Empty;

                // multi-client

                if (DeviceInformation.GetTransportSuppliedInfo().SupportsMultiClient)
                {
                    IsMultiClient = true;
                }
                else
                {
                    IsMultiClient = false;
                }


                // small image

                var imagePath = MidiImageAssetHelper.GetImageFullPathForEndpoint(DeviceInformation);

                if (imagePath.ToLower().EndsWith(".svg"))
                {
                    // SVG requires a specific decoder
                    var source = new SvgImageSource(new Uri(imagePath, UriKind.Absolute));
                    Image = source;
                }
                else
                {
                    // this works with PNG, JPG, etc.
                    var source = new BitmapImage(new Uri(imagePath, UriKind.Absolute));
                    Image = source;
                }

            }, null);

            Task.Run(() =>
            {
                System.Diagnostics.Debug.WriteLine("MidiEndpointWrapper: Getting MIDI 1.0 Ports");

                // MIDI 1.0 Input Ports / Sources
                var inputPorts = DeviceInformation.FindAllAssociatedMidi1PortsForThisEndpoint(Midi1PortFlow.MidiMessageSource);
                var outputPorts = DeviceInformation.FindAllAssociatedMidi1PortsForThisEndpoint(Midi1PortFlow.MidiMessageDestination);

                context.Post(_ =>
                {
                    Midi1InputPorts.Clear();
                    Midi1OutputPorts.Clear();

                    System.Diagnostics.Debug.WriteLine("MidiEndpointWrapper: Posting to UI Thread");

                    foreach (var source in inputPorts.OrderBy((p) => p.PortNumber))
                    {
                        Midi1InputPorts.Add(source);
                    }

                    // MIDI 1.0 Output Ports / Destinations
                    foreach (var destination in outputPorts.OrderBy((p) => p.PortNumber))
                    {
                        Midi1OutputPorts.Add(destination);
                    }

                    CountMidi1InputPorts = Midi1InputPorts.Count;
                    CountMidi1OutputPorts = Midi1OutputPorts.Count;

                    HasSingleInputPort = CountMidi1InputPorts == 1;
                    HasSingleOutputPort = CountMidi1OutputPorts == 1;

                    if (HasSingleInputPort)
                    {
                        SingleInputPortName = inputPorts[0].PortName;
                    }

                    if (HasSingleOutputPort)
                    {
                        SingleOutputPortName = outputPorts[0].PortName;
                    }

                    System.Diagnostics.Debug.WriteLine("MidiEndpointWrapper: Completed posting to UI Thread");

                }, null);

            });
        }

        public MidiEndpointWrapper(MidiEndpointDeviceInformation deviceInformation,
            IMidiTransportInfoService transportInfoService,
            INavigationService navigationService,
            ISynchronizationContextService synchronizationContextService,
            IMidiPanicService panicService)
        {
            System.Diagnostics.Debug.WriteLine("MidiEndpointWrapper: Constructing");

            _navigationService = navigationService;
            _synchronizationContextService = synchronizationContextService;
            _panicService = panicService;
            _transportInfoService = transportInfoService;

            ViewDeviceDetailsCommand = new RelayCommand<MidiEndpointWrapper>(
                (param) =>
                {
                    System.Diagnostics.Debug.WriteLine("View Device Details Command exec");

                    if (param == null)
                    {
                        _navigationService.NavigateTo(typeof(DeviceDetailViewModel).FullName!, this.Id);
                    }
                    else
                    {
                        _navigationService.NavigateTo(typeof(DeviceDetailViewModel).FullName!, param);
                    }

                });

            SendPanicCommand = new RelayCommand(
                () =>
                {
                    System.Diagnostics.Debug.WriteLine("Sending panic");

                    // TODO: Could make this a bit faster by sending only for valid groups
                    _panicService.SendMidiPanic(DeviceInformation.EndpointDeviceId);

                });

            DeviceInformation = deviceInformation;

            RefreshData();


        }
    }


}
