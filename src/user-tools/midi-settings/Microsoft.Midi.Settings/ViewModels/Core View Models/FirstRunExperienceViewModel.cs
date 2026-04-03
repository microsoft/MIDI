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
using Microsoft.Midi.Settings.Services;
using Microsoft.UI.Xaml;
using Microsoft.UI.Xaml.Controls;
using Microsoft.Windows.AppLifecycle;
using Microsoft.Windows.Devices.Midi2.Endpoints.BasicLoopback;
using Microsoft.Windows.Devices.Midi2.Endpoints.Loopback;
using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.Linq;
using System.Runtime.InteropServices;
using System.Text;
using System.Threading.Tasks;
using System.Windows.Input;
using Windows.ApplicationModel.Core;

namespace Microsoft.Midi.Settings.ViewModels
{
    public partial class FirstRunExperienceViewModel : ObservableRecipient, ISettingsSearchTarget
    {
        public static IList<string> GetSearchKeywords()
        {
            // TODO: these need to be localized, so should refer to resources instead
            return new[] { "setup", "config file", "first run", "default loopback", "naming" };
        }

        public static string GetSearchPageTitle()
        {
            return "First-run Setup";
        }

        public static string GetSearchPageDescription()
        {
            return "Set up a configuration file and other common MIDI settings.";
        }


        private IMidiConfigFileService _configFileService;
        private IMidiDefaultsService _defaultsService;
        private INavigationService _navigationService;
        private IMidiServiceRegistrySettingsService _registryService;
        private IMessageBoxService _messageBoxService;

        public string ConfigFileName
        {
            get; private set;
        }

        public string ConfigFilesLocation
        {
            get
            {
                return _configFileService.GetConfigFilesLocation();
            }
        }

        public bool ConfigFileExists
        {
            get
            {
                return _configFileService.ConfigFileExists(ConfigFileName);
            }
        }

        
        public ICommand CompleteFirstRunSetupCommand
        {
            get;
        }

        [ObservableProperty]
        public bool useNewStyleWinMMPortNames;

        [ObservableProperty]
        public bool createConfigurationFile;

        [ObservableProperty]
        public bool createDefaultLoopbackEndpoints;

        [ObservableProperty]
        public bool setServiceToAutoStart;

        [ObservableProperty]
        public string errorMessage;

        public bool CanPersistChanges
        {
            get
            {
                return CreateConfigurationFile || _configFileService.IsConfigFileActive;
            }
        }

