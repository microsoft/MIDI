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
using Microsoft.UI.Xaml.Controls;
using Microsoft.UI.Xaml.Markup;
using Microsoft.Windows.Devices.Midi2.Endpoints.Loopback;
using System;
using System.Collections.Generic;
using System.Collections.ObjectModel;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using System.Windows.Input;
using Windows.Foundation;

namespace Microsoft.Midi.Settings.ViewModels
{
    public class MidiLoopbackEndpointPair
    {
        public MidiEndpointDeviceInformation LoopA { get; internal set; }
        public MidiEndpointDeviceInformation LoopB { get; internal set; }
    }


    public partial class EndpointsLoopViewModel : SingleTransportEndpointViewModelBase, INavigationAware, ISettingsSearchTarget
    {
        public static IList<string> GetSearchKeywords()
        {
            // TODO: these need to be localized, so should refer to resources instead
            return new[] { "endpoints", "ports", "loopback", "app to app", "default loopback" };
        }

        public static string GetSearchPageTitle()
        {
            return "Manage Loopback Endpoints";
        }

        public static string GetSearchPageDescription()
        {
            return "Set up loopback endpoints for simple bidirectional communication between different apps.";
        }



        private IMidiConfigFileService _midiConfigFileService;

        public ICommand ShowCreateLoopbackPairsDialogCommand
        {
            get; private set;
        }

        public ICommand CreateLoopbackPairsCommand
        {
            get; private set;
        }


        public bool IsConfigFileActive
        {
            get { return _midiConfigFileService.IsConfigFileActive; }
        }


        private string _newLoopbackEndpointAName;
        public string NewLoopbackEndpointAName
        {
            get { return _newLoopbackEndpointAName; }
            set
            {
                _newLoopbackEndpointAName = value.Trim();
                UpdateValidState();
                OnPropertyChanged();
            }
        }

        private string _newLoopbackEndpointBName;
        public string NewLoopbackEndpointBName
        {
            get { return _newLoopbackEndpointBName; }
            set
            {
                _newLoopbackEndpointBName = value.Trim();
                UpdateValidState();
                OnPropertyChanged();
            }
        }

        private string _newUniqueIdentifier;
        public string NewUniqueIdentifier
        {
            get { return _newUniqueIdentifier; }
            set
            {
                _newUniqueIdentifier = value.Trim();

                UpdateValidState();
                OnPropertyChanged();
            }
        }

        [ObservableProperty]
        public bool newLoopbackIsPersistent;


        [ObservableProperty]
        public bool newLoopbackSettingsAreValid;

        [ObservableProperty]
        public string validationErrorMessage;


        private void UpdateValidState()
        {
            // validate both names are not duplicates of each other
            if (string.IsNullOrWhiteSpace(NewLoopbackEndpointAName) || 
                string.IsNullOrWhiteSpace(NewLoopbackEndpointBName))
            {
                ValidationErrorMessage = "Both endpoint names are required.";
                NewLoopbackSettingsAreValid = false;
                return;
            }

            if (NewLoopbackEndpointAName.ToUpper().Trim() == NewLoopbackEndpointBName.ToUpper().Trim())
            {
                ValidationErrorMessage = "Endpoints A and B cannot have the same name.";
                NewLoopbackSettingsAreValid = false;
                return;
            }


            // TODO: validate both names are not duplicates of any other endpoint name

            // validate the unique id is good
            if (string.IsNullOrWhiteSpace(NewUniqueIdentifier))
            {
                ValidationErrorMessage = "Unique identifier is required.";
                NewLoopbackSettingsAreValid = false;
                return;
            }


            // validate the unique id isn't already in use with another loopback
            foreach (var pair in MidiLoopbackEndpointPairs)
            {
                if (pair.LoopA.GetTransportSuppliedInfo().SerialNumber.ToUpper() == NewUniqueIdentifier.ToUpper() ||
                    pair.LoopB.GetTransportSuppliedInfo().SerialNumber.ToUpper() == NewUniqueIdentifier.ToUpper())
                {
                    ValidationErrorMessage = "Unique identifier is already in use with another loopback endpoint.";
                    NewLoopbackSettingsAreValid = false;
                    return;
                }
            }

            ValidationErrorMessage = string.Empty;
            NewLoopbackSettingsAreValid = true;
        }

