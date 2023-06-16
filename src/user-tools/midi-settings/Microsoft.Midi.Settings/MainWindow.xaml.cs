using Microsoft.Midi.Settings.Helpers;
using Microsoft.UI.Xaml.Media;

namespace Microsoft.Midi.Settings;

public sealed partial class MainWindow : WindowEx
{
    public MainWindow()
    {
        InitializeComponent();

        AppWindow.SetIcon(Path.Combine(AppContext.BaseDirectory, "Assets/DIN.png"));
        Content = null;
        Title = "AppDisplayName".GetLocalized();

        //Content = new Microsoft.UI.Xaml.Controls.Grid(); // workaround for WinAppSDK bug http://task.ms/43347736
        //this.SystemBackdrop = new MicaBackdrop();

    }
}
