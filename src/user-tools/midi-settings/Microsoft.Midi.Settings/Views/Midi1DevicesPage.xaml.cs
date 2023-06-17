using Microsoft.Midi.Settings.ViewModels;
using Microsoft.UI.Xaml.Controls;

namespace Microsoft.Midi.Settings.Views;

public sealed partial class Midi1DevicesPage : Page
{
    public Midi1DevicesViewModel ViewModel
    {
        get;
    }

    public Midi1DevicesPage()
    {
        ViewModel = App.GetService<Midi1DevicesViewModel>();
        InitializeComponent();
    }
}
