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
using System.Runtime.InteropServices;
using System.Text;
using System.Threading.Tasks;
using System.Windows.Input;
using Windows.Devices.Enumeration;
using Windows.Foundation;

namespace Microsoft.Midi.Settings.ViewModels
{
    public class MidiLoopbackEndpointPair
    {
        public string AssociationId;
        public MidiEndpointWrapper LoopA { get; internal set; }
        public MidiEndpointDeviceInformation LoopB { get; internal set; }


        public ICommand RemoveCommand
        {
            get; private set;
        }

        private readonly IMidiConfigFileService _configFileService;

        public MidiLoopbackEndpointPair(IMidiConfigFileService configFileService)
        {
            _configFileService = configFileService;

            RemoveCommand = new RelayCommand(
                () =>
                {
                    var associationId = MidiLoopbackEndpointManager.GetAssociationId(LoopA!.DeviceInformation);

                    if (associationId != Guid.Empty)
                    {
                        var config = new MidiLoopbackEndpointRemovalConfig(associationId);

                        var response = MidiLoopbackEndpointManager.RemoveTransientLoopbackEndpoints(config);

                        // todo: remove from config file

                        if (_configFileService.CurrentConfig != null)
                        {
                            _configFileService.CurrentConfig.RemoveLoopbackEndpointPair(associationId);
                        }
                    }
                    else
                    {
                        // association id was empty, so not found.
                    }

                });
            _configFileService = configFileService;
        }


    }


    public partial class EndpointsLoopViewModel : ObservableRecipient, INavigationAware, ISettingsSearchTarget
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


        public ICommand ShowCreateLoopbackPairsDialogCommand
        {
            get; private set;
        }

        public ICommand CreateLoopbackPairsCommand
        {
            get; private set;
        }

        public ICommand CreateDefaultLoopbackPairsCommand
        {
            get; private set;
        }

        public bool IsConfigFileActive
        {
            get { return _configFileService.IsConfigFileActive; }
        }


        private string _newLoopbackEndpointBaseName;
        public string NewLoopbackEndpointBaseName
        {
            get { return _newLoopbackEndpointBaseName; }
            set
            {
                _newLoopbackEndpointBaseName = value;
                UpdateValidState();
                OnPropertyChanged();
            }
        }

        private string _newLoopbackDescription;
        public string NewLoopbackDescription
        {
            get { return _newLoopbackDescription; }
            set
            {
                _newLoopbackDescription = value;
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
                _newUniqueIdentifier = value;

                UpdateValidState();
                OnPropertyChanged();
            }
        }



        private string _newLoopbackEndpointAName;
        public string NewLoopbackEndpointAName
        {
            get { return _newLoopbackEndpointAName; }
            set
            {
                _newLoopbackEndpointAName = value;
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
                _newLoopbackEndpointBName = value;
                UpdateValidState();
                OnPropertyChanged();
            }
        }

        private string _newLoopbackEndpointADescription;
        public string NewLoopbackEndpointADescription
        {
            get { return _newLoopbackEndpointADescription; }
            set
            {
                _newLoopbackEndpointADescription = value;
                UpdateValidState();
                OnPropertyChanged();
            }
        }

        private string _newLoopbackEndpointBDescription;
        public string NewLoopbackEndpointBDescription
        {
            get { return _newLoopbackEndpointBDescription; }
            set
            {
                _newLoopbackEndpointBDescription = value;
                UpdateValidState();
                OnPropertyChanged();
            }
        }




        [ObservableProperty]
        private bool newLoopbackIsPersistent;


        [ObservableProperty]
        private bool newLoopbackSettingsAreValid;

        [ObservableProperty]
        private string validationErrorMessage;

        [ObservableProperty]
        private bool defaultLoopbackPairExists;


