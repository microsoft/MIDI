// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

using Microsoft.Midi.Settings.Contracts.Services;
using Microsoft.Midi.Settings.Helpers;
using Microsoft.Midi.Settings.Services;
using Microsoft.UI.Xaml.Media;
using WinUIEx;

namespace Microsoft.Midi.Settings;

public sealed partial class MainWindow// : WinUIEx.WindowEx
{
    public MainWindow()
    {
        InitializeComponent();

        AppWindow.SetIcon(@"\Assets\AppIcon.ico");
        AppWindow.TitleBar.ExtendsContentIntoTitleBar = true;
        AppWindow.TitleBar.ButtonBackgroundColor = Microsoft.UI.Colors.Transparent;

        //AppWindow.SetIcon(Path.Combine(AppContext.BaseDirectory, "Assets/DIN_Settings.png"));
        Content = null;
        Title = "AppDisplayName".GetLocalized();


        //MinWidth = 640;
        //MinHeight = 480;

        //Content = new Microsoft.UI.Xaml.Controls.Grid(); // workaround for WinAppSDK bug http://task.ms/43347736
        //this.SystemBackdrop = new MicaBackdrop();


        //var icon = Icon.FromFile(@"Assets\DIN_Settings.ico");
        //this.SetTaskBarIcon(icon);


        //this.PositionChanged += MainWindow_PositionChanged;
        //this.SizeChanged += MainWindow_SizeChanged;

        this.Closed += MainWindow_Closed;
    }

    private void MainWindow_Closed(object sender, UI.Xaml.WindowEventArgs args)
    {

        var settingsService = App.GetService<IGeneralSettingsService>();

        if (settingsService != null)
        {
            WindowRect r = new WindowRect(
                this.AppWindow.Position.X,
                this.AppWindow.Position.Y,
                this.AppWindow.Size.Width,
                this.AppWindow.Size.Height);

            settingsService.SetMainWindowPositionAndSize(r);
        }

    }

}
