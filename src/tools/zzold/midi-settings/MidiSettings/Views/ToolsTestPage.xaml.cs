using CommunityToolkit.WinUI.UI.Controls;

using Microsoft.UI.Xaml.Controls;

using MidiSettings.ViewModels;

namespace MidiSettings.Views;

public sealed partial class ToolsTestPage : Page
{
    public ToolsTestViewModel ViewModel
    {
        get;
    }

    public ToolsTestPage()
    {
        ViewModel = App.GetService<ToolsTestViewModel>();
        InitializeComponent();
    }

}
