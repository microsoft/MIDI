// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

using System;
using System.Collections.Generic;
using System.Collections.ObjectModel;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using CommunityToolkit.Mvvm.ComponentModel;
using Microsoft.Midi.Settings.Contracts.Services;
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
        public bool hasGroupTerminalBlocks;

        [ObservableProperty]
        public bool hasParent;

        [ObservableProperty]
        public bool showMidi2Properties;


        [ObservableProperty]
        public MidiEndpointDeviceInformation deviceInformation;

        [ObservableProperty]
        public MidiEndpointWrapper endpointWrapper;


        public ObservableCollection<MidiFunctionBlock> FunctionBlocks = [];
        public ObservableCollection<MidiGroupTerminalBlock> GroupTerminalBlocks = [];


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

        private readonly ISynchronizationContextService _synchronizationContextService;
        public DeviceDetailViewModel(ISynchronizationContextService synchronizationContextService)
        {
            _synchronizationContextService = synchronizationContextService;

            System.Diagnostics.Debug.WriteLine("Start clearing properties");

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
            this.HasGroupTerminalBlocks = false;
            this.ShowMidi2Properties = false;

            System.Diagnostics.Debug.WriteLine("Finished clearing properties");
        }

        public void OnNavigatedFrom()
        {
           // throw new NotImplementedException();
        }

        public void OnNavigatedTo(object parameter)
        {
            System.Diagnostics.Debug.WriteLine("Start setting device-specific properties properties");

            this.EndpointWrapper = (MidiEndpointWrapper)parameter;
            this.DeviceInformation = this.EndpointWrapper.DeviceInformation;

            this.TransportSuppliedInfo = this.DeviceInformation.GetTransportSuppliedInfo();
            this.UserSuppliedInfo = this.DeviceInformation.GetUserSuppliedInfo();
            this.DeviceIdentity = this.DeviceInformation.GetDeclaredDeviceIdentity();
            this.StreamConfiguration = this.DeviceInformation.GetDeclaredStreamConfiguration();
            this.EndpointInfo = this.DeviceInformation.GetDeclaredEndpointInfo();
            this.ParentDeviceInformation = this.DeviceInformation.GetParentDeviceInformation();

            this.HasParent = this.ParentDeviceInformation != null;

            ShowMidi2Properties = (this.TransportSuppliedInfo.NativeDataFormat == MidiEndpointNativeDataFormat.UniversalMidiPacketFormat);

            // we're working with observable collections here, so need to dispatch
            _synchronizationContextService.GetUIContext().Post(_ =>
            {
                System.Diagnostics.Debug.WriteLine("Enter Dispatcher worker: function blocks and group terminal blocks");

                FunctionBlocks.Clear();
                GroupTerminalBlocks.Clear();

                foreach (var fb in DeviceInformation.GetDeclaredFunctionBlocks())
                {
                    FunctionBlocks.Add(fb);
                }

                foreach (var gtb in DeviceInformation.GetGroupTerminalBlocks())
                {
                    GroupTerminalBlocks.Add(gtb);
                }

                this.HasFunctionBlocks = FunctionBlocks.Count > 0;
                this.HasGroupTerminalBlocks = GroupTerminalBlocks.Count > 0;

                System.Diagnostics.Debug.WriteLine("Exit Dispatcher worker: function blocks and group terminal blocks");
            }, null);


            System.Diagnostics.Debug.WriteLine("Finished setting device-specific properties properties");
        }
    }
}
