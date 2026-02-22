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


    [DllImport("User32.dll", SetLastError = true, CharSet = CharSet.Auto)]
    static extern int MessageBox(
        IntPtr hWnd,
        string lpText,
        string lpCaption,
        int uType);


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
                var success = _consoleToolsService.RestartMidiService();

                if (!success)
                {
                    MessageBox(
                        (IntPtr)0,
                        "Unable to restart MIDI service.",
                        "AppDisplayName".GetLocalized(),
                        0);
                }

            });

        MidiDiagCommand = new RelayCommand(
            () =>
            {
                var success = _diagnosticsService.CaptureMidiDiagOutputToNotepad();

                if (!success)
                {
                    MessageBox(
                        (IntPtr)0,
                        "Unable to open mididiag tool to capture diagnostic information. You may want to reinstall the SDK Runtime and Tools.",
                        "AppDisplayName".GetLocalized(),
                        0);
                }
            });
    }


    public void OnNavigatedFrom()
    {
    }


    public void OnNavigatedTo(object parameter)
    {
    }

}
