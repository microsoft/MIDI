using Microsoft.Midi.Settings.Helpers;
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


        //Content = new Microsoft.UI.Xaml.Controls.Grid(); // workaround for WinAppSDK bug http://task.ms/43347736
        //this.SystemBackdrop = new MicaBackdrop();


        //var icon = Icon.FromFile(@"Assets\DIN_Settings.ico");
        //this.SetTaskBarIcon(icon);



        // todo: save and restore these settings. If packaged, it comes for free once you set the PersistenceId.

        this.CenterOnScreen(1600, 1100);

    }
}
