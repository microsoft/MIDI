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
using System.Net;
using System.Text;
using System.Threading.Tasks;
using System.Windows.Input;
using Windows.ApplicationModel.DataTransfer;
using Windows.Devices.Enumeration;

namespace Microsoft.Midi.Settings.ViewModels
{
    public partial class DeviceDetailViewModel : ObservableRecipient, INavigationAware
    {
        public ICommand CopyEndpointDeviceIdCommand
        {
            get; private set;
        }

        public ICommand SubmitEndpointCustomizationCommand
        {
            get; private set;
        }


        [ObservableProperty]
        private bool hasFunctionBlocks;

        [ObservableProperty]
        private bool hasGroupTerminalBlocks;

        [ObservableProperty]
        private bool hasParent;

        [ObservableProperty]
        private bool showMidi2Properties;


        [ObservableProperty]
        private MidiEndpointWrapper endpointWrapper;


        public ObservableCollection<MidiFunctionBlock> FunctionBlocks = [];
        public ObservableCollection<MidiGroupTerminalBlock> GroupTerminalBlocks = [];


        [ObservableProperty]
        private MidiEndpointTransportSuppliedInfo transportSuppliedInfo;

        [ObservableProperty]
        private MidiEndpointUserSuppliedInfo userSuppliedInfo;

        [ObservableProperty]
        private string name;

        [ObservableProperty]
        private string description;


        [ObservableProperty]
        private string customizedName;

        [ObservableProperty]
        private string customizedDescription;

        [ObservableProperty]
        private string customizedImageFileName;

        [ObservableProperty]
        private bool customizedRequiresNoteOffTranslation;

        [ObservableProperty]
        private bool customizedSupportsMidiPolyphonicExpression;

        [ObservableProperty]
        private ushort customizedRecommendedControlChangeAutomationIntervalMilliseconds;


        [ObservableProperty]
        private MidiDeclaredDeviceIdentity deviceIdentity;

        [ObservableProperty]
        private MidiDeclaredStreamConfiguration streamConfiguration;

        [ObservableProperty]
        private MidiDeclaredEndpointInfo endpointInfo;

        [ObservableProperty]
        private DeviceInformation parentDeviceInformation;

        private readonly ISynchronizationContextService _synchronizationContextService;
        private readonly IMidiEndpointCustomizationService _endpointCustomizationService;
        private readonly IMidiEndpointEnumerationService _endpointEnumerationService;

        public DeviceDetailViewModel(
            ISynchronizationContextService synchronizationContextService, 
            IMidiEndpointCustomizationService endpointCustomizationService,
            IMidiEndpointEnumerationService endpointEnumerationService)
        {
            _synchronizationContextService = synchronizationContextService;
            _endpointCustomizationService = endpointCustomizationService;
            _endpointEnumerationService = endpointEnumerationService;

            System.Diagnostics.Debug.WriteLine("Start clearing properties");

            _endpointEnumerationService.EndpointUpdated += (s, e) =>
            {
                if (EndpointWrapper != null && e.Id == EndpointWrapper.Id)
                {
                    _synchronizationContextService?.GetUIContext().Post(_ =>
                    {
                        System.Diagnostics.Debug.WriteLine("Refreshing VM properties due to Endpoint Update event.");
                        EndpointWrapper = e;
                        RefreshVM();
                    }, null);
                }
            };

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


            CopyEndpointDeviceIdCommand = new RelayCommand(
            () =>
            {
                var dataPackage = new DataPackage();
                dataPackage.SetText(EndpointWrapper.Id);
                Clipboard.SetContent(dataPackage);
            });



            SubmitEndpointCustomizationCommand = new RelayCommand(
            () =>
            {
                var custom = new MidiEndpointUserSuppliedInfo();

                custom.Name = string.IsNullOrWhiteSpace(CustomizedName) ? string.Empty : CustomizedName.Trim();
                custom.Description = string.IsNullOrWhiteSpace(CustomizedDescription) ? string.Empty : CustomizedDescription.Trim();
                custom.ImageFileName = string.IsNullOrWhiteSpace(CustomizedImageFileName) ? string.Empty : CustomizedImageFileName.Trim();
                custom.RequiresNoteOffTranslation = CustomizedRequiresNoteOffTranslation;
                custom.SupportsMidiPolyphonicExpression = CustomizedSupportsMidiPolyphonicExpression;
                custom.RecommendedControlChangeAutomationIntervalMilliseconds = CustomizedRecommendedControlChangeAutomationIntervalMilliseconds;

                var success = _endpointCustomizationService.UpdateEndpoint(
                    EndpointWrapper.DeviceInformation,
                    custom);

                if (success)
                {
                    // todo: update the properties again
                }

            });

        }


