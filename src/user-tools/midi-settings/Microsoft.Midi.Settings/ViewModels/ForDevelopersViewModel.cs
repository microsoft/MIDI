using CommunityToolkit.Mvvm.ComponentModel;
using CommunityToolkit.Mvvm.Input;
using Microsoft.Midi.Settings.Contracts.Services;
using Microsoft.Midi.Settings.Contracts.ViewModels;
using Microsoft.Midi.Settings.Helpers;
using System.Windows.Input;

namespace Microsoft.Midi.Settings.ViewModels;

public partial class ForDevelopersViewModel : ObservableRecipient
{
    IMidiServiceRegistrySettingsService m_registrySettingsService;

    public bool IsDeveloperModeEnabled => WindowsDeveloperModeHelper.IsDeveloperModeEnabled;


    [ObservableProperty]
    public bool enableDiscoveryAndProtocolNegotiation;

    [ObservableProperty]
    public bool enableMmcss;

    [ObservableProperty]
    public bool serviceRestartRequired;


    private UInt32 m_currentlySavedDiscoveryTimeoutValue;

    [ObservableProperty]
    public UInt32 discoveryTimeout;

    public UInt32 DiscoveryTimeoutMaximum => 50000;    // TODO: This needs to stay in sync with MidiDefs.h
    public UInt32 DiscoveryTimeoutMinimum => 500;      // TODO: This needs to stay in sync with MidiDefs.h

    [ObservableProperty]
    public bool discoveryTimeoutHasChanged;


    partial void OnDiscoveryTimeoutChanged(UInt32 value)
    {
        if (value != m_currentlySavedDiscoveryTimeoutValue)
        {
            DiscoveryTimeoutHasChanged = true;
        }
        else
        {
            DiscoveryTimeoutHasChanged = false;
        }
    }

    partial void OnEnableDiscoveryAndProtocolNegotiationChanged(bool value)
    {
        // todo: should check against original values
        ServiceRestartRequired = true;
    }

    partial void OnEnableMmcssChanged(bool value)
    {
        // todo: should check against original values
        ServiceRestartRequired = true;
    }


    public ICommand RestartServiceCommand { get; private set; }

    public ICommand CommitDiscoveryTimeoutCommand { get; private set; }

    public ICommand ShowMidiKernelStreamingDevicesCommand { get; private set; }


    public ForDevelopersViewModel(IMidiServiceRegistrySettingsService registrySettingsService)
    {
        m_registrySettingsService = registrySettingsService;

        // TODO read these values from the registry

        m_currentlySavedDiscoveryTimeoutValue = m_registrySettingsService.GetMidi2DiscoveryTimeoutMS();

        DiscoveryTimeout = m_currentlySavedDiscoveryTimeoutValue;

        EnableMmcss = m_registrySettingsService.GetUseMMCSS();
        EnableDiscoveryAndProtocolNegotiation = m_registrySettingsService.GetMidi2DiscoveryEnabled();

        ServiceRestartRequired = false;


        RestartServiceCommand = new RelayCommand(() =>
        {
            // TODO: Call into service service to restart
        });

        CommitDiscoveryTimeoutCommand = new RelayCommand(() =>
        {
            // TODO: save change to registry

            ServiceRestartRequired = true;
            m_currentlySavedDiscoveryTimeoutValue = DiscoveryTimeout;
        });

        ShowMidiKernelStreamingDevicesCommand = new RelayCommand(() =>
        {
            // TODO: save change to registry

            
        });

    }



}
