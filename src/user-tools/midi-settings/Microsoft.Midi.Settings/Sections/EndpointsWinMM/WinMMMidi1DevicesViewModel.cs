using CommunityToolkit.Mvvm.ComponentModel;
//using CommunityToolkit.WinUI.UI.Controls;
using Microsoft.Midi.Settings.Contracts.ViewModels;
using System;
using System.Collections.Generic;
using System.Collections.ObjectModel;
using System.ComponentModel;
using System.Linq;
using System.Security.Cryptography;
using System.Text;
using System.Threading.Tasks;
using Windows.Devices.Enumeration;
using Windows.Devices.Midi;


namespace Microsoft.Midi.Settings.ViewModels
{
    public class WinMMMidi1DevicesViewModel : ObservableRecipient, INavigationAware
    {
        public WinMMMidi1DevicesViewModel()
        {

        }

        public void OnNavigatedFrom()
        {
        }

        public void OnNavigatedTo(object parameter)
        {
        }
    }
}