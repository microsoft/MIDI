using CommunityToolkit.WinUI.UI.Controls;

using Microsoft.UI.Xaml.Controls;

using MidiSettings.ViewModels;

namespace MidiSettings.Views;

public sealed partial class ToolsMonitorPage : Page
{
    public ToolsMonitorViewModel ViewModel
    {
        get;
    }

    public ToolsMonitorPage()
    {
        ViewModel = App.GetService<ToolsMonitorViewModel>();
        InitializeComponent();
    }

}
