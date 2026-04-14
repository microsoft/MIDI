// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

using Microsoft.Midi.Settings.Activation;
using Microsoft.Midi.Settings.Contracts.Services;
using Microsoft.Midi.Settings.Views;
using Microsoft.UI.Xaml;
using Microsoft.UI.Xaml.Controls;


namespace Microsoft.Midi.Settings.Services;

public class ActivationService : IActivationService
{
    private readonly ActivationHandler<LaunchActivatedEventArgs> _defaultHandler;
    private readonly IEnumerable<IActivationHandler> _activationHandlers;
    private readonly IThemeSelectorService _themeSelectorService;
    private readonly IGeneralSettingsService _generalSettingsService;
    private readonly IMidiSdkService _sdkService;
    private readonly IMidiEndpointEnumerationService _enumerationService;
    private readonly IMessageBoxService _messageBoxService;

    private UIElement? _shell = null;

    public ActivationService(
        ActivationHandler<LaunchActivatedEventArgs> defaultHandler, 
        IEnumerable<IActivationHandler> activationHandlers, 
        IThemeSelectorService themeSelectorService, 
        IGeneralSettingsService generalSettingsService,
        IMidiSdkService sdkService,
        IMidiEndpointEnumerationService enumerationService,
        IMessageBoxService messageBoxService)
    {
        _sdkService = sdkService;
        _defaultHandler = defaultHandler;
        _activationHandlers = activationHandlers;
        _themeSelectorService = themeSelectorService;
        _generalSettingsService = generalSettingsService;
        _enumerationService = enumerationService;
        _messageBoxService = messageBoxService;
    }

    private void PositionMainWindow()
    {
        App.GetService<ILoggingService>().LogInfo("Enter");

        bool provided = false;

        var extents = _generalSettingsService.GetMainWindowPositionAndSize();

        if (extents == null)
        {
            extents = new WindowRect();
        }
        else
        {
            provided = true;
        }

        // arbitrary minimum size to ensure we're not just a floating title bar
        if (extents.Width < 200 || extents.Height < 200)
        {
            extents.Width = 200;
            extents.Height = 200;
        }

        if (extents.Left < 0)
        {
            extents.Left = 0;
        }

        if (extents.Top < 0)
        {
            extents.Top = 0;
        }

        //App.MainWindow.AppWindow.Resize(new global::Windows.Graphics.SizeInt32(extents.Width, extents.Height));


        // TODO: Need to check to see if we're off-screen

        //var hwnd = App.MainWindow.GetWindowHandle();
        //var hmon = global::Windows.Win32.MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST);

        if (!provided)
        {
            App.MainWindow.CenterOnScreen();
        }
        else
        {

            var position = new global::Windows.Graphics.PointInt32(extents.Left, extents.Top);
            var size = new global::Windows.Graphics.SizeInt32(extents.Width, extents.Height);

            App.MainWindow.AppWindow.Move(position);
            App.MainWindow.AppWindow.Resize(size);

            App.MainWindow.AppWindow.MoveInZOrderAtTop();
        }

        App.GetService<ILoggingService>().LogInfo("Exit");
    }


