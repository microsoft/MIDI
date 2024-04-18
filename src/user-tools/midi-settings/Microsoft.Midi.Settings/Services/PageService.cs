using CommunityToolkit.Mvvm.ComponentModel;

using Microsoft.Midi.Settings.Contracts.Services;
using Microsoft.Midi.Settings.ViewModels;
using Microsoft.Midi.Settings.Views;
using Microsoft.UI.Xaml.Controls;

namespace Microsoft.Midi.Settings.Services;

public class PageService : IPageService
{
    private readonly Dictionary<string, Type> _pages = new();

    public PageService()
    {
        Configure<DevicesViewModel, DevicesPage>();
        Configure<DeviceDetailViewModel, DeviceDetailPage>();

        Configure<ForDevelopersViewModel, ForDevelopersPage>();
        //Configure<MainViewModel, MainPage>();
        Configure<ManagementSessionsViewModel, ManagementSessionsPage>();
        Configure<PluginsProcessingViewModel, PluginsProcessingPage>();
        Configure<PluginsTransportViewModel, PluginsTransportPage>();
        Configure<SettingsViewModel, SettingsPage>();
        Configure<SetupViewModel, SetupPage>();
        //Configure<ShellViewModel, ShellPage>();
        //Configure<ToolsConsoleViewModel, ToolsConsolePage>();
        //Configure<ToolsMonitorViewModel, ToolsMonitorPage>();
        Configure<ToolsSysExViewModel, ToolsSysExPage>();
        Configure<ToolsTestViewModel, ToolsTestPage>();
        Configure<TroubleshootingViewModel, TroubleshootingPage>();

        Configure<Midi1DevicesViewModel, Midi1DevicesPage>();

        Configure<HomeViewModel, HomePage>();
    }

    public Type GetPageType(string key)
    {
        Type? pageType;
        lock (_pages)
        {
            if (!_pages.TryGetValue(key, out pageType))
            {
                throw new ArgumentException($"Page not found: {key}. Did you forget to call PageService.Configure?");
            }
        }

        return pageType;
    }

    private void Configure<VM, V>()
        where VM : ObservableObject
        where V : Page
    {
        lock (_pages)
        {
            var key = typeof(VM).FullName!;
            if (_pages.ContainsKey(key))
            {
                throw new ArgumentException($"The key {key} is already configured in PageService");
            }

            var type = typeof(V);
            if (_pages.Any(p => p.Value == type))
            {
                throw new ArgumentException($"This type is already configured with key {_pages.First(p => p.Value == type).Key}");
            }

            _pages.Add(key, type);
        }
    }
}
