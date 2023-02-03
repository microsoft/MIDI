using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using CommunityToolkit.Mvvm.ComponentModel;
using Microsoft.Midi.Settings.Contracts.ViewModels;

namespace Microsoft.Midi.Settings.ViewModels
{
    public class DevicesViewModel : ObservableRecipient, INavigationAware
    {
        public DevicesViewModel()
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
