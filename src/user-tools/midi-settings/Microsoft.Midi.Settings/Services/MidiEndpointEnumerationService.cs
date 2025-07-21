using Microsoft.Midi.Settings.Contracts.Services;
using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace Microsoft.Midi.Settings.Services
{
    public class MidiEndpointEnumerationService : IMidiEndpointEnumerationService
    {
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
            //_watcher.Removed += OnDeviceWatcherEndpointRemoved;
            //_watcher.Added += OnDeviceWatcherEndpointAdded;
            _watcher.EnumerationCompleted += OnDeviceWatcherEnumerationCompleted;

            _watcher.Start();
        }

        private void OnDeviceWatcherEnumerationCompleted(MidiEndpointDeviceWatcher sender, object args)
        {
            // todo
        }

        //private void OnDeviceWatcherEndpointAdded(MidiEndpointDeviceWatcher sender, MidiEndpointDeviceInformationAddedEventArgs args)
        //{
        //    MidiEndpointDevices.Add(args.AddedDevice);
        //}

        //private void OnDeviceWatcherEndpointRemoved(MidiEndpointDeviceWatcher sender, MidiEndpointDeviceInformationRemovedEventArgs args)
        //{
        //    foreach (MidiEndpointDeviceInformation info in MidiEndpointDevices)
        //    {
        //        if (info.Id == args.Id)
        //        {
        //            MidiEndpointDevices.Remove(info);
        //            break;
        //        }
        //    }
        //}

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
