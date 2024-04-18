using Microsoft.Midi.Settings.Contracts.Services;
using Microsoft.Midi.Settings.ViewModels;
using Microsoft.UI.Xaml.Controls;

namespace Microsoft.Midi.Settings.Views;

public sealed partial class Midi1DevicesPage : Page
{
    private ILoggingService _loggingService;

    public Midi1DevicesViewModel ViewModel
    {
        get;
    }

    public Midi1DevicesPage()
    {
        _loggingService = App.GetService<ILoggingService>();

        ViewModel = App.GetService<Midi1DevicesViewModel>();
        InitializeComponent();
    }
}
