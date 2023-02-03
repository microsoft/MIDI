using Microsoft.Midi.Settings.Helpers;

namespace Microsoft.Midi.Settings;

public sealed partial class MainWindow : WindowEx
{
    public MainWindow()
    {
        InitializeComponent();

        AppWindow.SetIcon(Path.Combine(AppContext.BaseDirectory, "Assets/DIN.png"));
        Content = null;
        Title = "AppDisplayName".GetLocalized();
    }
}
