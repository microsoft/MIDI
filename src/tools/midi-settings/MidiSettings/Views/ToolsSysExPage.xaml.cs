using CommunityToolkit.WinUI.UI.Controls;

using Microsoft.UI.Xaml.Controls;

using MidiSettings.ViewModels;

namespace MidiSettings.Views;

public sealed partial class ToolsSysExPage : Page
{
    public ToolsSysExViewModel ViewModel
    {
        get;
    }

    public ToolsSysExPage()
    {
        ViewModel = App.GetService<ToolsSysExViewModel>();
        InitializeComponent();
    }

}
