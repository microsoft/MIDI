// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

using CommunityToolkit.Mvvm.ComponentModel;
using Microsoft.Midi.Settings.Contracts.Services;
using Microsoft.Midi.Settings.Helpers;
using Microsoft.Midi.Settings.Models;
using Microsoft.Midi.Settings.Services;
using Microsoft.Midi.Settings.Views;
using Microsoft.UI.Xaml;
using Microsoft.UI.Xaml.Navigation;
using Microsoft.Windows.Devices.Midi2.Utilities.RuntimeInformation;
using System.ComponentModel;

namespace Microsoft.Midi.Settings.ViewModels;

public partial class ShellViewModel : ObservableRecipient
{
    private bool _isBackEnabled;
    private object? _selected;

    private readonly IGeneralSettingsService _generalSettingsService;
    private readonly IMidiSettingsSearchService _settingsSearchService;
    private readonly IMidiConfigFileService _configFileService;
    private readonly IMidiSdkService _sdkService;

    public string SdkVersionString => MidiRuntimeInformation.GetInstalledVersion().ToString();

    public string? CurrentConfigName => _configFileService?.CurrentConfig?.FileName;

    public bool IsDeveloperModeEnabled => WindowsDeveloperModeHelper.IsDeveloperModeEnabled;

    public bool IsRunningAsAdministrator => UserHelper.CurrentUserHasAdminRights();

    public bool IsServiceAvailable
    {
        get
        {
            return _sdkService.IsServiceInitialized;
        }
    }

    //public bool AreDeveloperOptionsEnabled
    //{
    //    get => _generalSettingsService.ShowDeveloperOptions;
    //}

    public INavigationService NavigationService
    {
        get;
    }

    public INavigationViewService NavigationViewService
    {
        get;
    }

    public bool IsBackEnabled
    {
        get => _isBackEnabled;
        set => SetProperty(ref _isBackEnabled, value);
    }

    public object? Selected
    {
        get => _selected;
        set => SetProperty(ref _selected, value);
    }

    public bool IsValidConfigLoaded
    {
        get => _configFileService.IsConfigFileActive;
    }

    public IList<MidiSettingsSearchResult> GetSearchResults(string query)
    {
        return _settingsSearchService.GetFilteredResults(query);
    }


    [ObservableProperty]
    private bool showNetworkMidi2InLeftNav = false;

    [ObservableProperty]
    private bool showLoopbackMidiInLeftNav = false;

    [ObservableProperty]
    private bool showBle10MidiInLeftNav = false;

    [ObservableProperty]
    private bool showBle20MidiInLeftNav = false;

    [ObservableProperty]
    private bool showVirtualPatchBayInLeftNav = false;

    [ObservableProperty]
    private bool showSysExToolsInLeftNav = false;

    [ObservableProperty]
    private bool showToolsSectionInLeftNav = false;

    private void UpdateVisibilityOfLeftNavSections()
    {
        var transports = App.GetService<IMidiTransportInfoService>().GetAllTransports();

        foreach (var transport in transports)
        {
            switch (transport.TransportCode)
            {
                case "LOOP":
                    ShowLoopbackMidiInLeftNav = _configFileService.IsConfigFileActive;
                    break;
                case "BLE10":
                    ShowBle10MidiInLeftNav = _configFileService.IsConfigFileActive;
                    break;
                case "BLE20":
                    ShowBle20MidiInLeftNav = _configFileService.IsConfigFileActive;
                    break;
                case "NET2UDP":
                    ShowNetworkMidi2InLeftNav = _configFileService.IsConfigFileActive;
                    break;
                case "VPB":
                    ShowVirtualPatchBayInLeftNav = _configFileService.IsConfigFileActive;
                    break;

            }
        }


        // SysEx and other tools
        if (_generalSettingsService.GetPreviewToolsEnabled())
        {
            ShowSysExToolsInLeftNav = true;

            // sysex is the only tool
            ShowToolsSectionInLeftNav = true;
        }


    }


    public ShellViewModel(
        INavigationService navigationService, 
        INavigationViewService navigationViewService,
        IGeneralSettingsService generalSettingsService,
        IMidiConfigFileService midiConfigFileService,
        IMidiSdkService sdkService,
        IMidiSettingsSearchService settingsSearchService
        )
    {
        _settingsSearchService = settingsSearchService;
        _sdkService = sdkService;
        _configFileService = midiConfigFileService;
        _generalSettingsService = generalSettingsService;

        NavigationService = navigationService;
        NavigationService.Navigated += OnNavigated;
        NavigationViewService = navigationViewService;


        UpdateVisibilityOfLeftNavSections();

        _configFileService.ActiveConfigFileChanged += (s, e) =>
        {
            UpdateVisibilityOfLeftNavSections();
        };
    }

    public void RefreshSearchData()
    {
        _settingsSearchService.Refresh();
    }

    private void OnNavigated(object sender, NavigationEventArgs e)
    {
        IsBackEnabled = NavigationService.CanGoBack;

        if (e.SourcePageType == typeof(SettingsPage))
        {
            Selected = NavigationViewService.SettingsItem;
            return;
        }

        var selectedItem = NavigationViewService.GetSelectedItem(e.SourcePageType);
        if (selectedItem != null)
        {
            Selected = selectedItem;
        }
    }
}
