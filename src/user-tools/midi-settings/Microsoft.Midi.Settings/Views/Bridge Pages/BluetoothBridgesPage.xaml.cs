// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

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
    public sealed partial class BluetoothBridgesPage : Page
    {
        private ILoggingService _loggingService;


        public BluetoothBridgeViewModel ViewModel
        {
            get;
        }


        public BluetoothBridgesPage()
        {
            ViewModel = App.GetService<BluetoothBridgeViewModel>();
            _loggingService = App.GetService<ILoggingService>();

            try
            {
                InitializeComponent();
            }
            catch (Exception ex)
            {
                _loggingService.LogError("Error initializing page", ex);
            }
        }

        bool m_showCreateDialog = false;

        //private void ViewModel_ShowCreateDialog(object? sender, EventArgs e)
        //{
        //    m_showCreateDialog = true;
        //}


        private async void CreateNewBridge_Click(object sender, RoutedEventArgs e)
        {
            Dialog_CreateBridge.Resources["ContentDialogMaxHeight"] = Math.Max(1200.0, this.ActualHeight);

            // TODO: Read this from local settings

            var result = await Dialog_CreateBridge.ShowAsync();
        }

    }
}

