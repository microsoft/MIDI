// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

using Microsoft.Win32;
using System;
using System.Collections.Generic;
using System.Collections.ObjectModel;
using System.Linq;
using System.Resources;
using System.Text;
using System.Threading.Tasks;
using Windows.UI.Popups;

using Microsoft.Windows.Devices.Midi2.Utilities.Update;


namespace Microsoft.Midi.Settings.Models;

// TODO: Make this an injected singleton
public class AppState
{
    private MidiEndpointDeviceWatcher? _watcher = null;


    private static AppState? _current;

    private MidiDesktopAppSdkInitializer? _initializer;
    private bool _serviceInitialized = false;

    private AppState()
    {

    }

    public bool IsServiceInitialized()
    {
        return _serviceInitialized;
    }

    public bool InitializeSdk()
    {
        try
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
                //var dialog = new MessageDialog("Unable to initialize the Windows MIDI Services SDK runtime. Is it installed? Exiting.");
                //dialog.ShowAsync().Wait();

                return false;
            }

            return true;
        }
        catch (Exception)
        {
            return false;
        }
    }

    public bool InitializeService()
    {
        try
        {
            if (_initializer == null)
            {
                // TODO: Failed
                return false;

            }

            if (!_initializer!.EnsureServiceAvailable())
            {
                // TODO: Localize these messages
                //var dialog = new MessageDialog("The Windows MIDI Services SDK is installed, but we failed to start the service.");
                //dialog.ShowAsync().Wait();

                return false;
            }

            _serviceInitialized = true;

            StartDeviceWatcher(true);

            return true;
        }
        catch (Exception)
        {
            return false;
        }
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

    // TODO: These should be in an Update service, not the state class

    public string GetInstalledSdkVersionString()
    {
        string val = "Unable to query SDK version";

        if (_initializer != null)
        {
            val = _initializer.GetInstalledSdkDescription(true, true, true);
        }

        return val;
    }


    // TODO: These should be in an Update service, not the state class

    public bool IsNewerSdkVersionAvailableForDownload()
    {
        if (_initializer != null)
        {
            return _initializer.IsNewerVersionAvailableForDownload();
        }

        return false;
    }

    // TODO: These should be in an Update service, not the state class

    public MidiRuntimeRelease GetNewerSdkDownloadInformation()
    {
        
    }


    //public Uri GetMidiSdkInstallerUri()
    //{
    //    return new Uri(MidiDesktopAppSdkInitializer.LatestMidiAppSdkDownloadUrl);
    //}


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
