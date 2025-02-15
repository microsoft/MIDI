using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Runtime.InteropServices.WindowsRuntime;
using Windows.Foundation;
using Windows.Foundation.Collections;
using Microsoft.UI.Xaml;
using Microsoft.UI.Xaml.Controls;
using Microsoft.UI.Xaml.Controls.Primitives;
using Microsoft.UI.Xaml.Data;
using Microsoft.UI.Xaml.Input;
using Microsoft.UI.Xaml.Media;
using Microsoft.UI.Xaml.Navigation;
using Microsoft.Midi.Settings.Contracts.Services;
using Microsoft.Midi.Settings.Controls;
using Microsoft.Midi.Settings.ViewModels;

// To learn more about WinUI, the WinUI project structure,
// and more about our project templates, see: http://aka.ms/winui-project-info.

namespace Microsoft.Midi.Settings.Views
{
    /// <summary>
    /// An empty page that can be used on its own or navigated to within a Frame.
    /// </summary>
    public sealed partial class EndpointsLoopPage : Page
    {
        private ILoggingService _loggingService;


        public EndpointsLoopViewModel ViewModel
        {
            get;
        }


        public EndpointsLoopPage()
        {
            ViewModel = App.GetService<EndpointsLoopViewModel>();
            _loggingService = App.GetService<ILoggingService>();

            Loaded += DevicesPage_Loaded;

            InitializeComponent();
        }

        private void DevicesPage_Loaded(object sender, RoutedEventArgs e)
        {
            ViewModel.DispatcherQueue = this.DispatcherQueue;

            ViewModel.RefreshDeviceCollection();
        }


        // work around WinUI binding bug
        private void MidiEndpointDeviceListItemControl_Loaded(object sender, RoutedEventArgs e)
        {
            ((MidiEndpointDeviceListItemControl)sender).ViewDeviceDetailsCommand = ViewModel.ViewDeviceDetailsCommand;
        }

        private async void CreateNewLoopbackPair_Click(object sender, RoutedEventArgs e)
        {
            var result = await Dialog_CreateLoopbackEndpoints.ShowAsync();
        }
    }
}

