// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

using Microsoft.Midi.Settings.Contracts.Services;
using Microsoft.Midi.Settings.ViewModels;


namespace Microsoft.Midi.Settings.Services;

public class MidiEndpointEnumerationService : IMidiEndpointEnumerationService
{
    public event EventHandler<MidiEndpointDeviceInformation> EndpointUpdated;
    public event EventHandler<MidiEndpointDeviceInformation> EndpointAdded;
    public event EventHandler<MidiEndpointDeviceInformation> EndpointRemoved;



    private List<MidiEndpointWrapper> _endpoints = [];
    public IList<MidiEndpointWrapper> GetEndpoints()
    {
        // return a copy
        return _endpoints.ToList();
    }


    public IList<MidiEndpointWrapper> GetEndpointsForTransportCode(string transportCode)
    {
        return _endpoints.Where(e => e.TransportSuppliedInfo.TransportCode == transportCode).ToList();
    }
    public IList<MidiEndpointWrapper> GetEndpointsForTransportId(Guid TransportId)
    {
        return _endpoints.Where(e => e.TransportSuppliedInfo.TransportId == TransportId).ToList();
    }

    public IList<MidiEndpointWrapper> GetEndpointsForPurpose(MidiEndpointDevicePurpose purpose)
    {
        return _endpoints.Where(e => e.DeviceInformation.EndpointPurpose == purpose).ToList();
    }


    public MidiEndpointWrapper GetEndpointById(string endpointDeviceId)
    {
        return _endpoints.Where(e => e.Id == endpointDeviceId).FirstOrDefault()!;
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
        _loggingService.LogInfo("Enter");

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
        _watcher.Updated += OnDeviceWatcherEndpointUpdated;
        _watcher.Removed += OnDeviceWatcherEndpointRemoved;
        _watcher.Added += OnDeviceWatcherEndpointAdded;
        _watcher.EnumerationCompleted += OnDeviceWatcherEnumerationCompleted;

        _watcher.Start();

        _loggingService.LogInfo("Exit");
    }


    public void Start()
    {
        _loggingService.LogInfo("Enter");

        StartDeviceWatcher(false);

        _loggingService.LogInfo("Exit");
    }

    private readonly ISynchronizationContextService _synchronizationContextService;
    private readonly ILoggingService _loggingService;
    public MidiEndpointEnumerationService(
        ISynchronizationContextService synchronizationContextService, 
        ILoggingService loggingService)
    {
        _synchronizationContextService = synchronizationContextService;
        _loggingService = loggingService;
    }


    private MidiEndpointWrapper BuildWrapper(MidiEndpointDeviceInformation deviceInformation)
    {
        _loggingService.LogInfo("Building MidiEndpointWrapper");

        return new MidiEndpointWrapper(
            deviceInformation,
            App.GetService<IMidiTransportInfoService>(),
            App.GetService<INavigationService>(),
            _synchronizationContextService,
            App.GetService<IMidiConsoleToolsService>(),
            App.GetService<IMidiPanicService>(),
            _loggingService);

    }


    private void OnDeviceWatcherEnumerationCompleted(MidiEndpointDeviceWatcher sender, object args)
    {
        // todo
    }

    private void OnDeviceWatcherEndpointAdded(MidiEndpointDeviceWatcher sender, MidiEndpointDeviceInformationAddedEventArgs args)
    {
        _loggingService.LogInfo("Enter");

        System.Diagnostics.Debug.WriteLine("MidiEndpointEnumerationService: EndpointAdded");

        _synchronizationContextService?.GetUIContext().Post(_ =>
        {
            var info = BuildWrapper(args.AddedDevice);

            lock (_endpoints)
            {
                _endpoints.Add(info);
            }

            if (EndpointAdded != null)
            {
                EndpointAdded(this, args.AddedDevice);
            }
        }, null);

        _loggingService.LogInfo("Exit");
    }

    private void OnDeviceWatcherEndpointRemoved(MidiEndpointDeviceWatcher sender, MidiEndpointDeviceInformationRemovedEventArgs args)
    {
        _loggingService.LogInfo("Enter");

        foreach (MidiEndpointWrapper info in _endpoints)
        {
            if (info.Id == args.EndpointDeviceId)
            {
                lock (_endpoints)
                {
                    _endpoints.Remove(info);
                }

                if (EndpointRemoved != null)
                {
                    EndpointRemoved(this, info.DeviceInformation);
                }
                break;
            }
        }

        _loggingService.LogInfo("Exit");
    }

    private void OnDeviceWatcherEndpointUpdated(MidiEndpointDeviceWatcher sender, MidiEndpointDeviceInformationUpdatedEventArgs args)
    {
        _loggingService.LogInfo("Enter");

        foreach (MidiEndpointWrapper info in _endpoints)
        {
            if (info.Id == args.EndpointDeviceId)
            {
                info.RefreshData(sender.EnumeratedEndpointDevices[args.EndpointDeviceId]);

                if (EndpointUpdated != null)
                {
                    EndpointUpdated(this, info.DeviceInformation);
                }
                break;
            }
        }

        _loggingService.LogInfo("Exit");

    }

    private void OnDeviceWatcherStopped(MidiEndpointDeviceWatcher sender, object args)
    {
        // nothing to do.
    }

    private void ShutDownDeviceWatcher()
    {
        _loggingService.LogInfo("Enter");

        if (_watcher != null)
        {
            _watcher.Stop();

            _watcher.Stopped -= OnDeviceWatcherStopped;
            //_watcher.Updated -= OnDeviceWatcherEndpointUpdated;
            //_watcher.Removed -= OnDeviceWatcherEndpointRemoved;
            //_watcher.Added -= OnDeviceWatcherEndpointAdded;

            _watcher = null;
        }

        _loggingService.LogInfo("Exit");
    }


}
