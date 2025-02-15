namespace Microsoft.Midi.Settings.Contracts.Services;

public interface IMidiConfigFileService
{
    bool IsConfigFileActive { get; }

    string GetDefaultConfigName();
    string CleanupConfigName(string configName);
    string BuildConfigFileName(string configName);
    string GetConfigFilePath();

    bool ConfigExists(string configName);

    bool CreateNewConfigFile(string configName);
    bool SetCurrentConfig(string configName);


    bool StoreLoopbackEndpointPair(Microsoft.Windows.Devices.Midi2.Endpoints.Loopback.MidiLoopbackEndpointCreationConfig creationConfig);
}
