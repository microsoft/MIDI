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
    public partial class LivePerformanceSettingsViewModel : ObservableRecipient, INavigationAware, ISettingsSearchTarget
    {
        public static IList<string> GetSearchKeywords()
        {
            // TODO: these need to be localized, so should refer to resources instead
            return new[] { "settings", "tweak", "optimization", "performance", "live" };
        }

        public static string GetSearchPageTitle()
        {
            return "Live Performance Settings for Musicians";
        }

        public static string GetSearchPageDescription()
        {
            return "Enables easy access to a number of Windows settings that are common for musicians and performers.";
        }



        private readonly INavigationService _navigationService;
        private readonly IMidiEndpointEnumerationService _enumerationService;
        private readonly IMidiTransportInfoService _transportInfoService;
        private readonly ISynchronizationContextService _synchronizationContextService;



        public LivePerformanceSettingsViewModel(
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


        public void OnNavigatedFrom()
        {
        }


        public void OnNavigatedTo(object parameter)
        {
        }


    }
}
