using System;
using System.Collections.Generic;
using System.Collections.ObjectModel;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using CommunityToolkit.Mvvm.ComponentModel;
using Microsoft.Midi.Settings.Contracts.ViewModels;
using Microsoft.Midi.Settings.Models;
using Microsoft.Midi.Settings.SdkMock;

namespace Microsoft.Midi.Settings.ViewModels
{
    public class DevicesViewModel : ObservableRecipient, INavigationAware
    {
        public DevicesViewModel()
        {
        
        }


        public ObservableCollection<MidiDevice> Devices
        {
            get
            {
                if (!AppState.Current.HasActiveMidiSession || AppState.Current.MidiSession is null)
                {
                    AppState.Current.CreateMidiSession();
                }

                return AppState.Current.MidiSession.Devices;

            }
        }








        public void OnNavigatedFrom()
        {
        }
        public void OnNavigatedTo(object parameter)
        {
        }
    }
}
