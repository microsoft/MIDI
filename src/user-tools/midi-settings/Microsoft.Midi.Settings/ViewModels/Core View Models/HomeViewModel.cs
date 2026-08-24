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
using Windows.UI.Popups;

namespace Microsoft.Midi.Settings.ViewModels
{
    public partial class HomeViewModel : ObservableRecipient, INavigationAware
    {

        
        public ICommand CommonTaskAssignMidi1DeviceToNewDriverCommand
        {
            get; private set;
        }

        public ICommand CommonTaskSetServiceAutoStartCommand
        {
            get; private set;
        }

        public ICommand LaunchFirstRunExperienceCommand
        {
            get; private set;
        }

        public ICommand CommonTaskOpenMidiConsoleCommand
        {
            get; private set;
        }

        public ICommand CommonTaskMidiDiagCommand
        {
            get; private set;
        }

        public IReadOnlyList<MidiToolAppInfo> ToolApps
        {
            get; private set;
        }

        public bool HasToolApps => ToolApps.Count > 0;

        
        //public ICommand LaunchNewSdkVersionUpdateCommand
        //{
        //    get; private set;
        //}

      

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

        public bool IsValidConfigLoaded
        {
            get => _configFileService.IsConfigFileActive;
        }

        public bool IsServiceAvailable
        {
            get
            {
                return _sdkService.IsServiceInitialized;
            }
        }


        public bool IsFirstRunSetupComplete
        {
            get
            {
                return _configFileService.IsConfigFileActive;
            }
        }


        [ObservableProperty]
        private bool isNetworkMidi2Available;

        [ObservableProperty]
        private bool isBasicLoopbackTransportAvailable;

        public string CurrentConfigurationName
        {
            get
            {
                if (_configFileService.CurrentConfig != null)
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




        [ObservableProperty]
        bool isNewerSdkRuntimeDownloadAvailable;



        private readonly IMidiTransportInfoService _transportInfoService;
        private readonly INavigationService _navigationService;
        private readonly IMidiConfigFileService _configFileService;
        private readonly IMidiSdkService _sdkService;
        private readonly IMidiConsoleToolsService _consoleToolsService;
        private readonly IMidiToolsService _toolsService;
        private readonly IMidiDiagnosticsService _diagnosticsService;
        private readonly ILoggingService _loggingService;


        public HomeViewModel(
            INavigationService navigationService,
            IMidiConfigFileService midiConfigFileService,
            IMidiSdkService sdkService,
            IMidiTransportInfoService transportInfoService,
            IMidiConsoleToolsService consoleToolsService,
            IMidiToolsService toolsService,
            IMidiDiagnosticsService diagnosticsService,
            ILoggingService loggingService
            )
        {
            _loggingService = loggingService;

            _navigationService = navigationService;
            _configFileService = midiConfigFileService;
            _sdkService = sdkService;
            _transportInfoService = transportInfoService;
            _consoleToolsService = consoleToolsService;
            _toolsService = toolsService;
            _diagnosticsService = diagnosticsService;

            ToolApps = _toolsService.GetInstalledTools();

            CommonTaskAssignMidi1DeviceToNewDriverCommand = new RelayCommand(
                () =>
                {
                    _navigationService.NavigateTo(typeof(AdvancedUsbSettingsViewModel).FullName!);
                });

            CommonTaskSetServiceAutoStartCommand = new RelayCommand(
                () =>
                {
                    _navigationService.NavigateTo(typeof(GlobalMidiSettingsViewModel).FullName!);
                });

            LaunchFirstRunExperienceCommand = new RelayCommand(
                () =>
                {
                    _navigationService.NavigateTo(typeof(FirstRunExperienceViewModel).FullName!);
                });


            // TODO: need to display an error if it returns false
            CommonTaskOpenMidiConsoleCommand = new RelayCommand(
                () =>
                {
                    _consoleToolsService.OpenMidiConsole();
                });

            // TODO: need to display an error if it returns false
            CommonTaskMidiDiagCommand = new RelayCommand(
                () =>
                {
                    _diagnosticsService.CaptureMidiDiagOutputToNotepad();
                });

            IsBasicLoopbackTransportAvailable = _transportInfoService.IsTransportAvailable("BLOOP") && IsValidConfigLoaded;

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
            _loggingService.LogInfo($"Enter");

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
