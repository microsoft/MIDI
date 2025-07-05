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

        public ICommand LaunchFirstRunExperienceCommand
        {
            get; private set;
        }

        public ICommand LaunchNewSdkVersionUpdateCommand
        {
            get; private set;
        }

        public ICommand CommonTaskCreateLoopbackEndpointsCommand
        {
            get; private set;
        }

        public ICommand CommonTaskSendSysExCommand
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

        public bool IsServiceAvailable => AppState.Current.IsServiceInitialized();


        public bool IsFirstRunSetupComplete
        {
            get
            {
                return m_configFileService.IsConfigFileActive;
            }
        }

        public bool IsNewSdkRuntimeDownloadAvailable
        {
            get
            {
                return AppState.Current.IsNewerSdkVersionAvailableForDownload();
            }
        }

        public string NewSdkRuntimeDownloadInformation
        {
            get
            {
                return AppState.Current.NewerSdkDownloadInformation();
            }
        }


        public string CurrentConfigurationName
        {
            get
            {
                if (IsValidConfigLoaded && m_configFileService.CurrentConfig.Header != null)
                {
                    return m_configFileService.CurrentConfig.Header.Name;
                }
                else
                {
                    return string.Empty;
                }
            }
        }

        public string CurrentConfigurationFileName
        {
            get
            {
                if (m_configFileService.CurrentConfig != null)
                {
                    return m_configFileService.CurrentConfig.FileName;
                }
                else
                {
                    return string.Empty;
                }
            }
        }


        public HomeViewModel(INavigationService navigationService, IMidiConfigFileService midiConfigFileService)
        {
            _navigationService = navigationService;
            m_configFileService = midiConfigFileService;

            LaunchFirstRunExperienceCommand = new RelayCommand(
                () =>
                {
                    _navigationService.NavigateTo(typeof(FirstRunExperienceViewModel).FullName!);
                });

            CommonTaskCreateLoopbackEndpointsCommand = new RelayCommand(
                () =>
                {
                    _navigationService.NavigateTo(typeof(EndpointsLoopViewModel).FullName!, "create");
                });

            CommonTaskSendSysExCommand = new RelayCommand(
                () =>
                {
                    _navigationService.NavigateTo(typeof(ToolsSysExViewModel).FullName!, "send");
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
