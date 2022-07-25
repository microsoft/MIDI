using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

//using System.Text.Json;
using MidiConfig.Schema;
using System.Text.Json.Serialization;
using System.Text.Json;
using System.Security.AccessControl;
using System.Security.Principal;

using Microsoft.Extensions.Logging;

namespace MidiConfig
{
    public class MidiServicesConfig
    {
        private readonly ILogger<MidiServicesConfig> _logger;
        public MidiServicesConfig(ILogger<MidiServicesConfig> logger)
        {
            _logger = logger;

            // _logger.LogInformation("DEBUG: MidiServicesConfig constructor");

            MidiConfig = new Config();
            LoadDefaults();
        }

        public Config MidiConfig { get; set; }

        public void LoadDefaults()
        {
 
            _logger.LogDebug("MidiServicesConfig : Loading Defaults");

            // TODO: We need to get this schema version from someplace else
            MidiConfig.Header.SchemaVersion = new Version(1, 0, 0);

            MidiConfig.Locations.MainAppFolder = FileManager.AppDataFolder;
            MidiConfig.Locations.SetupsFolder = FileManager.DefaultSetupsFolder;
            MidiConfig.Locations.PluginsFolder = FileManager.DefaultPluginsFolder;
            MidiConfig.Locations.IconsFolder = FileManager.DefaultIconsFolder;

            MidiConfig.CurrentSetupFileNameWithoutPath = FileManager.DefaultSetupFileName;

            FileManager.LoadDefaults();

           CreateConfigFile(true);

        }

        public void Load()
        {
            _logger.LogDebug("MidiServicesConfig : Loading Config File");

            string fullPath = FileManager.ConfigFileName;

            if (!Directory.Exists(FileManager.ConfigFileFolder))
            {
                throw new Exception($"Config folder does not exist or is inaccessible. {FileManager.ConfigFileFolder}.");
            }

            if (!File.Exists(fullPath))
            {
                throw new Exception($"Config file does not exist or is inaccessible. {fullPath}.");
            }

            // Now we can load it up

            JsonSerializerOptions options = GetSerializerOptions();

            Config? config;

            using (FileStream stream = File.OpenRead(fullPath))
            {
                config = JsonSerializer.Deserialize<Config>(stream, options);

                if (config == null)
                {
                    // bad file
                    throw new Exception($"Config file could not deserialize. Corrupted? {fullPath}.");
                }
            }

            MidiConfig = config;
        }


        public void Save()
        {
            _logger.LogDebug("MidiServicesConfig : Saving Config File");

            CreateConfigFile();
        }


        private void CreateConfigFile(bool onlyIfMissing = false)
        {
            try
            {
                _logger.LogDebug("DEBUG: MidiServicesConfig : CreateConfigFile");

                string fileName = FileManager.ConfigFileName;

                _logger.LogDebug($"DEBUG: MainConfigFileLocation = {fileName}");

                string configDirectory = FileManager.ConfigFileFolder;

                if (!Directory.Exists(configDirectory))
                {
                    throw new Exception($"Configuration directory is missing. {configDirectory}");
                }

                // If the config file already exists, back it up

                if (File.Exists(fileName))
                {
                    //System.Diagnostics.Debug.WriteLine("MainConfigFile exists}");

                    if (onlyIfMissing)
                    {
                        return;
                    }

                    FileManager.MoveFileToBackup(fileName, FileType.ConfigFile);
                }

                // write new file
                JsonSerializerOptions options = GetSerializerOptions();

                using (FileStream stream = File.Create(fileName))
                {
                    JsonSerializer.Serialize<Config>(stream, MidiConfig, options);
                }

                _logger.LogDebug("DEBUG: MidiServicesConfig : CreateConfigFile completed");

            }
            catch (Exception ex)
            {
                _logger.LogError("Exception creating config file. " + ex.ToString());
            }
        }


        private JsonSerializerOptions GetSerializerOptions()
        {
            return new JsonSerializerOptions()
            {
                AllowTrailingCommas = true,
                WriteIndented = true,
            };
        }





    }
}
