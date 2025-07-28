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
using Microsoft.Windows.Devices.Midi2.Utilities.Update;
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
        private readonly INavigationService _navigationService;
        private readonly IMidiConfigFileService _configFileService;
        private readonly IMidiUpdateService _updateService;
        private readonly IMidiSdkService _sdkService;


        public event EventHandler<string> UpdateFailed;

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
                return _sdkService.InstalledRuntimeDetailedVersionString;
            }
        }

        [ObservableProperty]
        private Uri midiSdkDownloadUri;

        [ObservableProperty]
        private Uri midiSdkReleaseNotesUri;

        [ObservableProperty]
        private string midiSdkNewReleaseVersionString;

        [ObservableProperty]
        private string midiSdkNewReleaseDateString;

        [ObservableProperty]
        private string midiSdkCurrentReleaseVersionString;

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

        private MidiRuntimeRelease? _newRelease;



        public void CheckForSdkUpdates()
        {
            try
            {

                IsNewerSdkRuntimeDownloadAvailable = false;
                _newRelease = null;

                // TODO: This needs to check the correct channel
                var newRelease = _updateService.CheckForUpdates();
                var installedVersion = _sdkService.InstalledVersion;

                if (newRelease == null || installedVersion == null) return;

                MidiSdkCurrentReleaseVersionString = installedVersion.ToString();

                if (newRelease.Version.IsGreaterThan(installedVersion))
                {
                    _newRelease = newRelease;

                    MidiSdkDownloadUri = newRelease.DirectDownloadUriForCurrentRuntimeArchitecture;
                    MidiSdkReleaseNotesUri = newRelease.ReleaseNotesUri;
                    MidiSdkNewReleaseDateString = newRelease.BuildDate.ToString("MMMM dd, yyyy");
                    MidiSdkNewReleaseVersionString = newRelease.Version.ToString();

                    IsNewerSdkRuntimeDownloadAvailable = true;
                }
                else
                {
                    IsNewerSdkRuntimeDownloadAvailable = false;
                }
            }
            catch (Exception ex)
            { 
                // error trying to check for a new release. Firewall? No network?

                System.Diagnostics.Debug.WriteLine(ex);
                IsNewerSdkRuntimeDownloadAvailable = false;
            }

        }


        [ObservableProperty]
        bool isNewerSdkRuntimeDownloadAvailable;


        public string NewerSdkRuntimeDownloadVersion
        {
            get
            {
                if (_newRelease != null)
                {
                    return _newRelease.Version.ToString();
                }

                return "";
            }
        }

        public string NewSdkRuntimeDownloadAvailableMessage
        {
            get
            {
                if (_newRelease != null)
                {
                    return _newRelease.Description;
                }

                return string.Empty;
            }
        }


        

        public async void StartSdkUpdate()
        {
            // TODO: We need to make sure we pull the correct architecture

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
            IMidiUpdateService updateService,
            IMidiSdkService sdkService
            )
        {
            _navigationService = navigationService;
            _configFileService = midiConfigFileService;
            _updateService = updateService;
            _sdkService = sdkService;

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


            // TODO: Move this impl to a service
            CommonTaskOpenMidiConsoleCommand = new RelayCommand(
                () =>
                {
                    try
                    {
                        string arguments =
                            " new-tab --title \"Windows MIDI Services Console\"" +
                            " cmd /k midi.exe";

                        var consoleProcess = new System.Diagnostics.Process();

                        consoleProcess.StartInfo.FileName = "wt";
                        consoleProcess.StartInfo.Arguments = arguments;
                        consoleProcess.StartInfo.UseShellExecute = true;
                        consoleProcess.Start();

                        //consoleProcess.WaitForExit();
                    }
                    catch (Exception)
                    {
                        //var dialog = new MessageDialog("Error opening MIDI console");
                        //dialog.Content = ex.ToString();

                        //dialog.ShowAsync().Wait();
                    }
                });

            // TODO: Move this impl to a service
            CommonTaskMidiDiagCommand = new RelayCommand(
                () =>
                {
                    try
                    {
                        
                        string mididiagTempFileName = Path.GetTempFileName();

                        using (var consoleProcess = new System.Diagnostics.Process())
                        {
                            consoleProcess.StartInfo.FileName = $"mididiag.exe";
                            consoleProcess.StartInfo.RedirectStandardOutput = true;
                            consoleProcess.Start();
                            //consoleProcess.WaitForExit();

                            var output = consoleProcess.StandardOutput.ReadToEnd();

                            var fileStream = File.OpenWrite(mididiagTempFileName);

                            using (StreamWriter writer = new StreamWriter(fileStream))
                            {
                                writer.WriteLine(output);
                            }
                        }

                        var notepadProcess = new System.Diagnostics.Process();
                        notepadProcess.StartInfo.FileName = "notepad.exe";
                        notepadProcess.StartInfo.Arguments = mididiagTempFileName;
                        notepadProcess.Start();

                    }
                    catch (Exception)
                    {
                        //var dialog = new MessageDialog("Error opening MIDI console");
                        //dialog.Content = ex.ToString();

                        //dialog.ShowAsync().Wait();
                    }
                });



            if (_updateService.GetAutoCheckForUpdatesEnabled())
            {
                CheckForSdkUpdates();
            }

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
