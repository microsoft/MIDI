using Microsoft.Midi.Settings.Contracts.Services;
using Microsoft.Midi.Settings.ViewModels;
using System;
using System.Collections.Generic;
using System.Collections.ObjectModel;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace Microsoft.Midi.Settings.Services
{
    public class MidiEndpointEnumerationService : IMidiEndpointEnumerationService
    {
        private List<MidiEndpointWrapper> _endpoints = [];
        public IList<MidiEndpointWrapper> GetEndpoints()
        {
            // return a copy
            return _endpoints.ToList();
        }


        public IList<MidiEndpointWrapper> GetEndpointsForTransportCode(string transportCode)
        {
            return _endpoints.Where(e => e.TransportCode == transportCode).ToList();
        }
        public IList<MidiEndpointWrapper> GetEndpointsForTransportId(Guid TransportId)
        {
            return _endpoints.Where(e => e.TransportId == TransportId).ToList();
        }

        public IList<MidiEndpointWrapper> GetEndpointsForPurpose(MidiEndpointDevicePurpose purpose)
        {
            return _endpoints.Where(e => e.DeviceInformation.EndpointPurpose == purpose).ToList();
        }


        private MidiEndpointDeviceWatcher? _watcher = null;

        public MidiEndpointDeviceWatcher MidiEndpointDeviceWatcher
        {
            get
            {
                return _watcher!;
            }
        }

        private void StartDeviceWatcher(bool includeAll)
        {
            if (_watcher != null)
            {
                ShutDownDeviceWatcher();
            }

            var filter = MidiEndpointDeviceInformationFilters.AllStandardEndpoints;

            if (includeAll)
            {
                filter |= MidiEndpointDeviceInformationFilters.DiagnosticLoopback;
                filter |= MidiEndpointDeviceInformationFilters.DiagnosticPing;
                filter |= MidiEndpointDeviceInformationFilters.VirtualDeviceResponder;
            }

            _watcher = MidiEndpointDeviceWatcher.Create(filter);

            _watcher.Stopped += OnDeviceWatcherStopped;
            //_watcher.Updated += OnDeviceWatcherEndpointUpdated;
            _watcher.Removed += OnDeviceWatcherEndpointRemoved;
            _watcher.Added += OnDeviceWatcherEndpointAdded;
            _watcher.EnumerationCompleted += OnDeviceWatcherEnumerationCompleted;

            _watcher.Start();
        }


        public void Start()
        {
            StartDeviceWatcher(true);
        }

        private readonly ISynchronizationContextService _synchronizationContextService;
        public MidiEndpointEnumerationService(ISynchronizationContextService synchronizationContextService)
        {
            _synchronizationContextService = synchronizationContextService;
        }


        private MidiEndpointWrapper BuildWrapper(MidiEndpointDeviceInformation deviceInformation)
        {
            return new MidiEndpointWrapper(
                deviceInformation,
                App.GetService<IMidiTransportInfoService>(),
                App.GetService<INavigationService>(),
                _synchronizationContextService);
        }


        private void OnDeviceWatcherEnumerationCompleted(MidiEndpointDeviceWatcher sender, object args)
        {
            // todo
        }

        private void OnDeviceWatcherEndpointAdded(MidiEndpointDeviceWatcher sender, MidiEndpointDeviceInformationAddedEventArgs args)
        {
            System.Diagnostics.Debug.WriteLine("MidiEndpointEnumerationService: EndpointAdded");

            _synchronizationContextService?.GetUIContext().Post(_ =>
            {
                _endpoints.Add(BuildWrapper(args.AddedDevice));
            }, null);
        }

        private void OnDeviceWatcherEndpointRemoved(MidiEndpointDeviceWatcher sender, MidiEndpointDeviceInformationRemovedEventArgs args)
        {
            foreach (MidiEndpointWrapper info in _endpoints)
            {
                if (info.Id == args.EndpointDeviceId)
                {
                    //_synchronizationContextService?.GetUIContext().Post(_ =>
                    //{
                        _endpoints.Remove(info);
                    //}, null);

                    break;
                }
            }
        }

        //private void OnDeviceWatcherEndpointUpdated(MidiEndpointDeviceWatcher sender, MidiEndpointDeviceInformationUpdatedEventArgs args)
        //{
        //    if (EndpointDeviceUpdated != null)
        //    {
        //        EndpointDeviceUpdated(sender, args);
        //    }
        //}

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
                //_watcher.Updated -= OnDeviceWatcherEndpointUpdated;
                //_watcher.Removed -= OnDeviceWatcherEndpointRemoved;
                //_watcher.Added -= OnDeviceWatcherEndpointAdded;

                _watcher = null;
            }
        }


    }
}
