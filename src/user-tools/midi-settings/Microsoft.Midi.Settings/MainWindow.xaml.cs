using Microsoft.Midi.Settings.Contracts.Services;
using Microsoft.Midi.Settings.Helpers;
using Microsoft.Midi.Settings.Services;
using Microsoft.UI.Xaml.Media;
using WinUIEx;

namespace Microsoft.Midi.Settings;

public sealed partial class MainWindow : WinUIEx.WindowEx
{
    public MainWindow()
    {
        InitializeComponent();

        //AppWindow.SetIcon(Path.Combine(AppContext.BaseDirectory, "Assets/DIN_Settings.png"));
        Content = null;
        Title = "AppDisplayName".GetLocalized();

        PersistenceId = "MidiSettings_MainWindow";
        MinWidth = 640;
        MinHeight = 480;

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

    //private void MainWindow_SizeChanged(object sender, UI.Xaml.WindowSizeChangedEventArgs args)
    //{
    //    // todo: keep the value in app settings and save when app is closed
    //}

    //private void MainWindow_PositionChanged(object? sender, global::Windows.Graphics.PointInt32 e)
    //{
    //    // todo: keep the value in app settings and save when app is closed
    //}
}
