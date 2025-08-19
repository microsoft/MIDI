// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

using Microsoft.Midi.Settings.ViewModels;
using Microsoft.Midi.Settings;
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
using Microsoft.UI.Windowing;
using Windows.Storage.Pickers;
using WinRT.Interop;
using Microsoft.UI.Xaml.Media.Imaging;
using Windows.Storage.Streams;
using Windows.Storage;
using Microsoft.Midi.Settings.Contracts.Services;
using Microsoft.Windows.AppLifecycle;

// To learn more about WinUI, the WinUI project structure,
// and more about our project templates, see: http://aka.ms/winui-project-info.

namespace Microsoft.Midi.Settings.Views
{
    /// <summary>
    /// An empty page that can be used on its own or navigated to within a Frame.
    /// </summary>
    public sealed partial class DeviceDetailPage : Page
    {
        private ILoggingService _loggingService;

        public DeviceDetailViewModel ViewModel
        {
            get;
        }


        public DeviceDetailPage()
        {
            ViewModel = App.GetService<DeviceDetailViewModel>();

            Loaded += DeviceDetailPage_Loaded;

            _loggingService = App.GetService<ILoggingService>();

            this.InitializeComponent();
        }

        private void DeviceDetailPage_Loaded(object sender, RoutedEventArgs e)
        {
            
        }

        private async void Button_Click(object sender, RoutedEventArgs e)
        {
            if (ViewModel.DeviceInformation == null)
            {
                return;
            }


            editUserDefinedPropertiesDialog.Width = this.Width / 3;


            // TODO: Should probably have these in the viewmodel as
            // we'll need a renderable image anyway. This code is temp

            if (!string.IsNullOrEmpty(ViewModel.CustomizedLargeImagePath))
            {
                var file = await StorageFile.GetFileFromPathAsync(ViewModel.CustomizedLargeImagePath);

                if (file != null)
                {
                    // Open a stream for the selected file.
                    // The 'using' block ensures the stream is disposed
                    // after the image is loaded.
                    using IRandomAccessStream fileStream = await file.OpenAsync(FileAccessMode.Read);
                    // Set the image source to the selected bitmap.
                    var bitmapImage = new BitmapImage();

                    bitmapImage.SetSource(fileStream);

                    UserMetadataLargeImagePreview.Source = bitmapImage;
                }
            }

            if (!string.IsNullOrEmpty(ViewModel.CustomizedSmallImagePath))
            {
                var file = await StorageFile.GetFileFromPathAsync(ViewModel.CustomizedSmallImagePath);

                if (file != null)
                {
                    // Open a stream for the selected file.
                    // The 'using' block ensures the stream is disposed
                    // after the image is loaded.
                    using IRandomAccessStream fileStream = await file.OpenAsync(FileAccessMode.Read);

                    // Set the image source to the selected bitmap.
                    var bitmapImage = new BitmapImage();

                    bitmapImage.SetSource(fileStream);

                    UserMetadataSmallImagePreview.Source = bitmapImage;
                }
            }



            ContentDialogResult result = await editUserDefinedPropertiesDialog.ShowAsync(ContentDialogPlacement.Popup);

            if (result == ContentDialogResult.Primary)
            {
                // save the changes


                // refresh the device information?

            }
        }

        // edit popup
        private async void BrowseLargeImage_Click(object sender, RoutedEventArgs e)
        {
            var filePicker = App.MainWindow.CreateOpenFilePicker();

            filePicker.SuggestedStartLocation = PickerLocationId.PicturesLibrary;
            filePicker.FileTypeFilter.Add(".png");
            filePicker.FileTypeFilter.Add(".jpg");
            filePicker.FileTypeFilter.Add(".svg");

            var file = await filePicker.PickSingleFileAsync();

            if (file != null)
            {
                // update the edit vm
                ViewModel.CustomizedLargeImagePath = file.Path;

                // Open a stream for the selected file.
                // The 'using' block ensures the stream is disposed
                // after the image is loaded.
                using (IRandomAccessStream fileStream =
                    await file.OpenAsync(FileAccessMode.Read))
                {
                    // Set the image source to the selected bitmap.
                    var bitmapImage = new BitmapImage();

                    bitmapImage.SetSource(fileStream);

                    UserMetadataLargeImagePreview.Source = bitmapImage;
                }

            }
        }

        // edit popup
        private async void BrowseSmallImage_Click(object sender, RoutedEventArgs e)
        {
            var filePicker = App.MainWindow.CreateOpenFilePicker();

            filePicker.SuggestedStartLocation = PickerLocationId.PicturesLibrary;
            filePicker.FileTypeFilter.Add(".png");
            filePicker.FileTypeFilter.Add(".jpg");
            filePicker.FileTypeFilter.Add(".svg");

            var file = await filePicker.PickSingleFileAsync();

            if (file != null)
            {
                // update the edit vm
                ViewModel.CustomizedSmallImagePath = file.Path;

                // Open a stream for the selected file.
                // The 'using' block ensures the stream is disposed
                // after the image is loaded.
                using IRandomAccessStream fileStream = await file.OpenAsync(FileAccessMode.Read);

                // Set the image source to the selected bitmap.
                var bitmapImage = new BitmapImage();

                bitmapImage.SetSource(fileStream);

                UserMetadataSmallImagePreview.Source = bitmapImage;
            }

        }

        private void OnOpenConsoleMonitor(object sender, RoutedEventArgs e)
        {
            // TODO: Change this to use the Endpoint Monitor Service

            try
            {
                string arguments =
                    " endpoint " +
                    ViewModel.DeviceInformation.EndpointDeviceId +
                    " monitor";

                using (var monitorProcess = new System.Diagnostics.Process())
                {
                    monitorProcess.StartInfo.FileName = "midi.exe";
                    monitorProcess.StartInfo.Arguments = arguments;

                    monitorProcess.Start();
                }
            }
            catch (Exception ex)
            {
                var dialog = new MessageDialog("Error opening console");
                dialog.Content = ex.ToString();

                dialog.ShowAsync().Wait();
            }
        }
    }
}
