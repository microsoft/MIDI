// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

using System;
using System.Collections.Generic;
using System.Linq;
using System.Security.Principal;
using System.ServiceProcess;
using System.Text;
using System.Threading.Tasks;
using System.Windows.Input;
using CommunityToolkit.Mvvm.ComponentModel;
using CommunityToolkit.Mvvm.Input;
using Microsoft.Midi.Settings.Contracts.Services;
using Microsoft.Midi.Settings.Contracts.ViewModels;
using Microsoft.Midi.Settings.Models;
using Microsoft.UI.Dispatching;


namespace Microsoft.Midi.Settings.ViewModels;
public class TroubleshootingViewModel : ObservableRecipient, INavigationAware
{
    private DispatcherQueue _dispatcherQueue;

    public ICommand RestartServiceCommand
    {
        get; private set;
    }

    public ICommand MidiDiagCommand
    {
        get; private set;
    }


    public bool IsUserRunningAsAdmin
    {
        get { return UserHelper.CurrentUserHasAdminRights(); }
    }

    private readonly INavigationService _navigationService;
    private readonly IMidiConsoleToolsService _consoleToolsService;
    private readonly IMidiDiagnosticsService _diagnosticsService;

    public TroubleshootingViewModel(
        INavigationService navigationService,
        IMidiConsoleToolsService consoleToolsService,
        IMidiDiagnosticsService diagnosticsService)
    {
        _consoleToolsService = consoleToolsService;
        _navigationService = navigationService;
        _diagnosticsService = diagnosticsService;

        _dispatcherQueue = DispatcherQueue.GetForCurrentThread();

        RestartServiceCommand = new RelayCommand(
            () =>
            {
                _consoleToolsService.RestartMidiService();
            });

        MidiDiagCommand = new RelayCommand(
            () =>
            {
                _diagnosticsService.CaptureMidiDiagOutputToNotepad();
            });
    }


    public void OnNavigatedFrom()
    {
    }


    public void OnNavigatedTo(object parameter)
    {
    }

}
