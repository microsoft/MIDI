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
using Microsoft.UI.Dispatching;
using Microsoft.UI.Xaml;
using Microsoft.UI.Xaml.Controls;
using System.Runtime.InteropServices;


namespace Microsoft.Midi.Settings.Services;

public class ActivationService : IActivationService
{
    private readonly ActivationHandler<LaunchActivatedEventArgs> _defaultHandler;
    private readonly IEnumerable<IActivationHandler> _activationHandlers;
    private readonly IThemeSelectorService _themeSelectorService;
    private readonly IGeneralSettingsService _generalSettingsService;
    private readonly IMidiSdkService _sdkService;
    private readonly IMidiEndpointEnumerationService _enumerationService;

    private UIElement? _shell = null;

    public ActivationService(
        ActivationHandler<LaunchActivatedEventArgs> defaultHandler, 
        IEnumerable<IActivationHandler> activationHandlers, 
        IThemeSelectorService themeSelectorService, 
        IGeneralSettingsService generalSettingsService,
        IMidiSdkService sdkService,
        IMidiEndpointEnumerationService enumerationService)
    {
        _sdkService = sdkService;
        _defaultHandler = defaultHandler;
        _activationHandlers = activationHandlers;
        _themeSelectorService = themeSelectorService;
        _generalSettingsService = generalSettingsService;
        _enumerationService = enumerationService;
    }

    private void PositionMainWindow()
    {
        bool provided = false;

        var extents = _generalSettingsService.GetMainWindowPositionAndSize();

        if (extents == null)
        {
            extents = new WindowRect();
        }
        else
        {
            provided = true;

            // TODO: This code needs to know the size of the monitor, and 
            // these numbers are arbitrary
            if (extents.Width < 1200)
            {
                extents.Width = 1200;
            }

            if (extents.Height < 700)
            {
                extents.Height = 700;
            }
        }

        App.MainWindow.AppWindow.Resize(new global::Windows.Graphics.SizeInt32(extents.Width, extents.Height));


        // TODO: Need to check to see if we're off-screen

        //var hwnd = App.MainWindow.GetWindowHandle();
        //var hmon = global::Windows.Win32.MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST);

        if (!provided)
        {
            App.MainWindow.CenterOnScreen();
        }
        else
        {
            App.MainWindow.AppWindow.Move(new global::Windows.Graphics.PointInt32(extents.Left, extents.Top));
        }

    }

    [DllImport("User32.dll", SetLastError = true, CharSet = CharSet.Auto)]
    static extern int MessageBox(
    IntPtr hWnd,
    string lpText,
    string lpCaption,
    int uType);


    public async Task ActivateAsync(object activationArgs)
    {
        App.MainWindow.Hide();

        // Execute tasks before activation.
        await InitializeAsync();

        WindowsDeveloperModeHelper.Refresh();

        var serviceController = MidiServiceHelper.GetServiceController();

        if (serviceController != null)
        {
            if (!MidiServiceHelper.ServiceIsReallyRunning(serviceController))
            {
                // just initialize the service
                System.Diagnostics.Debug.WriteLine("Showing loading window");

                // we use the main Window's dispatcher, since AppWindow doesn't surface this
                App.Splash.DispatcherQueue.TryEnqueue(DispatcherQueuePriority.High, () =>
                {
                    App.Splash.Show();
                    App.Splash.Activate();
                });
            }

            // run as a task so the splash screen gets a chance to
            // actually show up if it has been displayed.
            await Task.Run(() =>
            {
                if (_sdkService == null) return;

                if (!_sdkService.InitializeSdk())
                {
                    MessageBox(
                        (IntPtr)0,
                        "Error_UnableToInitializeMidiRuntime".GetLocalized(),
                        "Error_StartupMessageBoxTitle".GetLocalized(),
                        0);

                    //    Exit();
                }
                else if (!_sdkService.InitializeService())
                {
                    MessageBox(
                        (IntPtr)0,
                        "Error_UnableToStartMidiService".GetLocalized(),
                        "Error_StartupMessageBoxTitle".GetLocalized(),
                        0);

                    //Exit();
                }
                else
                {
                    _enumerationService.Start();
                }
            });
        }

        //App.MainWindow = (MainWindow)e!;

        // restore window size from last close
        PositionMainWindow();

        // Set the MainWindow Content.
        if (App.MainWindow.Content == null)
        {
            _shell = App.GetService<ShellPage>();
            App.MainWindow.Content = _shell ?? new Frame();
        }

        // Handle activation via ActivationHandlers.
        await HandleActivationAsync(activationArgs);

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
    }

    private async Task HandleActivationAsync(object activationArgs)
    {
        var activationHandler = _activationHandlers.FirstOrDefault(h => h.CanHandle(activationArgs));

        if (activationHandler != null)
        {
            await activationHandler.HandleAsync(activationArgs);
        }

        if (_defaultHandler.CanHandle(activationArgs))
        {
            await _defaultHandler.HandleAsync(activationArgs);
        }
    }

    private async Task InitializeAsync()
    {
        await _themeSelectorService.InitializeAsync().ConfigureAwait(false);
        await _generalSettingsService.InitializeAsync().ConfigureAwait(false);
        await Task.CompletedTask;
    }

    private async Task StartupAsync()
    {
        await _themeSelectorService.SetRequestedThemeAsync();
        await Task.CompletedTask;
    }
}
