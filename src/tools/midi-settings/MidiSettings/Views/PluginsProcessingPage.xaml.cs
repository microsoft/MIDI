using CommunityToolkit.WinUI.UI.Controls;

using Microsoft.UI.Xaml.Controls;

using MidiSettings.ViewModels;

namespace MidiSettings.Views;

public sealed partial class PluginsProcessingPage : Page
{
    public PluginsProcessingViewModel ViewModel
    {
        get;
    }

    public PluginsProcessingPage()
    {
        ViewModel = App.GetService<PluginsProcessingViewModel>();
        InitializeComponent();
    }

}
