using System.ComponentModel;
using CommunityToolkit.Mvvm.ComponentModel;

using Microsoft.Midi.Settings.Contracts.Services;
using Microsoft.Midi.Settings.Helpers;
using Microsoft.Midi.Settings.Views;
using Microsoft.UI.Xaml.Navigation;

namespace Microsoft.Midi.Settings.ViewModels;

public class ShellViewModel : ObservableRecipient
{
    private bool _isBackEnabled;
    private object? _selected;

    private readonly IGeneralSettingsService _generalSettingsService;



    public bool AreDeveloperOptionsEnabled
    {
        get => _generalSettingsService.ShowDeveloperOptions;
    }

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

    public ShellViewModel(INavigationService navigationService, 
        INavigationViewService navigationViewService, 
        IGeneralSettingsService generalSettingsService)
    {
        NavigationService = navigationService;
        NavigationService.Navigated += OnNavigated;
        NavigationViewService = navigationViewService;

        _generalSettingsService = generalSettingsService;
        _generalSettingsService.SettingsChanged += _generalSettingsService_SettingsChanged;
    }

    private void _generalSettingsService_SettingsChanged(object? sender, EventArgs e)
    {
        OnPropertyChanged("AreDeveloperOptionsEnabled");
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