        private void RefreshVM()
        {
            if (EndpointWrapper != null)
            {
                Name = EndpointWrapper.Name;
                Description = EndpointWrapper.Description;

                this.TransportSuppliedInfo = EndpointWrapper.DeviceInformation.GetTransportSuppliedInfo();
                this.UserSuppliedInfo = EndpointWrapper.DeviceInformation.GetUserSuppliedInfo();
                this.DeviceIdentity = EndpointWrapper.DeviceInformation.GetDeclaredDeviceIdentity();
                this.StreamConfiguration = EndpointWrapper.DeviceInformation.GetDeclaredStreamConfiguration();
                this.EndpointInfo = EndpointWrapper.DeviceInformation.GetDeclaredEndpointInfo();
                this.ParentDeviceInformation = EndpointWrapper.DeviceInformation.GetParentDeviceInformation();

                this.HasParent = this.ParentDeviceInformation != null;

            }

            // Prepopulate customization properties with the ones from the endpoint
            CustomizedName = string.IsNullOrWhiteSpace(UserSuppliedInfo.Name) ? string.Empty : UserSuppliedInfo.Name.Trim();
            CustomizedDescription = string.IsNullOrWhiteSpace(UserSuppliedInfo.Description) ? string.Empty : UserSuppliedInfo.Description.Trim();
            CustomizedImageFileName = string.IsNullOrWhiteSpace(UserSuppliedInfo.ImageFileName) ? string.Empty : UserSuppliedInfo.ImageFileName.Trim();
            CustomizedRequiresNoteOffTranslation = UserSuppliedInfo.RequiresNoteOffTranslation;
            CustomizedSupportsMidiPolyphonicExpression = UserSuppliedInfo.SupportsMidiPolyphonicExpression;
            CustomizedRecommendedControlChangeAutomationIntervalMilliseconds = UserSuppliedInfo.RecommendedControlChangeAutomationIntervalMilliseconds;

            FunctionBlocks.Clear();
            GroupTerminalBlocks.Clear();

            if (EndpointWrapper != null)
            {
                foreach (var fb in EndpointWrapper.DeviceInformation.GetDeclaredFunctionBlocks())
                {
                    FunctionBlocks.Add(fb);
                }

                foreach (var gtb in EndpointWrapper.DeviceInformation.GetGroupTerminalBlocks())
                {
                    GroupTerminalBlocks.Add(gtb);
                }
            }

            this.HasFunctionBlocks = FunctionBlocks.Count > 0;
            this.HasGroupTerminalBlocks = GroupTerminalBlocks.Count > 0;

            ShowMidi2Properties = (this.TransportSuppliedInfo.NativeDataFormat == MidiEndpointNativeDataFormat.UniversalMidiPacketFormat);

        }

        public void OnNavigatedFrom()
        {
           // throw new NotImplementedException();
        }

        public void OnNavigatedTo(object parameter)
        {
            if (parameter is string)
            {
                this.EndpointWrapper = _endpointEnumerationService.GetEndpointById((string)parameter);
            }
            else if (parameter is MidiEndpointWrapper)
            {
                this.EndpointWrapper = (MidiEndpointWrapper)parameter;
            }

            // we're working with observable collections and properties, so need to dispatch
            _synchronizationContextService.GetUIContext().Post(_ =>
            {
                System.Diagnostics.Debug.WriteLine("Enter Dispatcher worker: function blocks and group terminal blocks");

                RefreshVM();

                System.Diagnostics.Debug.WriteLine("Exit Dispatcher worker: function blocks and group terminal blocks");
            }, null);

        }
    }
}
