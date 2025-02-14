namespace Microsoft.Midi.Settings.Contracts.Services;

public interface IMidiConfigFileService
{
    string GetDefaultConfigName();
    string CleanupConfigName(string configName);
    string BuildConfigFileName(string configName);
    string GetConfigFilePath();

    bool ConfigExists(string configName);

    bool CreateNewConfigFile(string configName);
    bool SetCurrentConfig(string configName);
}
