using CommunityToolkit.WinUI.UI.Controls;

using Microsoft.UI.Xaml.Controls;

using MidiSettings.ViewModels;

namespace MidiSettings.Views;

public sealed partial class SetupPage : Page
{
    public SetupViewModel ViewModel
    {
        get;
    }

    public SetupPage()
    {
        ViewModel = App.GetService<SetupViewModel>();
        InitializeComponent();
    }

}
