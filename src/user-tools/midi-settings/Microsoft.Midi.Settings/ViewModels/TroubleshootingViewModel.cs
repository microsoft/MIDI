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
            async (param) =>
            {
                System.Diagnostics.Debug.WriteLine("RestartServiceCommand exec");

                //_navigationService.NavigateTo(typeof(DeviceDetailViewModel).FullName!, param);

            });
    }


    public void RestartService()
    {
        var controller = MidiServiceHelper.GetServiceController();
        MidiServiceHelper.StopService(controller);
        MidiServiceHelper.StartService(controller);
    }


    public void OnNavigatedFrom()
    {
    }


    public void OnNavigatedTo(object parameter)
    {
    }

}
