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

        public ImageSource SmallImage { get; private set; }

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

            ViewDeviceDetailsCommand = new RelayCommand<MidiEndpointWrapper>(
                (param) =>
                {
                    System.Diagnostics.Debug.WriteLine("View Device Details Command exec");

                    if (param == null)
                    {
                        _navigationService.NavigateTo(typeof(DeviceDetailViewModel).FullName!, this);
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

            Name = deviceInformation.Name;
            Id = deviceInformation.EndpointDeviceId;
            TransportCode = deviceInformation.GetTransportSuppliedInfo().TransportCode;
            TransportId = deviceInformation.GetTransportSuppliedInfo().TransportId;

            ManufacturerName = deviceInformation.GetTransportSuppliedInfo().ManufacturerName.Trim();

            HasManufacturerName = ManufacturerName != string.Empty;

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

            // multi-client

            if (deviceInformation.GetTransportSuppliedInfo().SupportsMultiClient)
            {
                IsMultiClient = true;
            }
            else
            {
                IsMultiClient = false;
            }


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

            // TODO: These may be a bit expensive to get. TBD

            Task.Run(() =>
            {
                System.Diagnostics.Debug.WriteLine("MidiEndpointWrapper: Getting MIDI 1.0 Ports");

                // MIDI 1.0 Input Ports / Sources
                var inputPorts = deviceInformation.FindAllAssociatedMidi1PortsForThisEndpoint(Midi1PortFlow.MidiMessageSource);
                var outputPorts = deviceInformation.FindAllAssociatedMidi1PortsForThisEndpoint(Midi1PortFlow.MidiMessageDestination);


                _synchronizationContextService.GetUIContext()?.Post(_ =>
                {
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
    }


}
