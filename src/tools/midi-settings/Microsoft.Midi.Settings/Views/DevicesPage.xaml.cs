// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License. See LICENSE in the project root for license information.

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
using Microsoft.Midi.Settings.ViewModels;
using Microsoft.Midi.Settings.SdkMock;
using Microsoft.Midi.Settings.SdkMock.TransportClientSdk;
using Microsoft.Midi.Settings.Models;

// To learn more about WinUI, the WinUI project structure,
// and more about our project templates, see: http://aka.ms/winui-project-info.

namespace Microsoft.Midi.Settings.Views
{
    /// <summary>
    /// An empty page that can be used on its own or navigated to within a Frame.
    /// </summary>
    public sealed partial class DevicesPage : Page
    {
        public DevicesViewModel ViewModel
        {
            get;
        }
        public DevicesPage()
        {
            ViewModel = App.GetService<DevicesViewModel>();
            InitializeComponent();
        }

        // this code needs to be moved to the ViewModel
        private async void TempCreateNetworkEndpoint_Click(object sender, RoutedEventArgs e)
        {
            if (!AppState.Current.HasActiveMidiSession)
            {
                AppState.Current.CreateMidiSession();
            }

            if (AppState.Current.MidiSession != null)
            {
                var transportClientPlugin = new NetworkMidiServerClientPlugin();

                var jsonParameters = transportClientPlugin.GetJsonConfiguration();

                var device = await AppState.Current.MidiSession.CreateNewUmpDeviceAsync("some_network2_class_identifier", jsonParameters);

                // binding fails with the WinRT PropertySet type, so replicating
                foreach (string key in device.UmpEndpoint.Properties.Keys)
                {
                    device.Properties.Add(key, device.UmpEndpoint.Properties[key]);
                }



                System.Diagnostics.Debug.WriteLine("Created UMP Device " + device.Id);

            }
        }
    }
}
