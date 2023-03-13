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
    private string _versionDescription;

    public bool IsDeveloperModeEnabled => WindowsDeveloperModeHelper.IsDeveloperModeEnabled;

    public bool ShowHowToEnableDeveloperMode => !IsDeveloperModeEnabled;
    public bool ShowDeveloperOptions
    {
        get => _generalSettingsService.ShowDeveloperOptions;
        set => _generalSettingsService.ShowDeveloperOptions = value;
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


    public SettingsViewModel(IThemeSelectorService themeSelectorService, ILocalSettingsService localSettingsService, IGeneralSettingsService generalSettingsService)
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

    private static string GetAppName()
    {
        return "AppDisplayName".GetLocalized();
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
