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
using System;
using System.Collections.Generic;
using System.Linq;
using System.Runtime.InteropServices;
using System.Security.Principal;
using System.ServiceProcess;
using System.Text;
using System.Threading.Tasks;
using System.Windows.Input;


namespace Microsoft.Midi.Settings.ViewModels;
public partial class TroubleshootingViewModel : ObservableRecipient, INavigationAware
{
    private DispatcherQueue _dispatcherQueue;

    public ICommand RestartServiceCommand
    {
        get; private set;
    }

    //public ICommand OpenRegEdit64BitDrivers32
    //{
    //    get; private set;
    //}

    //public ICommand OpenRegEdit32BitDrivers32
    //{
    //    get; private set;
    //}

    public ICommand MidiDiagCommand
    {
        get; private set;
    }

    public ICommand MidiFixRegCommand
    {
        get; private set;
    }

    public ICommand RestoreInBoxComponentsCommand
    {
        get; private set;
    }

    public ICommand CaptureMidiLogsCommand
    {
        get; private set;
    }


    public bool IsUserRunningAsAdmin
    {
        get { return UserHelper.CurrentUserHasAdminRights(); }
    }

    [ObservableProperty]
    private List<FoundRegistryEntry> drivers32MidiEntries;

    [ObservableProperty]
    private List<FoundRegistryEntry> drivers32WOWMidiEntries;

    public void RefreshRegistryValues()
    {
        Drivers32MidiEntries = _diagnosticsService.GetDrivers32MidiEntries();

        Drivers32WOWMidiEntries = _diagnosticsService.GetDrivers32WOWMidiEntries();
    }




    private readonly INavigationService _navigationService;
    private readonly IMidiConsoleToolsService _consoleToolsService;
    private readonly IMidiDiagnosticsService _diagnosticsService;
    private readonly IMessageBoxService _messageBoxService;

    public TroubleshootingViewModel(
        INavigationService navigationService,
        IMidiConsoleToolsService consoleToolsService,
        IMidiDiagnosticsService diagnosticsService,
        IMessageBoxService messageBoxService)
    {
        _consoleToolsService = consoleToolsService;
        _navigationService = navigationService;
        _diagnosticsService = diagnosticsService;
        _messageBoxService = messageBoxService;

        _dispatcherQueue = DispatcherQueue.GetForCurrentThread();

        RestartServiceCommand = new RelayCommand(
            () =>
            {
                var success = _consoleToolsService.RestartMidiService();

                if (!success)
                {
                    _messageBoxService.ShowError("Error_UnableToRestartMidiService".GetLocalized());
                }

            });

        MidiDiagCommand = new RelayCommand(
            () =>
            {
                var success = _diagnosticsService.CaptureMidiDiagOutputToNotepad();

                if (!success)
                {
                    _messageBoxService.ShowError("Error_UnableToOpenMidiDiag".GetLocalized());
                }
            });

        MidiFixRegCommand = new RelayCommand(
            () =>
            {
                var success = _diagnosticsService.MidiFixReg();

                if (!success)
                {
                    _messageBoxService.ShowError("Error_UnableToOpenMidiFixReg".GetLocalized());
                }
                else
                {
                    RefreshRegistryValues();

                    _messageBoxService.ShowInfo("Message_RegistryRepairedPleaseRestartAllMIDIApps".GetLocalized());
                }
            });

        RestoreInBoxComponentsCommand = new RelayCommand(
            () =>
            {
                var success = _diagnosticsService.RestoreInBoxComponentRegistrations();

                if (!success)
                {
                    _messageBoxService.ShowError("Error_UnableToRestoreInBoxComponentRegistrations".GetLocalized());
                }
            });

        CaptureMidiLogsCommand = new RelayCommand(
            () =>
            {
                var fileName = _diagnosticsService.CaptureMidiLogsToFile();

                if (fileName != string.Empty)
                {
                    // open explorer with the file selected
                    consoleToolsService.OpenExplorerWithFile(fileName);
                }
                else
                {
                    // TODO: add the path to the UI
                    _messageBoxService.ShowError("Error_UnableToCaptureMidiLogsToFile".GetLocalized());
                }

            });

        //OpenRegEdit64BitDrivers32 = new RelayCommand(
        //    () => 
        //    {
        //        var success = _diagnosticsService.LaunchRegeditWithDrivers32Location();

        //        if (!success)
        //        {
        //            _messageBoxService.ShowError("Error_UnableToLaunchRegedit".GetLocalized());
        //        }
        //    });

        //OpenRegEdit32BitDrivers32 = new RelayCommand(
        //    () =>
        //    {
        //        var success = _diagnosticsService.LaunchRegeditWithDrivers32WOWLocation();

        //        if (!success)
        //        {
        //            _messageBoxService.ShowError("Error_UnableToLaunchRegedit".GetLocalized());
        //        }
        //    });

    }


    public void OnNavigatedFrom()
    {
    }


    public void OnNavigatedTo(object parameter)
    {
        RefreshRegistryValues();
    }

}
