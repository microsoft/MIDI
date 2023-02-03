using CommunityToolkit.WinUI.UI.Controls;

using Microsoft.Midi.SettingsApp.ViewModels;
using Microsoft.UI.Xaml.Controls;

namespace Microsoft.Midi.SettingsApp.Views;

public sealed partial class ListDetailsPage : Page
{
    public ListDetailsViewModel ViewModel
    {
        get;
    }

    public ListDetailsPage()
    {
        ViewModel = App.GetService<ListDetailsViewModel>();
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
