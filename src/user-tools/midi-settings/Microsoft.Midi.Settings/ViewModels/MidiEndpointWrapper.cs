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
using Windows.Devices.Enumeration;

namespace Microsoft.Midi.Settings.ViewModels
{
    public partial class MidiEndpointWrapper : ObservableRecipient
    {
        public ICommand ViewDeviceDetailsCommand
        {
            get; private set;
        }

        public ICommand SendPanicCommand
        {
            get; private set;
        }

        public ICommand MonitorEndpointCommand
        {
            get; private set;
        }


        [ObservableProperty]
        private bool hasManufacturerName;

        [ObservableProperty]
        private string name;

        [ObservableProperty]
        private string description;


        [ObservableProperty]
        private string id;

        [ObservableProperty]
        private string uniqueIdentifier;

        [ObservableProperty]
        private bool hasUniqueIdentifier;

        [ObservableProperty]
        private bool isMultiClient;

        [ObservableProperty]
        private bool isNativeUmp;

        [ObservableProperty]
        private bool supportsMidi2;

        [ObservableProperty]
        private ImageSource image;

        [ObservableProperty]
        private bool hasUsbVidPid;

        [ObservableProperty]
        private string usbVendorIdFormatted;

        [ObservableProperty]
        private string usbProductIdFormatted;


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
        private MidiEndpointTransportSuppliedInfo transportSuppliedInfo;

        [ObservableProperty]
        private MidiEndpointUserSuppliedInfo userSuppliedInfo;

        [ObservableProperty]
        private MidiDeclaredDeviceIdentity deviceIdentity;

        [ObservableProperty]
        private MidiDeclaredStreamConfiguration streamConfiguration;

        [ObservableProperty]
        private MidiDeclaredEndpointInfo endpointInfo;

        public ObservableCollection<MidiFunctionBlock> FunctionBlocks = [];
        public ObservableCollection<MidiGroupTerminalBlock> GroupTerminalBlocks = [];

        [ObservableProperty]
        private bool hasFunctionBlocks;

        [ObservableProperty]
        private bool hasGroupTerminalBlocks;


        [ObservableProperty]
        private DeviceInformation? parentDeviceInformation;

        [ObservableProperty]
        private bool hasParent;




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

                HasManufacturerName = !String.IsNullOrWhiteSpace(DeviceInformation.GetTransportSuppliedInfo().ManufacturerName);


                // General metadata

                TransportSuppliedInfo = DeviceInformation.GetTransportSuppliedInfo();
                UserSuppliedInfo = DeviceInformation.GetUserSuppliedInfo();
                DeviceIdentity = DeviceInformation.GetDeclaredDeviceIdentity();
                StreamConfiguration = DeviceInformation.GetDeclaredStreamConfiguration();

                // USB VID/PID

                if (TransportSuppliedInfo.VendorId > 0)
                {
                    UsbVendorIdFormatted = TransportSuppliedInfo.VendorId.ToString("X4");
                }
                else
                {
                    UsbVendorIdFormatted = string.Empty;
                }

                if (TransportSuppliedInfo.ProductId > 0)
                {
                    UsbProductIdFormatted = TransportSuppliedInfo.ProductId.ToString("X4");
                }
                else
                {
                    UsbProductIdFormatted = string.Empty;
                }

                HasUsbVidPid = TransportSuppliedInfo.VendorId > 0 &&
                               TransportSuppliedInfo.ProductId > 0;


                // native UMP

                if (TransportSuppliedInfo.NativeDataFormat == MidiEndpointNativeDataFormat.UniversalMidiPacketFormat)
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
                if (!String.IsNullOrWhiteSpace(UserSuppliedInfo.Description))
                {
                    Description = UserSuppliedInfo.Description;
                }
                else
                {
                    // look up the name of the transport given the transport id
                    Description = "A " +
                        _transportInfoService.GetTransportForId(TransportSuppliedInfo.TransportId).Name +
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

                if (!String.IsNullOrWhiteSpace(DeviceInformation.GetDeclaredEndpointInfo().ProductInstanceId))
                {
                    UniqueIdentifier = DeviceInformation.GetDeclaredEndpointInfo().ProductInstanceId;
                }
                else if (!String.IsNullOrWhiteSpace(TransportSuppliedInfo.SerialNumber))
                {
                    UniqueIdentifier = TransportSuppliedInfo.SerialNumber;
                }
                else
                {
                    UniqueIdentifier = string.Empty;
                }

                HasUniqueIdentifier = !String.IsNullOrWhiteSpace(UniqueIdentifier);

                // multi-client

                if (TransportSuppliedInfo.SupportsMultiClient)
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


                // function blocks
                FunctionBlocks.Clear();
                foreach (var fb in DeviceInformation.GetDeclaredFunctionBlocks())
                {
                    FunctionBlocks.Add(fb);
                }
                HasFunctionBlocks = FunctionBlocks.Count > 0;

                // group terminal blocks
                GroupTerminalBlocks.Clear();
                foreach (var gtb in DeviceInformation.GetGroupTerminalBlocks())
                {
                    GroupTerminalBlocks.Add(gtb);
                }
                HasGroupTerminalBlocks = GroupTerminalBlocks.Count > 0;

                // parent device info
                ParentDeviceInformation = DeviceInformation.GetParentDeviceInformation();
                HasParent = ParentDeviceInformation != null;


            }, null);


            Task.Run(() =>
            {
                System.Diagnostics.Debug.WriteLine("MidiEndpointWrapper: Getting MIDI 1.0 Ports");

                // MIDI 1.0 Input Ports / Sources
                var inputPorts = DeviceInformation.FindAllAssociatedMidi1PortsForThisEndpoint(Midi1PortFlow.MidiMessageSource);
                var outputPorts = DeviceInformation.FindAllAssociatedMidi1PortsForThisEndpoint(Midi1PortFlow.MidiMessageDestination);

                context.Post(_ =>
                {
                    System.Diagnostics.Debug.WriteLine("MidiEndpointWrapper: Posting to UI Thread to update MIDI 1 port list");

                    Midi1InputPorts.Clear();
                    Midi1OutputPorts.Clear();

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

        private readonly INavigationService _navigationService;
        private readonly ISynchronizationContextService _synchronizationContextService;
        private readonly IMidiPanicService _panicService;
        private readonly IMidiTransportInfoService _transportInfoService;
        private readonly IMidiConsoleToolsService _consoleToolsService;

        public MidiEndpointWrapper(MidiEndpointDeviceInformation deviceInformation,
            IMidiTransportInfoService transportInfoService,
            INavigationService navigationService,
            ISynchronizationContextService synchronizationContextService,
            IMidiConsoleToolsService consoleToolsService,
            IMidiPanicService panicService)
        {
            System.Diagnostics.Debug.WriteLine("MidiEndpointWrapper: Constructing");

            _navigationService = navigationService;
            _synchronizationContextService = synchronizationContextService;
            _panicService = panicService;
            _transportInfoService = transportInfoService;
            _consoleToolsService = consoleToolsService;

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


            MonitorEndpointCommand = new RelayCommand(
                () =>
                {
                    System.Diagnostics.Debug.WriteLine("Monitor");

                    _consoleToolsService.MonitorEndpoint(DeviceInformation.EndpointDeviceId);                   

                });

            DeviceInformation = deviceInformation;

            RefreshData();


        }
    }


}
