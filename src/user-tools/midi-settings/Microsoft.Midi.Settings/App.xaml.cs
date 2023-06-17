using Microsoft.Extensions.DependencyInjection;
using Microsoft.Extensions.Hosting;
using Microsoft.Midi.Settings.Activation;
using Microsoft.Midi.Settings.Contracts.Services;
using Microsoft.Midi.Settings.Core.Contracts.Services;
using Microsoft.Midi.Settings.Core.Services;
using Microsoft.Midi.Settings.Helpers;
using Microsoft.Midi.Settings.Models;
using Microsoft.Midi.Settings.Services;
using Microsoft.Midi.Settings.ViewModels;
using Microsoft.Midi.Settings.Views;
using Microsoft.UI.Xaml;

namespace Microsoft.Midi.Settings;

// To learn more about WinUI 3, see https://docs.microsoft.com/windows/apps/winui/winui3/.
public partial class App : Application
{
    // The .NET Generic Host provides dependency injection, configuration, logging, and other services.
    // https://docs.microsoft.com/dotnet/core/extensions/generic-host
    // https://docs.microsoft.com/dotnet/core/extensions/dependency-injection
    // https://docs.microsoft.com/dotnet/core/extensions/configuration
    // https://docs.microsoft.com/dotnet/core/extensions/logging
    public IHost Host
    {
        get;
    }

    public static T GetService<T>()
        where T : class
    {
        if ((App.Current as App)!.Host.Services.GetService(typeof(T)) is not T service)
        {
            throw new ArgumentException($"{typeof(T)} needs to be registered in ConfigureServices within App.xaml.cs.");
        }

        return service;
    }

    public static WindowEx MainWindow { get; } = new MainWindow();

    public App()
    {
        InitializeComponent();

        Host = Microsoft.Extensions.Hosting.Host.
        CreateDefaultBuilder().
        UseContentRoot(AppContext.BaseDirectory).
        ConfigureServices((context, services) =>
        {
            // Default Activation Handler
            services.AddTransient<ActivationHandler<LaunchActivatedEventArgs>, DefaultActivationHandler>();

            // Other Activation Handlers

            // Services
            services.AddSingleton<ILocalSettingsService, LocalSettingsService>();
            services.AddSingleton<IGeneralSettingsService, GeneralSettingsService>();

            services.AddSingleton<IThemeSelectorService, ThemeSelectorService>();
            services.AddTransient<INavigationViewService, NavigationViewService>();

            services.AddSingleton<IActivationService, ActivationService>();
            services.AddSingleton<IPageService, PageService>();
            services.AddSingleton<INavigationService, NavigationService>();

            // Core Services
            services.AddSingleton<ISampleDataService, SampleDataService>();
            services.AddSingleton<IFileService, FileService>();

            // Views and ViewModels
            services.AddTransient<Midi1DevicesViewModel>();
            services.AddTransient<Midi1DevicesPage>();
            services.AddTransient<SettingsViewModel>();
            services.AddTransient<SettingsPage>();

            services.AddTransient<ContentGridDetailViewModel>();
            services.AddTransient<ContentGridDetailPage>();
            services.AddTransient<ContentGridViewModel>();
            services.AddTransient<ContentGridPage>();

            services.AddTransient<MainViewModel>();
            services.AddTransient<MainPage>();
            services.AddTransient<ShellPage>();
            services.AddTransient<ShellViewModel>();

            services.AddTransient<ForDevelopersPage>();
            services.AddTransient<ForDevelopersViewModel>();

            services.AddTransient<DevicesPage>();
            services.AddTransient<DevicesViewModel>();

            services.AddTransient<ToolsMonitorPage>();
            services.AddTransient<ToolsMonitorViewModel>();

            services.AddTransient<ManagementSessionsPage>();
            services.AddTransient<ManagementSessionsViewModel>();

            services.AddTransient<PluginsProcessingPage>();
            services.AddTransient<PluginsProcessingViewModel>();

            services.AddTransient<PluginsTransportPage>();
            services.AddTransient<PluginsTransportViewModel>();

            services.AddTransient<SetupPage>();
            services.AddTransient<SetupViewModel>();

            services.AddTransient<ToolsConsolePage>();
            services.AddTransient<ToolsConsoleViewModel>();

            services.AddTransient<ToolsSysExPage>();
            services.AddTransient<ToolsSysExViewModel>();

            services.AddTransient<ToolsTestPage>();
            services.AddTransient<ToolsTestViewModel>();

            services.AddTransient<TroubleshootingPage>();
            services.AddTransient<TroubleshootingViewModel>();


            // Configuration
            services.Configure<LocalSettingsOptions>(context.Configuration.GetSection(nameof(LocalSettingsOptions)));
        }).
        Build();

        UnhandledException += App_UnhandledException;
    }

    private void App_UnhandledException(object sender, Microsoft.UI.Xaml.UnhandledExceptionEventArgs e)
    {
        // TODO: Log and handle exceptions as appropriate.
        // https://docs.microsoft.com/windows/windows-app-sdk/api/winrt/microsoft.ui.xaml.application.unhandledexception.
    }

    protected async override void OnLaunched(LaunchActivatedEventArgs args)
    {
        base.OnLaunched(args);

        await App.GetService<IActivationService>().ActivateAsync(args);
    }
}
