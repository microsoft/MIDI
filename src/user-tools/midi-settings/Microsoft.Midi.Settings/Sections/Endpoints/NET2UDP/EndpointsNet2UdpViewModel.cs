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
using Microsoft.Midi.Settings.Services;
using Microsoft.UI.Dispatching;
using Microsoft.Windows.Devices.Midi2.Endpoints.Network;
using System;
using System.Collections.Generic;
using System.Collections.ObjectModel;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using System.Windows.Input;

namespace Microsoft.Midi.Settings.ViewModels
{
    public partial class EndpointsNet2UdpViewModel : SingleTransportEndpointViewModelBase
    {
        private IMidiConfigFileService m_midiConfigFileService;

        public bool IsConfigFileActive
        {
            get { return m_midiConfigFileService.IsConfigFileActive; }
        }


        public ICommand CreateNetworkHostCommand
        {
            get; private set;
        }

        [ObservableProperty]
        public string? newHostName;

        [ObservableProperty]
        public string? newHostUniqueId;

        [ObservableProperty]
        public string? newHostPort;

        [ObservableProperty]
        public string? newHostServiceId;

        [ObservableProperty]
        public bool newHostAdvertise = true;

        [ObservableProperty]
        public bool newHostUmpOnly = false;

        [ObservableProperty]
        public bool newHostIsPersistent = true;

        [ObservableProperty]
        public bool newNetworkHostSettingsAreValid = false;

        [ObservableProperty]
        public string validationErrorMessage;

        private void CreateNewNetworkHost()
        {
            var config = new MidiNetworkHostCreationConfig();

            config.Advertise = NewHostAdvertise;
            config.AuthenticationType = MidiNetworkAuthenticationType.NoAuthentication; // TEMP: set this up with actual configured auth
            config.ProductInstanceId = NewHostUniqueId;
            //config.HostInstanceName = 
            config.Name = NewHostName;
            config.Id = new Guid().ToString();
            config.UmpOnly = NewHostUmpOnly;
            config.UseAutomaticPortAllocation = string.IsNullOrEmpty(NewHostPort) || NewHostPort.Trim() == string.Empty;


            var result = MidiNetworkEndpointManager.CreateNetworkHost(config);

            if (result.Success)
            {
                // TODO: Tell the UI to display success
            }
            else
            {
                // TODO: Tell the UI to display failure
            }

        }

        public EndpointsNet2UdpViewModel(INavigationService navigationService, IMidiConfigFileService midiConfigFileService) : base("NET2UDP", navigationService)
        {
            m_midiConfigFileService = midiConfigFileService;

            CreateNetworkHostCommand = new RelayCommand(
                () =>
                {
                    if (!NewNetworkHostSettingsAreValid)
                    {
                        return;
                    }

                    CreateNewNetworkHost();

                });
        }

    }
}
