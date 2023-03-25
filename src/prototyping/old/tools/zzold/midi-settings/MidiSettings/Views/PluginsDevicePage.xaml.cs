using CommunityToolkit.WinUI.UI.Controls;

using Microsoft.UI.Xaml.Controls;

using MidiSettings.ViewModels;

namespace MidiSettings.Views;

public sealed partial class PluginsDevicePage : Page
{
    public PluginsDeviceViewModel ViewModel
    {
        get;
    }

    public PluginsDevicePage()
    {
        ViewModel = App.GetService<PluginsDeviceViewModel>();
        InitializeComponent();
    }

}
