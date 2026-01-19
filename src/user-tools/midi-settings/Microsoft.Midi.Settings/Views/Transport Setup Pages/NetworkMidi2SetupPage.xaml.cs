// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

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
using Windows.UI.Popups;
using WinRT.Interop;

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

            if (result == ContentDialogResult.Primary)
            {
                CreateManualConnectionButton.IsEnabled = false;
                if (ViewModel.CreateClientDirect())
                {
                    // success

                    var dialog = new ContentDialog()
                    {
                        XamlRoot = this.XamlRoot,
                        Title = "Network Client Created",
                        Content = "The new client has been created and also saved to the service configuration file. When the specified device appears on the network, and if it accepts the connection request, the MIDI endpoint will be created.",
                        CloseButtonText = "OK"
                    };

                    await dialog.ShowAsync();
                }
                else
                {
                    var dialog = new ContentDialog()
                    {
                        XamlRoot = this.XamlRoot,
                        Title = "Failed to create Network Client",
                        Content = "The network client could not be created.",
                        CloseButtonText = "OK"
                    };

                    await dialog.ShowAsync();

                    System.Diagnostics.Debug.WriteLine("Failed to create new network client.");
                }
                CreateManualConnectionButton.IsEnabled = true;
            }

        }

        private async void CreateClientFromHostList_Click(object sender, RoutedEventArgs e)
        {
            if (ViewModel.CreateClientFromMdns(((Button)sender).Tag!.ToString()!))
            {
                var dialog = new ContentDialog()
                {
                    XamlRoot = this.XamlRoot,
                    Title = "Connected",
                    Content = "The MIDI Service will connect to this host and create the MIDI endpoints shortly.",
                    CloseButtonText = "OK"
                };

                await dialog.ShowAsync();
            }

            // TODO: Update status of the host to show there's a connection

                // TODO: Show a disconnect button
        }


        private async void CreateHost_Click(object sender, RoutedEventArgs e)
        {
            UpdateDialogSize(Dialog_CreateHost);

            var result = await Dialog_CreateHost.ShowAsync();

            if (result == ContentDialogResult.Primary)
            {
                CreateHostButton.IsEnabled = false;
                if (ViewModel.CreateHost())
                {
                    // success

                    var dialog = new ContentDialog()
                    {
                        XamlRoot = this.XamlRoot,
                        Title = "Network Host Created",
                        Content = "The new host has been created and also saved to the service configuration file. It will take a moment to be visible on the network. If you do not see it after a minute, check your firewall settings for midisrv.exe.",
                        CloseButtonText="OK"
                    };

                    await dialog.ShowAsync();
                }
                else
                {
                    var dialog = new ContentDialog()
                    {
                        XamlRoot = this.XamlRoot,
                        Title = "Failed to create Network Host",
                        Content = "The network host could not be created. Double-check that the data is valid and the host name has not already been used.",
                        CloseButtonText = "OK"
                    };

                    await dialog.ShowAsync();

                    System.Diagnostics.Debug.WriteLine("Failed to create new network host.");
                }
                CreateHostButton.IsEnabled = true;
            }
        }

        private async void AdvancedSettings_Click(object sender, RoutedEventArgs e)
        {
            UpdateDialogSize(Dialog_AdvancedSettings);

            var result = await Dialog_AdvancedSettings.ShowAsync();
        }

    }
}
