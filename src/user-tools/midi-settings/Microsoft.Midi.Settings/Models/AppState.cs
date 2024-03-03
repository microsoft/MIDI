using System;
using System.Collections.Generic;
using System.Collections.ObjectModel;
using System.Linq;
using System.Resources;
using System.Text;
using System.Threading.Tasks;


namespace Microsoft.Midi.Settings.Models;

// TODO: Make this an injected singleton
public class AppState
{
    private MidiEndpointDeviceWatcher? _watcher = null;


    private static AppState? _current;

    private AppState()
    {
        StartDeviceWatcher(true);
    }

    public static AppState Current
    {
        get
        {
            if (_current is null)
                _current = new AppState();

            return _current;
        }
    }

    public MidiEndpointDeviceWatcher MidiEndpointDeviceWatcher
    {
        get
        {
            return _watcher;
        }
    }


    public IList<MidiServiceTransportPluginInfo> TransportPlugins
    {
        get { return MidiService.GetInstalledTransportPlugins(); }
    }

    private void StartDeviceWatcher(bool includeAll)
    {
        if (_watcher != null)
        {
            ShutDownDeviceWatcher();
        }

        var filter = MidiEndpointDeviceInformationFilters.AllTypicalEndpoints;

        if (includeAll)
        {
            filter |= MidiEndpointDeviceInformationFilters.IncludeDiagnosticLoopback;
            filter |= MidiEndpointDeviceInformationFilters.IncludeDiagnosticPing;
            filter |= MidiEndpointDeviceInformationFilters.IncludeVirtualDeviceResponder;
        }

        _watcher = MidiEndpointDeviceWatcher.CreateWatcher(filter);

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
