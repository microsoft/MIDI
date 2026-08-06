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


    bool StoreLoopbackEndpointPair(Windows.Devices.Midi2.Transports.Loopback.MidiLoopbackEndpointCreationConfig creationConfig);
    bool RemoveLoopbackEndpointPair(Guid associationId);

    bool StoreBasicLoopbackEndpoint(Windows.Devices.Midi2.Transports.BasicLoopback.MidiBasicLoopbackEndpointCreationConfig creationConfig);
    bool RemoveBasicLoopbackEndpoint(Guid associationId);
    bool StoreBasicLoopbackMutedProperty(Guid associationId, bool isMuted);


    bool StoreNetworkHost(Windows.Devices.Midi2.Transports.Network.MidiNetworkHostCreationConfig creationConfig);
    bool StoreNetworkClient(Windows.Devices.Midi2.Transports.Network.MidiNetworkClientConnectConfig creationConfig);



    bool RemoveNetworkHost(string hostEntryId);

    bool StoreEndpointCustomization(Windows.Devices.Midi2.ServiceConfig.MidiServiceEndpointCustomizationConfig updateConfig);


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
