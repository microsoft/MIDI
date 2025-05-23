using Microsoft.Midi.Settings.Contracts.Services;
using Microsoft.Midi.Settings.ViewModels;
using Microsoft.UI.Xaml;
using Microsoft.UI.Xaml.Controls;
using Microsoft.UI.Xaml.Controls.Primitives;
using Microsoft.UI.Xaml.Data;
using Microsoft.UI.Xaml.Input;
using Microsoft.UI.Xaml.Media;
using Microsoft.UI.Xaml.Navigation;
using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Runtime.InteropServices.WindowsRuntime;
using Windows.Foundation;
using Windows.Foundation.Collections;

// To learn more about WinUI, the WinUI project structure,
// and more about our project templates, see: http://aka.ms/winui-project-info.

namespace Microsoft.Midi.Settings.Views
{
    /// <summary>
    /// An empty page that can be used on its own or navigated to within a Frame.
    /// </summary>
    public sealed partial class NetworkMidi2SetupPage : Page
    {
        public NetworkMidi2SetupViewModel ViewModel
        {
            get;
        }

        private void Page_Loaded(object sender, RoutedEventArgs e)
        {
            ViewModel.DispatcherQueue = this.DispatcherQueue;

            ViewModel.RefreshHostsCollection();
        }


        public NetworkMidi2SetupPage()
        {
            ViewModel = App.GetService<NetworkMidi2SetupViewModel>();
            //_loggingService = App.GetService<ILoggingService>();

            Loaded += Page_Loaded;


            InitializeComponent();
        }

        private void UpdateDialogSize(ContentDialog dialog)
        {
            dialog.Resources["ContentDialogMaxHeight"] = Math.Max(800.0, this.ActualHeight);
        }

        private async void CreateManualConnection_Click(object sender, RoutedEventArgs e)
        {
            UpdateDialogSize(Dialog_CreateDirectConnection);

            var result = await Dialog_CreateDirectConnection.ShowAsync();
        }

        private async void CreateHost_Click(object sender, RoutedEventArgs e)
        {
            UpdateDialogSize(Dialog_CreateHost);

            var result = await Dialog_CreateHost.ShowAsync();
        }

        private async void AdvancedSettings_Click(object sender, RoutedEventArgs e)
        {
            UpdateDialogSize(Dialog_AdvancedSettings);

            var result = await Dialog_AdvancedSettings.ShowAsync();
        }

    }
}
