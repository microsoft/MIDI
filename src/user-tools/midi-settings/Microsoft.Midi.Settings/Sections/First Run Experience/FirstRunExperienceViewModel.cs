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
using Microsoft.Windows.Devices.Midi2.Endpoints.Loopback;
using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using System.Windows.Input;

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


        private IMidiConfigFileService m_configFileService;
        private IMidiDefaultsService m_defaultsService;
        private INavigationService m_navigationService;
        private IMidiServiceRegistrySettingsService m_registryService;

        public string ConfigFileName
        {
            get; private set;
        }

        public string ConfigFilesLocation
        {
            get
            {
                return m_configFileService.GetConfigFilesLocation();
            }
        }

        public bool ConfigFileExists
        {
            get
            {
                return m_configFileService.ConfigFileExists(ConfigFileName);
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
                return CreateConfigurationFile || m_configFileService.IsConfigFileActive;
            }
        }

        private void CompleteFirstRunSetup()
        {
            bool needServiceRestart = false;

            ErrorMessage = string.Empty;

            if (CreateConfigurationFile)
            {
                string newConfigName = m_defaultsService.GetDefaultMidiConfigName(); ;
                string newConfigFileName = m_defaultsService.GetDefaultMidiConfigFileName();

                if (ConfigFileName == null || ConfigFileName == string.Empty)
                {
                    ConfigFileName = newConfigFileName;
                }

                // if the config file exists, we don't overwrite it
                if (!m_configFileService.ConfigFileExists(ConfigFileName))
                {
                    // this can be false if the config file already existed
                    bool newConfigCreated = m_configFileService.CreateNewConfigFile(newConfigName, newConfigFileName);
                }

                if (m_registryService.UpdateRegistryCurrentConfigFileName(ConfigFileName))
                {
                    needServiceRestart = true;

                    var config = new MidiConfigFile(ConfigFileName);
                    config.Load();

                    m_configFileService.CurrentConfig = config;
                }
                else
                {
                    //  show an error and leave
                    ErrorMessage = "Unable to update registry config file name.";
                    return;
                }

            }

            if (CreateDefaultLoopbackEndpoints)
            {
                if (m_configFileService.CurrentConfig != null)
                {
                    var creationConfig = m_defaultsService.GetDefaultLoopbackCreationConfig();


                    // TODO: Verify they don't already exist


                    var result = MidiLoopbackEndpointManager.CreateTransientLoopbackEndpoints(creationConfig);

                    if (result.Success)
                    {
                        m_configFileService.CurrentConfig.StoreLoopbackEndpointPair(creationConfig);
                    }
                    else
                    {
                        // update error information
                        ErrorMessage = "Error creating loopback endpoints. " + result.ErrorInformation;
                    }
                }
                else
                {
                    // TODO: Report that a config file is needed
                    ErrorMessage = "Cannot create loopback endpoints without a valid config file.";
                }
            }

            if (UseNewStyleWinMMPortNames)
            {
                bool storedUseNewStyleWinMMPortNames = m_registryService.GetDefaultUseNewStyleMidi1PortNaming();

                if (storedUseNewStyleWinMMPortNames != UseNewStyleWinMMPortNames)
                {
                    if (UseNewStyleWinMMPortNames)
                    {
                        m_registryService.SetDefaultUseNewStyleMidi1PortNaming(Midi1PortNameSelectionProperty.PortName_UseInterfacePlusPinName);
                        needServiceRestart = true;
                    }
                    else
                    {
                        m_registryService.SetDefaultUseNewStyleMidi1PortNaming(Midi1PortNameSelectionProperty.PortName_UseLegacyWinMM);
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
                info.Arguments = "/c \"midi service set-auto-start --restart=false\"";

                using (var proc = Process.Start(info))
                {
                    if (proc != null)
                    {
                        proc.WaitForExit();
                        needServiceRestart = true;
                    }
                }
            }

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
                        needServiceRestart = false;
                    }
                }

            }

            // set the flag saying the first-run setup is all done
            // or maybe we just want to trigger this on having a valid config file
            // that isn't the Default.midiconfig.json from earlier builds

            m_navigationService.NavigateTo(typeof(HomeViewModel).FullName!);
        }



        public FirstRunExperienceViewModel(
            INavigationService navigationService,
            IMidiConfigFileService configService, 
            IMidiDefaultsService defaultsService,
            IMidiServiceRegistrySettingsService registryService)
        {
            m_navigationService = navigationService;
            m_defaultsService = defaultsService;
            m_configFileService = configService;
            m_registryService = registryService;

            CreateConfigurationFile = true;
            CreateDefaultLoopbackEndpoints = true;
            SetServiceToAutoStart = true;

            // todo: if that config exists, we use it and disable that control.


            // if reg setting for MIDI 1 already set, reflect that setting
            UseNewStyleWinMMPortNames = m_registryService.GetDefaultUseNewStyleMidi1PortNaming();

            //UpdateConfigFileName();

            CompleteFirstRunSetupCommand = new RelayCommand(() =>
                {
                    CompleteFirstRunSetup();
                });
            m_registryService = registryService;
        }
    }
}
