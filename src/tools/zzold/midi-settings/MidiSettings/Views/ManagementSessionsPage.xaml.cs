using CommunityToolkit.WinUI.UI.Controls;

using Microsoft.UI.Xaml.Controls;

using MidiSettings.ViewModels;

namespace MidiSettings.Views;

public sealed partial class ManagementSessionsPage : Page
{
    public ManagementSessionsViewModel ViewModel
    {
        get;
    }

    public ManagementSessionsPage()
    {
        ViewModel = App.GetService<ManagementSessionsViewModel>();
        InitializeComponent();
    }

}
