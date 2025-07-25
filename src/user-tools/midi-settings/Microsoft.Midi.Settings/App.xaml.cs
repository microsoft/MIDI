﻿// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

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
using System.Runtime.InteropServices;
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

#pragma warning disable 8603
    public static T GetService<T>()
        where T : class
    {
        var curr = App.Current as App;

        if (curr != null && curr.Host != null && curr.Host.Services != null)
        {
            if (curr!.Host.Services.GetService(typeof(T)) is not T service)
            {
                throw new ArgumentException($"{typeof(T)} needs to be registered in ConfigureServices within App.xaml.cs.");
            }

            return service;
        }

        return null;
    }
#pragma warning restore 8603


    [DllImport("User32.dll", SetLastError = true, CharSet = CharSet.Auto)]
    static extern int MessageBox(
        IntPtr hWnd,
        string lpText,
        string lpCaption,
        int uType);



    // this is set from the splashscreen handling in the activation service
    public static WinUIEx.WindowEx MainWindow { get; } = new MainWindow();

    public static MidiServiceInitializationProgressWindow Splash { get; } = new MidiServiceInitializationProgressWindow();

    public App()
    {
        UnhandledException += App_UnhandledException;

        try
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
                    services.AddSingleton<IMidiTransportInfoService, MidiTransportInfoService>();
                    services.AddSingleton<IMidiConfigFileService, MidiConfigFileService>();
                    services.AddSingleton<IMidiDefaultsService, MidiDefaultsService>();
                    services.AddSingleton<IMidiServiceRegistrySettingsService, MidiServiceRegistrySettingsService>();
                    
                    services.AddSingleton<IMidiUpdateService, MidiUpdateService>();
                    services.AddSingleton<IMidiEndpointEnumerationService, MidiEndpointEnumerationService>();
                    services.AddSingleton<IMidiSdkService, MidiSdkService>();


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

                    services.AddTransient<NetworkMidi2SetupPage>();
                    services.AddTransient<NetworkMidi2SetupViewModel>();

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

                    services.AddTransient<ConfigurationsPage>();
                    services.AddTransient<ConfigurationsViewModel>();

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


            //}
            //else
            //{
            //    MessageBox(
            //        (IntPtr)0,
            //        "Error_UnableToInitializeMidiRuntime".GetLocalized(),
            //        "Error_StartupMessageBoxTitle".GetLocalized(),
            //        0);

            //    Exit();
            //}
        }
        catch (Exception ex)
        {
            MessageBox(
                (IntPtr)0,
                "Error_ExceptionThrownDuringSdkInitialization".GetLocalized() + ex.ToString(),
                "Error_StartupMessageBoxTitle".GetLocalized(),
                0);

            Exit();
        }

        //App.GetService<ILoggingService>().LogError("This is a test message");
    }

    private void App_UnhandledException(object sender, Microsoft.UI.Xaml.UnhandledExceptionEventArgs e)
    {
        // TODO: Log and handle exceptions as appropriate.
        // https://docs.microsoft.com/windows/windows-app-sdk/api/winrt/microsoft.ui.xaml.application.unhandledexception.

        try
        {
            App.GetService<ILoggingService>().LogError(e.Message);

            MessageBox((IntPtr)0, e.Message, "Unhandled Error", 0);
        }
        catch (Exception)
        {
            // just eat it. we're clearly in a messed-up state
        }

    }

    //private MidiDesktopAppSdkInitializer? _midiInitializer = null;

    //public MidiDesktopAppSdkInitializer? MidiInitializer => _midiInitializer;

    protected async override void OnLaunched(LaunchActivatedEventArgs args)
    {
        try
        {

           // InitializationProgressWindow.Completed += (s,e) => 


            base.OnLaunched(args);

            // this is not required for app startup, but is needed for other app features, so we initialize after launch
            // we do not fail if it fails because this app must run when the service is not present -- it's part of the
            // power user/developer install process.
           // AppState.Current.InitializeService();

            await App.GetService<IActivationService>().ActivateAsync(args);
        }
        catch (Exception)
        {
            // an exception here usually results from a race condition during initialization when initialization failed.
            // so just eat it here.
        }
    }


}
