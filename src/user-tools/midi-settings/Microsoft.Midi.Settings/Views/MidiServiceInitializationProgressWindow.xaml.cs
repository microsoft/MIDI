using Microsoft.Midi.Settings.Models;
using Microsoft.UI.Dispatching;
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

// To learn more about WinUI, the WinUI project structure,
// and more about our project templates, see: http://aka.ms/winui-project-info.

namespace Microsoft.Midi.Settings.Views
{
    /// <summary>
    /// An empty window that can be used on its own or navigated to within a Frame.
    /// </summary>
    public sealed partial class MidiServiceInitializationProgressWindow : WinUIEx.WindowEx
    {

        public MidiServiceInitializationProgressWindow()
        {
            //this.ExtendsContentIntoTitleBar = true;
            this.IsTitleBarVisible = false;

            HwndExtensions.ToggleWindowStyle(this.GetWindowHandle(), false, WindowStyle.Border );
            
            this.SetIsAlwaysOnTop(true);
            this.SetWindowSize(500, 250);
            this.CenterOnScreen();


            InitializeComponent();
        }

    }
}

