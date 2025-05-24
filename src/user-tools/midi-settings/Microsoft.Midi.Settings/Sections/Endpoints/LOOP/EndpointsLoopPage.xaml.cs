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
using Microsoft.Midi.Settings.Contracts.ViewModels;
using Microsoft.UI.Dispatching;

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

            ViewModel.ShowCreateDialog += ViewModel_ShowCreateDialog;

            Loaded += Page_Loaded;
            Unloaded += Page_Unloaded;

            InitializeComponent();
        }

        bool m_showCreateDialog = false;

        private void ViewModel_ShowCreateDialog(object? sender, EventArgs e)
        {
            m_showCreateDialog = true;
        }


        private void Page_Unloaded(object sender, RoutedEventArgs e)
        {
            ViewModel.ShowCreateDialog -= ViewModel_ShowCreateDialog;
        }
        private void Page_Loaded(object sender, RoutedEventArgs e)
        {
            ViewModel.DispatcherQueue = this.DispatcherQueue;

            ViewModel.RefreshDeviceCollection();

            if (m_showCreateDialog)
            {
                // showing the dialog fails if it is attempted before Loaded has completed
                // as a result, we set a flag in the event from the VM and then
                // show it here, in loaded. Not my favorite code.

                DispatcherQueue.TryEnqueue(DispatcherQueuePriority.Normal, async () =>
                {
                    var result = await Dialog_CreateLoopbackEndpoints.ShowAsync();
                });

                m_showCreateDialog = false;
            }

        }

        private async void CreateNewLoopbackPair_Click(object sender, RoutedEventArgs e)
        {
            Dialog_CreateLoopbackEndpoints.Resources["ContentDialogMaxHeight"] = Math.Max(800.0, this.ActualHeight);

            var result = await Dialog_CreateLoopbackEndpoints.ShowAsync();
        }



        //public void OnNavigatedFrom()
        //{
        //}

        //public void OnNavigatedTo(object parameter)
        //{
        //    if (parameter != null && ((string)parameter).ToLower() == "create")
        //    {
        //        DispatcherQueue.TryEnqueue(DispatcherQueuePriority.Normal, async () =>
        //        {
        //            var result = await Dialog_CreateLoopbackEndpoints.ShowAsync();
        //        });
        //    }
        //}



    }
}

