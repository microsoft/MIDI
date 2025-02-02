using Microsoft.Win32;
using System;
using System.Collections.Generic;
using System.Collections.ObjectModel;
using System.Linq;
using System.Resources;
using System.Text;
using System.Threading.Tasks;
using Windows.UI.Popups;


namespace Microsoft.Midi.Settings.Models;

// TODO: Make this an injected singleton
public class AppState
{
    private MidiEndpointDeviceWatcher? _watcher = null;


    private static AppState? _current;

    private MidiDesktopAppSdkInitializer? _initializer;

    private AppState()
    {

    }

    public bool Initialize()
    {
        _initializer = Microsoft.Windows.Devices.Midi2.Initialization.MidiDesktopAppSdkInitializer.Create();

        if (_initializer == null)
        {
            // TODO: Failed
            return false;

        }

        if (!_initializer!.InitializeSdkRuntime())
        {
            // TODO: Localize these messages
            var dialog = new MessageDialog("Unable to initialize the Windows MIDI Services SDK runtime. Is it installed? Exiting.");
            dialog.ShowAsync().Wait();

            return false;
        }

        if (!_initializer!.EnsureServiceAvailable())
        {
            // TODO: Localize these messages
            var dialog = new MessageDialog("The Windows MIDI Services SDK is installed, but we failed to start the service. Exiting.");
            dialog.ShowAsync().Wait();

            return false;
        }

        StartDeviceWatcher(true);

        return true;
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


    public string GetCurrentSetupFileName()
    {
        // read from registry

        var val = (string)Registry.GetValue(@"HKEY_LOCAL_MACHINE\SOFTWARE\Microsoft\Windows MIDI Services", "CurrentConfig", "")!;

        return val;
    }

    public string GetInstalledSdkVersionString()
    {
        string val = "Unable to query SDK version";

        if (_initializer != null)
        {
            val = _initializer.GetInstalledSdkDescription(true, true, true);
        }

        return val;
    }

    public Uri GetMidiSdkInstallerUri()
    {
        return new Uri(MidiDesktopAppSdkInitializer.LatestMidiAppSdkDownloadUrl);
    }


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
