using CommunityToolkit.Mvvm.ComponentModel;
using CommunityToolkit.Mvvm.Input;
using Microsoft.Midi.Settings.Contracts.Services;
using Microsoft.Windows.Devices.Midi2.Endpoints.Network;
using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using System.Windows.Input;

namespace Microsoft.Midi.Settings.ViewModels;

public partial class MidiNetworkConfiguredHostWrapper : ObservableRecipient
{
    [ObservableProperty]
    private MidiNetworkConfiguredHost host;

    [ObservableProperty]
    private bool hasStarted;


    public ICommand StartCommand { get; private set; }
    public ICommand StopCommand { get; private set; }
    public ICommand DeleteCommand { get; private set; }

    private readonly IMidiConfigFileService _configFileService;
    private readonly ILoggingService _loggingService;
    private readonly IMessageBoxService _messageBoxService;

    public MidiNetworkConfiguredHostWrapper(
        MidiNetworkConfiguredHost host,
        IMidiConfigFileService configFileService,
        ILoggingService loggingService,
        IMessageBoxService messageBoxService
        )
    {
        _loggingService = loggingService;
        _configFileService = configFileService;
        _messageBoxService = messageBoxService;

        Host = host;

        HasStarted = host.HasStarted;


        StartCommand = new RelayCommand(async () =>
        {
            var result = await MidiNetworkTransportManager.StartNetworkHostAsync(host.HostId);

            if (result.Success)
            {
                // todo: need to refresh props

                HasStarted = true;
            }
            else
            {
                _messageBoxService.ShowError($"Failed to start host. {result.ErrorInformation}");

                _loggingService.LogError($"Failed to start host. {result.ErrorInformation}");
            }

        });


        StopCommand = new RelayCommand(async () =>
        {
            _loggingService.LogInfo($"Enter");

            var result = await MidiNetworkTransportManager.StopNetworkHostAsync(host.HostId);

            if (result.Success)
            {
                // todo: need to refresh props

                HasStarted = false;
            }
            else
            {
                _messageBoxService.ShowError($"Failed to stop host. {result.ErrorInformation}");

                _loggingService.LogError($"Failed to stop host. {result.ErrorInformation}");
            }

            // TODO
        });


        DeleteCommand = new RelayCommand(async () =>
        {
            _loggingService.LogInfo($"Enter");

            // first, stop the host.

            var result = await MidiNetworkTransportManager.StopNetworkHostAsync(host.HostId);

            if (result.Success)
            {
                // todo: need to refresh props

                HasStarted = false;

                if (_configFileService.CurrentConfig != null)
                {
                    // remove it from the config file
                    _configFileService.CurrentConfig.RemoveNetworkHost(host.HostId);
                }

            }
            else
            {
                _messageBoxService.ShowError($"Failed to stop host. {result.ErrorInformation}");

                _loggingService.LogError($"Failed to stop host. {result.ErrorInformation}");
                
            }

            // TODO
        });

    }

}
