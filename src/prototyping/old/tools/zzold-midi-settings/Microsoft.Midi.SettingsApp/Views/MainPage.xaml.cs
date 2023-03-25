using Microsoft.Midi.SettingsApp.ViewModels;
using Microsoft.UI.Xaml.Controls;

namespace Microsoft.Midi.SettingsApp.Views;

public sealed partial class MainPage : Page
{
    public MainViewModel ViewModel
    {
        get;
    }

    public MainPage()
    {
        ViewModel = App.GetService<MainViewModel>();
        InitializeComponent();
    }
}
