//using System;
using Microsoft.Extensions.DependencyInjection;
using Microsoft.Extensions.Hosting;
using Microsoft.UI.Xaml;

using MidiSettings.Activation;
using MidiSettings.Contracts.Services;
using MidiSettings.Core.Contracts.Services;
using MidiSettings.Core.Services;
using MidiSettings.Helpers;
using MidiSettings.Models;
using MidiSettings.Services;
using MidiSettings.ViewModels;
using MidiSettings.Views;

// To learn more about WinUI3, see: https://docs.microsoft.com/windows/apps/winui/winui3/.
namespace MidiSettings;

public partial class App : Application
{
    // The .NET Generic Host provides dependency injection, configuration, logging, and other services.
    // https://docs.microsoft.com/dotnet/core/extensions/generic-host
    // https://docs.microsoft.com/dotnet/core/extensions/dependency-injection
    // https://docs.microsoft.com/dotnet/core/extensions/configuration
    // https://docs.microsoft.com/dotnet/core/extensions/logging
    private static readonly IHost _host = Host
        .CreateDefaultBuilder()
        .ConfigureServices((context, services) =>
        {
            // Default Activation Handler
            services.AddTransient<ActivationHandler<LaunchActivatedEventArgs>, DefaultActivationHandler>();

            // Other Activation Handlers

            // Services
            services.AddSingleton<ILocalSettingsService, LocalSettingsServicePackaged>();
            services.AddSingleton<IThemeSelectorService, ThemeSelectorService>();
            services.AddTransient<INavigationViewService, NavigationViewService>();

            services.AddSingleton<IActivationService, ActivationService>();
            services.AddSingleton<IPageService, PageService>();
            services.AddSingleton<INavigationService, NavigationService>();

            // Core Services
            services.AddSingleton<ISampleDataService, SampleDataService>();
            services.AddSingleton<IFileService, FileService>();

            // Views and ViewModels
            services.AddTransient<DevicesViewModel>();
            services.AddTransient<DevicesPage>();

            services.AddTransient<PluginsDeviceViewModel>();
            services.AddTransient<PluginsDevicePage>();

            services.AddTransient<PluginsProcessingViewModel>();
            services.AddTransient<PluginsProcessingPage>();

            services.AddTransient<ToolsConsoleViewModel>();
            services.AddTransient<ToolsConsolePage>();

            services.AddTransient<ManagementMonitorViewModel>();
            services.AddTransient<ManagementMonitorPage>();

            services.AddTransient<ManagementSessionsViewModel>();
            services.AddTransient<ManagementSessionsPage>();

            services.AddTransient<ToolsSysExViewModel>();
            services.AddTransient<ToolsSysExPage>();

            services.AddTransient<ToolsTestViewModel>();
            services.AddTransient<ToolsTestPage>();


            services.AddTransient<SettingsViewModel>();
            services.AddTransient<SettingsPage>();
            
            services.AddTransient<ShellPage>();
            services.AddTransient<ShellViewModel>();



            // Configuration
            services.Configure<LocalSettingsOptions>(context.Configuration.GetSection(nameof(LocalSettingsOptions)));
        })
        .Build();

    public static T GetService<T>()
        where T : class
    {
        return _host.Services.GetService(typeof(T)) as T;
    }

    public static Window MainWindow { get; set; } = new Window() { Title = "AppDisplayName".GetLocalized() };
    //public static WinUIEx.WindowEx MainWindow { get; set; } = new WinUIEx.WindowEx() { Title = "AppDisplayName".GetLocalized() };

    public App()
    {
        InitializeComponent();
        UnhandledException += App_UnhandledException;
    }

    private void App_UnhandledException(object sender, UnhandledExceptionEventArgs e)
    {
        // TODO: Log and handle exceptions as appropriate.
        // For more details, see https://docs.microsoft.com/windows/winui/api/microsoft.ui.xaml.unhandledexceptioneventargs.
    }

    protected async override void OnLaunched(LaunchActivatedEventArgs args)
    {
        base.OnLaunched(args);

        SetWindowSize();

        var activationService = App.GetService<IActivationService>();
        await activationService.ActivateAsync(args);
    }

    // eventually need to remove this and make sure that app opens at last size and location
    // But right now, during development, the default size is super annoying, and resets
    // with every launch
    private void SetWindowSize()
    {
        System.IntPtr hWnd = WinRT.Interop.WindowNative.GetWindowHandle(MainWindow);
        var windowId = Microsoft.UI.Win32Interop.GetWindowIdFromWindow(hWnd);
        var appWindow = Microsoft.UI.Windowing.AppWindow.GetFromWindowId(windowId);

        appWindow.Resize(new Windows.Graphics.SizeInt32 { Width = 1600, Height = 950 });
    }

}
