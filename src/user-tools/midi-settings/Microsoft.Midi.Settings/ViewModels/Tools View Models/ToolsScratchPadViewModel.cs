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
using System;
using System.Collections.Generic;
using System.Collections.ObjectModel;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using System.Windows.Input;

namespace Microsoft.Midi.Settings.ViewModels
{
    public partial class ToolsScratchPadViewModel : ObservableRecipient, ISettingsSearchTarget, INavigationAware
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


        [ObservableProperty]
        MidiEndpointWrapper? selectedEndpoint;

        [ObservableProperty]
        MidiGroupForDisplay? selectedGroup;

        public ObservableCollection<MidiEndpointWrapper> MidiEndpoints { get; } = [];

        [ObservableProperty]
        string allScratchPadText;


        public ICommand AddSysExIdentityRequest
        {
            get;
        }

        public ICommand AddBlankSysExBlock
        {
            get;
        }

        private void AddNewLineIfNeeded()
        {
            if (!string.IsNullOrWhiteSpace(AllScratchPadText) && !AllScratchPadText.EndsWith('\n'))
            {
                AllScratchPadText += "\n";
            }
        }

        private readonly IMidiSdkService _sdkService;
        private readonly IMidiEndpointEnumerationService _endpointEnumerationService;
        private readonly IMidiSessionService _sessionService;
        private readonly ISynchronizationContextService _synchronizationContextService;

        public ToolsScratchPadViewModel(
            IMidiSdkService sdkService,
            IMidiSessionService sessionService,
            IMidiEndpointEnumerationService endpointEnumerationService,
            ISynchronizationContextService synchronizationContextService
            )
        {
            _sessionService = sessionService;
            _sdkService = sdkService;
            _endpointEnumerationService = endpointEnumerationService;
            _synchronizationContextService = synchronizationContextService;


            allScratchPadText = string.Empty;

            AddSysExIdentityRequest = new RelayCommand(
            () =>
            {
                AddNewLineIfNeeded();
                AllScratchPadText += "F0 7E 7F 06 01 F7\n";
            });

            AddBlankSysExBlock = new RelayCommand(
            () =>
            {
                AddNewLineIfNeeded();
                AllScratchPadText += "F0 00 00 00 00 00 00 F7\n";
            });


        }


        public void RefreshDeviceCollection()
        {
            _synchronizationContextService.GetUIContext().Post(_ =>
            {
                MidiEndpoints.Clear();

                // now get all the endpoint devices and put them in groups by transport

                var endpoints = _endpointEnumerationService.GetEndpointsForPurpose(MidiEndpointDevicePurpose.NormalMessageEndpoint).OrderBy(x => x.Name);

                foreach (var endpoint in endpoints)
                {
                    MidiEndpoints.Add(endpoint);
                }

            }, null);

        }

        public void OnNavigatedTo(object parameter)
        {
            RefreshDeviceCollection();
        }

        public void OnNavigatedFrom()
        {

        }




    }
}
