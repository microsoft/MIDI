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
//using Microsoft.Midi.Settings.SdkMock;
//using Microsoft.Midi.Settings.SdkMock.TransportClientSdk;
using Microsoft.Midi.Settings.Models;
using CommunityToolkit.Labs.WinUI;
using Microsoft.Midi.Settings.Contracts.Services;


// To learn more about WinUI, the WinUI project structure,
// and more about our project templates, see: http://aka.ms/winui-project-info.

namespace Microsoft.Midi.Settings.Views
{
    /// <summary>
    /// An empty page that can be used on its own or navigated to within a Frame.
    /// </summary>
    public sealed partial class DevicesPage : Page
    {
        private ILoggingService _loggingService;


        public DevicesViewModel ViewModel
        {
            get;
        }


        public DevicesPage()
        {
            ViewModel = App.GetService<DevicesViewModel>();
            _loggingService = App.GetService<ILoggingService>();

            Loaded += DevicesPage_Loaded;

            InitializeComponent();
        }

        private void DevicesPage_Loaded(object sender, RoutedEventArgs e)
        {
            ViewModel.RefreshDeviceCollection();
        }



        // work around WinUI binding bug
        private void EndpointSettingsCard_Loaded(object sender, RoutedEventArgs e)
        {
            ((SettingsCard)sender).Command = ViewModel.ViewDeviceDetailsCommand;
        }
    }
}
