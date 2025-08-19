// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

using System.Reflection;
using System.Windows.Input;

using CommunityToolkit.Mvvm.ComponentModel;
using CommunityToolkit.Mvvm.Input;

using Microsoft.Midi.Settings.Contracts.Services;
using Microsoft.Midi.Settings.Contracts.ViewModels;
using Microsoft.Midi.Settings.Helpers;
using Microsoft.Midi.Settings.Services;
using Microsoft.UI.Xaml;
using Microsoft.Windows.Devices.Midi2.Common;
using Microsoft.Windows.Devices.Midi2.Utilities.RuntimeInformation;
using Windows.ApplicationModel;

namespace Microsoft.Midi.Settings.ViewModels;

public class SettingsViewModel : ObservableRecipient, ISettingsSearchTarget
{
    public static IList<string> GetSearchKeywords()
    {
        // TODO: these need to be localized, so should refer to resources instead
        return new[] { "app settings", "auto update", "developer mode", "theme", "light mode", "dark mode" };
    }

    public static string GetSearchPageTitle()
    {
        return "MIDI Settings App Settings";
    }

    public static string GetSearchPageDescription()
    {
        return "Configure settings specifically for this application.";
    }

    private readonly IThemeSelectorService _themeSelectorService;
    private readonly ILocalSettingsService _localSettingsService;
    private readonly IGeneralSettingsService _generalSettingsService;
    private readonly IMidiUpdateService _midiUpdateService;

    private ElementTheme _elementTheme;
    //private bool _showDeveloperOptions;
    private string _versionDescription;

    private bool _isPreviewChannelEnabled = false;
    private bool _isUpdateCheckingEnabled = true;

    public bool IsDeveloperModeEnabled => WindowsDeveloperModeHelper.IsDeveloperModeEnabled;

    public bool ShowHowToEnableDeveloperMode => !IsDeveloperModeEnabled;
    //public bool ShowDeveloperOptions
    //{
    //    get => _generalSettingsService.ShowDeveloperOptions;
    //    set => _generalSettingsService.ShowDeveloperOptions = value;
    //}


    public bool IsPreviewChannelEnabled
    {
        get => _midiUpdateService.GetCurrentPreferredChannel() == MidiRuntimeReleaseTypes.Preview;
        set => SetProperty(_isPreviewChannelEnabled, value, (newValue) => 
        { 
            //_isPreviewChannelEnabled = newValue;  

            if (newValue)
            {
                _midiUpdateService.SetCurrentPreferredChannel(Windows.Devices.Midi2.Utilities.RuntimeInformation.MidiRuntimeReleaseTypes.Preview);
            }
            else
            {
                _midiUpdateService.SetCurrentPreferredChannel(Windows.Devices.Midi2.Utilities.RuntimeInformation.MidiRuntimeReleaseTypes.Stable);
            }
        });
    }

    
    public bool IsUpdateCheckingEnabled
    {
        get => _midiUpdateService.GetAutoCheckForUpdatesEnabled();
        set => SetProperty(_isUpdateCheckingEnabled, value, (newValue) => 
        { 
            _isUpdateCheckingEnabled = newValue;

            _midiUpdateService.SetAutoCheckForUpdatesEnabled(newValue);
        });
    }


    public ElementTheme ElementTheme
    {
        get => _elementTheme;
        set => SetProperty(ref _elementTheme, value);
    }

    public string VersionDescription
    {
        get => _versionDescription;
        set => SetProperty(ref _versionDescription, value);
    }

    public ICommand SwitchThemeCommand
    {
        get;
    }


    public SettingsViewModel(
        IThemeSelectorService themeSelectorService, 
        IMidiUpdateService midiUpdateService,
        ILocalSettingsService localSettingsService, 
        IGeneralSettingsService generalSettingsService)
    {
        _midiUpdateService = midiUpdateService;
        _themeSelectorService = themeSelectorService;
        _localSettingsService = localSettingsService;
        _generalSettingsService = generalSettingsService;

        _elementTheme = _themeSelectorService.Theme;
        _versionDescription = GetVersionDescription();

        SwitchThemeCommand = new RelayCommand<ElementTheme>(
            async (param) =>
            {
                if (ElementTheme != param)
                {
                    ElementTheme = param;
                    await _themeSelectorService.SetThemeAsync(param);
                }
            });

        _isUpdateCheckingEnabled = _midiUpdateService.GetAutoCheckForUpdatesEnabled();

        _isPreviewChannelEnabled = 
            (_midiUpdateService.GetCurrentPreferredChannel() == MidiRuntimeReleaseTypes.Preview);
    }

    private static string GetVersionDescription()
    {
        return MidiRuntimeInformation.GetInstalledVersion().ToString();
    }
}
