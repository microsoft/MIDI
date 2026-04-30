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
using Microsoft.Midi.Settings.Controls;
using Newtonsoft.Json.Linq;
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
    public partial class BluetoothBridgeViewModel : ObservableRecipient, ISettingsSearchTarget, INavigationAware
    {
        public static IList<string> GetSearchKeywords()
        {
            // TODO: these need to be localized, so should refer to resources instead
            return new[] { "sysex", "system exclusive", "scratch pad" };
        }

        public static string GetSearchPageTitle()
        {
            return "MIDI 1.0 Scratch Pad";
        }

        public static string GetSearchPageDescription()
        {
            return "Send arbitrary MIDI 1.0 data to an endpoint.";
        }


        public ICommand CreateNewBridgeCommand
        {
            get;
        }


        [ObservableProperty]
        private bool isConfigFileActive = false;

        [ObservableProperty]
        private bool newBridgeSettingsAreValid = false;

        [ObservableProperty]
        private bool bleMidi1PortsFound = false;


        public ObservableCollection<DeviceInformation> SourcePorts { get; private set; } = [];
        public ObservableCollection<DeviceInformation> DestinationPorts { get; private set; } = [];

        [ObservableProperty]
        private DeviceInformation selectedSourcePort;

        [ObservableProperty]
        private DeviceInformation selectedDestinationPort;



        private bool CreateNewBridge()
        {
            // TODO


            return false;
        }

        private void RefreshMidi1Ports()
        {
            SourcePorts.Clear();
            var sources = _midiWinRTMidi1PortEnumerationService.GetBleSourcePorts();
            foreach (var source in sources)
            {
                SourcePorts.Add(source);
            }


            DestinationPorts.Clear();
            var destinations = _midiWinRTMidi1PortEnumerationService.GetBleDestinationPorts();
            foreach (var destination in destinations)
            {
                DestinationPorts.Add(destination);
            }

            BleMidi1PortsFound = SourcePorts.Count > 0 || DestinationPorts.Count > 0;

            if (SourcePorts.Count == 1)
            {
                SelectedSourcePort = SourcePorts[0];
            }

            if (DestinationPorts.Count == 1)
            {
                SelectedDestinationPort = DestinationPorts[0];
            }

        }


        private readonly IMidiSdkService _sdkService;
        private readonly IMessageBoxService _messageBoxService;
        private readonly IMidiEndpointEnumerationService _endpointEnumerationService;
        private readonly IMidiSessionService _sessionService;
        private readonly IMidiWinRTMidi1PortEnumerationService _midiWinRTMidi1PortEnumerationService;
        private readonly ISynchronizationContextService _synchronizationContextService;
        private readonly ILoggingService _loggingService;
        private readonly IMidiConfigFileService _configFileService;

        public BluetoothBridgeViewModel(
            IMidiSdkService sdkService,
            IMidiSessionService sessionService,
            IMidiEndpointEnumerationService endpointEnumerationService,
            ISynchronizationContextService synchronizationContextService,
            IMessageBoxService messageBoxService,
            IMidiConfigFileService configFileService,
            IMidiWinRTMidi1PortEnumerationService midiWinRTMidi1PortEnumerationService,
            ILoggingService loggingService
            )
        {
            _loggingService = loggingService;
            _messageBoxService = messageBoxService;
            _sessionService = sessionService;
            _sdkService = sdkService;
            _endpointEnumerationService = endpointEnumerationService;
            _synchronizationContextService = synchronizationContextService;
            _configFileService = configFileService;
            _midiWinRTMidi1PortEnumerationService = midiWinRTMidi1PortEnumerationService;


            IsConfigFileActive = _configFileService.IsConfigFileActive;

            CreateNewBridgeCommand = new RelayCommand(() =>
            {
                if (CreateNewBridge())
                {
                    // all good
                }
                else
                {
                    // TODO: Show error dialog
                }
            });

        }


        public void OnNavigatedTo(object parameter)
        {
            RefreshMidi1Ports();


        }

        public void OnNavigatedFrom()
        {

        }




    }
}
