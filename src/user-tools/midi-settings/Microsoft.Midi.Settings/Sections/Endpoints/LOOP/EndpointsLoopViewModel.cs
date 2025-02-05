using CommunityToolkit.Mvvm.ComponentModel;
using CommunityToolkit.Mvvm.Input;
using Microsoft.Midi.Settings.Contracts.Services;
using Microsoft.Midi.Settings.Contracts.ViewModels;
using Microsoft.Midi.Settings.Models;
using Microsoft.Midi.Settings.Services;
using Microsoft.UI.Dispatching;
using Microsoft.UI.Xaml.Controls;
using System;
using System.Collections.Generic;
using System.Collections.ObjectModel;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using System.Windows.Input;

namespace Microsoft.Midi.Settings.ViewModels
{
    public partial class EndpointsLoopViewModel : SingleTransportEndpointViewModelBase, INavigationAware
    {
        public ICommand CreateLoopbackPairsCommand
        {
            get; private set;
        }


        public EndpointsLoopViewModel(INavigationService navigationService) : base("LOOP", navigationService)
        {
            CreateLoopbackPairsCommand = new RelayCommand(
                () =>
                {
                    System.Diagnostics.Debug.WriteLine("Create Loopback Pair Command exec");

                    // show popup

                    var window = new CreateLoopbackEndpointsWindow();

                    window.EndpointsCreated += Window_EndpointsCreated;

                    window.Show();

                });
        }

        private void Window_EndpointsCreated(object? sender, EventArgs e)
        {
            RefreshDeviceCollection();

            var s = sender as CreateLoopbackEndpointsWindow;

            if (s != null)
            {
                // unwire event
                s.EndpointsCreated -= Window_EndpointsCreated;
            }
        }
    }
}
