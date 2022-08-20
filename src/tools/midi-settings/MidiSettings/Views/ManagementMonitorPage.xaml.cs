using CommunityToolkit.WinUI.UI.Controls;

using Microsoft.UI.Xaml.Controls;

using MidiSettings.ViewModels;

namespace MidiSettings.Views;

public sealed partial class ManagementMonitorPage : Page
{
    public ManagementMonitorViewModel ViewModel
    {
        get;
    }

    public ManagementMonitorPage()
    {
        ViewModel = App.GetService<ManagementMonitorViewModel>();
        InitializeComponent();
    }

}
