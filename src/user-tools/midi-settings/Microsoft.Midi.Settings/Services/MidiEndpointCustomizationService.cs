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
        // build the json and send up through the transport

        var configUpdate = new MidiServiceEndpointCustomizationConfig(deviceInformation.GetTransportSuppliedInfo().TransportId);

        configUpdate.Name = updatedUserSuppliedInfo.Name;
        configUpdate.Description = updatedUserSuppliedInfo.Description;
        configUpdate.SmallImageFileName = updatedUserSuppliedInfo.SmallImagePath;
        configUpdate.LargeImageFileName = updatedUserSuppliedInfo.LargeImagePath;
        configUpdate.RequiresNoteOffTranslation = updatedUserSuppliedInfo.RequiresNoteOffTranslation;
        configUpdate.SupportsMidiPolyphonicExpression = updatedUserSuppliedInfo.SupportsMidiPolyphonicExpression;
        configUpdate.RecommendedControlChangeIntervalMilliseconds = updatedUserSuppliedInfo.RecommendedControlChangeAutomationIntervalMilliseconds;

        // TODO: May want to have additional criteria in the future
        configUpdate.MatchCriteria.EndpointDeviceId = deviceInformation.EndpointDeviceId;

        System.Diagnostics.Debug.WriteLine(configUpdate.GetConfigJson());

        var response = MidiServiceConfig.UpdateTransportPluginConfig(configUpdate);

        if (response.Status == MidiServiceConfigResponseStatus.Success)
        {
            // TODO: Save to local config file


            return true;
        }
        else
        {
            return false;
        }

    }

}
