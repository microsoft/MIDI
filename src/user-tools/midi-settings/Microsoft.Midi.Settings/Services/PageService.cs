// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

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
        Configure<EndpointsAllViewModel, EndpointsAllPage>();

        Configure<EndpointsLoopViewModel, EndpointsLoopPage>();
        Configure<EndpointsNet2UdpViewModel, EndpointsNet2UdpPage>();
        Configure<NetworkMidi2SetupViewModel, NetworkMidi2SetupPage>();
        Configure<EndpointsBle10ViewModel, EndpointsBle10Page>();


        Configure<DeviceDetailViewModel, DeviceDetailPage>();
        Configure<RoutesViewModel, RoutesPage>();

        Configure<ForDevelopersViewModel, ForDevelopersPage>();

        //Configure<MainViewModel, MainPage>();
        Configure<ManagementSessionsViewModel, ManagementSessionsPage>();
        Configure<PluginsProcessingViewModel, PluginsProcessingPage>();
        Configure<PluginsTransportViewModel, PluginsTransportPage>();
        Configure<SettingsViewModel, SettingsPage>();
        Configure<ConfigurationsViewModel, ConfigurationsPage>();
        //Configure<ShellViewModel, ShellPage>();
        //Configure<ToolsConsoleViewModel, ToolsConsolePage>();
        //Configure<ToolsMonitorViewModel, ToolsMonitorPage>();
        Configure<ToolsSysExViewModel, ToolsSysExPage>();
        Configure<ToolsTestViewModel, ToolsTestPage>();
        Configure<TroubleshootingViewModel, TroubleshootingPage>();

        Configure<GlobalMidiSettingsViewModel, GlobalMidiSettingsPage>();

        Configure<FirstRunExperienceViewModel, FirstRunExperiencePage>();


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
