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
using Microsoft.Midi.Settings.Core.Contracts.Services;
using Microsoft.Midi.Settings.Core.Models;
using Microsoft.Midi.Settings.Core.Services;
using Microsoft.Midi.Settings.Models;
using Microsoft.Midi.Settings.SdkMock;
using Microsoft.Midi.Settings.SdkMock.Messages;
using Microsoft.Midi.Settings.Services;
using Microsoft.UI.Dispatching;
using Windows.UI.Core;

namespace Microsoft.Midi.Settings.ViewModels
{
    public class ToolsMonitorViewModel : ObservableRecipient, INavigationAware
    {
        private readonly INavigationService _navigationService;
        private readonly ISampleDataService _sampleDataService;

        public ObservableCollection<MidiMessageViewModel> Messages { get; } = new ObservableCollection<MidiMessageViewModel>();
        public ObservableCollection<string> UmpEndpointNames { get; } = new ObservableCollection<string>();
        public ObservableCollection<string> AllGroups { get; } = new ObservableCollection<string>();
        public ObservableCollection<string> AllChannels { get; } = new ObservableCollection<string>();


        public ICommand MonitorOnScreenCommand
        {
            get;
        }

        private DispatcherQueue _dispatcherQueue;

        public ToolsMonitorViewModel(INavigationService navigationService, ISampleDataService sampleDataService)
        {
            _navigationService = navigationService;
            _sampleDataService = sampleDataService;

            MonitorOnScreenCommand = new RelayCommand(OnStartMonitoringToScreenAsyncCommand);

            _dispatcherQueue = DispatcherQueue.GetForCurrentThread();
        }


        private MidiDevice _activeDevice;

        public async void OnStartMonitoringToScreenAsyncCommand()
        {
            System.Diagnostics.Debug.WriteLine("OnStartMonitoringToScreenAsyncCommand");

            if (AppState.Current.HasActiveMidiSession && AppState.Current.MidiSession != null && AppState.Current.MidiSession.Devices.Count > 0)
            {
                System.Diagnostics.Debug.WriteLine("Active MIDI Session: Using real data source");

                // try to get real data from the device in the session

                // right now, hard coded to device [0]. Change this later.
                _activeDevice = AppState.Current.MidiSession.Devices[0];

                _activeDevice.Messages.CollectionChanged += SourceDeviceMessages_CollectionChanged;

                if (!_activeDevice.IsReceiving)
                {
                    await _activeDevice.StartReceivingAsync();
                }

            }
            else
            {
                System.Diagnostics.Debug.WriteLine("No active MIDI Session: Using dummy data");

                // no active MIDI session, so revert back to dummy data
                // just for prototype demo purposes

                var messages = await _sampleDataService.GetUmpMonitorDataAsync();

                foreach (var item in messages)
                {
                    Messages.Add(item);
                }
            }
        }


        public async void OnNavigatedTo(object parameter)
        {
            Messages.Clear();
            UmpEndpointNames.Clear();


            var endpoints = await _sampleDataService.GetUmpEndpointNamesAsync();

            foreach (var item in endpoints)
            {
                UmpEndpointNames.Add(item);
            }

            AllGroups.Add("All (Keys, Tone Generator, Sequencer)");
            AllChannels.Add("All (1-16)");

            for (int i = 1; i <= 16; i++) 
            { 
                AllGroups.Add(i.ToString());
                AllChannels.Add(i.ToString());
            }

        }

        private void SourceDeviceMessages_CollectionChanged(object? sender, System.Collections.Specialized.NotifyCollectionChangedEventArgs e)
        {
            if (e.NewItems != null)
            {
                foreach (Ump ump in e.NewItems)
                {
                    // TEMP timestamp will never come from the client in the future. The service will set this
                    var timestamp = Environment.TickCount64;

                    // yet another copy. I know this is super inefficient.
                    var msgVM = new MidiMessageViewModel(timestamp, ump.Words, _activeDevice.Name, _activeDevice.Address);

                    if (_dispatcherQueue.HasThreadAccess)
                    {
                        Messages.Add(msgVM);
                    }
                    else
                    {
                        _dispatcherQueue.TryEnqueue(() => { Messages.Add(msgVM); });
                    }
                }
            }
        }



        public void OnNavigatedFrom()
        {
        }
    }
}
