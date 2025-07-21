// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

using CommunityToolkit.WinUI.Animations;
using System;
using System.Collections.Generic;
using System.Linq;
using System.Runtime.InteropServices;
using System.Text;
using System.Threading.Tasks;

using global::Windows.Win32.Foundation;
using global::Windows.Win32;
using global::Windows.Win32.UI.Shell;
using global::Windows.Win32.UI.Shell.Common;

namespace Microsoft.Midi.Settings.Services
{

#if false

    // this exists because native WinUI dialogs do not work when the app
    // is run as Administrator
    public class FileDialogService
    {
        public struct FilterInfo
        {
            public string Name;        // name to show to the user like "Driver Files"
            public string Extensions;  // extensions include wildcards like "*.drv"
        }

        public FileInfo? PickFile(string title, string defaultFileName, string defaultExtension, FilterInfo[] filters)
        {
            string fileName = string.Empty;

            try
            {
                unsafe
                {
                    COMDLG_FILTERSPEC[] filterspecs = new COMDLG_FILTERSPEC[filters.Length];

                    for (uint i = 0; i < filters.Length; i++)
                    {
                        filterspecs[i].pszName = (char*)filters[i].Name;
                        filterspecs[i].pszSpec = (char*)filters[i].Extensions;
                    }

                    fixed (char* pszName = filterName)
                    {
                        fixed (char* pszSpec = filterExtensions)
                        {
                            



                            PInvoke.CoCreateInstance(
                                typeof(FileOpenDialog).GUID,
                                null,
                                global::Windows.Win32.System.Com.CLSCTX.CLSCTX_INPROC_SERVER,
                                out IFileOpenDialog openDialog).ThrowOnFailure();

                            openDialog.SetTitle(title);
                            openDialog.SetFileName(defaultFileName);
                            openDialog.SetDefaultExtension(defaultExtension);
                            openDialog.SetFileTypes(filterspecs.AsSpan());
                            openDialog.Show((global::Windows.Win32.Foundation.HWND)WinRT.Interop.WindowNative.GetWindowHandle(App.MainWindow));

                            openDialog.GetResult(out IShellItem item);
                            item.GetDisplayName(SIGDN.SIGDN_FILESYSPATH, out PWSTR win32FileName);

                            fileName = win32FileName.ToString();

                            Marshal.FreeCoTaskMem(new IntPtr(win32FileName.Value));

                            return new FileInfo(fileName);
                        }
                    }
                }
            }
            catch (Exception)
            {
                // when the user cancels, we get an exception
                return null;
            }

        }


    }


#endif
}
