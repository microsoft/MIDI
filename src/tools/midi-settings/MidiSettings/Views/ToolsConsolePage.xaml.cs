using CommunityToolkit.WinUI.UI.Controls;

using Microsoft.UI.Xaml.Controls;

using MidiSettings.ViewModels;

namespace MidiSettings.Views;

public sealed partial class ToolsConsolePage : Page
{
    public ToolsConsoleViewModel ViewModel
    {
        get;
    }

    public ToolsConsolePage()
    {
        ViewModel = App.GetService<ToolsConsoleViewModel>();
        InitializeComponent();
    }

}
