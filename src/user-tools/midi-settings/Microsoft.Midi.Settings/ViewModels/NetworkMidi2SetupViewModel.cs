// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

using CommunityToolkit.Mvvm.ComponentModel;
using System;
using System.Collections.Generic;
using System.Collections.ObjectModel;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

using Microsoft.Windows.Devices.Midi2.Endpoints.Network;
using Microsoft.UI.Dispatching;
using Microsoft.Midi.Settings.Contracts.Services;
using CommunityToolkit.WinUI.Collections;
using Microsoft.Midi.Settings.Contracts.ViewModels;

namespace Microsoft.Midi.Settings.ViewModels
{
    public class MidiNetworkRemoteHostEntry
    {
        public MidiNetworkAdvertisedHost? AdvertisedHost { get; internal set; }

        public string? Name { get; internal set; }
        public bool IsConnected { get; internal set; }

        public bool IsDirectConnect { get; internal set; }
        
        public string? DirectConnectionAddress { get; internal set; }

        public string StringifiedIpAddresses { get; internal set; }

    }


    public partial class NetworkMidi2SetupViewModel : ObservableRecipient, ISettingsSearchTarget
    {
        public static IList<string> GetSearchKeywords()
        {
            // TODO: these need to be localized, so should refer to resources instead
            return new[] { "endpoints", "ports", "network", "udp", "remote", "host", "connect", "wifi", "mdns" };
        }

        public static string GetSearchPageTitle()
        {
            return "Network MIDI 2.0 Setup";
        }

        public static string GetSearchPageDescription()
        {
            return "Set up a Network MIDI 2.0 host on this PC, and also connect to external network MIDI 2.0 hosts.";
        }

        private readonly IMidiConfigFileService _configFileService;
        private MidiNetworkAdvertisedHostWatcher _watcher;

        public ObservableCollection<MidiNetworkRemoteHostEntry> RemoteHostEntries = [];

        [ObservableProperty]
        private AdvancedCollectionView remoteHostEntriesSorted;

        public ObservableCollection<MidiNetworkRemoteHostEntry> LocalHostEntries = [];

        [ObservableProperty]
        private AdvancedCollectionView localHostEntriesSorted;


        public DispatcherQueue? DispatcherQueue { get; set; }

        [ObservableProperty]
        public string transportDescription;




        [ObservableProperty]
        private string newHostEndpointName;

        [ObservableProperty]
        private string newHostServiceInstanceName;

        [ObservableProperty]
        private string newHostProductInstanceId;

        [ObservableProperty]
        private bool newHostEnableAdvertising;

        [ObservableProperty]
        private bool newHostEnableMidi1Ports;

        [ObservableProperty]
        private bool newHostUseAutomaticPortNumber;

        [ObservableProperty]
        private string newHostPortNumber;



        [ObservableProperty]
        private string newClientIdentifier;

        [ObservableProperty]
        private string newClientDeviceId;

        [ObservableProperty]
        private string newClientHostNameOrIP;

        [ObservableProperty]
        private string newClientPortNumber;

        [ObservableProperty]
        private bool newClientEnableMidi1Ports;

        [ObservableProperty]
        private string newClientComment;


