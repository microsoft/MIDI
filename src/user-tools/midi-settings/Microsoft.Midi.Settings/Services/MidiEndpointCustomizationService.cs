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

    public bool UpdateEndpoint(MidiEndpointDeviceInformation deviceInformation, MidiEndpointUserSuppliedInfo updatedUserSuppliedInfo)
    {
        // Assumption: Caller updates properties with existing values before sending.
        // That way, any values that are blank/empty we know will need to be cleared 
        // out within the service. In the locally stored json, we'll just leave those
        // values out.


        // build the json and send up through the transport

        // TODO: maybe create a common SDK type for the config, to ensure it and the service are in sync?
        // motivated people will find the code here if there's no config type, so obscurity won't help


//        MidiServiceConfig.UpdateTransportPluginConfig(configUpdate);

        // if success, also save to the local config file



        return true;
    }

}
