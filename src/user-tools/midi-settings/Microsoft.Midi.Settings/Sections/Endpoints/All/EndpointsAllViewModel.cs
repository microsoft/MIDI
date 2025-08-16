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
using Microsoft.UI.Xaml.Media;
using Microsoft.UI.Xaml.Media.Imaging;
using Microsoft.Windows.Devices.Midi2.Utilities.Metadata;
using System;
using System.Collections.Generic;
using System.Collections.ObjectModel;
using System.Linq;
using System.Net;
using System.Text;
using System.Threading.Tasks;
using System.Windows.Input;

namespace Microsoft.Midi.Settings.ViewModels
{
    public partial class EndpointsAllViewModel : ObservableRecipient, INavigationAware, ISettingsSearchTarget
    {
        public static IList<string> GetSearchKeywords()
        {
            // TODO: these need to be localized, so should refer to resources instead
            return new[] { "endpoints", "ports", "monitor", "panic" };
        }

        public static string GetSearchPageTitle()
        {
            return "All Endpoints";
        }


        private readonly INavigationService _navigationService;
        private readonly IMidiEndpointEnumerationService _enumerationService;
        private readonly IMidiTransportInfoService _transportInfoService;
        private readonly ISynchronizationContextService _synchronizationContextService;

        public EndpointsAllViewModel(
            INavigationService navigationService,
            IMidiEndpointEnumerationService enumerationService,
            IMidiTransportInfoService transportInfoService,
            ISynchronizationContextService synchronizationContextService
            )
        {
            _navigationService = navigationService;
            _enumerationService = enumerationService;
            _transportInfoService = transportInfoService;
            _synchronizationContextService = synchronizationContextService;

        }

        public ObservableCollection<MidiEndpointWrapper> Endpoints { get; } = [];

        [ObservableProperty]
        private string filterString = string.Empty;

        // todo: this should have a sort order and filter
        public void RefreshMidiEndpointDevices()
        {
            _synchronizationContextService.GetUIContext()?.Post(_ =>
            {
                var allEndpoints = _enumerationService.GetEndpoints().OrderBy(e => e.Name);


                foreach (var endpoint in allEndpoints)
                {
                    if (FilterString.Trim() == string.Empty)
                    {
                        Endpoints.Add(endpoint);
                    }
                    else
                    {
                        // TODO: process with filter

                        if (endpoint.Name.StartsWith(FilterString))
                        {
                            Endpoints.Add(endpoint);
                        }
                    }

                }
            }, null);
        }


        public void OnNavigatedFrom()
        {
        }


        public void OnNavigatedTo(object parameter)
        {
            RefreshMidiEndpointDevices();
        }


    }
}
