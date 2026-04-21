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

namespace Microsoft.Midi.Settings.ViewModels
{
    public partial class EndpointBridgeViewModel : ObservableRecipient, ISettingsSearchTarget, INavigationAware
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


        private readonly IMidiSdkService _sdkService;
        private readonly IMessageBoxService _messageBoxService;
        private readonly IMidiEndpointEnumerationService _endpointEnumerationService;
        private readonly IMidiSessionService _sessionService;
        private readonly ISynchronizationContextService _synchronizationContextService;
        private readonly ILoggingService _loggingService;

        public EndpointBridgeViewModel(
            IMidiSdkService sdkService,
            IMidiSessionService sessionService,
            IMidiEndpointEnumerationService endpointEnumerationService,
            ISynchronizationContextService synchronizationContextService,
            IMessageBoxService messageBoxService,
            ILoggingService loggingService
            )
        {
            _loggingService = loggingService;
            _messageBoxService = messageBoxService;
            _sessionService = sessionService;
            _sdkService = sdkService;
            _endpointEnumerationService = endpointEnumerationService;
            _synchronizationContextService = synchronizationContextService;

        }


        public void OnNavigatedTo(object parameter)
        {

        }

        public void OnNavigatedFrom()
        {

        }




    }
}