        private string CleanupUniqueId(string source)
        {
            string result = string.Empty;

            foreach (char ch in source.ToCharArray())
            {
                if (char.IsAsciiLetterOrDigit(ch))
                {
                    result += ch;
                }
            }

            return result;
        }

        private void GenerateNewUniqueId()
        {
            var uniqueId = CleanupUniqueId(GuidHelper.CreateNewGuid().ToString());

            NewUniqueIdentifier = uniqueId;
        }

        private void CreateNewLoopbackEndpoints()
        {
            var endpointA = new MidiLoopbackEndpointDefinition();
            var endpointB = new MidiLoopbackEndpointDefinition();

            endpointA.Name = NewLoopbackEndpointAName.Trim();
            endpointB.Name = NewLoopbackEndpointBName.Trim();

            endpointA.UniqueId = CleanupUniqueId(NewUniqueIdentifier);
            endpointB.UniqueId = CleanupUniqueId(NewUniqueIdentifier);

            // descriptions are optional
            endpointA.Description = string.Empty;
            endpointB.Description = string.Empty;

            var associationId = GuidHelper.CreateNewGuid();

            var creationConfig = new MidiLoopbackEndpointCreationConfig(associationId, endpointA, endpointB);

            var result = MidiLoopbackEndpointManager.CreateTransientLoopbackEndpoints(creationConfig);

            // TODO: if that worked, and these are persistent, add to configuration file

            if (IsConfigFileActive && NewLoopbackIsPersistent)
            {
                if (result.Success)
                {
                    if (_midiConfigFileService.CurrentConfig != null)
                    {
                        _midiConfigFileService.CurrentConfig.StoreLoopbackEndpointPair(creationConfig);

                        RefreshDeviceCollection();
                    }
                    else
                    {
                        // no config file
                    }
                }
                else
                {
                    // update error information
                }
            }
        }


        public ObservableCollection<MidiLoopbackEndpointPair> MidiLoopbackEndpointPairs { get; } = [];
        private void LoadExistingEndpointPairs()
        {
            // this keeps track of ids for dedup purposes
            var ids = new Dictionary<string, bool>();

            var endpoints = _enumerationService.GetEndpoints()
                .Where(x => x.DeviceInformation.GetTransportSuppliedInfo().TransportId == MidiLoopbackEndpointManager.TransportId);


            foreach (var endpoint in endpoints)
            {
                var associated = MidiLoopbackEndpointManager.GetAssociatedLoopbackEndpoint(endpoint.DeviceInformation);

                if (associated != null)
                {
                    if (!ids.ContainsKey(endpoint.Id) &&
                        !ids.ContainsKey(associated.EndpointDeviceId))
                    {
                        var pair = new MidiLoopbackEndpointPair();

                        pair.LoopA = endpoint.DeviceInformation;
                        pair.LoopB = associated;

                        MidiLoopbackEndpointPairs.Add(pair);

                        ids.Add(pair.LoopA.EndpointDeviceId, true);
                        ids.Add(pair.LoopB.EndpointDeviceId, true);
                    }
                }
            }
        }




        public EndpointsLoopViewModel(
            INavigationService navigationService, 
            IMidiConfigFileService midiConfigFileService,
            IMidiEndpointEnumerationService enumerationService
            ) : base("LOOP", navigationService, enumerationService)
        {
            LoadExistingEndpointPairs();

            _midiConfigFileService = midiConfigFileService;

            GenerateNewUniqueId();

            if (_midiConfigFileService.IsConfigFileActive)
            {
                NewLoopbackIsPersistent = true;
            }

            CreateLoopbackPairsCommand = new RelayCommand(
                () =>
                {
                    if (!NewLoopbackSettingsAreValid)
                    {
                        return;
                    }

                    CreateNewLoopbackEndpoints();

                });

            //ShowCreateLoopbackPairsDialogCommand = new RelayCommand(
            //    async () =>
            //    {
            //        //var dialog = new Dialog_CreateLoopbackEndpoints();

            //        var dialog = new ContentDialog();
            //        dialog.Content = XamlReader.Load()

            //        await dialog.ShowAsync();
            //    });
        }



        public void OnNavigatedFrom()
        {
        }

        public event EventHandler ShowCreateDialog;

        public void OnNavigatedTo(object parameter)
        {
            if (parameter != null && ((string)parameter).ToLower() == "create")
            {
                ShowCreateDialog?.Invoke(this, new EventArgs());
            }
        }

    }
}
