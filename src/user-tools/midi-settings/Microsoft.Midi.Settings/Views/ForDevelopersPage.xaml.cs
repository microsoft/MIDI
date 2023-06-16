using Microsoft.Midi.Settings.ViewModels;
using Microsoft.UI.Xaml.Controls;

namespace Microsoft.Midi.Settings.Views;

public sealed partial class ForDevelopersPage : Page
{
    public ForDevelopersViewModel ViewModel
    {
        get;
    }

    public ForDevelopersPage()
    {
        ViewModel = App.GetService<ForDevelopersViewModel>();
        InitializeComponent();
    }


    // Note: This page can be directly navigated to even if dev options are off. So need to check that here and 
    // throw up a "enable dev options in settings" type of message
}