        private void UpdateValidState()
        {
            if (!string.IsNullOrEmpty(NewLoopbackEndpointAName) && !string.IsNullOrEmpty(NewLoopbackEndpointBName))
            {
                // we're using the direct names


            }
            else
            {
                if (string.IsNullOrWhiteSpace(NewLoopbackEndpointBaseName))
                {
                    ValidationErrorMessage = "Both endpoint names are required. Please provide a base name, or specify individual names";
                    NewLoopbackSettingsAreValid = false;
                    return;
                }
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
                if (pair.LoopA.DeviceInformation.GetTransportSuppliedInfo().SerialNumber.ToUpper() == NewUniqueIdentifier.ToUpper() ||
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



        [DllImport("User32.dll", SetLastError = true, CharSet = CharSet.Auto)]
        static extern int MessageBox(
            IntPtr hWnd,
            string lpText,
            string lpCaption,
            int uType);


        private void CreateNewLoopbackEndpoints()
        {
            var endpointA = new MidiLoopbackEndpointDefinition();
            var endpointB = new MidiLoopbackEndpointDefinition();


            if (!string.IsNullOrEmpty(NewLoopbackEndpointAName) && !string.IsNullOrEmpty(NewLoopbackEndpointBName))
            {
                // Advanced setup chosen

                endpointA.Name = NewLoopbackEndpointAName.Trim();
                endpointB.Name = NewLoopbackEndpointBName.Trim();

                if (!string.IsNullOrEmpty(NewLoopbackEndpointADescription))
                {
                    endpointA.Description = NewLoopbackEndpointADescription.Trim();
                }

                if (!string.IsNullOrEmpty(NewLoopbackEndpointBDescription))
                {
                    endpointB.Description = NewLoopbackEndpointBDescription.Trim();
                }
            }
            else
            {
                // quick setup chosen

                endpointA.Name = NewLoopbackEndpointBaseName.Trim() + " (A)";
                endpointB.Name = NewLoopbackEndpointBaseName.Trim() + " (B)";

                if (!string.IsNullOrEmpty(NewLoopbackDescription))
                {
                    endpointA.Description = NewLoopbackDescription.Trim();
                    endpointB.Description = NewLoopbackDescription.Trim();
                }
            }


            // TODO: Check for duplicates not just for loopbacks, but any other ports in the system
            LoadExistingEndpointPairs();

            bool duplicateNameFound = false;

            string compareAName = endpointA.Name.ToLower().Trim();
            string compareBName = endpointB.Name.ToLower().Trim();

            foreach (var pair in MidiLoopbackEndpointPairs)
            {
                string a = pair.LoopA.Name.ToLower().Trim();
                string b = pair.LoopB.Name.ToLower().Trim();

                if (a == compareAName || a == compareBName)
                {
                    duplicateNameFound = true;
                    break;
                }
                else if (b == compareAName || b == compareBName)
                {
                    duplicateNameFound = true;
                    break;
                }

            }

            if (duplicateNameFound)
            {
                // TODO: Need to show this and prevent the dialog from closing

                ValidationErrorMessage = "The loopback endpoint names must be unique across all MIDI endpoint names.";
                NewLoopbackSettingsAreValid = false;

                // these message boxes are ugly, but it's the fastest way to get this out right now
                MessageBox(
                    (IntPtr)0,
                    ValidationErrorMessage,
                    "Unable to create loopback endpoints",
                    0);

                return;
            }



            endpointA.UniqueId = CleanupUniqueId(NewUniqueIdentifier);
            endpointB.UniqueId = CleanupUniqueId(NewUniqueIdentifier);

            // descriptions are optional
            var associationId = GuidHelper.CreateNewGuid();

            var creationConfig = new MidiLoopbackEndpointCreationConfig(associationId, endpointA, endpointB);

            var result = MidiLoopbackEndpointManager.CreateTransientLoopbackEndpoints(creationConfig);

            // TODO: if that worked, and these are persistent, add to configuration file

            if (IsConfigFileActive && NewLoopbackIsPersistent)
            {
                if (result.Success)
                {
                    if (_configFileService.CurrentConfig != null)
                    {
                        _configFileService.CurrentConfig.StoreLoopbackEndpointPair(creationConfig);

                        //RefreshDeviceCollection();
                        LoadExistingEndpointPairs();
                    }
                    else
                    {
                        // no config file

                        // these message boxes are ugly, but it's the fastest way to get this out right now
                        MessageBox(
                            (IntPtr)0,
                            "Missing MIDI configuration file",
                            "Unable to create loopback endpoints",
                            0);

                    }
                }
                else
                {
                    // these message boxes are ugly, but it's the fastest way to get this out right now
                    MessageBox(
                        (IntPtr)0,
                        "Message from MIDI service: " + result.ErrorInformation,
                        "Unable to create loopback endpoints",
                        0);
                }
            }

            if (result.Success)
            {
                NewLoopbackEndpointAName = string.Empty;
                NewLoopbackEndpointBName = string.Empty;

                NewLoopbackEndpointADescription = string.Empty;
                NewLoopbackEndpointBDescription = string.Empty;

                NewLoopbackEndpointBaseName = string.Empty;
                NewLoopbackDescription = string.Empty;

                GenerateNewUniqueId();
            }    

        }


        public ObservableCollection<MidiLoopbackEndpointPair> MidiLoopbackEndpointPairs { get; } = [];

        
        private void LoadExistingEndpointPairs()
        {
            System.Diagnostics.Debug.WriteLine($"- LoadExistingEndpointPairs().");

            _contextService.GetUIContext().Post(_ =>
            {
                lock (MidiLoopbackEndpointPairs)
                {
                    System.Diagnostics.Debug.WriteLine($"- LoadExistingEndpointPairs() (UI) : Acquired lock. Updating endpoints.");

                    var endpoints = _enumerationService.GetEndpointsForTransportId(MidiLoopbackEndpointManager.TransportId);

                    // this keeps track of ids for dedup purposes
                    var ids = new Dictionary<string, bool>();

                    MidiLoopbackEndpointPairs.Clear();

                    foreach (var endpoint in endpoints)
                    {
                        var associated = MidiLoopbackEndpointManager.GetAssociatedLoopbackEndpoint(endpoint.DeviceInformation);

                        if (associated != null)
                        {
                            if (!ids.ContainsKey(endpoint.DeviceInformation.EndpointDeviceId) &&
                                !ids.ContainsKey(associated.EndpointDeviceId))
                            {
                                var pair = new MidiLoopbackEndpointPair(_configFileService);

                                pair.LoopA = endpoint;
                                pair.LoopB = associated;

                                MidiLoopbackEndpointPairs.Add(pair);

                                ids.Add(pair.LoopA.DeviceInformation.EndpointDeviceId, true);
                                ids.Add(pair.LoopB.EndpointDeviceId, true);
                            }
                        }
                    }

                    if (_defaultsService.DoesDefaultLoopbackAlreadyExist())
                    {
                        DefaultLoopbackPairExists = true;
                    }
                    else
                    {
                        DefaultLoopbackPairExists = false;
                    }

                    System.Diagnostics.Debug.WriteLine($"- LoadExistingEndpointPairs() (UI) : Completed.");
                }

            }, null);

        }



        private readonly IMidiConfigFileService _configFileService;
        private readonly IMidiDefaultsService _defaultsService;
        private readonly ISynchronizationContextService _contextService;
        private readonly INavigationService _navigationService;
        private readonly IMidiEndpointEnumerationService _enumerationService;

        public EndpointsLoopViewModel(
                        INavigationService navigationService, 
                        IMidiConfigFileService configFileService,
                        IMidiEndpointEnumerationService enumerationService,
                        IMidiDefaultsService defaultsService,
                        ISynchronizationContextService contextService

            )
        {
            _configFileService = configFileService;
            _defaultsService = defaultsService;
            _contextService = contextService;
            _navigationService = navigationService;
            _enumerationService = enumerationService;

            LoadExistingEndpointPairs();

            GenerateNewUniqueId();

            if (_configFileService.IsConfigFileActive)
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


            CreateDefaultLoopbackPairsCommand = new RelayCommand(
                () =>
                {
                    var creationConfig = _defaultsService.GetDefaultLoopbackCreationConfig();

                    var result = MidiLoopbackEndpointManager.CreateTransientLoopbackEndpoints(creationConfig);

                    // TODO: Display error if needed

                    if (result.Success)
                    {
                        if (_configFileService.CurrentConfig != null)
                        {
                            _configFileService.CurrentConfig.StoreLoopbackEndpointPair(creationConfig);
                        }
                        else
                        {
                            // unable to save. Current config is null
                        }

                        //LoadExistingEndpointPairs();
                    }
                    

                });

            _enumerationService.EndpointUpdated += (s, e) =>
            {
                System.Diagnostics.Debug.WriteLine($"- Endpoint '{e.Name}' updated.");
                if (e.GetTransportSuppliedInfo().TransportId == MidiLoopbackEndpointManager.TransportId)
                {
                    LoadExistingEndpointPairs();
                }
            };

            _enumerationService.EndpointAdded += (s, e) =>
            {
                System.Diagnostics.Debug.WriteLine($"- Endpoint '{e.Name}' added.");
                if (e.GetTransportSuppliedInfo().TransportId == MidiLoopbackEndpointManager.TransportId)
                {
                    LoadExistingEndpointPairs();
                }
            };

            _enumerationService.EndpointRemoved += (s, e) =>
            {
                System.Diagnostics.Debug.WriteLine($"- Endpoint '{e.Name}' removed.");
                if (e.GetTransportSuppliedInfo().TransportId == MidiLoopbackEndpointManager.TransportId)
                {
                    LoadExistingEndpointPairs();
                }
            };

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
