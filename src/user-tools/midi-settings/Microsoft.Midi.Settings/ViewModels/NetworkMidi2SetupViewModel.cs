// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

using CommunityToolkit.Mvvm.ComponentModel;
using CommunityToolkit.Mvvm.Input;
using CommunityToolkit.WinUI.Collections;
using Microsoft.Midi.Settings.Contracts.Services;
using Microsoft.Midi.Settings.Contracts.ViewModels;
using Microsoft.UI.Dispatching;
using Microsoft.Windows.Devices.Midi2.Endpoints.Network;
using System;
using System.Collections.Generic;
using System.Collections.ObjectModel;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using System.Windows.Input;
using Windows.Media.Protection.PlayReady;

namespace Microsoft.Midi.Settings.ViewModels
{
    public partial class MidiNetworkRemoteHostEntry : ObservableRecipient
    {
        public MidiNetworkAdvertisedHost? AdvertisedHost { get; internal set; }

        [ObservableProperty]
        private string? configEntryId;

        [ObservableProperty]
        private string? name;

        [ObservableProperty]
        private bool isConnected;

        [ObservableProperty]
        private bool isDirectConnect;

        [ObservableProperty]
        private string? remoteAddress;

        [ObservableProperty]
        private string? remotePort;

        [ObservableProperty]
        private string stringifiedIpAddresses;


        public ICommand ConnectCommand { get; private set; }
        public ICommand DisconnectCommand { get; private set; }

        public MidiNetworkRemoteHostEntry()
        {
            ConfigEntryId = Guid.NewGuid().ToString("B");

            ConnectCommand = new RelayCommand(() =>
            {
                // TEMP!

                if (AdvertisedHost == null)
                {
                    return;
                }

                var config = new MidiNetworkClientConnectConfig();
                config.Comment = AdvertisedHost.FullName;
                config.Id = ConfigEntryId;
                config.MatchCriteria.DeviceId = AdvertisedHost.DeviceId;
                config.MatchCriteria.DirectHostNameOrIPAddress = AdvertisedHost.IPAddresses[0];
                config.MatchCriteria.DirectPort = AdvertisedHost.Port;
                config.UmpEndpointName = AdvertisedHost.UmpEndpointName;

                var result = MidiNetworkTransportManager.ConnectNetworkClientAsync(config).GetAwaiter().GetResult();

                if (result.Success)
                {
                    IsConnected = true;
                }
                else
                {
                    System.Diagnostics.Debug.WriteLine("Unable to connect to remote host");
                    System.Diagnostics.Debug.WriteLine(result.ErrorInformation);
                }

            });

            DisconnectCommand = new RelayCommand(() =>
            {
                var config = new MidiNetworkClientDisconnectConfig();
                config.Id = ConfigEntryId;

                var result = MidiNetworkTransportManager.DisconnectNetworkClientAsync(config).GetAwaiter().GetResult();

                if (result.Success)
                {
                    IsConnected = false;
                }
                else
                {
                    System.Diagnostics.Debug.WriteLine("Unable to disconnect to remote host");
                    System.Diagnostics.Debug.WriteLine(result.ErrorInformation);
                }
            });

        }
    }


    public partial class NetworkMidi2SetupViewModel : ObservableRecipient, ISettingsSearchTarget, INavigationAware
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

        [ObservableProperty]
        private string newClientEndpointName;


        public ObservableCollection<MidiNetworkConfiguredHostWrapper> ConfiguredHosts { get; } = [];

        public ObservableCollection<MidiNetworkConfiguredClient> ConfiguredClients { get; } = [];

        public NetworkMidi2SetupViewModel(
            IMidiTransportInfoService transportInfoService, 
            IMidiConfigFileService configFileService)
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
                    entry.RemoteAddress = e.AddedHost.IPAddresses[0];
                    entry.RemotePort = e.AddedHost.Port.ToString();

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