        private void CompleteFirstRunSetup()
        {
            bool needServiceRestart = false;
            bool serviceRestarted = false;

            ErrorMessage = string.Empty;

            if (CreateConfigurationFile)
            {
                string newConfigName = _defaultsService.GetDefaultMidiConfigName(); ;
                string newConfigFileName = _defaultsService.GetDefaultMidiConfigFileName();

                if (ConfigFileName == null || ConfigFileName == string.Empty)
                {
                    ConfigFileName = newConfigFileName;
                }

                // if the config file exists, we don't overwrite it
                if (!_configFileService.ConfigFileExists(ConfigFileName))
                {
                    // this can be false if the config file already existed
                    bool newConfigCreated = _configFileService.CreateNewConfigFile(newConfigName, newConfigFileName);
                }

                if (_registryService.UpdateRegistryCurrentConfigFileName(ConfigFileName))
                {
                    needServiceRestart = true;

                    var config = new MidiConfigFile(ConfigFileName);
                    config.Load();

                    _configFileService.CurrentConfig = config;
                }
                else
                {
                    //  show an error and leave
                    ErrorMessage = "Error_UnableToUpdateRegistryConfigFileName".GetLocalized();
                    return;
                }

            }

            if (CreateDefaultLoopbackEndpoints)
            {
                if (_configFileService.CurrentConfig != null)
                {
                    var midi2CreationConfig = _defaultsService.GetDefaultLoopbackCreationConfig();

                    // Create the MIDI 2.0 loopback

                    var midi2result = MidiLoopbackEndpointManager.CreateTransientLoopbackEndpoints(midi2CreationConfig);

                    if (midi2result.Success)
                    {
                        _configFileService.CurrentConfig.StoreLoopbackEndpointPair(midi2CreationConfig);
                    }
                    else
                    {
                        // update error information
                        ErrorMessage = "Error_UnableToCreateLoopbackWithMessage".GetLocalized() + " (MIDI2 Bidirectional) " + midi2result.ErrorInformation;
                    }


                    // Create the basic loopback

                    var midi1CreationConfig = _defaultsService.GetDefaultBasicLoopbackCreationConfig();

                    var midi1Result = MidiBasicLoopbackEndpointManager.CreateTransientLoopbackEndpoint(midi1CreationConfig);

                    if (midi1Result.Success)
                    {
                        _configFileService.CurrentConfig.StoreBasicLoopbackEndpoint(midi1CreationConfig);
                    }
                    else
                    {
                        // update error information
                        ErrorMessage = "Error_UnableToCreateLoopbackWithMessage".GetLocalized() + " (MIDI1 Basic) " + midi1Result.ErrorInformation;

                    }


                }
                else
                {
                    // TODO: Report that a config file is needed
                    ErrorMessage = "Error_UnableToCreateLoopbackEndpointMissingConfigurationFile".GetLocalized();
                }

            }

            if (UseNewStyleWinMMPortNames)
            {
                bool storedUseNewStyleWinMMPortNames = _registryService.GetDefaultUseNewStyleMidi1PortNaming();

                if (storedUseNewStyleWinMMPortNames != UseNewStyleWinMMPortNames)
                {
                    if (UseNewStyleWinMMPortNames)
                    {
                        _registryService.SetDefaultUseNewStyleMidi1PortNaming(Midi1PortNamingApproach.UseNewStyle);
                        needServiceRestart = true;
                    }
                    else
                    {
                        _registryService.SetDefaultUseNewStyleMidi1PortNaming(Midi1PortNamingApproach.UseClassicCompatible);
                        needServiceRestart = true;
                    }
                }
            }

            if (SetServiceToAutoStart)
            {
                ProcessStartInfo info = new ProcessStartInfo();
                info.FileName = "cmd.exe";
                info.UseShellExecute = true;
                info.Verb = "runas";

                info.Arguments = "/c \"midi service set-auto-start --restart=true\"";

                using (var proc = Process.Start(info))
                {
                    if (proc != null)
                    {
                        proc.WaitForExit();
                    }

                    needServiceRestart = false; // we take care of service restart above
                    serviceRestarted = true;
                }
            }


            // TODO: If ErrorMessage is not empty, we should display it here and not restart the service or app



            if (needServiceRestart)
            {
                ProcessStartInfo info = new ProcessStartInfo();
                info.FileName = "cmd.exe";
                info.UseShellExecute = true;
                info.Verb = "runas";
                info.Arguments = "/c \"midi service restart\"";

                using (var proc = Process.Start(info))
                {
                    if (proc != null)
                    {
                        proc.WaitForExit();
                    }

                    needServiceRestart = false;
                    serviceRestarted = true;
                }

            }


            if (serviceRestarted)
            {
                _messageBoxService.ShowInfo("Message_SettingsAppWillNowRestart".GetLocalized(), "AppDisplayName".GetLocalized());

                // For now, we'll just restart the app, but it would be 
                // better to reload config file and reload data
                string arguments = string.Empty;

                var restartError = AppInstance.Restart(arguments);

                switch (restartError)
                {
                    case AppRestartFailureReason.RestartPending:
                        _messageBoxService.ShowInfo("Error_SettingsAppCouldNotAutomaticallyRestart".GetLocalized(), "AppDisplayName".GetLocalized());
                        break;

                    case AppRestartFailureReason.InvalidUser:
                        _messageBoxService.ShowInfo("Error_SettingsAppCouldNotAutomaticallyRestart".GetLocalized(), "AppDisplayName".GetLocalized());
                        break;

                    case AppRestartFailureReason.Other:
                        _messageBoxService.ShowInfo("Error_SettingsAppCouldNotAutomaticallyRestart".GetLocalized(), "AppDisplayName".GetLocalized());
                        break;
                }
            }

            //m_navigationService.NavigateTo(typeof(HomeViewModel).FullName!);
        }



        public FirstRunExperienceViewModel(
            INavigationService navigationService,
            IMidiConfigFileService configService, 
            IMidiDefaultsService defaultsService,
            IMidiServiceRegistrySettingsService registryService,
            IMessageBoxService messageBoxService)
        {
            _navigationService = navigationService;
            _defaultsService = defaultsService;
            _configFileService = configService;
            _registryService = registryService;
            _messageBoxService = messageBoxService;

            CreateConfigurationFile = true;
            CreateDefaultLoopbackEndpoints = true;
            SetServiceToAutoStart = true;

            // todo: if that config exists, we use it and disable that control.


            // if reg setting for MIDI 1 already set, reflect that setting
            UseNewStyleWinMMPortNames = _registryService.GetDefaultUseNewStyleMidi1PortNaming();

            //UpdateConfigFileName();

            CompleteFirstRunSetupCommand = new RelayCommand(() =>
                {
                    CompleteFirstRunSetup();
                });
            _registryService = registryService;
        }
    }
}
