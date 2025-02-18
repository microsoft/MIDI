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
using Windows.Storage.Pickers;

// To learn more about WinUI, the WinUI project structure,
// and more about our project templates, see: http://aka.ms/winui-project-info.

namespace Microsoft.Midi.Settings.Views
{
    /// <summary>
    /// An empty page that can be used on its own or navigated to within a Frame.
    /// </summary>
    public sealed partial class ToolsSysExPage : Page
    {
        public ToolsSysExViewModel ViewModel
        {
            get;
        }
        public ToolsSysExPage()
        {
            ViewModel = App.GetService<ToolsSysExViewModel>();

            this.Loaded += Page_Loaded;

            InitializeComponent();

        }


        private void Page_Loaded(object sender, RoutedEventArgs e)
        {
            ViewModel.DispatcherQueue = this.DispatcherQueue;

            ViewModel.RefreshDeviceCollection();
        }

        private async void OnPickSysExFile(object sender, RoutedEventArgs e)
        {
            var picker = new FileOpenPicker();

            var window = App.MainWindow;
            var hWnd = WinRT.Interop.WindowNative.GetWindowHandle(window);

            WinRT.Interop.InitializeWithWindow.Initialize(picker, hWnd);

            picker.ViewMode = PickerViewMode.List;
            picker.SuggestedStartLocation = PickerLocationId.Downloads;
            picker.FileTypeFilter.Add(".syx");
            picker.FileTypeFilter.Add(".mid");

            var file = await picker.PickSingleFileAsync();

            if (file != null)
            {
                ViewModel.SelectedFile = file;
                ViewModel.SelectedFileName = file.Name;
            }

        }
    }
}
