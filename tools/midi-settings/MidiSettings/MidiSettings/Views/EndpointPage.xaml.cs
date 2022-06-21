using CommunityToolkit.WinUI.UI.Controls;

using Microsoft.UI.Xaml.Controls;

using MidiSettings.ViewModels;

namespace MidiSettings.Views;

public sealed partial class EndpointPage : Page
{
    public EndpointViewModel ViewModel
    {
        get;
    }

    public EndpointPage()
    {
        ViewModel = App.GetService<EndpointViewModel>();
        InitializeComponent();
    }

    private void OnViewStateChanged(object sender, ListDetailsViewState e)
    {
        if (e == ListDetailsViewState.Both)
        {
            ViewModel.EnsureItemSelected();
        }
    }
}
