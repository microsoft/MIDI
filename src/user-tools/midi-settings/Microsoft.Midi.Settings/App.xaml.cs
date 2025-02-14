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
using Windows.UI.Popups;

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

    public static WinUIEx.WindowEx MainWindow { get; } = new MainWindow();

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
            services.AddSingleton<ILoggingService, LoggingService>();

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

            // MIDI Services
            services.AddSingleton<IMidiConfigFileService, MidiConfigFileService>();


            // Views and ViewModels
            services.AddTransient<WinRTMidi1DevicesViewModel>();
            services.AddTransient<WinRTMidi1DevicesPage>();

            services.AddTransient<WinMMMidi1DevicesViewModel>();
            services.AddTransient<WinMMMidi1DevicesPage>();

            services.AddTransient<SettingsViewModel>();
            services.AddTransient<SettingsPage>();

            services.AddTransient<ShellPage>();
            services.AddTransient<ShellViewModel>();

            services.AddTransient<ForDevelopersPage>();
            services.AddTransient<ForDevelopersViewModel>();


            services.AddTransient<EndpointsAllPage>();
            services.AddTransient<EndpointsAllViewModel>();

            services.AddTransient<EndpointsAppPage>();
            services.AddTransient<EndpointsAppViewModel>();

            services.AddTransient<EndpointsBle10Page>();
            services.AddTransient<EndpointsBle10ViewModel>();

            services.AddTransient<EndpointsKSPage>();
            services.AddTransient<EndpointsKSViewModel>();

            services.AddTransient<EndpointsKsaPage>();
            services.AddTransient<EndpointsKsaViewModel>();

            services.AddTransient<EndpointsNet2UdpPage>();
            services.AddTransient<EndpointsNet2UdpViewModel>();

            services.AddTransient<EndpointsLoopPage>();
            services.AddTransient<EndpointsLoopViewModel>();

            services.AddTransient<EndpointsDiagPage>();
            services.AddTransient<EndpointsDiagViewModel>();


            services.AddTransient<DeviceDetailPage>();
            services.AddTransient<DeviceDetailViewModel>();


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

            services.AddTransient<HomePage>();
            services.AddTransient<HomeViewModel>();

            services.AddTransient<FirstRunExperiencePage>();
            services.AddTransient<FirstRunExperienceViewModel>();

            // Configuration
            services.Configure<LocalSettingsOptions>(context.Configuration.GetSection(nameof(LocalSettingsOptions)));
        }).
        Build();

        UnhandledException += App_UnhandledException;

        //App.GetService<ILoggingService>().LogError("This is a test message");
    }

    private void App_UnhandledException(object sender, Microsoft.UI.Xaml.UnhandledExceptionEventArgs e)
    {
        // TODO: Log and handle exceptions as appropriate.
        // https://docs.microsoft.com/windows/windows-app-sdk/api/winrt/microsoft.ui.xaml.application.unhandledexception.

        App.GetService<ILoggingService>().LogError(e.Message);
    }

    private MidiDesktopAppSdkInitializer? _midiInitializer = null;

    public MidiDesktopAppSdkInitializer? MidiInitializer => _midiInitializer;

    protected async override void OnLaunched(LaunchActivatedEventArgs args)
    {
        base.OnLaunched(args);

        // initialize the SDK
        if (AppState.Current.Initialize())
        {
            await App.GetService<IActivationService>().ActivateAsync(args);

        }
    }
}
