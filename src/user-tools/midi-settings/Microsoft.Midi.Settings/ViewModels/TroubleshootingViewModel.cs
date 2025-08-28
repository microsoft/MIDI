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
    private INavigationService _navigationService;

    public ICommand RestartServiceCommand
    {
        get; private set;
    }

    public bool IsUserRunningAsAdmin
    {
        get { return UserHelper.CurrentUserHasAdminRights(); }
    }

    public TroubleshootingViewModel(INavigationService navigationService)
    {
        _navigationService = navigationService;

        _dispatcherQueue = DispatcherQueue.GetForCurrentThread();


        RestartServiceCommand = new RelayCommand<MidiEndpointDeviceInformation>(
            (param) =>
            {
                System.Diagnostics.Debug.WriteLine("RestartServiceCommand exec");

                //_navigationService.NavigateTo(typeof(DeviceDetailViewModel).FullName!, param);

            });
    }


    public void RestartService()
    {
        var controller = MidiServiceHelper.GetServiceController();

        if (controller != null)
        {
            MidiServiceHelper.StopService(controller);
            MidiServiceHelper.StartService(controller);
        }
        else
        {
            System.Diagnostics.Debug.WriteLine("Unable to get service controller");
        }
    }


    public void OnNavigatedFrom()
    {
    }


    public void OnNavigatedTo(object parameter)
    {
    }

}
