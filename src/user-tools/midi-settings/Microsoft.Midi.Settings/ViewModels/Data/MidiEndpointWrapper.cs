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
        private bool isMuted;

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
        private bool canMonitor;

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
            _loggingService.LogInfo($"Enter");

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


                IsMuted = DeviceInformation.IsMuted;

                // General metadata

                TransportSuppliedInfo = DeviceInformation.GetTransportSuppliedInfo();
                UserSuppliedInfo = DeviceInformation.GetUserSuppliedInfo();
                DeviceIdentity = DeviceInformation.GetDeclaredDeviceIdentity();
                StreamConfiguration = DeviceInformation.GetDeclaredStreamConfiguration();
                EndpointInfo = DeviceInformation.GetDeclaredEndpointInfo();

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
                UniqueIdentifier = string.Empty;
                HasUniqueIdentifier = false;

                if (!String.IsNullOrWhiteSpace(DeviceInformation.GetDeclaredEndpointInfo().ProductInstanceId))
                {
                    UniqueIdentifier = DeviceInformation.GetDeclaredEndpointInfo().ProductInstanceId;
                    HasUniqueIdentifier = !String.IsNullOrWhiteSpace(UniqueIdentifier);
                }
                else if (!String.IsNullOrWhiteSpace(TransportSuppliedInfo.SerialNumber))
                {
                    UniqueIdentifier = TransportSuppliedInfo.SerialNumber;
                    HasUniqueIdentifier = !String.IsNullOrWhiteSpace(UniqueIdentifier);

                    // todo: may want to move this to a central location
                    // these are hard-coded serial numbers found which are not actually unique
                    string[] invalidUniqueIndentifiers =
                    {
                        "no serial number",
                        "no_serial_number",
                        "noserial",
                        "ffff",
                        "0000",
                        "none"
                    };

                    bool invalidSerial = Array.Exists(invalidUniqueIndentifiers, serial => serial.Equals(UniqueIdentifier, StringComparison.OrdinalIgnoreCase));
                    if (invalidSerial)
                    {
                        HasUniqueIdentifier = false;
                    }
                }


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

                string imagePath = string.Empty;
                if (MidiImageAssetHelper.EndpointHasValidCustomImageAsset(DeviceInformation))
                {
                    imagePath = MidiImageAssetHelper.GetImageFullPathForEndpoint(DeviceInformation);
                }
                else
                {
                    imagePath = MidiImageAssetHelper.GetDefaultImageFullPathForEndpoint(DeviceInformation);
                }

                Image = App.GetService<IMidiEndpointImageService>().GetImageSource(imagePath);

                lock (FunctionBlocks)
                {
                    // function blocks
                    FunctionBlocks.Clear();
                    foreach (var fb in DeviceInformation.GetDeclaredFunctionBlocks())
                    {
                        FunctionBlocks.Add(fb);
                    }
                    HasFunctionBlocks = FunctionBlocks.Count > 0;
                }

                lock (GroupTerminalBlocks)
                {
                    // group terminal blocks
                    GroupTerminalBlocks.Clear();
                    foreach (var gtb in DeviceInformation.GetGroupTerminalBlocks())
                    {
                        GroupTerminalBlocks.Add(gtb);
                    }
                    HasGroupTerminalBlocks = GroupTerminalBlocks.Count > 0;
                }

                // parent device info
                ParentDeviceInformation = DeviceInformation.GetParentDeviceInformation();
                HasParent = ParentDeviceInformation != null;


            }, null);


            Task.Run(() =>
            {
                _loggingService.LogInfo($"Enter");

                System.Diagnostics.Debug.WriteLine("MidiEndpointWrapper: Getting MIDI 1.0 Ports");

                // MIDI 1.0 Input Ports / Sources
                var inputPorts = DeviceInformation.FindAllAssociatedMidi1PortsForThisEndpoint(Midi1PortFlow.MidiMessageSource);
                var outputPorts = DeviceInformation.FindAllAssociatedMidi1PortsForThisEndpoint(Midi1PortFlow.MidiMessageDestination);

                System.Diagnostics.Debug.WriteLine($"MidiEndpointWrapper: Returned {Name} MIDI 1 input ports:  {inputPorts.Count}");
                System.Diagnostics.Debug.WriteLine($"MidiEndpointWrapper: Returned {Name} MIDI 1 output ports: {outputPorts.Count}");

                context.Post(_ =>
                {
                    System.Diagnostics.Debug.WriteLine("MidiEndpointWrapper: Posting to UI Thread to update MIDI 1 port list");

                    lock (Midi1InputPorts)
                    {
                        Midi1InputPorts.Clear();
                        foreach (var source in inputPorts.OrderBy((p) => p.PortNumber))
                        {
                            Midi1InputPorts.Add(source);
                        }
                        CountMidi1InputPorts = Midi1InputPorts.Count;
                        HasSingleInputPort = CountMidi1InputPorts == 1;

                        if (HasSingleInputPort)
                        {
                            SingleInputPortName = inputPorts[0].PortName;
                        }
                    }

                    lock (Midi1OutputPorts)
                    {
                        Midi1OutputPorts.Clear();
                        // MIDI 1.0 Output Ports / Destinations
                        foreach (var destination in outputPorts.OrderBy((p) => p.PortNumber))
                        {
                            Midi1OutputPorts.Add(destination);
                        }
                        CountMidi1OutputPorts = Midi1OutputPorts.Count;
                        HasSingleOutputPort = CountMidi1OutputPorts == 1;

                        if (HasSingleOutputPort)
                        {
                            SingleOutputPortName = outputPorts[0].PortName;
                        }
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
        private readonly ILoggingService _loggingService;

        public MidiEndpointWrapper(MidiEndpointDeviceInformation deviceInformation,
            IMidiTransportInfoService transportInfoService,
            INavigationService navigationService,
            ISynchronizationContextService synchronizationContextService,
            IMidiConsoleToolsService consoleToolsService,
            IMidiPanicService panicService,
            ILoggingService loggingService)
        {
            _loggingService = loggingService;

            System.Diagnostics.Debug.WriteLine("MidiEndpointWrapper: Constructing");

            _navigationService = navigationService;
            _synchronizationContextService = synchronizationContextService;
            _panicService = panicService;
            _transportInfoService = transportInfoService;
            _consoleToolsService = consoleToolsService;

            CanMonitor = consoleToolsService.IsMidiConsolePresent();

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
                    _loggingService.LogInfo($"Enter");

                    System.Diagnostics.Debug.WriteLine("Sending panic");

                    // TODO: Could make this a bit faster by sending only for valid groups
                    _panicService.SendMidiPanic(DeviceInformation.EndpointDeviceId);

                });


            MonitorEndpointCommand = new RelayCommand(
                () =>
                {
                    _loggingService.LogInfo($"Enter");

                    System.Diagnostics.Debug.WriteLine("Monitor");

                    _consoleToolsService.MonitorEndpoint(DeviceInformation.EndpointDeviceId);                   

                });

            DeviceInformation = deviceInformation;

            RefreshData();


        }
    }


}
