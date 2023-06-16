using Microsoft.Midi.Settings.ViewModels;
using Microsoft.UI.Xaml.Controls;

namespace Microsoft.Midi.Settings.Views;

public sealed partial class ContentGridPage : Page
{
    public ContentGridViewModel ViewModel
    {
        get;
    }

    public ContentGridPage()
    {
        ViewModel = App.GetService<ContentGridViewModel>();
        InitializeComponent();
    }
}
