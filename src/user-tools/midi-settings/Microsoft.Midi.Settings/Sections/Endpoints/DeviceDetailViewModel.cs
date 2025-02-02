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
    public partial class DeviceDetailViewModel : ObservableRecipient, INavigationAware
    {
        public EndpointUserMetadata UserMetadata { get; private set; } = new EndpointUserMetadata();

        [ObservableProperty]
        public bool hasFunctionBlocks;

        [ObservableProperty]
        public bool hasParent;

        [ObservableProperty]
        public MidiEndpointDeviceInformation deviceInformation;

        [ObservableProperty]
        public IReadOnlyList<MidiFunctionBlock> functionBlocks;

        [ObservableProperty]
        public MidiEndpointTransportSuppliedInfo transportSuppliedInfo;

        [ObservableProperty]
        public MidiEndpointUserSuppliedInfo userSuppliedInfo;

        [ObservableProperty]
        public MidiDeclaredDeviceIdentity deviceIdentity;

        [ObservableProperty]
        public MidiDeclaredStreamConfiguration streamConfiguration;

        [ObservableProperty]
        public MidiDeclaredEndpointInfo endpointInfo;

        [ObservableProperty]
        public DeviceInformation parentDeviceInformation;

        public DeviceDetailViewModel()
        {
            FunctionBlocks = new List<MidiFunctionBlock>(); // to prevent null binding

            // ugly, but there to prevent binding errors. Making everything nullable
            // bleeds over into the xaml, requires converters, etc. messy AF.

            this.TransportSuppliedInfo = new MidiEndpointTransportSuppliedInfo();
            this.UserSuppliedInfo = new MidiEndpointUserSuppliedInfo();
            this.DeviceIdentity = new MidiDeclaredDeviceIdentity();
            this.StreamConfiguration = new MidiDeclaredStreamConfiguration();
            this.EndpointInfo = new MidiDeclaredEndpointInfo();
           // this.ParentDeviceInformation = null;

            this.HasParent = false;
            this.HasFunctionBlocks = false;
        }

        public void OnNavigatedFrom()
        {
           // throw new NotImplementedException();
        }

        public void OnNavigatedTo(object parameter)
        {
            this.DeviceInformation = (MidiEndpointDeviceInformation)parameter;

            this.FunctionBlocks = this.DeviceInformation.GetDeclaredFunctionBlocks();
            this.TransportSuppliedInfo = this.DeviceInformation.GetTransportSuppliedInfo();
            this.UserSuppliedInfo = this.DeviceInformation.GetUserSuppliedInfo();
            this.DeviceIdentity = this.DeviceInformation.GetDeclaredDeviceIdentity();
            this.StreamConfiguration = this.DeviceInformation.GetDeclaredStreamConfiguration();
            this.EndpointInfo = this.DeviceInformation.GetDeclaredEndpointInfo();
            this.ParentDeviceInformation = this.DeviceInformation.GetParentDeviceInformation();

            this.HasFunctionBlocks = FunctionBlocks.Count > 0;
            this.HasParent = this.ParentDeviceInformation != null;

        }
    }
}
