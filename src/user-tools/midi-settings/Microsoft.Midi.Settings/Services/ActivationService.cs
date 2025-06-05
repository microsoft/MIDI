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
using Windows.Devices.Display;
using Windows.Devices.Enumeration;
using Windows.Win32.UI.Shell;

namespace Microsoft.Midi.Settings.Services;

public class ActivationService : IActivationService
{
    private readonly ActivationHandler<LaunchActivatedEventArgs> _defaultHandler;
    private readonly IEnumerable<IActivationHandler> _activationHandlers;
    private readonly IThemeSelectorService _themeSelectorService;
    private readonly IGeneralSettingsService _generalSettingsService;
    private UIElement? _shell = null;

    public ActivationService(ActivationHandler<LaunchActivatedEventArgs> defaultHandler, IEnumerable<IActivationHandler> activationHandlers, IThemeSelectorService themeSelectorService, IGeneralSettingsService generalSettingsService)
    {
        _defaultHandler = defaultHandler;
        _activationHandlers = activationHandlers;
        _themeSelectorService = themeSelectorService;
        _generalSettingsService = generalSettingsService;
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


    public async Task ActivateAsync(object activationArgs)
    {
        // Execute tasks before activation.
        await InitializeAsync();

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

        // Activate the MainWindow.
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
