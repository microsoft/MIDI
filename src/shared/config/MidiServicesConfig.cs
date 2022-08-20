// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License. See LICENSE in the project root for license information.

using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

//using System.Text.Json;
using System.Text.Json.Serialization;
using System.Text.Json;
using System.Security.AccessControl;
using System.Security.Principal;

using Microsoft.Extensions.Logging;
using Microsoft.Windows.Midi.Internal.Config.Schema;

namespace Microsoft.Windows.Midi.Internal.Config
{
    public class MidiServicesConfig
    {
        private readonly ILogger _logger;
        //public MidiServicesConfig(ILogger<MidiServicesConfig> logger)
        public MidiServicesConfig(ILogger logger)
        {
            _logger = logger;

             //_logger.LogDebug("DEBUG: MidiServicesConfig constructor");

            MidiConfig = new ConfigRoot();
            LoadDefaults();
        }

        public ConfigRoot MidiConfig { get; set; }

        public void LoadDefaults()
        {
            _logger.LogDebug("MidiServicesConfig : Loading Defaults");

            // TODO: We need to get this schema version from someplace else
            MidiConfig.Header.SchemaVersion = new Version(1, 0, 0);

            MidiConfig.DataLocations.MainAppFolder = FileManager.AppDataFolder;
            MidiConfig.DataLocations.SetupsFolder = FileManager.DefaultSetupsFolder;
            MidiConfig.DataLocations.IconsFolder = FileManager.DefaultIconsFolder;

            MidiConfig.PluginsLocations.PluginsRootFolder = FileManager.DefaultPluginsRootFolder;
            MidiConfig.PluginsLocations.DevicePluginsSubFolder = FileManager.DefaultPluginsDeviceSubFolder;
            MidiConfig.PluginsLocations.ProcessingPluginsSubFolder = FileManager.DefaultPluginsProcessingSubFolder;

            MidiConfig.CurrentSetupFileNameWithoutPath = FileManager.DefaultSetupFileName;

            MidiConfig.Settings.QueueWordCapacityEndpointIncoming = 5000;
            MidiConfig.Settings.QueueWordCapacityEndpointOutgoing = 5000;
            MidiConfig.Settings.QueueWordCapacitySessionEndpointIncoming = 5000;
            MidiConfig.Settings.QueueWordCapacitySessionEndpointOutgoing = 5000;

            //FileManager.LoadDefaults();

            //CreateConfigFile(true);

        }

        public bool Load()
        {
            string fullPath = FileManager.ConfigFileFullNameWithPath;

            _logger.LogDebug($"MidiServicesConfig : Loading Config File (checking for existing file at {fullPath})");

            if (!Directory.Exists(FileManager.ConfigFileFolder))
            {
                _logger.LogError($"Config folder does not exist or is inaccessible. {FileManager.ConfigFileFolder}.");

                return false;
                //throw new FileNotFoundException($"Config folder does not exist or is inaccessible. {FileManager.ConfigFileFolder}.");
            }

            if (!File.Exists(fullPath))
            {
                _logger.LogError($"Config file does not exist or is inaccessible. {fullPath}.");

                return false;
                //throw new FileNotFoundException($"Config file does not exist or is inaccessible. {fullPath}.");
            }

            try
            {
                _logger.LogDebug($"MidiServicesConfig : Opening file and deserializing");

                // Now we can load it up

                JsonSerializerOptions options = GetSerializerOptions();

                ConfigRoot? config;

                using (FileStream stream = File.OpenRead(fullPath))
                {
                    config = JsonSerializer.Deserialize<ConfigRoot>(stream, options);

                    if (config == null)
                    {
                        // bad file
                        throw new Exception($"Config file could not deserialize. Corrupted? {fullPath}.");
                    }
                }

                MidiConfig = config;

                return true;
            }
            catch (Exception ex)
            {
                _logger.LogError($"Failed to load config file {fullPath}. Is the file corrupted? " + ex.ToString());
                throw;
            }

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

                string fileName = FileManager.ConfigFileFullNameWithPath;

                _logger.LogDebug($"DEBUG: MainConfigFileLocation = {fileName}");

                string configDirectory = FileManager.ConfigFileFolder;

                if (!Directory.Exists(configDirectory))
                {
                    throw new Exception($"Configuration directory is missing. {configDirectory}");
                }

                // If the config file already exists, back it up

                if (File.Exists(fileName))
                {
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
                    JsonSerializer.Serialize<ConfigRoot>(stream, MidiConfig, options);
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
