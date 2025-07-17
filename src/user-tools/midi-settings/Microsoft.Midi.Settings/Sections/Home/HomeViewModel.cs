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
    public partial class HomeViewModel : ObservableRecipient, INavigationAware
    {
        private readonly INavigationService _navigationService;
        private readonly IMidiConfigFileService _configFileService;
        private readonly IMidiUpdateService _updateService;


        public event EventHandler<string> UpdateFailed;

        public ICommand LaunchFirstRunExperienceCommand
        {
            get; private set;
        }

        //public ICommand LaunchNewSdkVersionUpdateCommand
        //{
        //    get; private set;
        //}

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

        [ObservableProperty]
        private bool isSdkDownloadInProgress;


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
                //return AppState.Current.GetMidiSdkInstallerUri();

                return new Uri("https://github.com/microsoft/MIDI/releases/download/customer-preview-2/arm64-full.zip");
            }
        }

        public bool IsValidConfigLoaded
        {
            get => _configFileService.IsConfigFileActive;
        }

        public bool IsServiceAvailable => AppState.Current.IsServiceInitialized();


        public bool IsFirstRunSetupComplete
        {
            get
            {
                return _configFileService.IsConfigFileActive;
            }
        }

        public bool IsNewerSdkRuntimeDownloadAvailable
        {
            get
            {
                // TODO: this should be in the update service, not app state

                //return AppState.Current.IsNewerSdkVersionAvailableForDownload();

                return true;
            }
        }

        //public string NewSdkRuntimeDownloadInformation
        //{
        //    get
        //    {
        //        return AppState.Current.NewerSdkDownloadInformation();
        //    }
        //}


        public string CurrentConfigurationName
        {
            get
            {
                if (IsValidConfigLoaded && _configFileService.CurrentConfig.Header != null)
                {
                    return _configFileService.CurrentConfig.Header.Name;
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
                if (_configFileService.CurrentConfig != null)
                {
                    return _configFileService.CurrentConfig.FileName;
                }
                else
                {
                    return string.Empty;
                }
            }
        }


        public async void StartSdkUpdate()
        {
            IsSdkDownloadInProgress = true;

            //Task.Run(() =>
            //{
            var updateUri = this.MidiSdkDownloadUri;

            bool success = await _updateService.DownloadAndInstallUpdate(updateUri);
            //});

            // if we got here, the download failed

            IsSdkDownloadInProgress = false;
            UpdateFailed(this, "Downloading the update file failed.");
        }

        public HomeViewModel(
            INavigationService navigationService, 
            IMidiConfigFileService midiConfigFileService, 
            IMidiUpdateService updateService)
        {
            _navigationService = navigationService;
            _configFileService = midiConfigFileService;
            _updateService = updateService;

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


            //LaunchNewSdkVersionUpdateCommand = new RelayCommand(
            //    () => 
            //    {
            //    });


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
