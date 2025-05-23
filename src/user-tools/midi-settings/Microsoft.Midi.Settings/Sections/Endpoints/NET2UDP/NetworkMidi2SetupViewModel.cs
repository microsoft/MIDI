using CommunityToolkit.Mvvm.ComponentModel;
using System;
using System.Collections.Generic;
using System.Collections.ObjectModel;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

using Microsoft.Windows.Devices.Midi2.Endpoints.Network;
using Microsoft.UI.Dispatching;

namespace Microsoft.Midi.Settings.ViewModels
{
    public class MidiNetworkRemoteHostEntry
    {
        public MidiNetworkAdvertisedHost? AdvertisedHost { get; internal set; }

        public string? Name { get; internal set; }
        public bool IsConnected { get; internal set; }

        public bool IsDirectConnect { get; internal set; }
        
        public string? DirectConnectionAddress { get; internal set; }

    }


    public partial class NetworkMidi2SetupViewModel : ObservableRecipient
    {
        private MidiNetworkAdvertisedHostWatcher _watcher;

        [ObservableProperty]
        public ObservableCollection<MidiNetworkRemoteHostEntry> remoteHostEntries = [];


        public DispatcherQueue? DispatcherQueue { get; set; }


        public NetworkMidi2SetupViewModel()
        {
            _watcher = MidiNetworkAdvertisedHostWatcher.Create();

            _watcher.Added += (s, e) =>
            {
                if (DispatcherQueue == null) return;

                DispatcherQueue.TryEnqueue(DispatcherQueuePriority.Normal, () =>
                {
                    var entry = new MidiNetworkRemoteHostEntry();
                    entry.Name = e.AddedHost.UmpEndpointName;
                    entry.AdvertisedHost = e.AddedHost;
                    entry.IsDirectConnect = false;

                    //TODO
                    entry.IsConnected = false;
                    entry.DirectConnectionAddress = null;


                    RemoteHostEntries.Add(entry);
                });
            };


            _watcher.Removed += (s, e) =>
            {
                // todo: remove host from collection
            };
        }

        public void RefreshHostsCollection()
        {
            _watcher.Start();
        }

    }
}
