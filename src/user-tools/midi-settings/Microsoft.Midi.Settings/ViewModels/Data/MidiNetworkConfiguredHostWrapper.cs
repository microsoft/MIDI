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

    public MidiNetworkConfiguredHostWrapper(
        MidiNetworkConfiguredHost host,
        IMidiConfigFileService configFileService
        )
    {
        _configFileService = configFileService;

        Host = host;

        HasStarted = host.HasStarted;


        StartCommand = new RelayCommand(async () =>
        {
            var result = await MidiNetworkTransportManager.StartNetworkHostAsync(host.Id);

            if (result.Success)
            {
                // todo: need to refresh props

                HasStarted = true;
            }
            else
            {
                // TODO

                System.Diagnostics.Debug.WriteLine("Failed to start host");
                System.Diagnostics.Debug.WriteLine(result.ErrorInformation);
            }

        });


        StopCommand = new RelayCommand(async () =>
        {
            var result = await MidiNetworkTransportManager.StopNetworkHostAsync(host.Id);

            if (result.Success)
            {
                // todo: need to refresh props

                HasStarted = false;
            }
            else
            {
                System.Diagnostics.Debug.WriteLine("Failed to stop host");
                System.Diagnostics.Debug.WriteLine(result.ErrorInformation);
            }

            // TODO
        });


        DeleteCommand = new RelayCommand(async () =>
        {
            // first, stop the host.

            var result = await MidiNetworkTransportManager.StopNetworkHostAsync(host.Id);

            if (result.Success)
            {
                // todo: need to refresh props

                HasStarted = false;

                if (_configFileService.CurrentConfig != null)
                {
                    // remove it from the config file
                    _configFileService.CurrentConfig.RemoveNetworkHost(host.Id);
                }

            }
            else
            {
                System.Diagnostics.Debug.WriteLine("Failed to stop host");
                System.Diagnostics.Debug.WriteLine(result.ErrorInformation);
            }

            // TODO
        });

    }

}
