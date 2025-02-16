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
    public class FirstRunExperienceViewModel : ObservableRecipient
    {
        private IMidiConfigFileService m_configFileService;
        private IMidiDefaultsService m_defaultsService;
        private INavigationService m_navigationService;

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


        public bool CreateConfigurationFile
        {
            get;set;
        }

        public bool CreateDefaultLoopbackEndpoints
        {
            get; set;
        }

        public bool SetServiceToAutoStart
        {
            get; set;
        }


        public bool CanPersistChanges
        {
            get
            {
                return CreateConfigurationFile || m_configFileService.IsConfigFileActive;
            }
        }

        private void CompleteFirstRunSetup()
        {
            if (CreateConfigurationFile)
            {
                string newConfigName = m_defaultsService.GetDefaultMidiConfigName(); ;
                string newConfigFileName = m_defaultsService.GetDefaultMidiConfigFileName();

                if (m_configFileService.CreateNewConfigFile(newConfigName, newConfigFileName))
                {
                    m_configFileService.UpdateRegistryCurrentConfigFile(ConfigFileName);
                }
                else
                {
                    // TODO: show an error
                }
            }


            if (CreateDefaultLoopbackEndpoints)
            {
                var creationConfig = m_defaultsService.GetDefaultLoopbackCreationConfig();

                var result = MidiLoopbackEndpointManager.CreateTransientLoopbackEndpoints(creationConfig);

                if (result.Success)
                {
                    m_configFileService.CurrentConfig.StoreLoopbackEndpointPair(creationConfig);

                    // todo: show results

                }
                else
                {
                    // update error information
                }
            }

            if (SetServiceToAutoStart)
            {
                ProcessStartInfo info = new ProcessStartInfo();
                info.FileName = "cmd.exe";
                info.UseShellExecute = true;
                info.Verb = "runas";
                info.Arguments = "/c \"midi service set-auto-start --restart & pause\"";

                var proc = Process.Start(info);
                if (proc != null)
                {
                    // service updated

                    // TODO: Need to wait for completion and then check return code                   
                }
            }
            else if (CreateConfigurationFile)
            {
                // we created a new config file, so we need to restart the service

                ProcessStartInfo info = new ProcessStartInfo();
                info.FileName = "cmd.exe";
                info.UseShellExecute = true;
                info.Verb = "runas";
                info.Arguments = "/c \"midi service restart\"";

                if (Process.Start(info) != null)
                {
                    // service restarted

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
            IMidiDefaultsService defaultsService)
        {
            m_navigationService = navigationService;
            m_defaultsService = defaultsService;
            m_configFileService = configService;

            CreateConfigurationFile = true;
            CreateDefaultLoopbackEndpoints = true;
            SetServiceToAutoStart = true;


            // todo: if that config exists, we use it and disable that control.

            //UpdateConfigFileName();

            CompleteFirstRunSetupCommand = new RelayCommand(() =>
                {
                    CompleteFirstRunSetup();
                });

        }




    }
}
