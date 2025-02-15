using CommunityToolkit.Mvvm.ComponentModel;
using CommunityToolkit.Mvvm.Input;
using Microsoft.Midi.Settings.Contracts.Services;
using Microsoft.Midi.Settings.Contracts.ViewModels;
using Microsoft.UI.Xaml;
using Microsoft.UI.Xaml.Controls;
using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using System.Windows.Input;

namespace Microsoft.Midi.Settings.ViewModels
{
    public class FirstRunExperienceViewModel : ObservableRecipient
    {
        private IMidiConfigFileService m_configFileService;

        private string m_configName;
        public string ConfigName
        {
            get => m_configName;
            set
            {
                m_configName = m_configFileService.CleanupConfigName(value);

                UpdateConfigFileName();
                OnPropertyChanged("ConfigName");
            }
        }

        public string ConfigFileName
        {
            get; private set;
        }

        public string ConfigFileLocation
        {
            get
            {
                return m_configFileService.GetConfigFilePath();                
            }
        }

        public bool ConfigExists
        {
            get
            {
                return m_configFileService.ConfigExists(ConfigName);
            }
        }

        

        public ICommand CreateConfigFileCommand
        {
            get;
        }


        private void UpdateConfigFileName()
        {
            ConfigFileName = m_configFileService.BuildConfigFileName(ConfigName);

            OnPropertyChanged("ConfigFileName");
            OnPropertyChanged("ConfigExists");
        }

        public FirstRunExperienceViewModel(IMidiConfigFileService configService)
        {
            m_configFileService = configService;
            ConfigName = m_configFileService.GetDefaultConfigName();

            UpdateConfigFileName();

            CreateConfigFileCommand = new RelayCommand(() =>
                {

                    if (m_configFileService.CreateNewConfigFile(ConfigName))
                    {
                        m_configFileService.SetCurrentConfig(ConfigName);
                    }
                    else
                    {
                        // TODO: show an error


                    }
                });


        }








    }
}
