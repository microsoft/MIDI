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
        private MidiEndpointCustomizationViewModel customizationViewModel;


        [ObservableProperty]
        private bool showGroupTerminalBlocks;

        [ObservableProperty]
        private bool showMidi2Properties;

        [ObservableProperty]
        private bool showCustomizeButton;

        [ObservableProperty]
        private MidiEndpointWrapper endpointWrapper;



        private readonly ISynchronizationContextService _synchronizationContextService;
        private readonly IMidiEndpointCustomizationService _endpointCustomizationService;
        private readonly IMidiEndpointEnumerationService _endpointEnumerationService;
        private readonly IMidiConfigFileService _configFileService;

        public DeviceDetailViewModel(
            ISynchronizationContextService synchronizationContextService, 
            IMidiEndpointCustomizationService endpointCustomizationService,
            IMidiEndpointEnumerationService endpointEnumerationService,
            IMidiConfigFileService configFileService)
        {
            _synchronizationContextService = synchronizationContextService;
            _endpointCustomizationService = endpointCustomizationService;
            _endpointEnumerationService = endpointEnumerationService;
            _configFileService = configFileService;

            System.Diagnostics.Debug.WriteLine("Start clearing properties");


            this.ShowMidi2Properties = false;
            this.ShowGroupTerminalBlocks = false;

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
                // cache this here because the update event may trigger before we save to the config file
                var updateConfig = CustomizationViewModel.GetUpdateConfig();

               // System.Diagnostics.Debug.WriteLine("Config json before updating endpoint.");
               // System.Diagnostics.Debug.WriteLine(updateConfig.GetConfigJson());

                var success = _endpointCustomizationService.UpdateEndpoint(
                    updateConfig);

             //   System.Diagnostics.Debug.WriteLine("Config json after updating endpoint.");
             //   System.Diagnostics.Debug.WriteLine(updateConfig.GetConfigJson());

                if (success)
                {
                    if (_configFileService.CurrentConfig != null)
                    {
                        if (_configFileService.CurrentConfig.StoreEndpointCustomization(updateConfig))
                        {
                            //System.Diagnostics.Debug.WriteLine("Config json after successfully saving to config.");
                            //System.Diagnostics.Debug.WriteLine(updateConfig.GetConfigJson());


                            // success
                            // todo: update the properties again
                        }
                        else
                        {
                            //System.Diagnostics.Debug.WriteLine("Config json after FAILING to save to config.");
                            //System.Diagnostics.Debug.WriteLine(updateConfig.GetConfigJson());

                            // todo: show error
                            System.Diagnostics.Debug.WriteLine("Could not save to config file.");
                        }
                    }
                    else
                    {
                        // could not save. No current config.
                        System.Diagnostics.Debug.WriteLine("Could not save. No current config.");
                    }
                }
                else
                {
                    // todo: show error
                }

            });

        }


        public void RefreshVM()
        {
            System.Diagnostics.Debug.WriteLine("DeviceDetailViewModel.RefreshVM");

            if (EndpointWrapper != null)
            {
                CustomizationViewModel = new MidiEndpointCustomizationViewModel(EndpointWrapper);

                // only show group terminal blocks for native UMP endpoints
                this.ShowGroupTerminalBlocks = EndpointWrapper.IsNativeUmp;
            }

            if (EndpointWrapper != null)
            {
                ShowMidi2Properties = (EndpointWrapper.TransportSuppliedInfo.NativeDataFormat == MidiEndpointNativeDataFormat.UniversalMidiPacketFormat);

                // TODO: This should pull from a property of the transport
                ShowCustomizeButton =
                    EndpointWrapper.TransportSuppliedInfo.TransportCode.ToUpper() == "KS" ||
                    EndpointWrapper.TransportSuppliedInfo.TransportCode.ToUpper() == "KSA";
            }
            else
            {
                ShowMidi2Properties = false;

                ShowCustomizeButton = false;
            }

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
                System.Diagnostics.Debug.WriteLine("Enter Dispatcher worker: OnNavigatedTo");

                RefreshVM();

                System.Diagnostics.Debug.WriteLine("Exit Dispatcher worker: OnNavigatedTo");
            }, null);

        }
    }
}
