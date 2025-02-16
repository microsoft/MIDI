using CommunityToolkit.Mvvm.ComponentModel;
using CommunityToolkit.Mvvm.Input;
using Microsoft.Midi.Settings.Contracts.Services;
using Microsoft.Midi.Settings.Contracts.ViewModels;
using Microsoft.Midi.Settings.Models;
using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using System.Windows.Input;

namespace Microsoft.Midi.Settings.ViewModels
{
    public class HomeViewModel : ObservableRecipient, INavigationAware
    {
        private readonly INavigationService _navigationService;
        private readonly IMidiConfigFileService m_configFileService;

        public ICommand LaunchFirstRunExperience
        {
            get; private set;
        }

        public string SystemTimerCurrentResolutionFormattedMilliseconds
        {
            get; set;
        }

        public string SystemTimerMaxResolutionFormattedMilliseconds
        {
            get; set;
        }

        public string SystemTimerMinResolutionFormattedMilliseconds
        {
            get; set;
        }


        public string MidiClockResolutionFormattedNanoseconds
        {
            get
            {
                return (MidiClock.ConvertTimestampTicksToNanoseconds(1)).ToString("N2") + " ns";
            }
        }


        public string MidiSdkVersionString
        {
            get
            {
                return AppState.Current.GetInstalledSdkVersionString();
            }
        }

        public Uri MidiSdkDownloadUri
        {
            get
            {
                return AppState.Current.GetMidiSdkInstallerUri();
            }
        }

        public bool IsValidConfigLoaded
        {
            get => m_configFileService.IsConfigFileActive;
        }


        public bool IsFirstRunSetupComplete
        {
            get
            {
                return m_configFileService.IsConfigFileActive;
            }
        }

        public string CurrentConfigurationName
        {
            get
            {
                return m_configFileService.CurrentConfig.Header.Name;
            }
        }

        public string CurrentConfigurationFileName
        {
            get
            {
                return m_configFileService.CurrentConfig.FileName;
            }
        }


        public HomeViewModel(INavigationService navigationService, IMidiConfigFileService midiConfigFileService)
        {
            _navigationService = navigationService;
            m_configFileService = midiConfigFileService;

            LaunchFirstRunExperience = new RelayCommand(
                () =>
                {
                    System.Diagnostics.Debug.WriteLine("View Device Details Command exec");

                    _navigationService.NavigateTo(typeof(FirstRunExperienceViewModel).FullName!);
                });
        }

        public void OnNavigatedFrom()
        {
            
        }

        public void OnNavigatedTo(object parameter)
        {
            var timerInfo = MidiClock.GetCurrentSystemTimerInfo();

            if (timerInfo.CurrentIntervalTicks > 0)
            {
                SystemTimerCurrentResolutionFormattedMilliseconds = MidiClock.ConvertTimestampTicksToMilliseconds(timerInfo.CurrentIntervalTicks).ToString("N3") + " ms";
                SystemTimerMaxResolutionFormattedMilliseconds = MidiClock.ConvertTimestampTicksToMilliseconds(timerInfo.MaximumIntervalTicks).ToString("N3") + " ms";
                SystemTimerMinResolutionFormattedMilliseconds = MidiClock.ConvertTimestampTicksToMilliseconds(timerInfo.MinimumIntervalTicks).ToString("N3") + " ms";
            }
            else
            {
                // failed to get timer info

                SystemTimerCurrentResolutionFormattedMilliseconds = "error";
                SystemTimerMaxResolutionFormattedMilliseconds = "error";
                SystemTimerMinResolutionFormattedMilliseconds = "error";
            }

        }






    }
}
