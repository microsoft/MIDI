// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

using Microsoft.Midi.Settings.Contracts.Services;
using global::Windows.Devices.Midi2.Transports.BasicLoopback;
using global::Windows.Devices.Midi2.Transports.Loopback;
using Windows.Foundation;

namespace Microsoft.Midi.Settings.Services;

public class MidiDefaultsService : IMidiDefaultsService
{
    const string DefaultLoopbackAUniqueId = "DEFAULT";
    const string DefaultLoopbackBUniqueId = "DEFAULT";

    const string DefaultBasicLoopbackUniqueId = "BASIC_DEF";

    public string GetDefaultMidiConfigName()
    {
        return MidiConfigConstants.DefaultConfigurationName;
    }
    public string GetDefaultMidiConfigFileName()
    {
        return MidiConfigConstants.DefaultConfigurationFileName;
    }

    private readonly ILoggingService _loggingService;
    public MidiDefaultsService(ILoggingService loggingService)
    {
        _loggingService = loggingService;
    }

    public bool DoesDefaultLoopbackAlreadyExist()
    {
        _loggingService.LogInfo($"Enter");

        if (MidiLoopbackManager.DoesLoopbackAExist(DefaultLoopbackAUniqueId))
        {
            return true;
        }
        else
        {
            return MidiLoopbackManager.DoesLoopbackBExist(DefaultLoopbackBUniqueId);
        }
    }

    public bool DoesDefaultBasicLoopbackAlreadyExist()
    {
        _loggingService.LogInfo($"Enter");

        return MidiBasicLoopbackManager.DoesLoopbackExist(DefaultBasicLoopbackUniqueId);
    }

}
