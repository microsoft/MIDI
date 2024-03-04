using System;
using System.Collections.Generic;
using System.Collections.ObjectModel;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using System.Windows.Input;
using CommunityToolkit.Mvvm.ComponentModel;
using CommunityToolkit.Mvvm.Input;
using Microsoft.Midi.Settings.Contracts.Services;
using Microsoft.Midi.Settings.Contracts.ViewModels;
using Microsoft.Midi.Settings.Models;
using Microsoft.UI.Dispatching;
using Microsoft.UI.Xaml;


//using Microsoft.Midi.Settings.SdkMock;

using Windows.Devices.Midi2;

namespace Microsoft.Midi.Settings.ViewModels
{
    public class DevicesViewModel : ObservableRecipient, INavigationAware
    {
        private MidiEndpointDeviceWatcher? _watcher = null;
        private DispatcherQueue _dispatcherQueue;

        private INavigationService _navigationService;

        public ICommand ViewDeviceDetailsCommand
        {
            get; private set;
        }

        public DevicesViewModel(INavigationService navigationService)
        {
            _navigationService = navigationService;

            _dispatcherQueue = DispatcherQueue.GetForCurrentThread();

            ViewDeviceDetailsCommand = new RelayCommand<MidiEndpointDeviceInformation>(
                async (param) =>
                {
                    System.Diagnostics.Debug.WriteLine("View Device Details Command exec");

                    _navigationService.NavigateTo(typeof(DeviceDetailViewModel).FullName!, param);

                });
        }

        public ObservableCollection<MidiEndpointDeviceInformation> MidiEndpointDevices { get; private set; } = 
            new ObservableCollection<MidiEndpointDeviceInformation>();

        public event EventHandler<MidiEndpointDeviceInformationUpdateEventArgs>? EndpointDeviceUpdated;



        // CollectionViewSource grouped by transport

        















        private void StartDeviceWatcher(bool includeAll)
        {
            if (_watcher != null)
            {
                ShutDownDeviceWatcher();
            }

            var filter = MidiEndpointDeviceInformationFilter.AllTypicalEndpoints;

            if (includeAll)
            {
                filter |= MidiEndpointDeviceInformationFilter.IncludeDiagnosticLoopback;
                filter |= MidiEndpointDeviceInformationFilter.IncludeDiagnosticPing;
                filter |= MidiEndpointDeviceInformationFilter.IncludeVirtualDeviceResponder;
            }

            _watcher = MidiEndpointDeviceWatcher.CreateWatcher(filter);

            _watcher.Stopped += OnDeviceWatcherStopped;
            _watcher.Updated += OnDeviceWatcherEndpointUpdated;
            _watcher.Removed += OnDeviceWatcherEndpointRemoved;
            _watcher.Added += OnDeviceWatcherEndpointAdded;
            _watcher.EnumerationCompleted += OnDeviceWatcherEnumerationCompleted;

            _watcher.Start();
        }

        private void OnDeviceWatcherEnumerationCompleted(MidiEndpointDeviceWatcher sender, object args)
        {
            // todo
        }

        private void OnDeviceWatcherEndpointAdded(MidiEndpointDeviceWatcher sender, MidiEndpointDeviceInformation args)
        {
            _dispatcherQueue.TryEnqueue(() =>
            {
                MidiEndpointDevices.Add(args);
            });
        }

        private void OnDeviceWatcherEndpointRemoved(MidiEndpointDeviceWatcher sender, global::Windows.Devices.Enumeration.DeviceInformationUpdate args)
        {
            _dispatcherQueue.TryEnqueue(() =>
            {
                foreach (MidiEndpointDeviceInformation info in MidiEndpointDevices)
                {
                    if (info.Id == args.Id)
                    {
                        MidiEndpointDevices.Remove(info);
                        break;
                    }
                }
            });
        }

        private void OnDeviceWatcherEndpointUpdated(MidiEndpointDeviceWatcher sender, MidiEndpointDeviceInformationUpdateEventArgs args)
        {
            _dispatcherQueue.TryEnqueue(() =>
            {
                if (EndpointDeviceUpdated != null)
                {
                    EndpointDeviceUpdated(sender, args);
                }
            });
        }

        private void OnDeviceWatcherStopped(MidiEndpointDeviceWatcher sender, object args)
        {
            // nothing to do.
        }

        private void ShutDownDeviceWatcher()
        {
            if (_watcher != null)
            {
                _watcher.Stop();

                _watcher.Stopped -= OnDeviceWatcherStopped;
                _watcher.Updated -= OnDeviceWatcherEndpointUpdated;
                _watcher.Removed -= OnDeviceWatcherEndpointRemoved;
                _watcher.Added -= OnDeviceWatcherEndpointAdded;

                _watcher = null;
            }
        }




        public void OnNavigatedFrom()
        {
            ShutDownDeviceWatcher();
        }


        public void OnNavigatedTo(object parameter)
        {
            StartDeviceWatcher(true);
        }
    }
}