    public async Task<bool> ActivateAsync(object activationArgs)
    {
        App.GetService<ILoggingService>().LogInfo("Enter");

        var splash = new Views.Core_Pages.LoadingPage();
        App.MainWindow.Content = splash;

        // restore window size from last close
        App.GetService<ILoggingService>().LogInfo("Positioning main window");
        PositionMainWindow();

        App.MainWindow.Show();

        // Execute tasks before activation.
        await InitializeAsync();

        WindowsDeveloperModeHelper.Refresh();


        //var serviceController = MidiServiceHelper.GetServiceController();

        //if (serviceController == null)
        //{
        //    App.GetService<ILoggingService>().LogError("Unable to get service controller");
        //    return false;
        //}

        //if (!MidiServiceHelper.ServiceIsReallyRunning(serviceController))
        //{
        //    // we use the main Window's dispatcher, since AppWindow doesn't surface this
        //    App.Splash.DispatcherQueue.TryEnqueue(DispatcherQueuePriority.High, () =>
        //    {
        //        App.Splash.Show();
        //        App.Splash.Activate();
        //    });
        //}

        //// Check for feature activation
        //bool featureEnabled = true;
        //featureEnabled = await MidiFeatureDetectionHelper.IsWindowsMidiServicesFeatureEnabledAsync();
        //if (!featureEnabled)
        //{
        //    App.GetService<ILoggingService>().LogInfo("Windows MIDI Services Feature has not been enabled, or there is a registry or service problem.");

        //    _messageBoxService.ShowError("Error_MidiServicesFeatureNotEnabled".GetLocalized(), "Error_WindowsMIDIServicesNotEnabledMessageBoxTitle".GetLocalized());

        //    return false;
        //}

        if (_sdkService == null)
        {
            App.GetService<ILoggingService>().LogError("SDK Service is null");
            return false;
        }

        // run as a task so the splash screen gets a chance to
        // actually show up if it has been displayed.
        await Task.Run(() =>
        {
            if (!_sdkService.InitializeSdk())
            {
                App.GetService<ILoggingService>().LogError("Unable to initialize SDK");

                _messageBoxService.ShowError("Error_UnableToInitializeMidiRuntime".GetLocalized(), "Error_StartupMessageBoxTitle".GetLocalized());

                return;
            }
            else if (!_sdkService.InitializeService())
            {
                App.GetService<ILoggingService>().LogError("Unable to initialize Service");

                _messageBoxService.ShowError("Error_UnableToStartMidiService".GetLocalized(), "Error_StartupMessageBoxTitle".GetLocalized());

           //     return;
            }
            else
            {
                _enumerationService.Start();
            }
        });

        if (!_sdkService.IsRuntimeInitialized /* || !_sdkService.IsServiceInitialized */)
        {
            App.GetService<ILoggingService>().LogError("Runtime and/or service are not initialized");
            return false;
        }

        //App.MainWindow = (MainWindow)e!;


        // Set the MainWindow Content.
        App.GetService<ILoggingService>().LogInfo("Setting main window content");
        //if (App.MainWindow.Content == null)
        {
            _shell = App.GetService<ShellPage>();
            App.MainWindow.Content = _shell ?? new Frame();
        }

        // Handle activation via ActivationHandlers.
        await HandleActivationAsync(activationArgs);

        App.GetService<ILoggingService>().LogInfo("Closing splash screen");
        App.Splash.Close();

        //if (App.Splash != null && App.Splash.Visible)
        //{
        //    App.Splash.DispatcherQueue.TryEnqueue(DispatcherQueuePriority.High, () =>
        //    {
        //        App.Splash.Close();
        //    });
        //}
        //App.Splash.Dispatcher.ProcessEvents(CoreProcessEventsOption.ProcessAllIfPresent);

        // Activate the MainWindow.
        //App.MainWindow.Show();
        App.MainWindow.Activate();

        // Execute tasks after activation.
        await StartupAsync();

        App.GetService<ILoggingService>().LogInfo("Exit");

        return true;
    }

    private async Task HandleActivationAsync(object activationArgs)
    {
        try
        {
            App.GetService<ILoggingService>().LogInfo("Enter");

            var activationHandler = _activationHandlers.FirstOrDefault(h => h.CanHandle(activationArgs));

            if (activationHandler != null)
            {
                await activationHandler.HandleAsync(activationArgs);
            }

            if (_defaultHandler.CanHandle(activationArgs))
            {
                await _defaultHandler.HandleAsync(activationArgs);
            }

            App.GetService<ILoggingService>().LogInfo("Exit");
        }
        catch (Exception ex)
        {
            App.GetService<ILoggingService>().LogError("Exception during HandleActivationAsync", ex);
        }
    }

    private async Task InitializeAsync()
    {
        try
        {
            App.GetService<ILoggingService>().LogInfo("Enter");

            await _themeSelectorService.InitializeAsync().ConfigureAwait(false);
            await _generalSettingsService.InitializeAsync().ConfigureAwait(false);
            await Task.CompletedTask;

            App.GetService<ILoggingService>().LogInfo("Exit");
        }
        catch (Exception ex)
        {
            App.GetService<ILoggingService>().LogError("Exception during InitializeAsync", ex);
        }
    }

    private async Task StartupAsync()
    {
        try
        {
            App.GetService<ILoggingService>().LogInfo("Enter");

            await _themeSelectorService.SetRequestedThemeAsync();
            await Task.CompletedTask;

            App.GetService<ILoggingService>().LogInfo("Exit");
        }
        catch (Exception ex)
        {
            App.GetService<ILoggingService>().LogError("Exception during StartupAsync", ex);
        }

    }
}
