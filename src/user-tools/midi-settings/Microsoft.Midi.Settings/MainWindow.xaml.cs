// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

using Microsoft.Midi.Settings.Contracts.Services;
using Microsoft.Midi.Settings.Helpers;
using Microsoft.Midi.Settings.Services;
using Microsoft.UI.Xaml.Media;
using System.Runtime.InteropServices;
using System.Threading.Tasks;
using WinUIEx;

namespace Microsoft.Midi.Settings;

public sealed partial class MainWindow// : WinUIEx.WindowEx
{
    // Win32 API for setting window min/max size
    [DllImport("user32.dll", SetLastError = true)]
    private static extern bool SetWindowPos(IntPtr hWnd, IntPtr hWndInsertAfter, int X, int Y, int cx, int cy, uint uFlags);

    [DllImport("user32.dll", SetLastError = true)]
    private static extern bool GetWindowRect(IntPtr hWnd, out RECT lpRect);

    [StructLayout(LayoutKind.Sequential)]
    private struct RECT
    {
        public int Left;
        public int Top;
        public int Right;
        public int Bottom;
    }

    private const uint SWP_FRAMECHANGED = 0x0020;
    private const uint SWP_NOMOVE = 0x0002;
    private const uint SWP_NOSIZE = 0x0001;
    private const uint SWP_NOZORDER = 0x0004;
    private const uint SWP_NOACTIVATE = 0x0010;
    private const uint SWP_SHOWWINDOW = 0x0040;

    private const int WM_GETMINMAXINFO = 0x0024;

    [StructLayout(LayoutKind.Sequential)]
    private struct POINT
    {
        public int x;
        public int y;
    }

    [StructLayout(LayoutKind.Sequential)]
    private struct MINMAXINFO
    {
        public POINT ptReserved;
        public POINT ptMaxSize;
        public POINT ptMaxPosition;
        public POINT ptMinTrackSize;
        public POINT ptMaxTrackSize;
    }

    [DllImport("comctl32.dll", SetLastError = true)]
    private static extern int SetWindowSubclass(IntPtr hWnd, SubclassProc pfnSubclass, uint uIdSubclass, IntPtr dwRefData);

    [DllImport("comctl32.dll", SetLastError = true)]
    private static extern int RemoveWindowSubclass(IntPtr hWnd, SubclassProc pfnSubclass, uint uIdSubclass);

    [DllImport("comctl32.dll", SetLastError = true)]
    private static extern IntPtr DefSubclassProc(IntPtr hWnd, uint uMsg, IntPtr wParam, IntPtr lParam);

    private delegate IntPtr SubclassProc(IntPtr hWnd, uint uMsg, IntPtr wParam, IntPtr lParam, IntPtr uIdSubclass, IntPtr dwRefData);

    private IntPtr _hwnd;
    private SubclassProc? _subclassProc;
    private const int MinWindowWidth = 1000;
    private const int MinWindowHeight = 800;

    public MainWindow()
    {
        InitializeComponent();

        AppWindow.SetIcon(@"\Assets\AppIcon.ico");
        AppWindow.TitleBar.ButtonBackgroundColor = Microsoft.UI.Colors.Transparent;

        Content = null;
        Title = "AppDisplayName".GetLocalized();

        // 设置窗口最小尺寸
        this.Activated += MainWindow_Activated;

        //Content = new Microsoft.UI.Xaml.Controls.Grid(); // workaround for WinAppSDK bug http://task.ms/43347736
        //this.SystemBackdrop = new MicaBackdrop();


        //var icon = Icon.FromFile(@"Assets\DIN_Settings.ico");
        //this.SetTaskBarIcon(icon);


        //this.PositionChanged += MainWindow_PositionChanged;
        //this.SizeChanged += MainWindow_SizeChanged;

        this.Closed += MainWindow_Closed;
    }

    private void MainWindow_Activated(object sender, UI.Xaml.WindowActivatedEventArgs args)
    {
        if (_hwnd == IntPtr.Zero)
        {
            _hwnd = WinRT.Interop.WindowNative.GetWindowHandle(this);
            if (_hwnd != IntPtr.Zero)
            {
                _subclassProc = new SubclassProc(WindowSubclassProc);
                SetWindowSubclass(_hwnd, _subclassProc, 0, IntPtr.Zero);
            }
        }
    }

    private IntPtr WindowSubclassProc(IntPtr hWnd, uint uMsg, IntPtr wParam, IntPtr lParam, IntPtr uIdSubclass, IntPtr dwRefData)
    {
        if (uMsg == WM_GETMINMAXINFO)
        {
            MINMAXINFO mmi = Marshal.PtrToStructure<MINMAXINFO>(lParam);
            mmi.ptMinTrackSize.x = MinWindowWidth;
            mmi.ptMinTrackSize.y = MinWindowHeight;
            Marshal.StructureToPtr(mmi, lParam, false);
            return IntPtr.Zero;
        }
        return DefSubclassProc(hWnd, uMsg, wParam, lParam);
    }

    private async void MainWindow_Closed(object sender, UI.Xaml.WindowEventArgs args)
    {
        var settingsService = App.GetService<IGeneralSettingsService>();

        if (settingsService != null)
        {
            WindowRect r = new WindowRect(
                this.AppWindow.Position.X,
                this.AppWindow.Position.Y,
                this.AppWindow.Size.Width,
                this.AppWindow.Size.Height);

            await Task.Run(() =>
            {
                settingsService.SetMainWindowPositionAndSize(r);
            });
        }

    }

}
