// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================


using CommunityToolkit.WinUI.Animations;
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
using System.Runtime.InteropServices;
using System.Runtime.InteropServices.WindowsRuntime;
using Windows.Foundation;
using Windows.Foundation.Collections;
using Windows.Storage.Pickers;

using global::Windows.Win32.Foundation;
using global::Windows.Win32;
using global::Windows.Win32.UI.Shell;
using global::Windows.Win32.UI.Shell.Common;


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
        }

        private async void OnPickSysExFile(object sender, RoutedEventArgs e)
        {
            string fileName = string.Empty;

            try
            {
                unsafe
                {
                    string filterName = "SysEx Files";
                    string filterExtensions = "*.mid;*.syx";

                    fixed (char* pszName = filterName)
                    {
                        fixed (char* pszSpec = filterExtensions)
                        {
                            var filterspecs = new COMDLG_FILTERSPEC[1];
                            filterspecs[0].pszName = pszName;
                            filterspecs[0].pszSpec = pszSpec;

                            PInvoke.CoCreateInstance(
                                typeof(FileOpenDialog).GUID,
                                null,
                                global::Windows.Win32.System.Com.CLSCTX.CLSCTX_INPROC_SERVER,
                                out IFileOpenDialog openDialog).ThrowOnFailure();

                            openDialog.SetTitle("Open the wdmaud2.drv to be copied into System32");
                            openDialog.SetDefaultExtension("drv");
                            openDialog.SetFileTypes(filterspecs.AsSpan());
                            openDialog.Show((global::Windows.Win32.Foundation.HWND)WinRT.Interop.WindowNative.GetWindowHandle(App.MainWindow));

                            openDialog.GetResult(out IShellItem item);
                            item.GetDisplayName(SIGDN.SIGDN_FILESYSPATH, out PWSTR win32FileName);

                            fileName = win32FileName.ToString();

                            Marshal.FreeCoTaskMem(new IntPtr(win32FileName.Value));
                        }
                    }
                }
            }
            catch (Exception)
            {
                // when the user cancels, we get an exception
            }

            if (string.IsNullOrEmpty(fileName))
            {
                return;
            }

            // Verify the file exists
            if (!File.Exists(fileName))
            {
                var dialog = new ContentDialog()
                {
                    Title = "Missing File",
                    Content = $"The file {fileName} does not appear to exist.",
                    CloseButtonText = "OK"
                };

                dialog.XamlRoot = this.XamlRoot;

                var result = await dialog.ShowAsync();
                return;
            }

            ViewModel.SelectedFileName = fileName;
        }
    }
}
