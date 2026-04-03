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
using Microsoft.Windows.Devices.Midi2.Endpoints.BasicLoopback;
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
    public class MidiBasicLoopbackEndpoint
    {
        public string AssociationId;
        public MidiEndpointWrapper Loop { get; internal set; }

        public ICommand RemoveCommand
        {
            get; private set;
        }

        public ICommand MuteCommand
        {
            get; private set;
        }

        public ICommand UnmuteCommand
        {
            get; private set;
        }



        private readonly IMidiConfigFileService _configFileService;

        public MidiBasicLoopbackEndpoint(IMidiConfigFileService configFileService)
        {
            _configFileService = configFileService;

            RemoveCommand = new RelayCommand(
                () =>
                {
                    var associationId = MidiBasicLoopbackEndpointManager.GetAssociationId(Loop!.DeviceInformation);

                    if (associationId != Guid.Empty)
                    {
                        var config = new MidiBasicLoopbackEndpointRemovalConfig(associationId);

                        var response = MidiBasicLoopbackEndpointManager.RemoveTransientLoopbackEndpoint(config);

                        if (_configFileService.CurrentConfig != null)
                        {
                            _configFileService.CurrentConfig.RemoveBasicLoopbackEndpoint(associationId);
                        }
                    }
                    else
                    {
                        // association id was empty, so not found.
                    }

                });

            MuteCommand = new RelayCommand(
                () =>
                {
                    var associationId = MidiBasicLoopbackEndpointManager.GetAssociationId(Loop!.DeviceInformation);

                    if (associationId != Guid.Empty)
                    {
                        var succeeded = MidiBasicLoopbackEndpointManager.MuteLoopback(associationId);

                        if (succeeded)
                        {
                            if (_configFileService.CurrentConfig != null)
                            {
                                // todo: check return value
                                _configFileService.CurrentConfig.StoreBasicLoopbackMutedProperty(associationId, true);
                            }
                        }
                        else
                        {
                        }
                    }
                    else
                    {
                        // association id was empty, so not found.
                        
                        // TODO Show error
                    }

                });


            UnmuteCommand = new RelayCommand(
                () =>
                {
                    var associationId = MidiBasicLoopbackEndpointManager.GetAssociationId(Loop!.DeviceInformation);

                    if (associationId != Guid.Empty)
                    {
                        var succeeded = MidiBasicLoopbackEndpointManager.UnmuteLoopback(associationId);

                        if (succeeded)
                        {
                            if (_configFileService.CurrentConfig != null)
                            {
                                _configFileService.CurrentConfig.StoreBasicLoopbackMutedProperty(associationId, false);
                            }
                        }
                        else
                        {

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


    public partial class EndpointsBasicLoopViewModel : ObservableRecipient, INavigationAware, ISettingsSearchTarget
    {
        public static IList<string> GetSearchKeywords()
        {
            // TODO: these need to be localized, so should refer to resources instead
            return new[] { "endpoints", "ports", "loopback", "app to app", "loopMIDI", "loopBE" };
        }

        public static string GetSearchPageTitle()
        {
            return "Manage Basic MIDI 1.0 Loopback Endpoints";
        }

        public static string GetSearchPageDescription()
        {
            return "Set up basic loopback endpoints for simple MIDI 1.0 communication between different apps.";
        }


        public ICommand ShowCreateLoopbackDialogCommand
        {
            get; private set;
        }

        public ICommand CreateLoopbackCommand
        {
            get; private set;
        }

        public ICommand CreateDefaultLoopbackCommand
        {
            get; private set;
        }

        public bool IsConfigFileActive
        {
            get { return _configFileService.IsConfigFileActive; }
        }


        private string _newLoopbackEndpointName;
        public string NewLoopbackEndpointName
        {
            get { return _newLoopbackEndpointName; }
            set
            {
                _newLoopbackEndpointName = value;
                UpdateValidState();
                OnPropertyChanged();
            }
        }

        private string _newLoopbackEndpointDescription;
        public string NewLoopbackEndpointDescription
        {
            get { return _newLoopbackEndpointDescription; }
            set
            {
                _newLoopbackEndpointDescription = value;
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




        [ObservableProperty]
        private bool newLoopbackIsPersistent;


        [ObservableProperty]
        private bool newLoopbackSettingsAreValid;

        [ObservableProperty]
        private string validationErrorMessage;

        [ObservableProperty]
        private bool defaultLoopbackExists;


        private void UpdateValidState()
        {
            if (string.IsNullOrWhiteSpace(NewLoopbackEndpointName))
            {
                ValidationErrorMessage = "Endpoint name is required.";
                NewLoopbackSettingsAreValid = false;
                return;
            }

            // TODO: validate name is not duplicate of any other endpoint name

            // validate the unique id is good
            if (string.IsNullOrWhiteSpace(NewUniqueIdentifier))
            {
                ValidationErrorMessage = "Unique identifier is required.";
                NewLoopbackSettingsAreValid = false;
                return;
            }


            // validate the unique id isn't already in use with another loopback
            foreach (var ep in MidiBasicLoopbackEndpoints)
            {
                if (ep.Loop.DeviceInformation.GetTransportSuppliedInfo().SerialNumber.ToUpper() == NewUniqueIdentifier.ToUpper())
                {
                    ValidationErrorMessage = "Unique identifier is already in use with another basic loopback endpoint.";
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

        private void CreateNewLoopbackEndpoint()
        {
            var endpoint = new MidiBasicLoopbackEndpointDefinition();

            if (!string.IsNullOrEmpty(NewLoopbackEndpointName))
            {
                // Advanced setup chosen

                endpoint.Name = NewLoopbackEndpointName.Trim();

                if (!string.IsNullOrEmpty(NewLoopbackEndpointDescription))
                {
                    endpoint.Description = NewLoopbackEndpointDescription.Trim();
                }
            }
            else
            {
                return;
            }

            // TODO: Check for duplicates not just for loopbacks, but any other ports in the system
            LoadExistingEndpoints();

            bool duplicateNameFound = false;

            string compareName = endpoint.Name!.ToLower().Trim();

            foreach (var ep in MidiBasicLoopbackEndpoints)
            {
                string a = ep.Loop.Name.ToLower().Trim();

                if (a == compareName)
                {
                    duplicateNameFound = true;
                    break;
                }
            }

            if (duplicateNameFound)
            {
                // TODO: Need to show this and prevent the dialog from closing

                NewLoopbackSettingsAreValid = false;

                _messageBoxService.ShowError("Error_ValidationLoopbackNameMustBeUnique".GetLocalized(), "Error_UnableToCreateLoopbackEndpoint".GetLocalized());
                return;
            }

            endpoint.UniqueId = CleanupUniqueId(NewUniqueIdentifier);

            // descriptions are optional
            var associationId = GuidHelper.CreateNewGuid();

            var creationConfig = new MidiBasicLoopbackEndpointCreationConfig(associationId, endpoint);

            var result = MidiBasicLoopbackEndpointManager.CreateTransientLoopbackEndpoint(creationConfig);

            // TODO: if that worked, and these are persistent, add to configuration file

            if (IsConfigFileActive && NewLoopbackIsPersistent)
            {
                if (result.Success)
                {
                    if (_configFileService.CurrentConfig != null)
                    {
                        _configFileService.CurrentConfig.StoreBasicLoopbackEndpoint(creationConfig);

                        //RefreshDeviceCollection();
                        LoadExistingEndpoints();
                    }
                    else
                    {
                        // no config file
                        _messageBoxService.ShowError("Error_UnableToCreateLoopbackEndpointMissingConfigurationFile".GetLocalized(), "AppDisplayName".GetLocalized());
                    }
                }
                else
                {
                    App.GetService<ILoggingService>().LogError($"Unable to create basic loopback. Error '{result.ErrorInformation}'");

                    // these message boxes are ugly, but it's the fastest way to get this out right now
                    _messageBoxService.ShowError("Error_UnableToCreateLoopbackWithMessage".GetLocalized(), "AppDisplayName".GetLocalized());
                }
            }

            if (result.Success)
            {
                NewLoopbackEndpointName = string.Empty;
                NewLoopbackEndpointDescription = string.Empty;

                GenerateNewUniqueId();
            }    

        }


        public ObservableCollection<MidiBasicLoopbackEndpoint> MidiBasicLoopbackEndpoints { get; } = [];

        
        private void LoadExistingEndpoints()
        {
            App.GetService<ILoggingService>().LogInfo($"Enter");

            _contextService.GetUIContext().Post(_ =>
            {
                lock (MidiBasicLoopbackEndpoints)
                {
                    App.GetService<ILoggingService>().LogInfo($"Acquired lock. Updating endpoints");

                    var endpoints = _enumerationService.GetEndpointsForTransportId(MidiBasicLoopbackEndpointManager.TransportId);

                    // this keeps track of ids for dedup purposes
                    var ids = new Dictionary<string, bool>();

                    MidiBasicLoopbackEndpoints.Clear();

                    foreach (var endpoint in endpoints)
                    {
                        var ep = new MidiBasicLoopbackEndpoint(_configFileService);

                        ep.Loop = endpoint;

                        MidiBasicLoopbackEndpoints.Add(ep);

                    }

                    if (_defaultsService.DoesDefaultBasicLoopbackAlreadyExist())
                    {
                        DefaultLoopbackExists = true;
                    }
                    else
                    {
                        DefaultLoopbackExists = false;
                    }

                    App.GetService<ILoggingService>().LogInfo($"Completed");
                }

            }, null);

        }



        private readonly IMidiConfigFileService _configFileService;
        private readonly IMidiDefaultsService _defaultsService;
        private readonly ISynchronizationContextService _contextService;
        private readonly INavigationService _navigationService;
        private readonly IMidiEndpointEnumerationService _enumerationService;
        private readonly IMessageBoxService _messageBoxService;


        public EndpointsBasicLoopViewModel(
                        INavigationService navigationService, 
                        IMidiConfigFileService configFileService,
                        IMidiEndpointEnumerationService enumerationService,
                        IMidiDefaultsService defaultsService,
                        ISynchronizationContextService contextService,
                        IMessageBoxService messageBoxService

            )
        {
            _configFileService = configFileService;
            _defaultsService = defaultsService;
            _contextService = contextService;
            _navigationService = navigationService;
            _enumerationService = enumerationService;
            _messageBoxService = messageBoxService;

            LoadExistingEndpoints();

            GenerateNewUniqueId();

            if (_configFileService.IsConfigFileActive)
            {
                NewLoopbackIsPersistent = true;
            }

            CreateLoopbackCommand = new RelayCommand(
                () =>
                {
                    if (!NewLoopbackSettingsAreValid)
                    {
                        return;
                    }

                    CreateNewLoopbackEndpoint();

                });


            CreateDefaultLoopbackCommand = new RelayCommand(
                () =>
                {
                    App.GetService<ILoggingService>().LogInfo($"Enter");

                    var creationConfig = _defaultsService.GetDefaultBasicLoopbackCreationConfig();

                    var result = MidiBasicLoopbackEndpointManager.CreateTransientLoopbackEndpoint(creationConfig);

                    // TODO: Display error if needed

                    if (result.Success)
                    {
                        App.GetService<ILoggingService>().LogInfo($"Service reports success");

                        if (_configFileService.CurrentConfig != null)
                        {
                            if (_configFileService.CurrentConfig.StoreBasicLoopbackEndpoint(creationConfig))
                            {
                                App.GetService<ILoggingService>().LogInfo($"Loopback endpoint stored");

                                LoadExistingEndpoints();
                            }
                            else
                            {
                                // TODO: unable to save. Current config is null
                            }

                        }

                    }
                    else
                    {
                        App.GetService<ILoggingService>().LogError("Service reports failure creating default loopback. '{result.ErrorInformation}'");

                        _messageBoxService.ShowError("Error_ServiceReportsFailureCreatingDefaultLoopback".GetLocalized() + $" '{result.ErrorInformation}'", "AppDisplayName".GetLocalized());

                        // TODO: Display to user
                    }
                })
            {

            };
            _enumerationService.EndpointUpdated += (s, e) =>
            {
                System.Diagnostics.Debug.WriteLine($"- Endpoint '{e.Name}' updated.");
                if (e.GetTransportSuppliedInfo().TransportId == MidiBasicLoopbackEndpointManager.TransportId)
                {
                    LoadExistingEndpoints();
                }
            };

            _enumerationService.EndpointAdded += (s, e) =>
            {
                System.Diagnostics.Debug.WriteLine($"- Endpoint '{e.Name}' added.");
                if (e.GetTransportSuppliedInfo().TransportId == MidiBasicLoopbackEndpointManager.TransportId)
                {
                    LoadExistingEndpoints();
                }
            };

            _enumerationService.EndpointRemoved += (s, e) =>
            {
                System.Diagnostics.Debug.WriteLine($"- Endpoint '{e.Name}' removed.");
                if (e.GetTransportSuppliedInfo().TransportId == MidiBasicLoopbackEndpointManager.TransportId)
                {
                    LoadExistingEndpoints();
                }
            };

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