                    FoldInRemoteHostConnection(entry);
                });
            };

            _watcher.EnumerationCompleted += (s, e) =>
            {
                LoadConfiguredClients();
            };


            _watcher.Removed += (s, e) =>
            {
                // todo: remove host from collection
            };

        }

        private void _watcher_EnumerationCompleted(MidiNetworkAdvertisedHostWatcher sender, object args)
        {
            throw new NotImplementedException();
        }

        public void RefreshHostsCollection()
        {
            _watcher.Start();
        }


        private void InitializeNewHostSettings()
        {
            // TODO: Should verify this name doesn't already exist
            // TODO: Should also verify that the name is valid for DNS
            string defaultServiceName = Environment.MachineName.ToLower() + "-midisrv-01";

            string defaultProductInstanceId = Environment.MachineName.ToLower() + "-midisrv";
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

            var result = MidiNetworkTransportManager.CreateNetworkHostAsync(config).GetAwaiter().GetResult();

            if (result.Success)
            {
                if (_configFileService.CurrentConfig != null)
                {
                    _configFileService.CurrentConfig.StoreNetworkHost(config);
                }
                else
                {
                    // unable to save. Config is null.
                }

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
            var config = new MidiNetworkClientConnectConfig();

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

            var result = MidiNetworkTransportManager.ConnectNetworkClientAsync(config).GetAwaiter().GetResult();

            if (result.Success)
            {
                if (_configFileService.CurrentConfig != null)
                {
                    _configFileService.CurrentConfig.StoreNetworkClient(config);
                }
                else
                {
                    // couldn't store the client. CurrentConfig is null
                }

                InitializeNewClientSettings();
            }

            return result.Success;
        }

        public bool CreateClientFromMdns(string deviceId)
        {
            //var config = new MidiNetworkClientEndpointCreationConfig();

            //config.Id = Guid.NewGuid().ToString();
            //config.MatchCriteria.DeviceId = deviceId;
            //config.Comment = "From MDNS";   // TODO

            ////config.NetworkProtocol = udp;
            //config.CreateOnlyUmpEndpoints = false;  // todo

            //System.Diagnostics.Debug.WriteLine("");
            //System.Diagnostics.Debug.WriteLine(config.GetConfigJson());
            //System.Diagnostics.Debug.WriteLine("");

            //var result = MidiNetworkTransportManager.CreateNetworkClient(config);

            //if (result.Success)
            //{
            //    if (_configFileService.CurrentConfig != null)
            //    {
            //        _configFileService.CurrentConfig.StoreNetworkClient(config);
            //    }
            //    else
            //    {
            //        // couldn't save. Current config is null
            //    }

            //    InitializeNewClientSettings();
            //}

            //return result.Success;

            return false;
        }


        private void LoadConfiguredHosts()
        {
            var hosts = MidiNetworkTransportManager.GetConfiguredHosts();

            ConfiguredHosts.Clear();

            foreach (var host in hosts)
            {
                ConfiguredHosts.Add(new MidiNetworkConfiguredHostWrapper(host, _configFileService));
            }

        }


        private void FoldInRemoteHostConnection(MidiNetworkRemoteHostEntry? entry)
        {
            if (entry == null)
            {
                return;
            }


            foreach (var client in ConfiguredClients)
            {
                if (!string.IsNullOrEmpty(client.ConnectedRemoteAddress) &&
                    !string.IsNullOrEmpty(client.ConnectedRemotePort) &&
                    entry.RemoteAddress == client.ConnectedRemoteAddress &&
                    entry.RemotePort == client.ConnectedRemotePort)
                {
                    entry.IsConnected = true;
                    // TODO: Other properties

                    break;
                }
            }

        }


        // TODO: Need to fold this in with the discovered remote hosts
        private void LoadConfiguredClients()
        {
            var clients = MidiNetworkTransportManager.GetConfiguredClients();

            ConfiguredClients.Clear();

            foreach (var client in clients)
            {
                ConfiguredClients.Add(client);

                // see if there's a remote host entry for this. If so, update it
                var entry = RemoteHostEntries.Where(e => (
                    e.RemoteAddress == client.ConnectedRemoteAddress &&
                    e.RemotePort == client.ConnectedRemotePort)).FirstOrDefault();

                FoldInRemoteHostConnection(entry);

            }

        }


        public void OnNavigatedTo(object parameter)
        {
            LoadConfiguredHosts();
            LoadConfiguredClients();
        }

        public void OnNavigatedFrom()
        {
            
        }
    }
}
