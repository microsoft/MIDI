// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

using Microsoft.Midi.Settings.Contracts.Services;
using Microsoft.Midi.Settings.ViewModels;
using Microsoft.UI.Xaml.Controls;
using Windows.Storage.Pickers;
using Windows.UI.Popups;

namespace Microsoft.Midi.Settings.Views;

using System.Runtime.InteropServices;
using global::Windows.Win32.Foundation;
using global::Windows.Win32;
using global::Windows.Win32.UI.Shell;
using global::Windows.Win32.UI.Shell.Common;
using System.Text;

public sealed partial class ForDevelopersPage : Page
{
    ILoggingService _loggingService;

    public ForDevelopersViewModel ViewModel
    {
        get;
    }

    public ForDevelopersPage()
    {
        ViewModel = App.GetService<ForDevelopersViewModel>();
        _loggingService = App.GetService<ILoggingService>();

        InitializeComponent();
    }

    private async void ReplaceWdmaud2Button_Click(object sender, UI.Xaml.RoutedEventArgs e)
    {
        // you need to drop to Win32 here because WinUI file open picker does
        // not work when you are in admin mode. Bug first filed in 2022.

        string fileName = string.Empty;

        try
        {
            unsafe
            {
                string filterName = "Driver Files";
                string filterExtensions = "*.drv";

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
                        openDialog.SetFileName("wdmaud2.drv");
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


        if (!string.IsNullOrEmpty(fileName))
        {
            // verify file is named wdmaud2.drv
            if (!fileName.ToLower().EndsWith(@"\wdmaud2.drv"))
            {
                var dialog = new ContentDialog()
                {
                    Title = "Unexpected File",
                    Content = $"The filename {fileName} is not correct. Please locate and pick the copy of the correctly-named wdmaud2.drv you want to use to replace what is in System32.",
                    CloseButtonText = "OK"
                };

                dialog.XamlRoot = this.XamlRoot;

                var result = await dialog.ShowAsync();
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

            // Verify we're not already in System32
            if (fileName.ToLower() == Environment.ExpandEnvironmentVariables(@"%systemroot%\System32\wdmaud2.drv").ToLower())
            {
                var dialog = new ContentDialog()
                {
                    Title = "Incorrect file",
                    Content = $"You cannot copy wdmaud2.drv over itself. Please pick the file that is not already in System32.",
                    CloseButtonText = "OK"
                };

                dialog.XamlRoot = this.XamlRoot;

                var result = await dialog.ShowAsync();
                return;
            }


            if (ViewModel.ReplaceWdmaud2(fileName))
            {
                var dialog = new ContentDialog()
                {
                    Title = "Success",
                    Content = $"The wdmaud2.drv WinMM driver file has been updated.",
                    CloseButtonText = "OK"
                };

                dialog.XamlRoot = this.XamlRoot;

                var result = await dialog.ShowAsync();
            }
            else
            {
                var dialog = new ContentDialog()
                {
                    Title = "Replace Failed",
                    Content = $"The replacement process did not complete.",
                    CloseButtonText = "OK"
                };

                dialog.XamlRoot = this.XamlRoot;

                var result = await dialog.ShowAsync();
            }
        }
    }

    private async void PrepareForDeveloperInstallButton_Click(object sender, UI.Xaml.RoutedEventArgs e)
    {
        if (ViewModel.PrepareForDeveloperInstall())
        {
            var dialog = new ContentDialog()
            {
                Title = "Success",
                Content = $"Windows is now ready for you to install the developer Service and Components install, or install other unsigned service components.",
                CloseButtonText = "OK"
            };

            dialog.XamlRoot = this.XamlRoot;

            var result = await dialog.ShowAsync();

        }
        else
        {
            var dialog = new ContentDialog()
            {
                Title = "Unable to Set up",
                Content = $"Unable to prepare Windows for the developer Service and Components install.",
                CloseButtonText = "OK"
            };

            dialog.XamlRoot = this.XamlRoot;

            var result = await dialog.ShowAsync();
        }
    }


    // Note: This page can be directly navigated to even if dev options are off. So need to check that here and 
    // throw up a "enable dev options in settings" type of message
}