        public NetworkMidi2SetupViewModel(IMidiTransportInfoService transportInfoService, IMidiConfigFileService configFileService)
        {
            _configFileService = configFileService;

            if (transportInfoService != null)
            {
                transportDescription = transportInfoService.GetTransportForCode("NET2UDP").Description;
            }
            else
            {
                transportDescription = string.Empty;
            }

            InitializeNewHostSettings();
            InitializeNewClientSettings();

            RemoteHostEntriesSorted = new AdvancedCollectionView(RemoteHostEntries);
            RemoteHostEntriesSorted.SortDescriptions.Add(new SortDescription("Name", SortDirection.Ascending));


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

                    foreach (var ip in e.AddedHost.IPAddresses)
                    {
                        if (!String.IsNullOrEmpty(entry.StringifiedIpAddresses))
                        {
                            entry.StringifiedIpAddresses += "; ";
                        }

                        entry.StringifiedIpAddresses += ip.ToString() + ":" + e.AddedHost.Port.ToString();
                    }

                    // TODO: Need to figure out, for each entry, if it's a
                    // local midisrv entry or a remote entry
                    // the local entries get added to the second list (separate collection)
                    // use the fullname and check against our own list of hosts.

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


        private void InitializeNewHostSettings()
        {
            // TODO: Should verify this name doesn't already exist
            // TODO: Should also verify that the name is valid for DNS
            string defaultServiceName = (Environment.MachineName.ToLower() + "-midisrv-01");
            string defaultProductInstanceId = defaultServiceName;
            string defaultEndpointName = "Windows " + Environment.MachineName;

            NewHostEnableAdvertising = true;
            NewHostEnableMidi1Ports = true;
            NewHostUseAutomaticPortNumber = true;

            NewHostServiceInstanceName = defaultServiceName;
            NewHostEndpointName = defaultEndpointName;
            NewHostProductInstanceId = defaultProductInstanceId;

            // TODO: Additional properties once we have them enabled
        }

        public bool CreateHost()
        {
            var config = new MidiNetworkHostCreationConfig();

            config.Id = NewHostEndpointName + "_" + NewHostProductInstanceId;
            config.Name = NewHostEndpointName;
            config.ServiceInstanceName = NewHostServiceInstanceName.ToLower();
            config.ProductInstanceId = NewHostProductInstanceId;

            config.Advertise = NewHostEnableAdvertising;
            config.CreateOnlyUmpEndpoints = !NewHostEnableMidi1Ports;

            //System.Diagnostics.Debug.WriteLine("");
            //System.Diagnostics.Debug.WriteLine(config.GetConfigJson());
            //System.Diagnostics.Debug.WriteLine("");

            var result = MidiNetworkTransportManager.CreateNetworkHost(config);

            if (result.Success)
            {
                _configFileService.CurrentConfig.StoreNetworkHost(config);

                InitializeNewHostSettings();
            }

            return result.Success;
        }



        private void InitializeNewClientSettings()
        {
            NewClientDeviceId = string.Empty;
            NewClientHostNameOrIP = string.Empty;
            NewClientIdentifier = Guid.NewGuid().ToString();
            NewClientPortNumber = string.Empty;
            NewClientEnableMidi1Ports = true;
        }

        public bool CreateClientDirect()
        {
            var config = new MidiNetworkClientEndpointCreationConfig();

            config.Id = NewClientIdentifier;
            config.MatchCriteria.DeviceId = NewClientDeviceId;
            config.MatchCriteria.DirectHostNameOrIPAddress = NewClientHostNameOrIP;
            config.Comment = NewClientComment;

            ushort port;
            if (!string.IsNullOrEmpty(NewClientPortNumber) && ushort.TryParse(NewClientPortNumber, out port))
            {
                config.MatchCriteria.DirectPort = port;
            }

            //config.NetworkProtocol = udp;
            config.CreateOnlyUmpEndpoints = !NewClientEnableMidi1Ports;

            System.Diagnostics.Debug.WriteLine("");
            System.Diagnostics.Debug.WriteLine(config.GetConfigJson());
            System.Diagnostics.Debug.WriteLine("");

            var result = MidiNetworkTransportManager.CreateNetworkClient(config);

            if (result.Success)
            {
                _configFileService.CurrentConfig.StoreNetworkClient(config);

                InitializeNewClientSettings();
            }

            return result.Success;
        }

        public bool CreateClientFromMdns(string deviceId)
        {
            var config = new MidiNetworkClientEndpointCreationConfig();

            config.Id = Guid.NewGuid().ToString();
            config.MatchCriteria.DeviceId = deviceId;
            config.Comment = "From MDNS";   // TODO

            //config.NetworkProtocol = udp;
            config.CreateOnlyUmpEndpoints = false;  // todo

            System.Diagnostics.Debug.WriteLine("");
            System.Diagnostics.Debug.WriteLine(config.GetConfigJson());
            System.Diagnostics.Debug.WriteLine("");

            var result = MidiNetworkTransportManager.CreateNetworkClient(config);

            if (result.Success)
            {
                _configFileService.CurrentConfig.StoreNetworkClient(config);

                InitializeNewClientSettings();
            }

            return result.Success;
        }

    }
}
