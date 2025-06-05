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
using Microsoft.Midi.Settings.Helpers;
using Microsoft.Midi.Settings.Services;
using Microsoft.UI.Xaml;

using Windows.ApplicationModel;

namespace Microsoft.Midi.Settings.ViewModels;

public class SettingsViewModel : ObservableRecipient
{
    private readonly IThemeSelectorService _themeSelectorService;
    private readonly ILocalSettingsService _localSettingsService;
    private readonly IGeneralSettingsService _generalSettingsService;
    private ElementTheme _elementTheme;
    //private bool _showDeveloperOptions;
    private string _versionDescription;

    private bool _isPreviewChannelEnabled = false;

    public bool IsDeveloperModeEnabled => WindowsDeveloperModeHelper.IsDeveloperModeEnabled;

    public bool ShowHowToEnableDeveloperMode => !IsDeveloperModeEnabled;
    //public bool ShowDeveloperOptions
    //{
    //    get => _generalSettingsService.ShowDeveloperOptions;
    //    set => _generalSettingsService.ShowDeveloperOptions = value;
    //}



    // TODO: This needs a callback when the property changes
    // probably need to call a service for this
    public bool IsPreviewChannelEnabled
    {
        get => _isPreviewChannelEnabled;
        set => SetProperty(ref _isPreviewChannelEnabled, value);
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
        ILocalSettingsService localSettingsService, 
        IGeneralSettingsService generalSettingsService)
    {
        _themeSelectorService = themeSelectorService;
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

        _localSettingsService = localSettingsService;
        _generalSettingsService = generalSettingsService;
    }

    private static string GetVersionDescription()
    {
        Version version;

        if (RuntimeHelper.IsMSIX)
        {
            var packageVersion = Package.Current.Id.Version;

            version = new(packageVersion.Major, packageVersion.Minor, packageVersion.Build, packageVersion.Revision);
        }
        else
        {
            version = Assembly.GetExecutingAssembly().GetName().Version!;
        }

        return $"{version.Major}.{version.Minor}.{version.Build}.{version.Revision}";
    }
}
