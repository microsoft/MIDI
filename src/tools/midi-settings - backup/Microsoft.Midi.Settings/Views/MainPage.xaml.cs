using Microsoft.Midi.Settings.ViewModels;
using Microsoft.UI.Xaml.Controls;

namespace Microsoft.Midi.Settings.Views;

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
