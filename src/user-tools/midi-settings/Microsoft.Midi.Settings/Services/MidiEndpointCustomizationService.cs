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

        // TODO: Maybe create a common config type in the SDK for this specific task


//        MidiServiceConfig.UpdateTransportPluginConfig(configUpdate);

        // if success, also save to the local config file



        return true;
    }

}
