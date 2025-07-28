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
using Microsoft.Midi.Settings.ViewModels;
using Microsoft.Midi.Settings.Contracts.Services;
using Microsoft.Midi.Settings.Services;

// To learn more about WinUI, the WinUI project structure,
// and more about our project templates, see: http://aka.ms/winui-project-info.

namespace Microsoft.Midi.Settings.Views
{
    /// <summary>
    /// An empty page that can be used on its own or navigated to within a Frame.
    /// </summary>
    public sealed partial class HomePage : Page
    {
        private ILoggingService _loggingService;


        public HomeViewModel ViewModel
        {
            get;
        }

        public HomePage()
        {
            _loggingService = App.GetService<ILoggingService>();

            ViewModel = App.GetService<HomeViewModel>();

            ViewModel.UpdateFailed += ViewModel_UpdateFailed;

            InitializeComponent();
        }

        private void ViewModel_UpdateFailed(object? sender, string e)
        {
            Dialog_DownloadingUpdate_Progress.Visibility = Visibility.Collapsed;
            Dialog_DownloadingUpdate_MainText.Text = "Download Failed...";               // TODO: Bind this to VM and have it set from a resource

            Dialog_DownloadingUpdate.PrimaryButtonText = "Close";
            Dialog_DownloadingUpdate.IsPrimaryButtonEnabled = true;
        }

#pragma warning disable 4014
        private void UpdateSdkRuntime_Click(object sender, RoutedEventArgs e)
        {
            // we show the dialog but do not wait on it
            Dialog_DownloadingUpdate.ShowAsync();

            ViewModel.StartSdkUpdate();
        }
#pragma warning restore 4014

    }
}
