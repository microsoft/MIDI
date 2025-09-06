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

public class MidiEndpointCustomizationService : IMidiEndpointCustomizationService
{
    private readonly IMidiConfigFileService _configFileService;


    public MidiEndpointCustomizationService(IMidiConfigFileService configFileService)
    {
        _configFileService = configFileService;
    }

    public bool UpdateEndpoint(MidiServiceEndpointCustomizationConfig configUpdate)
    {
        App.GetService<ILoggingService>().LogInfo("Enter");

        // build the json and send up through the transport

        System.Diagnostics.Debug.WriteLine(configUpdate.GetConfigJson());

        var response = MidiServiceConfig.UpdateTransportPluginConfig(configUpdate);

        if (response.Status == MidiServiceConfigResponseStatus.Success)
        {
            return true;
        }
        else
        {
            return false;
        }

    }

}
