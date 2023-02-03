using Microsoft.Midi.SettingsApp.ViewModels;
using Microsoft.UI.Xaml.Controls;

namespace Microsoft.Midi.SettingsApp.Views;

public sealed partial class ForDevelopersPage : Page
{
    public ForDevelopersViewModel ViewModel
    {
        get;
    }

    public ForDevelopersPage()
    {
        ViewModel = App.GetService<ForDevelopersViewModel>();
        InitializeComponent();
    }
}
