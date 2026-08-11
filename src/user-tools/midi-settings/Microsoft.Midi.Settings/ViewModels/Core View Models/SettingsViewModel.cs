// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

using System.Diagnostics.Eventing.Reader;
using System.Reflection;
using System.Windows.Input;

using CommunityToolkit.Mvvm.ComponentModel;
using CommunityToolkit.Mvvm.Input;

using Microsoft.Midi.Settings.Contracts.Services;
using Microsoft.Midi.Settings.Contracts.ViewModels;
using Microsoft.Midi.Settings.Helpers;
using Microsoft.Midi.Settings.Services;
using Microsoft.UI.Xaml;

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


    private ElementTheme _elementTheme;



    private bool _arePreviewToolsEnabled = false;

    public bool IsDeveloperModeEnabled => WindowsDeveloperModeHelper.IsDeveloperModeEnabled;

    public bool ShowHowToEnableDeveloperMode => !IsDeveloperModeEnabled;
    //public bool ShowDeveloperOptions
    //{
    //    get => _generalSettingsService.ShowDeveloperOptions;
    //    set => _generalSettingsService.ShowDeveloperOptions = value;
    //}


    
    public bool ArePreviewToolsEnabled
    {
        get => _generalSettingsService.GetPreviewToolsEnabled();
        set => SetProperty(_arePreviewToolsEnabled, value, (newValue) =>
        {
            _generalSettingsService.SetPreviewToolsEnabled(newValue);

            _arePreviewToolsEnabled = newValue;
        });
    }

    public ElementTheme ElementTheme
    {
        get => _elementTheme;
        set => SetProperty(ref _elementTheme, value);
    }

    public ICommand SwitchThemeCommand
    {
        get;
    }

    private readonly ILoggingService _loggingService;
    public SettingsViewModel(
        IThemeSelectorService themeSelectorService, 
        ILocalSettingsService localSettingsService, 
        IGeneralSettingsService generalSettingsService,
        ILoggingService loggingService)
    {
        
        _loggingService = loggingService;
        _themeSelectorService = themeSelectorService;
        _localSettingsService = localSettingsService;
        _generalSettingsService = generalSettingsService;

        _elementTheme = _themeSelectorService.Theme;

        SwitchThemeCommand = new RelayCommand<ElementTheme>(
            async (param) =>
            {
                if (ElementTheme != param)
                {
                    ElementTheme = param;
                    await _themeSelectorService.SetThemeAsync(param);
                }
            });


        _arePreviewToolsEnabled = _generalSettingsService.GetPreviewToolsEnabled();

    }

}
