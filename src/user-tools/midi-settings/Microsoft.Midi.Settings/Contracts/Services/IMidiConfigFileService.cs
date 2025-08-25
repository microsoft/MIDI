// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

namespace Microsoft.Midi.Settings.Contracts.Services;



public class MidiConfigFileHeader
{
    public string Comment { get; set; }
    public string Name { get; set; }
    public string Product { get; set; }
    public double FileVersion { get; set; }
}

public interface IMidiConfigFile
{
    MidiConfigFileHeader? Header { get; }

    string FileName { get; }
    bool Load();


    bool StoreLoopbackEndpointPair(Microsoft.Windows.Devices.Midi2.Endpoints.Loopback.MidiLoopbackEndpointCreationConfig creationConfig);
    bool RemoveLoopbackEndpointPair(Guid associationId);


    bool StoreNetworkHost(Microsoft.Windows.Devices.Midi2.Endpoints.Network.MidiNetworkHostCreationConfig creationConfig);
    bool StoreNetworkClient(Microsoft.Windows.Devices.Midi2.Endpoints.Network.MidiNetworkClientEndpointCreationConfig creationConfig);


    bool StoreEndpointCustomization(Microsoft.Windows.Devices.Midi2.ServiceConfig.MidiServiceEndpointCustomizationConfig updateConfig);


}


public interface IMidiConfigFileService
{
    event EventHandler ActiveConfigFileChanged;

    bool IsConfigFileActive { get; }

    IMidiConfigFile? CurrentConfig { get; set; }

    string GetConfigFilesLocation();

    string GetDefaultConfigName();

    string CleanupConfigName(string configName);

    bool ConfigFileExists(string configFileName);

    bool CreateNewConfigFile(string configName, string configFileName);

    string BuildConfigLocalFileNameFromConfigName(string configName);

//    bool UpdateRegistryCurrentConfigFile(string configFileName);

}
