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


}


public interface IMidiConfigFileService
{
    bool IsConfigFileActive { get; }

    IMidiConfigFile? CurrentConfig { get; }

    string GetConfigFilesLocation();

    string GetDefaultConfigName();

    string CleanupConfigName(string configName);

    bool ConfigFileExists(string configFileName);

    bool CreateNewConfigFile(string configName, string configFileName);

    string BuildConfigLocalFileNameFromConfigName(string configName);

    bool UpdateRegistryCurrentConfigFile(string configFileName);


}
