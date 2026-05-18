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

public partial class MidiNetworkConfiguredClientWrapper : ObservableRecipient
{
    [ObservableProperty]
    private MidiNetworkConfiguredClient client;


    public ICommand DisconnectCommand { get; private set; }

    private readonly IMidiConfigFileService _configFileService;
    private readonly ILoggingService _loggingService;
    private readonly IMessageBoxService _messageBoxService;



    [ObservableProperty]
    private UInt64 currentLatencyTicks;


    public MidiNetworkConfiguredClientWrapper(
        MidiNetworkConfiguredClient client,
        IMidiConfigFileService configFileService,
        ILoggingService loggingService,
        IMessageBoxService messageBoxService
        )
    {
        _loggingService = loggingService;
        _configFileService = configFileService;
        _messageBoxService = messageBoxService;

        Client = client;


        DisconnectCommand = new RelayCommand(async () =>
        {
            // TODO
        });


    }

}
