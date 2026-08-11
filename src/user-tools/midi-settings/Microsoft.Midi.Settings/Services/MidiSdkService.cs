// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

using Microsoft.Midi.Settings.Contracts.Services;


namespace Microsoft.Midi.Settings.Services;

public class MidiSdkService : IMidiSdkService
{
    private bool _serviceInitialized = false;

    private readonly ILoggingService _loggingService;
    public MidiSdkService(ILoggingService loggingService)
    {
        _loggingService = loggingService;
    }



    public bool IsServiceInitialized
    {
        get
        {
            return _serviceInitialized;
        }
    }


    public bool InitializeService()
    {
        _loggingService.LogInfo($"Enter");

        try
        {
             if (!MidiApi.EnsureServiceAvailable())
            {
                return false;
            }

            _serviceInitialized = true;

//            StartDeviceWatcher(true);

            return true;
        }
        catch (Exception ex)
        {
            _loggingService.LogError("Error initializing MIDI Service", ex);

            return false;
        }
    }




}
