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
using CommunityToolkit.WinUI.Controls;
using System.Windows.Input;

// To learn more about WinUI, the WinUI project structure,
// and more about our project templates, see: http://aka.ms/winui-project-info.

namespace Microsoft.Midi.Settings.Controls
{
    public sealed partial class MidiEndpointDeviceListItemControl : UserControl
    {
        bool _initialized = false;

        public MidiEndpointDeviceListItemControl()
        {
            //DataContext = this;

            //if (_endpointItem != null)
            //{
                this.InitializeComponent();
            //    _initialized = true;
            //}
        }

        private ICommand? _viewDeviceDetailsCommand = null;
        public ICommand? ViewDeviceDetailsCommand
        {
            get { return _viewDeviceDetailsCommand; }
            set
            {
                _viewDeviceDetailsCommand = value;

                RootSettingsCard.Command = ViewDeviceDetailsCommand;
            }
        }


        private MidiEndpointDeviceListItem? _endpointItem = null;
        public MidiEndpointDeviceListItem? EndpointItem
        { 
            get { return _endpointItem; } 
            set
            {
                if (value != null)
                {
                    _endpointItem = value;
                }
            }
        }


        // work around WinUI binding bug
        private void EndpointSettingsCard_Loaded(object sender, RoutedEventArgs e)
        {
            ((SettingsCard)sender).Command = ViewDeviceDetailsCommand;
        }

    }
}
