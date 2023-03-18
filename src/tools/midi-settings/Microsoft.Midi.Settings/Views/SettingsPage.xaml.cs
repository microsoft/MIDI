using Microsoft.Midi.Settings.ViewModels;
using Microsoft.UI.Xaml.Controls;

namespace Microsoft.Midi.Settings.Views;

// TODO: Set the URL for your privacy policy by updating SettingsPage_PrivacyTermsLink.NavigateUri in Resources.resw.
public sealed partial class SettingsPage : Page
{
    public SettingsViewModel ViewModel
    {
        get;
    }

    public SettingsPage()
    {
        ViewModel = App.GetService<SettingsViewModel>();

        InitializeComponent();
    }

    private void WindowsDeveloperSettingsHyperlinkButton_Click(object sender, UI.Xaml.RoutedEventArgs e)
    {
        var psi = new System.Diagnostics.ProcessStartInfo();

        psi.FileName = "ms-settings:developers";
        psi.UseShellExecute = true;

        System.Diagnostics.Process.Start(psi);
    }
}
