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
using System.ComponentModel;
using System.Linq;
using System.Net;
using System.Text;
using System.Threading.Tasks;
using System.Windows.Input;

namespace Microsoft.Midi.Settings.ViewModels
{
    public struct TransportFilterEntry
    {
        public string TransportCode { get; set; }
        public string Name { get; set; }
    }

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

        public static string GetSearchPageDescription()
        {
            return "List of all active MIDI endpoints on the system. Includes the ability to see details, customize the endpoint, and send MIDI panic.";
        }



        private readonly INavigationService _navigationService;
        private readonly IMidiEndpointEnumerationService _enumerationService;
        private readonly IMidiTransportInfoService _transportInfoService;
        private readonly ISynchronizationContextService _synchronizationContextService;
        private readonly IGeneralSettingsService _generalSettingsService;


        public ObservableCollection<TransportFilterEntry> Transports = [];


        [ObservableProperty]
        private bool useListView = false;

        [ObservableProperty]
        private bool useCardView = true;



        [ObservableProperty]
        private TransportFilterEntry selectedTransport;

        protected override void OnPropertyChanged(PropertyChangedEventArgs e)
        {
            base.OnPropertyChanged(e);

            if (e.PropertyName == nameof(SelectedTransport))
            {
                // apply filter
                RefreshMidiEndpointDevices();
            }
            else if (e.PropertyName == nameof(UseListView))
            {
                if (UseListView)
                {
                    UseCardView = false;
                    _generalSettingsService.SetEndpointListLastUsedView(EndpointListView.ListView);
                }
            }
            else if (e.PropertyName == nameof(UseCardView))
            {
                if (UseCardView)
                {
                    UseListView = false;
                    _generalSettingsService.SetEndpointListLastUsedView(EndpointListView.CardView);
                }
            }


        }


        private const string AllTransportsFilterCode = "All";

        public EndpointsAllViewModel(
            INavigationService navigationService,
            IMidiEndpointEnumerationService enumerationService,
            IMidiTransportInfoService transportInfoService,
            ISynchronizationContextService synchronizationContextService,
            IGeneralSettingsService generalSettingsService
            )
        {
            _navigationService = navigationService;
            _enumerationService = enumerationService;
            _transportInfoService = transportInfoService;
            _synchronizationContextService = synchronizationContextService;
            _generalSettingsService = generalSettingsService;

            // Read the view preference from settings file and enable card or list view based on that

            if (_generalSettingsService.GetEndpointListLastUsedView() == EndpointListView.ListView)
            {
                UseListView = true;
                UseCardView = false;
            }
            else
            {
                UseCardView = true;
                UseListView = false;
            }


            var all = new TransportFilterEntry();
            all.TransportCode = AllTransportsFilterCode;
            all.Name = "All endpoints for all transports";

            Transports.Add(all);

            foreach (var transport in _transportInfoService.GetAllTransports().OrderBy(t=>t.Name))
            {
                var entry = new TransportFilterEntry();
                entry.Name = transport.Name;
                entry.TransportCode = transport.TransportCode;

                Transports.Add(entry);
            }

            SelectedTransport = all;

        }

        public ObservableCollection<MidiEndpointWrapper> Endpoints { get; } = [];

        [ObservableProperty]
        private string filterString = string.Empty;

        // todo: this should have a sort order and filter
        public void RefreshMidiEndpointDevices()
        {
            _synchronizationContextService.GetUIContext()?.Post(_ =>
            {
                IList<MidiEndpointWrapper> results;

                if (SelectedTransport.TransportCode == AllTransportsFilterCode)
                {
                    results = _enumerationService.GetEndpoints();
                }
                else
                {
                    results = _enumerationService.GetEndpointsForTransportCode(SelectedTransport.TransportCode);
                }

                Endpoints.Clear();

                foreach (var endpoint in results.OrderBy(e=>e.Name))
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
